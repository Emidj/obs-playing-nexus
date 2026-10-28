// SPDX-License-Identifier: MIT
#pragma once
#include "playing/core/playing-core.hpp"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>
namespace playing::providers::traktor::detail {
using Microsoft::WRL::ComPtr;
inline std::filesystem::path documentsRoot(const std::string &version){
    PWSTR raw=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,KF_FLAG_DONT_VERIFY,nullptr,&raw)))return {};
    const std::filesystem::path result(raw);CoTaskMemFree(raw);return result/L"Native Instruments"/std::filesystem::u8path("Traktor "+version.substr(0,version.rfind('.')));
}
inline std::filesystem::path defaultCache(){
    std::wstring text(32768,L'\0');const auto size=GetEnvironmentVariableW(L"LOCALAPPDATA",text.data(),static_cast<DWORD>(text.size()));
    if(!size||size>=text.size())return {};text.resize(size);return std::filesystem::path(text)/L"OBS Playing Next/Artwork Cache";
}
// NI cache format observed in 4.4.1: format byte 8, uint32 LE width/height,
// followed by exactly width*height pixels in Windows BGRA order. Decode only
// this verified variant; no heuristic JPEG search or partial raw images.
inline std::vector<unsigned char> coverPng(const std::vector<unsigned char> &data){
    if(data.size()<13||data.size()>4*1024*1024||data[0]!=8)return {};
    std::uint32_t width=0,height=0;std::memcpy(&width,data.data()+1,4);std::memcpy(&height,data.data()+5,4);
    if(!width||!height||width>1024||height>1024||data.size()!=9+std::size_t(width)*height*4)return {};
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Apartment {HRESULT result;~Apartment(){if(SUCCEEDED(result))CoUninitialize();}} apartment{com};
    if(FAILED(com)&&com!=RPC_E_CHANGED_MODE)return {};
    ComPtr<IWICImagingFactory> factory;ComPtr<IStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))
        ||FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream))||FAILED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))
        ||FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))||FAILED(encoder->CreateNewFrame(&frame,nullptr))
        ||FAILED(frame->Initialize(nullptr))||FAILED(frame->SetSize(width,height)))return {};
    WICPixelFormatGUID pixel=GUID_WICPixelFormat32bppBGRA;
    if(FAILED(frame->SetPixelFormat(&pixel))||pixel!=GUID_WICPixelFormat32bppBGRA
        ||FAILED(frame->WritePixels(height,width*4,static_cast<UINT>(data.size()-9),const_cast<BYTE *>(data.data()+9)))
        ||FAILED(frame->Commit())||FAILED(encoder->Commit()))return {};
    STATSTG info{};if(FAILED(stream->Stat(&info,STATFLAG_NONAME))||info.cbSize.QuadPart<8||info.cbSize.QuadPart>4*1024*1024)return {};
    LARGE_INTEGER zero{};if(FAILED(stream->Seek(zero,STREAM_SEEK_SET,nullptr)))return {};
    std::vector<unsigned char> result(static_cast<std::size_t>(info.cbSize.QuadPart));ULONG count=0;
    if(FAILED(stream->Read(result.data(),static_cast<ULONG>(result.size()),&count))||count!=result.size())return {};
    return result;
}
inline std::string sha256(const std::vector<unsigned char> &bytes){
    BCRYPT_ALG_HANDLE algorithm=nullptr;if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    unsigned char hash[32]{};const auto result=BCryptHash(algorithm,nullptr,0,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),hash,32);BCryptCloseAlgorithmProvider(algorithm,0);
    if(result<0)return {};std::string text;for(auto x:hash){text+="0123456789abcdef"[x>>4];text+="0123456789abcdef"[x&15];}return text;
}
inline core::Artwork loadCover(const std::filesystem::path &library,const std::filesystem::path &cache,const std::string &key){
    using namespace core;
    if(key.empty())return {ArtworkStatus::Missing,{}};
    if(key.size()<5||key.size()>68||key[3]!='/'||key.find("..")!=std::string::npos||key.find('\\')!=std::string::npos
        ||!std::all_of(key.begin(),key.begin()+3,[](char c){return c>='0'&&c<='9';})
        ||!std::all_of(key.begin()+4,key.end(),[](char c){return (c>='A'&&c<='Z')||(c>='0'&&c<='9');}))return {ArtworkStatus::Failed,{}};
    if(!library.is_absolute()||!cache.is_absolute())return {ArtworkStatus::Missing,{}};
    try{
        for(const auto *variant:{"000","001","002"}){
            const auto source=library/L"Coverart"/std::filesystem::u8path(key+variant);std::error_code error;
            const auto size=std::filesystem::file_size(source,error);if(error||size<13||size>4*1024*1024)continue;
            std::ifstream input(source,std::ios::binary);std::vector<unsigned char> data(static_cast<std::size_t>(size));if(!input.read(reinterpret_cast<char *>(data.data()),static_cast<std::streamsize>(data.size())))continue;
            const auto id=sha256(data);if(id.empty())return {ArtworkStatus::Failed,{}};
            std::filesystem::create_directories(cache);const auto target=cache/std::filesystem::u8path("traktor-"+id+".png");
            if(!std::filesystem::is_regular_file(target)){
                const auto png=coverPng(data);if(png.empty())continue;
                std::uintmax_t total=0;for(const auto &entry:std::filesystem::directory_iterator(cache))if(entry.is_regular_file()){total+=entry.file_size();if(total>64*1024*1024)return {ArtworkStatus::Failed,{}};}
                if(total+png.size()>64*1024*1024)return {ArtworkStatus::Failed,{}};
                const auto temporary=std::filesystem::path(target.wstring()+L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetCurrentThreadId()));
                {std::ofstream output(temporary,std::ios::binary);output.write(reinterpret_cast<const char *>(png.data()),static_cast<std::streamsize>(png.size()));if(!output){std::filesystem::remove(temporary,error);return {ArtworkStatus::Failed,{}};}}
                std::filesystem::rename(temporary,target,error);if(error){std::filesystem::remove(temporary,error);if(!std::filesystem::is_regular_file(target))return {ArtworkStatus::Failed,{}};}
            }
            return {ArtworkStatus::Ready,ImageReference{ImageKind::LocalFile,target.u8string(),id}};
        }
        return {ArtworkStatus::Missing,{}};
    }catch(...){return {ArtworkStatus::Failed,{}};}
}
}
