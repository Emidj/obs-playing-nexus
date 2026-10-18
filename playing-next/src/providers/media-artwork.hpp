// SPDX-License-Identifier: MIT
#pragma once
#include "library-artwork.hpp"
#include <objbase.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <bcrypt.h>
namespace playing::providers::detail {
inline unsigned be(const unsigned char *p){return (unsigned(p[0])<<24)|(unsigned(p[1])<<16)|(unsigned(p[2])<<8)|p[3];}
inline unsigned sync(const unsigned char *p){return ((unsigned(p[0])&127)<<21)|((unsigned(p[1])&127)<<14)|((unsigned(p[2])&127)<<7)|(p[3]&127);}
// Seek atoms rather than loading video payloads; bounded depth/count/cover bytes.
inline std::vector<unsigned char> mp4Picture(std::ifstream &file,std::uint64_t begin,std::uint64_t end,unsigned depth,unsigned &count,bool cover=false){
    if(depth>8)return {};
    for(auto pos=begin;pos+8<=end&&++count<=4096;){
        file.clear();file.seekg(static_cast<std::streamoff>(pos));unsigned char h[16]{};if(!file.read(reinterpret_cast<char*>(h),8))return {};
        std::uint64_t size=be(h),header=8;if(size==1){if(!file.read(reinterpret_cast<char*>(h+8),8))return {};size=(std::uint64_t(be(h+8))<<32)|be(h+12);header=16;}else if(size==0)size=end-pos;
        if(size<header||size>end-pos)return {};const std::string type(reinterpret_cast<char*>(h+4),4);
        if(cover&&type=="data"){if(size<header+8||size-header-8>4*1024*1024)return {};file.seekg(static_cast<std::streamoff>(pos+header+8));std::vector<unsigned char> data(static_cast<std::size_t>(size-header-8));if(!file.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size())))return {};return data;}
        if(type=="moov"||type=="udta"||type=="meta"||type=="ilst"||type=="covr"){
            auto child=pos+header+(type=="meta"?4:0);if(child<=pos+size){auto found=mp4Picture(file,child,pos+size,depth+1,count,type=="covr");if(!found.empty())return found;}}
        pos+=size;
    }return {};
}
inline std::vector<unsigned char> mediaPicture(const std::filesystem::path &path){
    std::ifstream file(path,std::ios::binary);unsigned char head[10]{};if(!file.read(reinterpret_cast<char*>(head),10))return {};
    if(std::string(reinterpret_cast<char*>(head),3)=="ID3"&&(head[3]==3||head[3]==4)&&!(head[5]&128)){
        const auto size=sync(head+6);if(size<10||size>4*1024*1024)return {};std::vector<unsigned char> data(size);if(!file.read(reinterpret_cast<char*>(data.data()),size))return {};
        std::size_t offset=0;if(head[5]&64){if(size<4)return {};offset=head[3]==4?sync(data.data()):be(data.data())+4;}
        while(offset+10<=data.size()){const auto *p=data.data()+offset;const auto n=head[3]==4?sync(p+4):be(p+4);if(!n||n>data.size()-offset-10)break;
            if(std::string(reinterpret_cast<const char*>(p),4)=="APIC"&&p[8]==0&&p[9]==0){const auto *body=p+10;std::size_t i=1;while(i<n&&body[i])++i;if(i+2>=n)return {};i+=2;const bool wide=body[0]==1||body[0]==2;if(wide){while(i+1<n&&(body[i]||body[i+1]))i+=2;i+=2;}else{while(i<n&&body[i])++i;++i;}if(i<n)return {body+i,body+n};}
            offset+=10+n;
        }
    }else if(std::string(reinterpret_cast<char*>(head),4)=="fLaC"){
        file.clear();file.seekg(4);for(int block=0;block<64;++block){unsigned char h[4]{};if(!file.read(reinterpret_cast<char*>(h),4))break;const auto n=(unsigned(h[1])<<16)|(unsigned(h[2])<<8)|h[3];if((h[0]&127)==6){if(n<32||n>4*1024*1024)return {};std::vector<unsigned char> data(n);if(!file.read(reinterpret_cast<char*>(data.data()),n))return {};std::size_t i=4;const auto skip=[&](){if(i+4>data.size())return false;const auto length=be(data.data()+i);i+=4;if(length>data.size()-i)return false;i+=length;return true;};if(!skip()||!skip()||i+20>data.size())return {};i+=16;const auto length=be(data.data()+i);i+=4;if(length>data.size()-i)return {};return {data.data()+i,data.data()+i+length};}file.seekg(n,std::ios::cur);if(h[0]&128)break;}
    }else if(std::string(reinterpret_cast<char*>(head+4),4)=="ftyp"){unsigned count=0;return mp4Picture(file,0,std::filesystem::file_size(path),0,count);}return {};
}
inline std::optional<core::ImageReference> extractMediaCover(const std::string &name){
    try{const auto path=std::filesystem::u8path(name);if(!path.is_absolute()||!std::filesystem::is_regular_file(path))return {};const auto picture=mediaPicture(path);if(picture.size()<8||picture.size()>4*1024*1024)return {};
        const bool png=picture[0]==137&&picture[1]=='P'&&picture[2]=='N'&&picture[3]=='G',jpeg=picture[0]==255&&picture[1]==216;if(!png&&!jpeg)return {};
        // Deterministic content hash from BCrypt; cache files never contain credentials.
        BCRYPT_ALG_HANDLE algorithm=nullptr;if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};unsigned char digest[32]{};
        const auto status=BCryptHash(algorithm,nullptr,0,const_cast<PUCHAR>(picture.data()),static_cast<ULONG>(picture.size()),digest,32);BCryptCloseAlgorithmProvider(algorithm,0);if(status<0)return {};
        std::string id;for(auto c:digest){id+="0123456789abcdef"[c>>4];id+="0123456789abcdef"[c&15];}
        const auto root=environment(L"LOCALAPPDATA")/L"OBS Playing Next/Artwork Cache";std::filesystem::create_directories(root);const auto target=root/(id+(png?".png":".jpg"));
        if(!std::filesystem::exists(target)){std::uintmax_t resident=0;for(const auto &entry:std::filesystem::directory_iterator(root)){if(entry.is_regular_file()){resident+=entry.file_size();if(resident>64*1024*1024)return {};}}if(resident+picture.size()>64*1024*1024)return {};const auto temp=std::filesystem::path(target.wstring()+L".tmp-"+std::to_wstring(GetCurrentThreadId()));{std::ofstream output(temp,std::ios::binary);output.write(reinterpret_cast<const char*>(picture.data()),static_cast<std::streamsize>(picture.size()));if(!output)return {};}std::error_code error;std::filesystem::rename(temp,target,error);if(error)std::filesystem::remove(temp,error);}
        return core::ImageReference{core::ImageKind::LocalFile,target.u8string(),id};
    }catch(...){return {};}
}
// Only known library locations on mounted local drives; never recursively scan media.
inline std::vector<std::filesystem::path> virtualDjDatabases() {
    std::vector<std::filesystem::path> paths;
    const auto add = [&](const std::filesystem::path &path) {
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error)
            && std::find(paths.begin(), paths.end(), path) == paths.end()) paths.push_back(path);
    };
    for (const auto *name : {L"LOCALAPPDATA", L"APPDATA"}) {
        const auto root = environment(name);
        if (!root.empty()) add(root / L"VirtualDJ/database.xml");
    }
    for (const auto *name : {L"USERPROFILE", L"OneDrive", L"OneDriveConsumer"}) {
        const auto root = environment(name);
        if (!root.empty()) add(root / L"Documents/VirtualDJ/database.xml");
    }
    const DWORD drives = GetLogicalDrives();
    for (unsigned i = 0; i < 26; ++i) {
        if (!(drives & (DWORD(1) << i))) continue;
        std::wstring root = L"A:\\"; root[0] = static_cast<wchar_t>(L'A' + i);
        const auto type = GetDriveTypeW(root.c_str());
        if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) add(std::filesystem::path(root) / L"VirtualDJ/database.xml");
    }
    return paths;
}
inline std::string virtualDjAttribute(IXmlReader *reader, const wchar_t *name) {
    if (reader->MoveToAttributeByName(name, nullptr) != S_OK) return {};
    const wchar_t *value = nullptr; UINT n = 0;
    const auto status = reader->GetValue(&value, &n);
    std::wstring text;
    if (SUCCEEDED(status) && value && n <= 4096) text.assign(value, n);
    reader->MoveToElement();
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), 0);
    if (size) WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
