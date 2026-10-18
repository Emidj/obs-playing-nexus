// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace playing::providers::rekordbox {
// Narrow public C ABI used by the existing Lua reader. No SQLite source copied.
struct sqlite3;
struct sqlite3_stmt;
struct Api {
    HMODULE library = nullptr;
    int (*open)(const char *, sqlite3 **, int, const char *) = nullptr;
    int (*close)(sqlite3 *) = nullptr;
    int (*exec)(sqlite3 *, const char *, int (*)(void *, int, char **, char **), void *, char **) = nullptr;
    int (*prepare)(sqlite3 *, const char *, int, sqlite3_stmt **, const char **) = nullptr;
    int (*step)(sqlite3_stmt *) = nullptr;
    int (*finalize)(sqlite3_stmt *) = nullptr;
    const unsigned char *(*text)(sqlite3_stmt *, int) = nullptr;
    int (*bytes)(sqlite3_stmt *, int) = nullptr;
    int (*readonly)(sqlite3 *, const char *) = nullptr;
    int (*timeout)(sqlite3 *, int) = nullptr;
    void (*progress)(sqlite3 *, int, int (*)(void *), void *) = nullptr;
    int (*limit)(sqlite3 *, int, int) = nullptr;
    Api() = default;
    Api(const Api &) = delete;
    Api &operator=(const Api &) = delete;
    ~Api() { unload(); }
    void unload() noexcept { if (library) FreeLibrary(library); library = nullptr; }
    template<class T> void bind(T &value, const char *name)
    {
        const auto symbol = GetProcAddress(library, name);
        if (!symbol) throw std::runtime_error("Missing database ABI symbol");
        value = reinterpret_cast<T>(symbol);
    }
    void load(const std::string &path)
    {
        if (library) return;
        const auto filename = std::filesystem::u8path(path);
        library = LoadLibraryExW(filename.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!library) throw std::runtime_error("Database dependency unavailable");
        try {
            bind(open, "sqlite3_open_v2"); bind(close, "sqlite3_close_v2");
            bind(exec, "sqlite3_exec"); bind(prepare, "sqlite3_prepare_v2");
            bind(step, "sqlite3_step"); bind(finalize, "sqlite3_finalize");
            bind(text, "sqlite3_column_text"); bind(bytes, "sqlite3_column_bytes");
            bind(readonly, "sqlite3_db_readonly"); bind(timeout, "sqlite3_busy_timeout");
            bind(progress, "sqlite3_progress_handler");
            bind(limit, "sqlite3_limit");
        } catch (...) { FreeLibrary(library); library = nullptr; throw; }
    }
};
struct Connection {
    Api &api;
    sqlite3 *db = nullptr;
    explicit Connection(Api &a) : api(a) {}
    ~Connection() { reset(); }
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    void reset() noexcept { if (db) api.close(db); db = nullptr; }
};
struct Statement {
    Api &api;
    sqlite3_stmt *value = nullptr;
    explicit Statement(Api &a) : api(a) {}
    ~Statement() { if (value) api.finalize(value); }
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;
};
// Format passphrase, already bundled in the audited Lua; not account credentials.
inline constexpr const char *keySql = "PRAGMA cipher='sqlcipher'; PRAGMA legacy=4; PRAGMA key='402fd482c38817c35ffa8ffb8c7d93143b749e7d315df7a81732a1ff43608497';";
inline constexpr const char *latestSql = R"sql(
SELECT s.ID, s.HistoryID, s.TrackNo, c.Title, COALESCE(a.Name,''), c.ID
FROM djmdSongHistory s
JOIN djmdHistory h ON h.ID=s.HistoryID
JOIN djmdContent c ON c.ID=s.ContentID
LEFT JOIN djmdArtist a ON a.ID=c.ArtistID AND COALESCE(a.rb_local_deleted,0)=0
WHERE COALESCE(s.rb_local_deleted,0)=0 AND COALESCE(h.rb_local_deleted,0)=0
  AND COALESCE(c.rb_local_deleted,0)=0 AND COALESCE(c.Title,'')<>''
ORDER BY COALESCE(NULLIF(h.DateCreated,''),h.created_at) DESC,
         h.created_at DESC, h.ID DESC, s.TrackNo DESC, s.created_at DESC, s.ID DESC
LIMIT 1
)sql";
} // namespace playing::providers::rekordbox
