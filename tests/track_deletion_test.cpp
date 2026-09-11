#include "TestSupport.hpp"
#include "bang/LibraryCatalog.hpp"
#include "bang/LibraryStore.hpp"
#include "bang/TrackImporter.hpp"

#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using bang::test::require;

namespace {

void writeFile(const fs::path& path)
{
    std::ofstream file(path, std::ios::binary);
    file << "deletion test payload";
    require(file.good(), "create deletion fixture: " + path.string());
}

void checkDeletion(const std::string& extension)
{
    bang::test::TemporaryDirectory directory;
    bang::LibraryStore store(directory.path / "data");
    bang::LibraryCatalog catalog(store);
    const auto removed = store.addTrack({ .contentHash = "removed",
        .storedExtension = extension, .title = "Remove this track" });
    const auto retained = store.addTrack({ .contentHash = "retained",
        .storedExtension = extension, .title = "Keep this track" });
    const auto source = directory.path / ("original" + extension);
    writeFile(source);
    for (const auto& track : { removed, retained }) {
        writeFile(store.trackFilePath(track));
        writeFile(store.artworkPath(track.contentHash));
        store.setFavorite(track.id, true);
    }
    const auto playlist = store.createPlaylist("Keep this playlist");
    const auto secondPlaylist = store.createPlaylist("Another playlist");
    for (const auto id : { playlist, secondPlaylist }) {
        store.addToPlaylist(id, removed.id);
        store.addToPlaylist(id, retained.id);
    }
    const auto download = store.beginDownload("https://example.test/track", "yt-dlp");
    store.completeDownload(download, bang::DownloadStatus::Completed, "Saved history", removed.id);

    const auto hash = store.removeTrack(removed.id);
    require(hash == removed.contentHash, "return the deleted track's content hash");
    require(!store.trackById(removed.id).has_value()
            && !store.trackByHash(removed.contentHash).has_value(), "remove the track row");
    require(!store.isFavorite(removed.id) && store.isFavorite(retained.id),
        "remove only the deleted track's favorite");
    for (const auto id : { playlist, secondPlaylist }) {
        const auto members = catalog.playlistTracks(id);
        require(members.size() == 1 && members[0].track.id == retained.id,
            "remove the track from every playlist");
    }
    const auto history = catalog.recentDownloads();
    require(history.size() == 1 && history[0].id == download && !history[0].hasTrack
            && history[0].status == "completed" && history[0].message == "Saved history",
        "retain download history without a dangling track reference");
    require(fs::exists(store.trackFilePath(removed)),
        "database removal leaves files for explicit cleanup");

    bang::TrackImporter::deleteStoredFiles(store, *hash);
    require(!fs::exists(store.trackFilePath(removed))
            && !fs::exists(store.artworkPath(*hash)), "delete stored audio and artwork");
    require(!store.removeTrack(removed.id).has_value(), "repeated database removal is harmless");
    bang::TrackImporter::deleteStoredFiles(store, *hash);
    require(fs::exists(source) && fs::exists(store.trackFilePath(retained))
            && fs::exists(store.artworkPath(retained.contentHash))
            && catalog.allTracks().size() == 1 && catalog.playlists().size() == 2,
        "preserve source files, unrelated tracks, artwork, and playlists");
}

} // namespace

int main()
{
    try {
        checkDeletion(".mp3");
        checkDeletion(".mod");
        std::cout << "PASS MP3 and MOD deletion cleanup\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
