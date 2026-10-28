// SPDX-License-Identifier: MIT
#include "playing/providers/rekordbox-history.hpp"
#include "../src/providers/rekordbox-database.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <algorithm>

using namespace playing::providers;
namespace fs = std::filesystem;
namespace rb = playing::providers::rekordbox;
namespace {
int checks = 0;
void require(bool ok, const char *message)
{ ++checks; if (!ok) throw std::runtime_error(message); }
struct Fixture {
    rb::Api api;
    rb::Connection writer{api};
    fs::path file;
    Fixture(const std::string &dll, const fs::path &name) : file(name)
    {
        api.load(dll);
        require(api.open(file.u8string().c_str(), &writer.db, 6 | 0x10000, nullptr) == 0,
            "create isolated encrypted fixture");
        sql(rb::keySql);
        sql("PRAGMA journal_mode=WAL; PRAGMA wal_autocheckpoint=0;");
        sql("CREATE TABLE djmdHistory(ID TEXT,DateCreated TEXT,created_at TEXT,rb_local_deleted INT); CREATE TABLE djmdSongHistory(ID TEXT,HistoryID TEXT,ContentID TEXT,TrackNo INT,created_at TEXT,rb_local_deleted INT); CREATE TABLE djmdContent(ID TEXT,Title TEXT,ArtistID TEXT,rb_local_deleted INT); CREATE TABLE djmdArtist(ID TEXT,Name TEXT,rb_local_deleted INT);");
        sql(u8"INSERT INTO djmdHistory VALUES('h1','2026-09-20','2026-09-20',0),('h2','2026-09-21','2026-09-21',0); INSERT INTO djmdArtist VALUES('a1','Björk',0); INSERT INTO djmdContent VALUES('c1','Jóga 🎵','a1',0),('c2','Last - Track','missing',0); INSERT INTO djmdSongHistory VALUES('s1','h1','c2',99,'2026-09-20',0),('s2','h2','c1',1,'2026-09-21',0);");
    }
    void sql(const std::string &value)
    {
        if (api.exec(writer.db, value.c_str(), nullptr, nullptr, nullptr) != 0)
            throw std::runtime_error("fixture SQL failed");
    }
};
RefreshResult complete(ProviderRegistry &registry, const Request &request)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        auto result = registry.refresh(request);
        if (result.result != Refresh::Pending) return result;
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("worker timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void basic(const std::string &dll, const fs::path &root)
{
    Fixture fixture(dll, root / fs::u8path(u8"historial-ñ.db"));
    const auto cover=root/"share/PIONEER/Artwork/fixture/artwork.jpg";fs::create_directories(cover.parent_path());{std::ofstream f(cover,std::ios::binary);f<<"fixture cover";}
    fixture.sql("ALTER TABLE djmdContent ADD COLUMN ImagePath TEXT; UPDATE djmdContent SET ImagePath='/PIONEER/Artwork/fixture/artwork.jpg' WHERE ID='c1'");
    ProviderRegistry registry;
    require(registry.descriptors().empty(), "registry defaults empty");
    require(registerRekordboxHistory(registry, dll) == Registration::Registered, "explicit registration");
    require(!registry.descriptors()[0].automatic && registry.descriptors()[0].history, "history only");
    require(registry.acquire({"rekordbox"}).result == Acquisition::UnsupportedMode, "reject automatic");
    const Request request{"rekordbox", Mode::History, fixture.file.u8string()};
    const auto a = registry.acquire(request), b = registry.acquire(request);
    require(a.result == Acquisition::Acquired && b.result == Acquisition::Acquired
        && registry.activeRequests().size() == 1, "shared real reader");
    require(registry.refresh(request).result == Refresh::Pending, "sample schedules without waiting");
    require(complete(registry, request).result == Refresh::Applied, "real encrypted history published");
    const auto initial = registry.snapshot(a.token);
    require(initial == registry.snapshot(b.token) && initial->metadata->title == u8"Jóga 🎵"
        && initial->metadata->artist == u8"Björk", "Unicode newest session");
    require(initial->metadata->artwork.status==playing::core::ArtworkStatus::Ready&&initial->metadata->artwork.image&&fs::u8path(initial->metadata->artwork.image->location)==cover,"Content ID resolves share/PIONEER artwork");
    require(complete(registry, request).result == Refresh::Unchanged
        && registry.snapshot(a.token) == initial, "data_version cache preserves pointer");
    fixture.sql("INSERT INTO djmdSongHistory VALUES('s3','h2','c2',2,'2026-09-21',0)");
    require(complete(registry, request).result == Refresh::Applied
        && registry.snapshot(a.token)->metadata->title == "Last - Track"
        && registry.snapshot(a.token)->metadata->artist.empty(), "WAL change and missing artist");
    fixture.sql("INSERT INTO djmdHistory VALUES('deleted','2027-01-01','2027-01-01',1); INSERT INTO djmdSongHistory VALUES('s4','deleted','c1',1,'2027-01-01',0),('s5','h2','c1',3,'2026-09-21',1)");
    const auto before = registry.snapshot(a.token);
    require(initial->metadata->artwork.status==playing::core::ArtworkStatus::Ready&&initial->metadata->artwork.image&&fs::u8path(initial->metadata->artwork.image->location)==cover,"Content ID resolves share/PIONEER artwork");
    require(complete(registry, request).result == Refresh::Unchanged
        && registry.snapshot(a.token) == before, "deleted records ignored");
    fixture.sql("DELETE FROM djmdSongHistory");
    const auto empty = complete(registry, request);
    require(empty.result == Refresh::Applied && empty.issue == ProviderIssue::NoRecords
        && !registry.snapshot(a.token)->metadata, "empty library clears previous history track");
    auto bad = request; bad.encoding = HistoryEncoding::Ansi;
    require(registry.acquire(bad).result == Acquisition::StartFailed, "reject text file encoding");
    require(registry.release(a.token) && registry.activeRequests().size() == 1, "first release retains reader");
    require(registry.release(b.token) && registry.activeRequests().empty(), "last release stops reader");
    require(initial->metadata->title == u8"Jóga 🎵", "retained snapshot survives worker destruction");
    RekordboxHistory direct(request, dll);
    direct.stop(); direct.start(); direct.start(); direct.stop(); direct.stop(); direct.start(); direct.stop();
    require(true, "idempotent stop/restart");
    require(registerRekordboxHistory(registry, "obs-playing-db.dll") == Registration::Invalid,
        "dependency path must be explicit absolute");
}
void errors(const std::string &dll, const fs::path &root)
{
    Fixture fixture(dll, root / "recover.db");
    ProviderRegistry registry;
    registerRekordboxHistory(registry, dll);
    const Request request{"rekordbox", Mode::History, fixture.file.u8string()};
    const auto token = registry.acquire(request).token;
    complete(registry, request);
    const auto prior = registry.snapshot(token);
    fixture.sql("UPDATE djmdContent SET Title=replace(hex(zeroblob(2200)),'00','AA') WHERE ID='c1'");
    auto result = complete(registry, request);
    require(result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::InvalidMetadata
        && registry.snapshot(token) == prior, "oversized metadata preserves snapshot");
    fixture.sql("UPDATE djmdContent SET Title='Nul'||char(0)||'tail' WHERE ID='c1'");
    require(complete(registry, request).issue == ProviderIssue::InvalidMetadata
        && registry.snapshot(token) == prior, "embedded NUL rejected without truncation");
    fixture.sql("UPDATE djmdContent SET Title=CAST(X'FF' AS TEXT) WHERE ID='c1'");
    require(complete(registry, request).issue == ProviderIssue::InvalidMetadata, "invalid UTF8 rejected");
    fixture.sql("UPDATE djmdContent SET Title='Recovered' WHERE ID='c1'");
    require(complete(registry, request).result == Refresh::Applied
        && registry.snapshot(token)->metadata->title == "Recovered", "bad metadata recovery");
    fixture.sql("DROP TABLE djmdSongHistory");
    result = complete(registry, request);
    require(result.result == Refresh::Unavailable && result.issue == ProviderIssue::DatabaseSchema
        && !registry.snapshot(token)->metadata, "broken schema clears stale track");
    fixture.sql("CREATE TABLE djmdSongHistory(ID TEXT,HistoryID TEXT,ContentID TEXT,TrackNo INT,created_at TEXT,rb_local_deleted INT); INSERT INTO djmdSongHistory VALUES('fixed','h2','c1',1,'2026-09-21',0)");
    require(complete(registry, request).result == Refresh::Applied
        && registry.snapshot(token)->metadata->title == "Recovered", "schema recovery same session");
    registry.clear();

    const auto absent = root / "missing.db";
    const Request missing{"rekordbox", Mode::History, absent.u8string()};
    const auto missingToken = registry.acquire(missing).token;
    result = complete(registry, missing);
    require(result.result == Refresh::Unavailable && result.issue == ProviderIssue::MissingHistory
        && !fs::exists(absent), "missing database never created");
    Fixture reappeared(dll, absent);
    require(complete(registry, missing).result == Refresh::Applied
        && registry.snapshot(missingToken)->metadata->title == u8"Jóga 🎵", "missing then appeared recovery");
    registry.clear();

    const auto corrupt = root / "not-a-database.db";
    { std::ofstream output(corrupt, std::ios::binary); output << std::string(4096, 'X'); }
    const Request format{"rekordbox", Mode::History, corrupt.u8string()};
    registry.acquire(format);
    result = complete(registry, format);
    require(result.result == Refresh::Unavailable && result.issue == ProviderIssue::DatabaseFormat,
        "invalid encryption/format classified");
    // Rename only this new fixture after the reader closed its failed handle.
    fs::rename(corrupt, root / "format-evidence.bin");
    Fixture replacement(dll, corrupt);
    require(complete(registry, format).result == Refresh::Applied, "replacement database reconnects");
    registry.clear();

    Fixture busy(dll, root / "busy.db");
    busy.sql("PRAGMA journal_mode=DELETE; BEGIN EXCLUSIVE;");
    const Request locked{"rekordbox", Mode::History, busy.file.u8string()};
    registry.acquire(locked);
    result = complete(registry, locked);
    require(result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::DatabaseBusy,
        "real SQLite lock typed busy");
    busy.sql("ROLLBACK");
    require(complete(registry, locked).result == Refresh::Applied, "busy recovery");
    registry.clear();

    ProviderRegistry noDependency;
    registerRekordboxHistory(noDependency, (root / "missing-library.dll").u8string());
    noDependency.acquire(request);
    result = complete(noDependency, request);
    require(result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::DependencyUnavailable,
        "missing dependency distinguished from library missing");
    noDependency.clear();
    require(registerRekordboxHistory(noDependency, std::string("C:/bad\0library.dll", 18))
        == Registration::Invalid, "DLL embedded NUL rejected");

    Fixture expensive(dll, root / "deadline.db");
    expensive.sql("WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<1500) INSERT INTO djmdArtist SELECT 'a1','Slow',0 FROM n; WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<1500) INSERT INTO djmdContent SELECT 'c1','Slow','a1',0 FROM n;");
    const Request limited{"rekordbox", Mode::History, expensive.file.u8string()};
    registry.acquire(limited);
    result = complete(registry, limited);
    require(result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::ReadTimeout,
        "expensive SQL interrupted by progress budget");
    expensive.sql("DELETE FROM djmdArtist WHERE Name='Slow'; DELETE FROM djmdContent WHERE Title='Slow'");
    require(complete(registry, limited).result == Refresh::Applied, "deadline recovery");
    registry.refresh(limited);
    registry.clear();
    require(registry.activeRequests().empty(), "stop safely while result pending");
}
void optionsWrite(const fs::path &file, const std::string &text)
{
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output << text;
    if (!output) throw std::runtime_error("options fixture failed");
}
std::string quote(const std::string &text)
{
    std::string out = "\"";
    for (const char c : text) { if (c == '\\' || c == '"') out += '\\'; out += c; }
    return out + '"';
}
void discovery(const std::string &dll, const fs::path &root)
{
    Fixture standard(dll, root / "standard.db"), relocated(dll, root / fs::u8path(u8"reubicado-ñ-🎵.db"));
    relocated.sql("UPDATE djmdContent SET Title='Relocated' WHERE ID='c1'");
    const auto options = root / "options.json";
    RekordboxHistoryLocations source;
    std::atomic<int> calls{0};
    ProviderRegistry registry;
    registerRekordboxHistory(registry, dll, [&] { ++calls; return source; });
    const Request automaticPath{"rekordbox", Mode::History};
    auto acquire = [&] { registry.clear(); return registry.acquire(automaticPath).token; };
    acquire();
    auto result = complete(registry, automaticPath);
    require(result.result == Refresh::Unavailable && result.issue == ProviderIssue::MissingHistory,
        "empty discovery missing");
    source.databases = {standard.file.u8string(), relocated.file.u8string()};
    acquire();
    require(complete(registry, automaticPath).issue == ProviderIssue::AmbiguousHistory,
        "two real libraries ambiguous");
    require(complete(registry, automaticPath).issue == ProviderIssue::AmbiguousHistory,
        "ambiguity cannot cache arbitrary first candidate");
    source.databases = {standard.file.u8string(), standard.file.u8string()};
    auto token = acquire();
    require(complete(registry, automaticPath).result == Refresh::Applied, "duplicate paths deduplicated");
    const auto once = calls.load();
    complete(registry, automaticPath);
    registry.snapshot(token);
    require(calls.load() == once, "discovery cached until missing; snapshot has no IO");
    registry.clear();
    const auto alias = root / "same-file.db";
    standard.writer.reset(); // Windows cannot create a link while SQLite owns the writer handle.
    fs::create_hard_link(standard.file, alias);
    source.databases = {standard.file.u8string(), alias.u8string()};
    acquire();
    require(complete(registry, automaticPath).result == Refresh::Applied, "hardlink identity deduplicated");

    source.optionsFiles = {options.u8string()};
    source.databases = {standard.file.u8string(), relocated.file.u8string()};
    // Exercise both JSON escaped Unicode and UTF-16 surrogate decoding.
    auto encoded = quote(relocated.file.u8string());
    const auto accent = encoded.find(u8"ñ");
    encoded.replace(accent, std::string(u8"ñ").size(), "\\u00f1");
    const auto music = encoded.find(u8"🎵");
    encoded.replace(music, std::string(u8"🎵").size(), "\\uD83C\\uDFB5");
    optionsWrite(options, "\xef\xbb\xbf{\"options\":[[\"db-path\"," + encoded + "]],\"other\":[true,null,12.5e-2]}");
    token = acquire();
    require(complete(registry, automaticPath).result == Refresh::Applied
        && registry.snapshot(token)->metadata->title == "Relocated", "relocation overrides defaults; BOM Unicode escapes");
    optionsWrite(options, "[[\"db-path\"," + quote((root / "relocated-missing.db").u8string()) + "]]");
    acquire();
    require(complete(registry, automaticPath).issue == ProviderIssue::MissingHistory,
        "missing relocation cannot fall back to stale standard");
    optionsWrite(options, "[[\"db-path\"," + quote(standard.file.u8string()) + "],[\"db-path\"," + quote(relocated.file.u8string()) + "]]");
    acquire();
    require(complete(registry, automaticPath).issue == ProviderIssue::AmbiguousHistory,
        "multiple relocations ambiguous");
    for (const auto &bad : {std::string("[[\"db-path\",\"relative.db\"]]"),
         std::string("[[\"db-path\",\"C:/\\uD800.db\"]]"), std::string("[[\"db-path\",\"C:/a.db\"]"),
         std::string("[[\"db-path\",\"C:/a.db\",true]]"), std::string("{\"x\":- 1}"),
         std::string(65537, ' '), std::string("[[\"db-path\",\"C:/a\\u0000.db\"]]")}) {
        optionsWrite(options, bad); acquire();
        require(complete(registry, automaticPath).issue == ProviderIssue::ReadError,
            "malformed/oversized/relative options rejected");
    }
    registry.clear();
    const Request explicitPath{"rekordbox", Mode::History, standard.file.u8string()};
    registry.acquire(explicitPath);
    require(complete(registry, explicitPath).result == Refresh::Applied, "explicit path bypasses bad options");
    registry.clear();
    source.optionsFiles = {(root / "absent-options.json").u8string()};
    source.databases = {standard.file.u8string()};
    acquire();
    require(complete(registry, automaticPath).result == Refresh::Applied, "absent options permits default path");
    registry.clear();
    source.databases.assign(17, standard.file.u8string());
    acquire();
    require(complete(registry, automaticPath).issue == ProviderIssue::ReadError, "discovery input bounded");
}
void defaultLocations(const std::string &dll, const fs::path &root)
{
    struct AppdataScope {
        std::wstring previous = std::wstring(32768, L'\0');
        bool exists;
        explicit AppdataScope(const fs::path &path)
        {
            const auto count = GetEnvironmentVariableW(L"APPDATA", previous.data(),
                static_cast<DWORD>(previous.size()));
            exists = count > 0 && count < previous.size();
            previous.resize(exists ? count : 0);
            if (!SetEnvironmentVariableW(L"APPDATA", path.c_str())) throw std::runtime_error("fixture environment failed");
        }
        ~AppdataScope() { SetEnvironmentVariableW(L"APPDATA", exists ? previous.c_str() : nullptr); }
    };
    const auto appdata = root / "appdata";
    fs::create_directories(appdata / "Pioneer/rekordbox");
    fs::create_directories(appdata / "AlphaTheta/rekordbox");
    fs::create_directories(appdata / "AlphaTheta/rekordboxAgent/storage");
    Fixture pioneer(dll, appdata / "Pioneer/rekordbox/master.db");
    AppdataScope scope(appdata); // Process-local test environment, restored on every exit.
    ProviderRegistry registry;
    registerRekordboxHistory(registry, dll);
    const Request request{"rekordbox", Mode::History};
    registry.acquire(request);
    require(complete(registry, request).result == Refresh::Applied, "real Pioneer default route");
    registry.clear();
    Fixture alpha(dll, appdata / "AlphaTheta/rekordbox/master.db");
    alpha.sql("UPDATE djmdContent SET Title='AlphaTheta' WHERE ID='c1'");
    registry.acquire(request);
    require(complete(registry, request).issue == ProviderIssue::AmbiguousHistory, "real AlphaTheta default route counted");
    registry.clear();
    optionsWrite(appdata / "AlphaTheta/rekordboxAgent/storage/options.json",
        "[[\"db-path\"," + quote(alpha.file.u8string()) + "]]");
    const auto token = registry.acquire(request).token;
    require(complete(registry, request).result == Refresh::Applied
        && registry.snapshot(token)->metadata->title == "AlphaTheta", "real Agent options route precedence");
    registry.clear();
}
std::string readFile(const fs::path &file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("fixture read failed");
    return {std::istreambuf_iterator<char>(input), {}};
}
void parity(const fs::path &luaFile)
{
    auto lua = readFile(luaFile);
    lua.erase(std::remove(lua.begin(), lua.end(), '\r'), lua.end());
    const std::string marker = "local latest_sql=[[";
    auto begin = lua.find(marker);
    require(begin != std::string::npos, "audited Lua query marker");
    begin += marker.size();
    const auto end = lua.find("]]", begin);
    std::string nativeSql=rb::latestSql; const auto idColumn=nativeSql.find(", c.ID"); require(idColumn!=std::string::npos,"Content ID for correlated artwork"); nativeSql.erase(idColumn,6);
    require(end != std::string::npos && lua.substr(begin, end - begin) == nativeSql,
        "native SQL preserves audited history selection plus content ID");
}
void isolationAndResources(const std::string &dll, const fs::path &root)
{
    Fixture first(dll, root / "independent-a.db"), second(dll, root / "independent-b.db");
    second.sql("UPDATE djmdContent SET Title='Second library' WHERE ID='c1'");
    // Close fixture writers; all following operations use read-only readers.
    first.writer.reset(); second.writer.reset();
    const auto firstBytes = readFile(first.file), secondBytes = readFile(second.file);
    ProviderRegistry registry;
    registerRekordboxHistory(registry, dll);
    const Request a{"rekordbox", Mode::History, first.file.u8string()};
    const Request b{"rekordbox", Mode::History, second.file.u8string()};
    const auto one = registry.acquire(a), two = registry.acquire(b);
    complete(registry, a); complete(registry, b);
    require(registry.activeRequests().size() == 2 && registry.snapshot(one.token) != registry.snapshot(two.token),
        "two libraries use independent sessions");
    require(registry.snapshot(one.token)->metadata->title == u8"Jóga 🎵"
        && registry.snapshot(two.token)->metadata->title == "Second library", "independent library contents");
    registry.release(one.token);
    require(complete(registry, b).result == Refresh::Unchanged
        && registry.snapshot(two.token)->metadata->title == "Second library", "stopping first cannot stop second");
    registry.clear();
    DWORD before = 0, after = 0;
    require(GetProcessHandleCount(GetCurrentProcess(), &before) != FALSE, "capture isolated handle baseline");
    for (int cycle = 0; cycle < 40; ++cycle) {
        const auto consumer = registry.acquire(a);
        if (consumer.result != Acquisition::Acquired || complete(registry, a).result != Refresh::Applied)
            throw std::runtime_error("repeated real reader cycle failed");
        registry.release(consumer.token);
    }
    require(GetProcessHandleCount(GetCurrentProcess(), &after) != FALSE && after <= before + 2,
        "40 real cycles do not accumulate process handles");
    require(readFile(first.file) == firstBytes && readFile(second.file) == secondBytes,
        "main database bytes unchanged by readers");
    std::cout << "Resource fixture: handles before=" << before << "; after=" << after
        << "; cycles=40 (not a product performance measurement)\n";
}
}
int wmain(int argc, wchar_t **argv)
{
    try {
        require(argc == 3, "audited DLL and Lua arguments required");
        const std::string dll = fs::path(argv[1]).u8string();
        const auto root = fs::current_path() / ("rb-history-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(fs::create_directory(root), "new fixture directory");
        parity(fs::path(argv[2]));
        basic(dll, root);
        errors(dll, root);
        discovery(dll, root);
        defaultLocations(dll, root);
        isolationAndResources(dll, root);
        std::cout << "PASS: " << checks << " native rekordbox history checks; fixture " << root.u8string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n'; return 1;
    }
}
