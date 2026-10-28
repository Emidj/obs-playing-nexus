// SPDX-License-Identifier: MIT
#include "playing/providers/traktor-live.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
namespace playing::providers::traktor {
namespace {
constexpr std::uint64_t ceiling=UINT64_C(140737488355328),deckTable=0x317a1d8,clockTable=0x3183d60;
bool pointer(std::uint64_t p){return p>=65536&&p<ceiling&&p%8==0;}
template<class T>T value(const unsigned char *p){T x{};std::memcpy(&x,p,sizeof(x));return x;}
bool same(const core::Metadata &a,const core::Metadata &b){return a.title==b.title&&a.artist==b.artist;}
}
bool acceptsVersion(const std::string &v) noexcept{
    if(v.size()<7||v.size()>40||(v[0]!='3'&&v[0]!='4')||v[1]!='.')return false;
    unsigned parts=1,digits=0,number=0;
    for(std::size_t i=2;i<v.size();++i){const auto ch=v[i];
        if(ch>='0'&&ch<='9'){if(++digits>5)return false;number=number*10+static_cast<unsigned>(ch-'0');if(number>65535)return false;}
        else if(ch=='.'&&digits&&parts<3){++parts;digits=0;number=0;}
        else return false;
    }
    return parts==3&&digits!=0;
}
Selection select(const Frame &frame,std::optional<unsigned> previous){
    std::optional<unsigned> chosen;unsigned playing=0;
    for(unsigned i=0;i<4;++i){const auto &d=frame.decks[i];
        if(!pointer(d.address)||!pointer(d.transport))return {Issue::Layout,{},{},{}};
        for(unsigned j=0;j<i;++j)if(d.address==frame.decks[j].address||d.transport==frame.decks[j].transport)return {Issue::Layout,{},{},{}};
        if(d.track&&!core::validMetadata(*d.track))return {Issue::Layout,{},{},{}};
        if(!d.playing||!d.track)continue;
        ++playing;if(d.transport==frame.master)chosen=i+1;
    }
    if(!chosen&&previous&&*previous>=1&&*previous<=4){const auto &d=frame.decks[*previous-1];if(d.playing&&d.track)chosen=previous;}
    if(!chosen&&playing==1)for(unsigned i=0;i<4;++i)if(frame.decks[i].playing&&frame.decks[i].track)chosen=i+1;
    if(!chosen){if(playing>1)return {Issue::Ambiguous,{},{},{}};return {Issue::None,{},core::Observation{core::Playback::Paused,{}},{}};}
    const auto &d=frame.decks[*chosen-1];return {Issue::None,chosen,core::Observation{core::Playback::Playing,d.track},d.coverKey};
}
struct MemoryReader::Impl {
    virtualdj::ProcessMemory &memory;std::uint64_t module;std::string version;
    struct Layout {unsigned channel,name,transport,rate,track;bool combinedCover;};
    const Layout layout;
    std::array<std::uint64_t,4> decks{};std::uint64_t clock=0,cursor=65536,regionEnd=0,scanned=0;bool exhausted=false;
    std::array<std::uint64_t,3> tables;
    const std::array<const char *,3> names{{".?AV?$DeathNoteObservable@VTrackDeck@@@@",".?AVMasterClock@@",".?AVContentEntryTrack@@"}};
    struct Section {std::uint32_t rva=0,size=0;std::vector<unsigned char> bytes;};
    Section dataSection,rdataSection;unsigned symbolStage=0;bool symbolsReady=false;
    Impl(virtualdj::ProcessMemory &m,std::uint64_t b,std::string v):memory(m),module(b),version(std::move(v)),layout(!version.empty()&&version[0]=='3'?Layout{56,88,232,288,872,true}:Layout{80,112,256,312,928,false}),tables{{b+deckTable,b+clockTable,b+0x31b5e80}}{}
    // MSVC RTTI locators are 4-byte aligned even when object pointers are
    // 8-byte aligned. Scalar/descriptor reads must not impose pointer alignment.
    template<class T>bool get(std::uint64_t address,T &out){return address>=65536&&address<ceiling&&sizeof(out)<ceiling-address&&memory.read(address,&out,sizeof(out));}
    bool text(std::uint64_t address,std::string &out){
        std::array<unsigned char,32> header{},again{};
        if(!memory.read(address,header.data(),header.size()))return false;
        const auto n=value<std::uint64_t>(header.data()+16),capacity=value<std::uint64_t>(header.data()+24);
        if(n>4096||capacity<n||capacity>1048576||(capacity<16&&n>15))return false;
        out.assign(static_cast<std::size_t>(n),'\0');
        if(capacity<16){if(n)std::memcpy(out.data(),header.data(),static_cast<std::size_t>(n));}
        else if(n&&(!pointer(value<std::uint64_t>(header.data()))||!memory.read(value<std::uint64_t>(header.data()),out.data(),static_cast<std::size_t>(n))))return false;
        if(out.find('\0')!=std::string::npos||!memory.read(address,again.data(),again.size())||header!=again)return false;
        if(capacity>=16&&n){std::string check(static_cast<std::size_t>(n),'\0');if(!memory.read(value<std::uint64_t>(header.data()),check.data(),check.size())||out!=check)return false;}
        return true;
    }
    bool type(std::uint64_t table,const char *expected){
        std::uint64_t locator=0;if(!get(table-8,locator))return false;
        std::array<std::uint32_t,6> fields{};
        if(!get(locator,fields)||fields[0]!=1||fields[1]!=0||fields[5]>0x30000000||module+fields[5]!=locator||fields[3]>0x30000000)return false;
        const auto n=std::strlen(expected)+1;std::array<char,128> name{};
        return n<=name.size()&&memory.read(module+fields[3]+16,name.data(),n)&&std::strcmp(name.data(),expected)==0;
    }
    bool symbols(const std::atomic<bool> &stopping){
        if(symbolsReady)return true;
        if(symbolStage==0){
            bool known=true;for(unsigned i=0;i<3;++i)known=known&&type(tables[i],names[i]);
            if(known){symbolsReady=true;return true;}
            // Cold path for relocated/recompiled 3.x/4.x executables: read PE
            // metadata sections, locate actual RTTI and primary vtables rather
            // than assuming that an address from 4.4.1 survives a rebuild.
            std::array<unsigned char,64> dos{};std::array<unsigned char,264> nt{};
            if(!memory.read(module,dos.data(),dos.size())||value<std::uint16_t>(dos.data())!=0x5a4d){exhausted=true;return false;}
            const auto offset=value<std::uint32_t>(dos.data()+60);
            if(offset>1048576||!memory.read(module+offset,nt.data(),nt.size())||value<std::uint32_t>(nt.data())!=0x4550||value<std::uint16_t>(nt.data()+4)!=0x8664||value<std::uint16_t>(nt.data()+24)!=0x20b){exhausted=true;return false;}
            const auto count=value<std::uint16_t>(nt.data()+6),optional=value<std::uint16_t>(nt.data()+20);const auto imageSize=value<std::uint32_t>(nt.data()+80);
            if(!count||count>64||optional<112||optional>1024||imageSize<4096||imageSize>0x40000000){exhausted=true;return false;}
            for(unsigned i=0;i<count;++i){std::array<unsigned char,40> section{};if(!memory.read(module+offset+24+optional+i*40,section.data(),section.size())){exhausted=true;return false;}
                const auto rva=value<std::uint32_t>(section.data()+12),size=value<std::uint32_t>(section.data()+8);
                if(rva>=imageSize||size>imageSize-rva){exhausted=true;return false;}
                Section *target=nullptr;if(std::memcmp(section.data(),".data\0\0\0",8)==0)target=&dataSection;else if(std::memcmp(section.data(),".rdata\0\0",8)==0)target=&rdataSection;
                if(target){if(target->size){exhausted=true;return false;}target->rva=rva;target->size=std::min<std::uint32_t>(size,16*1024*1024);target->bytes.reserve(target->size);}
            }
            if(!dataSection.size||!rdataSection.size){exhausted=true;return false;}symbolStage=1;
        }
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(12);
        for(auto *section:{&dataSection,&rdataSection})while(section->bytes.size()<section->size){
            if(stopping||std::chrono::steady_clock::now()>=deadline)return false;
            const auto old=section->bytes.size(),length=std::min<std::size_t>(65536,section->size-old);section->bytes.resize(old+length);
            if(!memory.read(module+section->rva+old,section->bytes.data()+old,length)){exhausted=true;return false;}
        }
        for(unsigned kind=0;kind<3;++kind){
            const auto &bytes=dataSection.bytes;const auto length=std::strlen(names[kind])+1;
            const auto found=std::search(bytes.begin(),bytes.end(),names[kind],names[kind]+length);
            if(found==bytes.end()||found-bytes.begin()<16){exhausted=true;return false;}
            const auto descriptor=dataSection.rva+static_cast<std::uint32_t>(found-bytes.begin())-16;
            const auto &rdata=rdataSection.bytes;std::optional<std::uint64_t> locator;
            for(std::size_t i=0;i+24<=rdata.size();i+=4){const auto *p=rdata.data()+i;
                if(value<std::uint32_t>(p)==1&&value<std::uint32_t>(p+4)==0&&value<std::uint32_t>(p+12)==descriptor&&value<std::uint32_t>(p+20)==rdataSection.rva+i){
                    if(locator){exhausted=true;return false;}locator=module+rdataSection.rva+i;
                }
            }
            if(!locator){exhausted=true;return false;}std::optional<std::uint64_t> table;
            for(std::size_t i=0;i+16<=rdata.size();i+=8)if(value<std::uint64_t>(rdata.data()+i)==*locator){const auto candidate=module+rdataSection.rva+i+8;
                if(type(candidate,names[kind])){if(table){exhausted=true;return false;}table=candidate;}
            }
            if(!table){exhausted=true;return false;}tables[kind]=*table;
        }
        symbolsReady=true;dataSection.bytes.clear();dataSection.bytes.shrink_to_fit();rdataSection.bytes.clear();rdataSection.bytes.shrink_to_fit();return true;
    }
    bool discover(const std::atomic<bool> &stopping){
        if(exhausted)return false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(12);
        std::array<unsigned char,65536> bytes{};
        for(unsigned queries=0;queries<4096&&!stopping&&std::chrono::steady_clock::now()<deadline;++queries){
            if(clock&&std::all_of(decks.begin(),decks.end(),[](auto x){return x!=0;}))return true;
            if(cursor>=ceiling||scanned>=512*1024*1024){exhausted=true;return false;}
            if(cursor>=regionEnd){const auto r=memory.query(cursor);if(!r||r->bytes==0||r->address>cursor||r->bytes>ceiling-r->address){exhausted=true;return false;}
                regionEnd=r->address+r->bytes;
                if(regionEnd<=cursor){exhausted=true;return false;}
                if(!r->privateWritable){cursor=regionEnd;continue;}
            }
            const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(bytes.size(),regionEnd-cursor));
            if(memory.read(cursor,bytes.data(),count)){
                for(std::size_t i=0;i+8<=count;i+=8){const auto table=value<std::uint64_t>(bytes.data()+i);const auto address=cursor+i;
                    if(table==tables[1]){if(clock&&clock!=address){exhausted=true;return false;}clock=address;}
                    else if(table==tables[0]){std::string channel,name;
                        if(!text(address+layout.channel,channel)||channel.size()!=8||channel.compare(0,7,"Channel")!=0||channel[7]<'A'||channel[7]>'D'||!text(address+layout.name,name)||name!="TrackDeck")continue;
                        auto &slot=decks[static_cast<unsigned>(channel[7]-'A')];if(slot&&slot!=address){exhausted=true;return false;}slot=address;
                    }
                }
            }
            cursor+=count;scanned+=count;
        }
        return clock&&std::all_of(decks.begin(),decks.end(),[](auto x){return x!=0;});
    }
    Issue deck(unsigned slot,Deck &out){
        const auto address=decks[slot];std::uint64_t table=0,track=0;double rate=0;
        if(!get(address,table)||table!=tables[0])return Issue::Layout;
        std::string channel,name;if(!text(address+layout.channel,channel)||channel!="Channel"+std::string(1,static_cast<char>('A'+slot))||!text(address+layout.name,name)||name!="TrackDeck")return Issue::Layout;
        out.address=address;
        if(!get(address+layout.transport,out.transport)||!pointer(out.transport)||!get(address+layout.rate,rate)||!std::isfinite(rate)||std::abs(rate)>16||!get(address+layout.track,track))return Issue::Layout;
        out.playing=std::abs(rate)>0.0001;
        if(track){
            if(!pointer(track)||!get(track,table)||table!=tables[2])return Issue::Layout;
            core::Metadata metadata;std::string bucket,key;
            if(!text(track+48,metadata.title)||!text(track+80,metadata.artist)||!core::validMetadata(metadata)||metadata.title.empty())return Issue::Changed;
            if(layout.combinedCover){
                std::string combined;if(!text(track+560,combined))return Issue::Changed;
                if(!combined.empty()){
                    if(combined.size()<5||combined[3]!='\\')return Issue::Layout;
                    bucket=combined.substr(0,3);key=combined.substr(4);
                }
            }else if(!text(track+608,bucket)||!text(track+640,key))return Issue::Changed;
            if(!bucket.empty()||!key.empty()){
                if(bucket.size()!=3||!std::all_of(bucket.begin(),bucket.end(),[](char c){return c>='0'&&c<='9';})||key.empty()||key.size()>64||!std::all_of(key.begin(),key.end(),[](char c){return (c>='A'&&c<='Z')||(c>='0'&&c<='9');}))return Issue::Layout;
                out.coverKey=bucket+"/"+key;
            }
            out.track=std::move(metadata);
        }else if(out.playing)return Issue::Changed;
        return Issue::None;
    }
    Read read(const std::atomic<bool> &stopping){
        if(!acceptsVersion(version))return {Issue::Version,{}};
        if(!symbols(stopping))return {exhausted?Issue::Layout:Issue::Searching,{}};
        for(unsigned i=0;i<3;++i)if(!type(tables[i],names[i]))return {Issue::Layout,{}};
        if(!discover(stopping))return {exhausted?Issue::Layout:Issue::Searching,{}};
        Frame frame,again;std::uint64_t table=0;
        if(!get(clock,table)||table!=tables[1]||!get(clock+16,frame.master))return {Issue::Layout,{}};
        for(unsigned i=0;i<4;++i){const auto issue=deck(i,frame.decks[i]);if(issue!=Issue::None)return {issue,{}};}
        if(!get(clock+16,again.master)||frame.master!=again.master)return {Issue::Changed,{}};
        for(unsigned i=0;i<4;++i){const auto issue=deck(i,again.decks[i]);const auto &a=frame.decks[i],&b=again.decks[i];
            if(issue!=Issue::None||a.address!=b.address||a.transport!=b.transport||a.playing!=b.playing||bool(a.track)!=bool(b.track)||a.coverKey!=b.coverKey||(a.track&&!same(*a.track,*b.track)))return {Issue::Changed,{}};
        }
        return {Issue::None,std::move(frame)};
    }
};
MemoryReader::MemoryReader(virtualdj::ProcessMemory &m,std::uint64_t b,std::string v):impl_(std::make_unique<Impl>(m,b,std::move(v))){}
MemoryReader::~MemoryReader()=default;
Read MemoryReader::read(const std::atomic<bool> &stopping){if(stopping)return {Issue::Searching,{}};return impl_->read(stopping);}
}
