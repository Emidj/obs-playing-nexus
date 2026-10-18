// SPDX-License-Identifier: MIT
// Dependency contract only: not a provider, plugin, or installable payload.
#include <windows.h>
#include <sqlite3.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int checks = 0;
void check(bool ok, const char *message)
{
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
struct Library {
    HMODULE handle;
    explicit Library(const wchar_t *path)
        : handle(LoadLibraryExW(path, nullptr,
              LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32))
    { check(handle != nullptr, "load exact existing library"); }
    ~Library() { FreeLibrary(handle); }
    template<class T> T symbol(const char *name)
    {
        auto p = GetProcAddress(handle, name);
        check(p != nullptr, name);
        return reinterpret_cast<T>(p);
    }
};
struct Api {
    decltype(&sqlite3_open_v2) open;
    decltype(&sqlite3_close_v2) close;
    decltype(&sqlite3_exec) exec;
    decltype(&sqlite3_prepare_v2) prepare;
    decltype(&sqlite3_step) step;
    decltype(&sqlite3_finalize) finalize;
    decltype(&sqlite3_column_text) text;
    decltype(&sqlite3_db_readonly) readonly;
    decltype(&sqlite3_busy_timeout) timeout;
    decltype(&sqlite3_libversion) version;
    explicit Api(Library &l)
        : open(l.symbol<decltype(open)>("sqlite3_open_v2")),
          close(l.symbol<decltype(close)>("sqlite3_close_v2")),
          exec(l.symbol<decltype(exec)>("sqlite3_exec")),
          prepare(l.symbol<decltype(prepare)>("sqlite3_prepare_v2")),
          step(l.symbol<decltype(step)>("sqlite3_step")),
          finalize(l.symbol<decltype(finalize)>("sqlite3_finalize")),
          text(l.symbol<decltype(text)>("sqlite3_column_text")),
          readonly(l.symbol<decltype(readonly)>("sqlite3_db_readonly")),
          timeout(l.symbol<decltype(timeout)>("sqlite3_busy_timeout")),
          version(l.symbol<decltype(version)>("sqlite3_libversion")) {}
};
struct Db {
    Api &api;
    sqlite3 *handle = nullptr;
    explicit Db(Api &a) : api(a) {}
    ~Db() { if (handle) api.close(handle); }
    void sql(const char *s) { check(api.exec(handle, s, nullptr, nullptr, nullptr) == SQLITE_OK, s); }
    std::string scalar(const std::string &sql, int column = 0)
    {
        sqlite3_stmt *statement = nullptr;
        int code = api.prepare(handle, sql.c_str(), -1, &statement, nullptr);
        if (code != SQLITE_OK) {
            if (statement) api.finalize(statement);
            throw std::runtime_error("prepare failed");
        }
        code = api.step(statement);
        std::string result;
        if (code == SQLITE_ROW) {
            const auto p = api.text(statement, column);
            if (p) result = reinterpret_cast<const char *>(p);
        }
        api.finalize(statement);
        check(code == SQLITE_ROW || code == SQLITE_DONE, "query completes");
        return result;
    }
};
constexpr const char *key = "PRAGMA cipher='sqlcipher'; PRAGMA legacy=4; PRAGMA key='402fd482c38817c35ffa8ffb8c7d93143b749e7d315df7a81732a1ff43608497';";
}
int wmain(int argc, wchar_t **argv)
{
    try {
        check(argc == 3, "library and audited Lua paths required");
        Library library(argv[1]);
        Api api(library);
        std::cout << "SQLite runtime: " << api.version() << '\n';
        std::ifstream source{std::filesystem::path(argv[2])};
        std::string lua((std::istreambuf_iterator<char>(source)), {});
        const std::string marker = "local latest_sql=[[";
        auto begin = lua.find(marker);
        check(begin != std::string::npos, "audited latest query exists");
        begin += marker.size();
        auto end = lua.find("]]", begin);
        check(end != std::string::npos, "audited query terminates");
        const auto query = lua.substr(begin, end - begin);
        const auto root = std::filesystem::current_path() / ("rb-contract-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(std::filesystem::create_directory(root), "new isolated fixture directory");
        const auto path = root / std::filesystem::u8path(u8"historial-ñ.db");
        Db writer(api), reader(api);
        check(api.open(path.u8string().c_str(), &writer.handle, SQLITE_OPEN_READWRITE |
              SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) == SQLITE_OK, "create fixture only");
        writer.sql(key);
        check(writer.scalar("PRAGMA journal_mode=WAL") == "wal", "WAL enabled");
        writer.sql("PRAGMA wal_autocheckpoint=0; CREATE TABLE djmdHistory(ID TEXT,DateCreated TEXT,created_at TEXT,rb_local_deleted INT); CREATE TABLE djmdSongHistory(ID TEXT,HistoryID TEXT,ContentID TEXT,TrackNo INT,created_at TEXT,rb_local_deleted INT); CREATE TABLE djmdContent(ID TEXT,Title TEXT,ArtistID TEXT,rb_local_deleted INT); CREATE TABLE djmdArtist(ID TEXT,Name TEXT,rb_local_deleted INT);");
        writer.sql(u8"INSERT INTO djmdHistory VALUES('h1','2026-09-20','2026-09-20',0),('h2','2026-09-21','2026-09-21',0); INSERT INTO djmdArtist VALUES('a1','Björk - invitada',0); INSERT INTO djmdContent VALUES('c1','Jóga - Live','a1',0),('c2','Último tema','missing',0); INSERT INTO djmdSongHistory VALUES('s1','h1','c2',99,'2026-09-20',0),('s2','h2','c1',1,'2026-09-21',0);");
        {
            std::ifstream encrypted(path, std::ios::binary);
            char header[16]{};
            encrypted.read(header, sizeof(header));
            check(encrypted.gcount() == 16 && std::string(header, 15) != "SQLite format 3", "encrypted file header");
        }
        check(api.open(path.u8string().c_str(), &reader.handle, SQLITE_OPEN_READONLY |
              SQLITE_OPEN_FULLMUTEX, nullptr) == SQLITE_OK, "readonly Unicode path");
        check(api.readonly(reader.handle, "main") == 1, "readonly handle");
        check(api.timeout(reader.handle, 25) == SQLITE_OK, "bounded busy timeout");
        reader.sql(key);
        reader.sql("PRAGMA query_only=ON; PRAGMA trusted_schema=OFF;");
        check(reader.scalar(query, 3) == u8"Jóga - Live", "latest session, Unicode title");
        check(reader.scalar(query, 4) == u8"Björk - invitada", "structured artist");
        check(api.exec(reader.handle, "DELETE FROM djmdContent", nullptr, nullptr, nullptr) == SQLITE_READONLY, "reject mutation");
        const auto version = reader.scalar("PRAGMA data_version");
        check(reader.scalar("PRAGMA data_version") == version, "stable data version");
        writer.sql("INSERT INTO djmdSongHistory VALUES('s3','h2','c2',2,'2026-09-21',0)");
        check(reader.scalar("PRAGMA data_version") != version, "external WAL commit detected");
        check(reader.scalar(query, 3) == u8"Último tema", "WAL track before checkpoint");
        check(reader.scalar(query, 4).empty(), "missing artist");
        writer.sql("INSERT INTO djmdHistory VALUES('deleted','2027-01-01','2027-01-01',1); INSERT INTO djmdSongHistory VALUES('s4','deleted','c1',1,'2027-01-01',0),('s5','h2','c1',3,'2026-09-21',1)");
        check(reader.scalar(query, 3) == u8"Último tema", "deleted entries excluded");
        writer.sql("DELETE FROM djmdSongHistory");
        check(reader.scalar(query).empty(), "empty history");
        Db missing(api);
        auto absent = root / "absent.db";
        check(api.open(absent.u8string().c_str(), &missing.handle, SQLITE_OPEN_READONLY |
              SQLITE_OPEN_FULLMUTEX, nullptr) == SQLITE_CANTOPEN, "missing fails");
        check(!std::filesystem::exists(absent), "missing not created");
        writer.sql("DROP TABLE djmdSongHistory");
        sqlite3_stmt *statement = nullptr;
        int code = api.prepare(reader.handle, query.c_str(), -1, &statement, nullptr);
        // prepare_v2 may see cached schema; step reparses after the WAL change.
        if (code == SQLITE_OK) code = api.step(statement);
        if (statement) api.finalize(statement);
        check(code == SQLITE_ERROR, "unsupported schema fails");
        std::cout << checks << " dependency checks PASS; fixture: " << root.u8string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
