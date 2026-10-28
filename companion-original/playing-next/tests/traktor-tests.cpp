// SPDX-License-Identifier: MIT
#include "playing/providers/traktor-live.hpp"
#include "../src/providers/traktor-artwork.hpp"
#include <iostream>
#include <map>
#include <thread>
#include <cstring>
#include <functional>
#include <fstream>
using namespace playing::providers;
namespace {
unsigned checks=0;
void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
struct Fixture final:virtualdj::ProcessMemory {
    static constexpr std::uint64_t module=UINT64_C(0x140000000),heap=UINT64_C(0x200000000),clock=heap+0x5000;
    std::vector<unsigned char> image=std::vector<unsigned char>(0x3200000),data=std::vector<unsigned char>(0x40000);
    std::uint64_t stringNext=heap+0x28000;std::function<void(std::uint64_t)> beforeRead;bool alive_=true;unsigned queries=0;
    unsigned char *locate(std::uint64_t a,std::size_t n){
        if(a>=module&&a-module<=image.size()&&n<=image.size()-(a-module))return image.data()+static_cast<std::size_t>(a-module);
        if(a>=heap&&a-heap<=data.size()&&n<=data.size()-(a-heap))return data.data()+static_cast<std::size_t>(a-heap);return nullptr;
    }
    template<class T>void put(std::uint64_t a,T x){auto p=locate(a,sizeof(x));if(!p)throw std::runtime_error("fixture bounds");std::memcpy(p,&x,sizeof(x));}
    void text(std::uint64_t a,const std::string &s){auto p=locate(a,32);if(!p)throw std::runtime_error("fixture text");std::memset(p,0,32);
        put<std::uint64_t>(a+16,s.size());put<std::uint64_t>(a+24,s.size()<16?15:s.size());
        if(s.size()<16)std::memcpy(p,s.data(),s.size());else{stringNext=(stringNext+7)&~UINT64_C(7);put(a,stringNext);auto dest=locate(stringNext,s.size());if(!dest)throw std::runtime_error("fixture strings");std::memcpy(dest,s.data(),s.size());stringNext+=s.size()+1;}
    }
    static std::uint64_t deck(unsigned i){return heap+0x1000+i*0x1000;}
    static std::uint64_t track(unsigned i){return heap+0x10000+i*0x1000;}
    static std::uint64_t transport(unsigned i){return heap+0x6000+i*0x100;}
    void type(std::uint64_t table,unsigned index,const char *name){
        const auto col=module+0x3100000+index*64,td=module+0x20000+index*0x100;
        put(table-8,col);put<std::uint32_t>(col,1);put<std::uint32_t>(col+4,0);put<std::uint32_t>(col+12,static_cast<std::uint32_t>(td-module));put<std::uint32_t>(col+20,static_cast<std::uint32_t>(col-module));
        std::memcpy(locate(td+16,std::strlen(name)+1),name,std::strlen(name)+1);
    }
    Fixture(){
        put<std::uint16_t>(module,0x5a4d);put<std::uint32_t>(module+60,0x80);put<std::uint32_t>(module+0x80,0x4550);put<std::uint16_t>(module+0x84,0x8664);put<std::uint16_t>(module+0x86,2);put<std::uint16_t>(module+0x94,240);put<std::uint16_t>(module+0x98,0x20b);put<std::uint32_t>(module+0xd0,static_cast<std::uint32_t>(image.size()));
        const auto sections=module+0x80+24+240;
        std::memcpy(locate(sections,8),".rdata\0\0",8);put<std::uint32_t>(sections+8,0x100000);put<std::uint32_t>(sections+12,0x3100000);
        std::memcpy(locate(sections+40,8),".data\0\0\0",8);put<std::uint32_t>(sections+48,0x40000);put<std::uint32_t>(sections+52,0x10000);
        type(module+0x317a1d8,0,".?AV?$DeathNoteObservable@VTrackDeck@@@@");type(module+0x3183d60,1,".?AVMasterClock@@");type(module+0x31b5e80,2,".?AVContentEntryTrack@@");
        put(clock,module+0x3183d60);put(clock+16,transport(0));
        for(unsigned i=0;i<4;++i){const auto d=deck(i),t=track(i);put(d,module+0x317a1d8);text(d+80,"Channel"+std::string(1,static_cast<char>('A'+i)));text(d+112,"TrackDeck");put(d+256,transport(i));put(d+312,i==0?1.0:0.0);put(d+928,t);put(t,module+0x31b5e80);text(t+48,"Track "+std::to_string(i+1));text(t+80,u8"Björk — 宇多田");text(t+608,"");text(t+640,"");}
    }
    bool alive()const noexcept override{return alive_;}
    std::wstring imagePath()const override{return L"C:\\Fixture\\Traktor Pro 4.exe";}
    bool read(std::uint64_t a,void *p,std::size_t n)override{if(beforeRead)beforeRead(a);auto s=locate(a,n);if(!s)return false;std::memcpy(p,s,n);return true;}
    std::optional<Region> query(std::uint64_t a)override{++queries;if(a<module)return Region{a,module-a,false};if(a<module+image.size())return Region{module,image.size(),false};if(a<heap)return Region{a,heap-a,false};if(a<heap+data.size())return Region{heap,data.size(),true};return {};}
};
traktor::Read frame(traktor::MemoryReader &reader){std::atomic<bool> stop{false};for(unsigned i=0;i<200;++i){auto r=reader.read(stop);if(r.issue!=traktor::Issue::Searching)return r;}throw std::runtime_error("fixture discovery timeout");}
void contract(){
    using namespace traktor;check(acceptsVersion("4.4.1.22")&&acceptsVersion("4.0.0.0")&&acceptsVersion("4.4.1.23")&&acceptsVersion("4.9.9.9")&&acceptsVersion("3.5.0.270")&&acceptsVersion("3.11.0.0"),"3.x/4.x families rejected");check(!acceptsVersion("2.11.0.0")&&!acceptsVersion("5.0.0.0")&&!acceptsVersion("4.99999.0.0")&&!acceptsVersion("4.x"),"Invalid version family accepted");
    Fixture memory;MemoryReader reader(memory,Fixture::module,verifiedVersion);auto first=frame(reader);check(first.issue==Issue::None,"Traktor frame discovery");check(memory.queries>0,"No address-space query");
    check(first.frame.decks[0].track->title=="Track 1"&&first.frame.decks[0].track->artist==u8"Björk — 宇多田","UTF8 metadata");
    auto chosen=select(first.frame);check(chosen.deck==1u&&chosen.observation.playback==playing::core::Playback::Playing,"Master A selection");
    const auto q=memory.queries;check(frame(reader).issue==Issue::None&&q==memory.queries,"Discovery repeated after caching");
    memory.put(Fixture::deck(1)+312,1.0);memory.put(Fixture::clock+16,Fixture::transport(1));auto second=frame(reader);chosen=select(second.frame,1);check(chosen.deck==2u&&chosen.observation.metadata->title=="Track 2","Master B did not replace A");
    auto noMaster=second.frame;noMaster.master=0;check(select(noMaster).issue==Issue::Ambiguous,"Invented winner with two playing decks");check(select(noMaster,1).deck==1u,"Previous valid deck lost without master");
    memory.put(Fixture::deck(0)+312,0.0);memory.put(Fixture::deck(1)+312,0.0);chosen=select(frame(reader).frame,2);check(!chosen.observation.metadata&&chosen.observation.playback==playing::core::Playback::Paused,"Pause retained active text");
    memory.put<std::uint64_t>(Fixture::deck(3)+928,0);check(frame(reader).issue==Issue::None,"Empty deck rejected");
    memory.put(Fixture::deck(3)+312,1.0);check(frame(reader).issue==Issue::Changed,"Playing deck with no metadata accepted");memory.put(Fixture::deck(3)+312,0.0);
    memory.text(Fixture::track(1)+608,"053");memory.text(Fixture::track(1)+640,"ABC123");check(frame(reader).frame.decks[1].coverKey=="053/ABC123","Artwork correlation");
    memory.text(Fixture::track(1)+640,"../BAD");check(frame(reader).issue==Issue::Layout,"Cover traversal accepted");memory.text(Fixture::track(1)+640,"ABC123");
    memory.put<std::uint64_t>(Fixture::track(0)+48+16,5000);check(frame(reader).issue==Issue::Changed,"Unbounded title accepted");memory.text(Fixture::track(0)+48,"Track 1");
    memory.text(Fixture::track(0)+80,std::string("bad\xff"));check(frame(reader).issue==Issue::Changed,"Invalid UTF8 accepted");memory.text(Fixture::track(0)+80,"Artist");
    unsigned masterReads=0;memory.beforeRead=[&](std::uint64_t a){if(a==Fixture::clock+16&&++masterReads==2)memory.put(Fixture::clock+16,Fixture::transport(2));};check(frame(reader).issue==Issue::Changed,"Master race accepted");memory.beforeRead={};
    auto duplicate=first.frame;duplicate.decks[1].address=duplicate.decks[0].address;check(select(duplicate).issue==Issue::Layout,"Duplicate deck accepted");
    std::atomic<bool> cancelled{true};check(reader.read(cancelled).issue==Issue::Searching,"Discovery did not cancel");
    MemoryReader unknown(memory,Fixture::module,"5.0.0.0");check(frame(unknown).issue==Issue::Version,"Unsafe version accepted");
    memory.put<std::uint64_t>(Fixture::deck(0),Fixture::module+0x1234);check(frame(reader).issue==Issue::Layout,"Stale deck object accepted");
    Fixture relocated;
    const std::array<std::uint64_t,3> old{{0x317a1d8,0x3183d60,0x31b5e80}};
    const std::array<const char *,3> names{{".?AV?$DeathNoteObservable@VTrackDeck@@@@",".?AVMasterClock@@",".?AVContentEntryTrack@@"}};
    for(unsigned i=0;i<3;++i){relocated.put<std::uint64_t>(Fixture::module+old[i]-8,0);relocated.type(Fixture::module+old[i]+0x1000,i,names[i]);}
    relocated.put(Fixture::clock,Fixture::module+old[1]+0x1000);
    for(unsigned i=0;i<4;++i){relocated.put(Fixture::deck(i),Fixture::module+old[0]+0x1000);relocated.put(Fixture::track(i),Fixture::module+old[2]+0x1000);}
    MemoryReader family(relocated,Fixture::module,"4.0.0.0");auto dynamic=frame(family);check(dynamic.issue==Issue::None&&dynamic.frame.decks[0].track->title=="Track 1","Relocated 4.x RTTI discovery");
    const auto clockTable=Fixture::module+old[1]+0x1000;std::uint64_t col=0;relocated.read(clockTable-8,&col,8);std::array<std::uint32_t,6> descriptor{};relocated.read(col,descriptor.data(),sizeof(descriptor));descriptor[5]+=4;
    std::memcpy(relocated.locate(col+4,sizeof(descriptor)),descriptor.data(),sizeof(descriptor));relocated.put(clockTable-8,col+4);
    check(frame(family).issue==Issue::None,"4-byte MSVC RTTI alignment rejected");
    Fixture third;
    for(unsigned i=0;i<3;++i){third.put<std::uint64_t>(Fixture::module+old[i]-8,0);third.type(Fixture::module+old[i]+0x1000,i,names[i]);}
    third.put(Fixture::clock,Fixture::module+old[1]+0x1000);
    for(unsigned i=0;i<4;++i){const auto d=Fixture::deck(i),t=Fixture::track(i);
        std::memset(third.locate(d,1024),0,1024);third.put(d,Fixture::module+old[0]+0x1000);
        third.text(d+56,"Channel"+std::string(1,static_cast<char>('A'+i)));third.text(d+88,"TrackDeck");
        third.put(d+232,Fixture::transport(i));third.put(d+288,i==0?1.0:0.0);third.put(d+872,t);
        third.put(t,Fixture::module+old[2]+0x1000);third.text(t+560,i==0?"048\\QNZDY4CYHMR05DV5EIVJBMU0Y4DC":"");
    }
    MemoryReader legacy(third,Fixture::module,verifiedVersion3);auto v3=frame(legacy);
    check(v3.issue==Issue::None&&v3.frame.decks[0].track->title=="Track 1","Traktor 3 dynamic RTTI/layout");
    check(v3.frame.decks[0].coverKey=="048/QNZDY4CYHMR05DV5EIVJBMU0Y4DC","Traktor 3 combined cover reference");
    third.put(Fixture::deck(1)+288,1.0);third.put(Fixture::clock+16,Fixture::transport(1));
    check(select(frame(legacy).frame,1).deck==2u,"Traktor 3 MASTER switch");
    third.put(Fixture::deck(0)+288,0.0);third.put(Fixture::deck(1)+288,0.0);
    check(select(frame(legacy).frame).observation.playback==playing::core::Playback::Paused,"Traktor 3 pause");
    third.text(Fixture::track(0)+560,"048\\..\\BAD");check(frame(legacy).issue==Issue::Layout,"Traktor 3 cover traversal");
    third.text(Fixture::track(0)+560,"");check(frame(legacy).issue==Issue::None,"Traktor 3 missing cover");
    MemoryReader wrongLayout(third,Fixture::module,verifiedVersion);check(frame(wrongLayout).issue==Issue::Layout,"Cross-major deck layout accepted");
    MemoryReader emptyVersion(third,Fixture::module,"");check(frame(emptyVersion).issue==Issue::Version,"Empty version accepted");
}
std::vector<unsigned char> rawCover(){std::vector<unsigned char> b(17,0);b[0]=8;b[1]=2;b[5]=1;b[11]=255;b[12]=255;b[13]=255;b[16]=255;return b;}
void artwork(){
    using namespace traktor::detail;auto raw=rawCover();auto png=coverPng(raw);check(png.size()>8&&png[0]==137&&png[1]=='P',"NI artwork PNG conversion");
    auto wrong=raw;wrong[0]=9;check(coverPng(wrong).empty(),"Unknown NI pixel format accepted");wrong=raw;wrong.pop_back();check(coverPng(wrong).empty(),"Truncated NI cover accepted");wrong=raw;wrong[1]=0;check(coverPng(wrong).empty(),"Zero width accepted");
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> image;
    check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateStream(&stream))&&SUCCEEDED(stream->InitializeFromMemory(png.data(),static_cast<DWORD>(png.size())))&&SUCCEEDED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder))&&SUCCEEDED(decoder->GetFrame(0,&image)),"PNG decode");
    UINT width=0,height=0;check(SUCCEEDED(image->GetSize(&width,&height))&&width==2&&height==1,"NI cover dimensions");
    ComPtr<IWICFormatConverter> converter;unsigned char pixels[8]{};check(SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(image.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))&&SUCCEEDED(converter->CopyPixels(nullptr,8,8,pixels))&&pixels[0]==255&&pixels[1]==0&&pixels[2]==0&&pixels[4]==0&&pixels[6]==255,"BGRA color/channel order");
    converter.Reset();image.Reset();decoder.Reset();stream.Reset();factory.Reset();if(SUCCEEDED(com))CoUninitialize();
    const auto temporary=std::filesystem::temp_directory_path();const auto root=temporary/std::filesystem::path(L"playing-traktor-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    check(root.is_absolute()&&std::filesystem::equivalent(root.parent_path(),temporary)&&!std::filesystem::exists(root),"Fixture cleanup scope");std::filesystem::create_directories(root/L"Coverart/053");
    {std::ofstream out(root/L"Coverart/053/ABC000",std::ios::binary);out.write(reinterpret_cast<const char *>(raw.data()),static_cast<std::streamsize>(raw.size()));}
    const auto art=loadCover(root,root/L"cache","053/ABC");check(art.status==playing::core::ArtworkStatus::Ready&&art.image&&std::filesystem::is_regular_file(std::filesystem::u8path(art.image->location)),"Artwork file cache");
    check(loadCover(root,root/L"cache","053/../../BAD").status==playing::core::ArtworkStatus::Failed,"Cache path traversal");check(loadCover(root,root/L"cache","053/ABSENT").status==playing::core::ArtworkStatus::Missing,"Missing cover");
    std::filesystem::remove_all(root);
}
void lifecycle(){
    using namespace traktor;ProviderRegistry registry;check(registerProvider(registry)==Registration::Registered,"Traktor registration");check(registerProvider(registry)==Registration::Duplicate,"Duplicate registration");
    check(registry.descriptors().size()==1&&registry.descriptors()[0].automatic&&!registry.descriptors()[0].history,"False History capability");
    bool rejected=false;try{makeLive({"traktor",Mode::History,{}});}catch(const std::invalid_argument &){rejected=true;}check(rejected,"History request accepted");
    auto opens=std::make_shared<std::atomic<unsigned>>(0);Options options;options.opener=[opens]{++*opens;return virtualdj::Connection{virtualdj::ProcessIssue::None,std::make_unique<Fixture>(),Fixture::module,verifiedVersion};};
    ProviderRegistry shared;check(shared.registerProvider({"traktor","Fixture",true,false},[options](const Request &r){return makeLive(r,options);})==Registration::Registered,"Fixture registration");
    const Request request{"traktor",Mode::Automatic,{}};const auto a=shared.acquire(request),b=shared.acquire(request);check(a.result==Acquisition::Acquired&&b.result==Acquisition::Acquired,"Shared acquisition");
    bool ready=false;for(unsigned i=0;i<500&&!ready;++i){const auto r=shared.refresh(request);ready=r.snapshot&&r.snapshot->metadata&&r.snapshot->metadata->title=="Track 1";if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(ready&&*opens==1,"Shared worker did not publish");shared.release(a.token);check(shared.activeRequests().size()==1,"First consumer stopped worker");shared.release(b.token);check(shared.activeRequests().empty(),"Last consumer retained worker");
}
std::string json(const std::string &s){std::string out="\"";for(unsigned char x:s){if(x=='"'||x=='\\')out+='\\';if(x>=32)out+=static_cast<char>(x);}return out+'"';}
int live(int argc,char **argv){
    traktor::Options options;if(argc>3)options.cacheRoot=std::filesystem::u8path(argv[3]);
    auto provider=traktor::makeLive({"traktor",Mode::Automatic,{}},options);provider->start();
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(argc>2?std::stoi(argv[2]):15);bool received=false;std::string previous;
    while(std::chrono::steady_clock::now()<until){auto sample=provider->sample();if(sample.observation){const auto &o=*sample.observation;const auto title=o.metadata?o.metadata->title:std::string{};
            const std::string key=title+"/"+std::to_string(static_cast<int>(o.playback))+"/"+(o.metadata?std::to_string(static_cast<int>(o.metadata->artwork.status)):"");
            if(key!=previous){previous=key;std::cout<<"{\"playback\":"<<static_cast<int>(o.playback)<<",\"title\":"<<json(title)<<",\"artist\":"<<json(o.metadata?o.metadata->artist:"")<<",\"cover\":"<<json(o.metadata&&o.metadata->artwork.image?o.metadata->artwork.image->location:"")<<"}\n"<<std::flush;}
            received=received||bool(o.metadata);
        }else if(sample.issue!=ProviderIssue::None)std::cout<<"{\"issue\":"<<static_cast<int>(sample.issue)<<"}\n"<<std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    provider->stop();return received?0:2;
}
}
int main(int argc,char **argv){try{if(argc>1&&std::string(argv[1])=="--live")return live(argc,argv);contract();artwork();lifecycle();std::cout<<checks<<" Traktor memory/selection/artwork/worker checks passed; synthetic fixtures\n";return 0;}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