// Require a complete scan and a single matching file, including across drives.
// XML Author is the VirtualDJ artist field. Remix is appended in its display title.
inline std::optional<std::string> virtualDjMediaPath(const core::TrackState &track,
    const std::vector<std::filesystem::path> &databases) {
    std::string chosen;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    std::uintmax_t total = 0;
    for (const auto &database : databases) {
        const auto size = std::filesystem::file_size(database);
        total += size;
        if (size > 64 * 1024 * 1024 || total > 128 * 1024 * 1024) return {};
        IStream *streamRaw = nullptr;
        if (FAILED(SHCreateStreamOnFileEx(database.c_str(), STGM_READ | STGM_SHARE_DENY_NONE,
            FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &streamRaw))) return {};
        const auto releaseStream = [](IStream *p) { p->Release(); };
        std::unique_ptr<IStream, decltype(releaseStream)> stream(streamRaw, releaseStream);
        IXmlReader *readerRaw = nullptr;
        if (FAILED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void **>(&readerRaw), nullptr))) return {};
        const auto releaseReader = [](IXmlReader *p) { p->Release(); };
        std::unique_ptr<IXmlReader, decltype(releaseReader)> reader(readerRaw, releaseReader);
        if (FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit))
            || FAILED(reader->SetInput(stream.get()))) return {};
        XmlNodeType type{}; std::string path, title, artist, remix;
        HRESULT status = S_OK;
        while ((status = reader->Read(&type)) == S_OK) {
            if (std::chrono::steady_clock::now() >= deadline) return {};
            const wchar_t *raw = nullptr; reader->GetLocalName(&raw, nullptr);
            const std::wstring tag = raw ? raw : L"";
            if (type == XmlNodeType_Element && tag == L"Song") {
                path = virtualDjAttribute(reader.get(), L"FilePath");
                title.clear(); artist.clear(); remix.clear();
            } else if (type == XmlNodeType_Element && tag == L"Tags") {
                title = virtualDjAttribute(reader.get(), L"Title");
                artist = virtualDjAttribute(reader.get(), L"Author");
                if (artist.empty()) artist = virtualDjAttribute(reader.get(), L"Artist");
                remix = virtualDjAttribute(reader.get(), L"Remix");
            } else if (type == XmlNodeType_EndElement && tag == L"Song") {
                if (title.empty()) {
                    const auto stem = std::filesystem::u8path(path).stem().u8string();
                    const auto split = stem.find(" - ");
                    if (split != std::string::npos) {
                        if (artist.empty()) artist = stem.substr(0, split);
                        title = stem.substr(split + 3);
                    } else title = stem;
                }
                if (artist != track.artist || (title != track.title
                    && (remix.empty() || title + " (" + remix + ")" != track.title))) continue;
                if (path.empty() || !std::filesystem::u8path(path).is_absolute()) return {};
                if (!chosen.empty() && _stricmp(chosen.c_str(), path.c_str()) != 0) return {};
                chosen = path;
            }
        }
        if (status != S_FALSE) return {}; // Malformed/truncated XML cannot establish uniqueness.
    }
    return chosen.empty() ? std::nullopt : std::optional<std::string>(chosen);
}
class VirtualDjCover {
    std::string key_;
    core::Artwork cover_;
    std::chrono::steady_clock::time_point until_{};
public:
    void enrich(core::TrackState &track) {
        const auto key = track.title + '\n' + track.artist;
        if (key == key_ && std::chrono::steady_clock::now() < until_) { track.artwork = cover_; return; }
        key_ = key; until_ = std::chrono::steady_clock::now() + std::chrono::seconds(10); cover_ = {};
        try {
            if (const auto chosen = virtualDjMediaPath(track, virtualDjDatabases())) {
                const auto ref = extractMediaCover(*chosen);
                cover_ = ref ? core::Artwork{core::ArtworkStatus::Ready, ref}
                    : core::Artwork{core::ArtworkStatus::Missing, {}};
            }
        } catch (...) {}
        track.artwork = cover_;
    }
};
}
