#include "TestSupport.hpp"
#include "bang/LibraryCatalog.hpp"
#include "bang/LibraryStore.hpp"

#include <sqlite3.h>

#include <iostream>
#include <memory>

using bang::test::require;

namespace {

void createVersionOneLibrary(const std::filesystem::path& path)
{
    sqlite3* handle = nullptr;
    const int opened = sqlite3_open(path.c_str(), &handle);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(handle, sqlite3_close);
    require(opened == SQLITE_OK, "open version 1 fixture database");
    // Keep the old schema explicit so this fixture does not depend on current migrations.
    const char* schema = R"SQL(
        CREATE TABLE tracks(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            content_hash TEXT NOT NULL UNIQUE,
            title TEXT NOT NULL, artist TEXT NOT NULL DEFAULT '',
            album TEXT NOT NULL DEFAULT '', duration_ms INTEGER NOT NULL DEFAULT 0,
            source TEXT NOT NULL DEFAULT '', source_url TEXT NOT NULL DEFAULT '',
            added_at_ms INTEGER NOT NULL);
        CREATE TABLE playlists(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE, created_at_ms INTEGER NOT NULL);
        CREATE TABLE playlist_tracks(
            playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
            track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
            position INTEGER NOT NULL, UNIQUE(playlist_id, track_id));
        CREATE TABLE favorites(
            track_id INTEGER PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,
            added_at_ms INTEGER NOT NULL);
        CREATE TABLE downloads(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            request_url TEXT NOT NULL, backend TEXT NOT NULL,
            status TEXT NOT NULL, message TEXT NOT NULL DEFAULT '',
            track_id INTEGER REFERENCES tracks(id) ON DELETE SET NULL,
            started_at_ms INTEGER NOT NULL, finished_at_ms INTEGER);
        CREATE TABLE settings(name TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE INDEX idx_playlist_tracks_position ON playlist_tracks(playlist_id, position);
        INSERT INTO tracks VALUES(41, 'legacy-hash', 'Legacy title', 'Legacy artist',
            'Legacy album', 12345, 'youtube', 'https://example.test/track', 1000);
        INSERT INTO playlists VALUES(7, 'Saved playlist', 1001);
        INSERT INTO playlist_tracks VALUES(7, 41, 0);
        INSERT INTO favorites VALUES(41, 1002);
        INSERT INTO downloads VALUES(9, 'https://example.test/track', 'yt-dlp',
            'completed', 'Saved download', 41, 1000, 1003);
        INSERT INTO settings VALUES('volume', '73');
        PRAGMA user_version = 1;
    )SQL";
    const int result = sqlite3_exec(handle, schema, nullptr, nullptr, nullptr);
    require(result == SQLITE_OK, std::string("create version 1 fixture: ") + sqlite3_errmsg(handle));
}

void checkMigratedLibrary(bang::LibraryStore& store)
{
    sqlite3_stmt* statement = nullptr;
    const int prepared = sqlite3_prepare_v2(
        store.handle(), "PRAGMA user_version", -1, &statement, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> version(statement, sqlite3_finalize);
    require(prepared == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW
            && sqlite3_column_int(statement, 0) >= 2,
        "record that the version 2 migration has completed");
    const auto track = store.trackById(41);
    require(track.has_value(), "preserve the legacy track ID");
    require(track->contentHash == "legacy-hash" && track->title == "Legacy title"
            && track->artist == "Legacy artist" && track->album == "Legacy album"
            && track->durationMs == 12345 && track->addedAtMs == 1000
            && track->source == "youtube" && track->sourceUrl == "https://example.test/track",
        "preserve legacy track metadata");
    require(track->storedExtension == ".mp3"
            && store.trackFilePath(*track).filename() == "legacy-hash.mp3",
        "assign the MP3 extension to existing tracks");
    require(store.setting("volume") == "73", "preserve settings");

    bang::LibraryCatalog catalog(store);
    const auto playlists = catalog.playlists();
    require(playlists.size() == 1 && playlists[0].id == 7
            && playlists[0].name == "Saved playlist", "preserve playlists");
    const auto members = catalog.playlistTracks(7);
    require(members.size() == 1 && members[0].track.id == 41 && members[0].favorite,
        "preserve playlist membership and favorites");
    const auto history = catalog.recentDownloads();
    require(history.size() == 1 && history[0].id == 9 && history[0].hasTrack
            && history[0].track.id == 41 && history[0].track.storedExtension == ".mp3"
            && history[0].status == "completed" && history[0].message == "Saved download",
        "preserve download history and its track reference");
}

} // namespace

int main()
{
    try {
        bang::test::TemporaryDirectory directory;
        createVersionOneLibrary(directory.path / "library.sqlite3");
        {
            bang::LibraryStore store(directory.path);
            checkMigratedLibrary(store);
            const auto module = store.addTrack({ .contentHash = "module-hash",
                .storedExtension = ".mod", .title = "Module after migration" });
            require(module.id > 41 && module.storedExtension == ".mod",
                "allow new tracks with explicit extensions after migration");
        }
        bang::LibraryStore reopened(directory.path);
        checkMigratedLibrary(reopened);
        const auto module = reopened.trackByHash("module-hash");
        require(module.has_value() && module->storedExtension == ".mod"
                && bang::LibraryCatalog(reopened).allTracks().size() == 2,
            "reopening does not repeat or damage the migration");
        std::cout << "PASS version 1 migration and reopening\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
