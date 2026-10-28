// SPDX-License-Identifier: MIT
#pragma once
#include "rekordbox-database.hpp"
#include "playing/providers/rekordbox-history.hpp"
#include "playing/core/playing-core.hpp"
#include <chrono>
#include <fstream>
#include <algorithm>
#include <vector>
namespace playing::providers::detail {
inline std::filesystem::path environment(const wchar_t *name){std::wstring value(32768,L'\0');const auto n=GetEnvironmentVariableW(name,value.data(),static_cast<DWORD>(value.size()));if(!n||n>=value.size())return {};value.resize(n);return value;}
inline std::string quote(const std::string &value){std::string result="'";for(char c:value){result+=c;if(c=='\'')result+='\'';}return result+"'";}
// Worker-only read-only lookup. Ambiguous metadata never chooses an arbitrary cover.
class RekordboxCover {
    std::string library_,last_;core::Artwork cached_;
    std::chrono::steady_clock::time_point next_{};
    static int timeout(void *p){return std::chrono::steady_clock::now()>*static_cast<std::chrono::steady_clock::time_point*>(p);}
public:
    explicit RekordboxCover(std::string library={}):library_(std::move(library)){}
    void enrich(core::TrackState &track,const std::string &database={},const std::string &contentId={}){
        if(library_.empty())return;
        const auto key=database+'\n'+contentId+'\n'+track.title+'\n'+track.artist;
        if(key==last_&&std::chrono::steady_clock::now()<next_){track.artwork=cached_;return;}
        last_=key;next_=std::chrono::steady_clock::now()+std::chrono::seconds(1);cached_={};
        try{auto paths=std::vector<std::filesystem::path>{};if(!database.empty())paths.push_back(std::filesystem::u8path(database));else if(const auto path=discoverRekordboxDatabase())paths.push_back(std::filesystem::u8path(*path));
            if(paths.size()!=1)return;const auto path=paths.front();rekordbox::Api api;api.load(library_);rekordbox::Connection connection(api);
            if(api.open(path.u8string().c_str(),&connection.db,0x10001,nullptr)!=0||api.readonly(connection.db,"main")!=1)return;
            auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);api.progress(connection.db,1000,timeout,&deadline);api.timeout(connection.db,25);api.limit(connection.db,0,1024*1024);
            if(api.exec(connection.db,rekordbox::keySql,nullptr,nullptr,nullptr)!=0)return;
            if(api.exec(connection.db,"PRAGMA query_only=ON;PRAGMA trusted_schema=OFF",nullptr,nullptr,nullptr)!=0)return;
            const auto condition=contentId.empty()?"c.Title="+quote(track.title)+" AND COALESCE(a.Name,'')="+quote(track.artist):"c.ID="+quote(contentId);
            const auto sql="SELECT c.ImagePath FROM djmdContent c LEFT JOIN djmdArtist a ON a.ID=c.ArtistID WHERE COALESCE(c.rb_local_deleted,0)=0 AND "+condition+" LIMIT 2";
            rekordbox::Statement stmt(api);if(api.prepare(connection.db,sql.c_str(),-1,&stmt.value,nullptr)!=0)return;if(api.step(stmt.value)!=100)return;
            const auto raw=api.text(stmt.value,0);const int size=api.bytes(stmt.value,0);if(size<0||size>4096)return;const std::string image=raw?std::string(reinterpret_cast<const char*>(raw),static_cast<std::size_t>(size)):"";
            if(api.step(stmt.value)!=101)return;
            if(image.empty()){cached_={core::ArtworkStatus::Missing,{}};track.artwork=cached_;return;}
            // Rekordbox stores /PIONEER/Artwork/... relative to its share directory.
            std::filesystem::path resolved;
            if(image.rfind("/PIONEER/Artwork/",0)==0)resolved=path.parent_path()/L"share"/std::filesystem::u8path(image.substr(1));
            else {resolved=std::filesystem::u8path(image);if(!resolved.is_absolute())resolved=path.parent_path()/resolved;}
            if(std::filesystem::is_regular_file(resolved)){cached_={core::ArtworkStatus::Ready,core::ImageReference{core::ImageKind::LocalFile,resolved.u8string(),std::to_string(std::filesystem::last_write_time(resolved).time_since_epoch().count())}};next_=std::chrono::steady_clock::now()+std::chrono::seconds(10);}
            else cached_={core::ArtworkStatus::Missing,{}};
            track.artwork=cached_;
        }catch(...){track.artwork={};}
    }
};
} // namespace playing::providers::detail
