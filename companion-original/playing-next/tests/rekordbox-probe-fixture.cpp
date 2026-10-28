// SPDX-License-Identifier: MIT
// Test-only writer. Never linked into Core, Providers or the OBS probe DLL.
#include "../src/providers/rekordbox-database.hpp"
#include <iostream>
#include <thread>
#include <chrono>
namespace rb = playing::providers::rekordbox;
int wmain(int argc, wchar_t **argv)
{
    try {
        if (argc != 4) throw std::runtime_error("mode, fixture and DLL required");
        const std::filesystem::path file(argv[2]);
        const bool create = std::wstring(argv[1]) == L"create";
        if (!create && std::wstring(argv[1]) != L"append") throw std::runtime_error("unknown fixture mode");
        if (!file.is_absolute() || (create && std::filesystem::exists(file)))
            throw std::runtime_error("new absolute fixture required");
        rb::Api api; api.load(std::filesystem::path(argv[3]).u8string());
        rb::Connection writer(api);
        if (api.open(file.u8string().c_str(), &writer.db, create ? 6 : 2, nullptr))
            throw std::runtime_error("fixture open failed");
        auto sql = [&](const char *text) {
            if (api.exec(writer.db, text, nullptr, nullptr, nullptr)) throw std::runtime_error("fixture SQL failed");
        };
        sql(rb::keySql);
        if (create) {
            sql("PRAGMA journal_mode=WAL; PRAGMA wal_autocheckpoint=0; CREATE TABLE playing_probe_marker(Token TEXT); INSERT INTO playing_probe_marker VALUES('OBSPlayingIsolatedFixture-05B5'); CREATE TABLE djmdHistory(ID TEXT,DateCreated TEXT,created_at TEXT,rb_local_deleted INT); CREATE TABLE djmdSongHistory(ID TEXT,HistoryID TEXT,ContentID TEXT,TrackNo INT,created_at TEXT,rb_local_deleted INT); CREATE TABLE djmdContent(ID TEXT,Title TEXT,ArtistID TEXT,rb_local_deleted INT); CREATE TABLE djmdArtist(ID TEXT,Name TEXT,rb_local_deleted INT);");
            sql(u8"INSERT INTO djmdHistory VALUES('h1','2026-10-10','2026-10-10',0); INSERT INTO djmdArtist VALUES('a1','Björk',0); INSERT INTO djmdContent VALUES('c1','Jóga 🎵','a1',0),('c2','Next WAL 🎵','a1',0); INSERT INTO djmdSongHistory VALUES('s1','h1','c1',1,'2026-10-10',0);");
            std::cout << "CREATED_ISOLATED_FIXTURE" << std::endl;
        } else {
            // Refuse to mutate any database without our explicit isolated-fixture marker.
            rb::Statement marker(api);
            if (api.prepare(writer.db, "SELECT Token FROM playing_probe_marker", -1, &marker.value, nullptr)
                || api.step(marker.value) != 100) throw std::runtime_error("fixture marker absent");
            const auto token = api.text(marker.value, 0);
            if (!token || std::string(reinterpret_cast<const char *>(token)) != "OBSPlayingIsolatedFixture-05B5")
                throw std::runtime_error("fixture marker mismatch");
            // Finalize before write; keep writer open so OBS reads a committed WAL.
            api.finalize(marker.value); marker.value = nullptr;
            sql("PRAGMA wal_autocheckpoint=0; INSERT INTO djmdSongHistory VALUES('s2','h1','c2',2,'2026-10-10',0)");
            std::cout << "APPENDED_WAL_WRITER_OPEN" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(4));
        }
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
