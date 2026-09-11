#include "TestSupport.hpp"
#include "bang/Hash.hpp"
#include "bang/LibraryCatalog.hpp"
#include "bang/MetadataService.hpp"
#include "bang/PlaylistService.hpp"
#include "bang/ProcessRunner.hpp"
#include "bang/TrackImporter.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;
using bang::test::require;

int main()
{
    try {
        bang::test::TemporaryDirectory directory;
        const auto source = directory.path / "source.mp3";
        const auto ffmpeg = bang::ProcessRunner::findExecutable("ffmpeg");
        require(ffmpeg.has_value(), "ffmpeg is required to generate the import test audio");
        bang::RunOptions options;
        options.program = *ffmpeg;
        options.arguments = { "-v", "error", "-f", "lavfi", "-i",
            "sine=frequency=440:duration=0.1", "-c:a", "libmp3lame", source.string() };
        const auto generated = bang::ProcessRunner::run(options);
        require(generated.succeeded(), "generate MP3 fixture: " + generated.errorOutput);
        const auto sourceHash = bang::sha256File(source);

        bang::LibraryStore store(directory.path / "data");
        bang::LibraryCatalog catalog(store);
        bang::TrackImporter importer(store);
        const bang::AudioMetadata metadata {
            .title = "Imported title", .artist = "Imported artist", .album = "Imported album"
        };
        const auto first = importer.importFile(source, metadata, "local", {});
        const auto storedPath = store.trackFilePath(first.track);
        require(first.isNew && first.track.contentHash == sourceHash
                && fs::is_regular_file(storedPath) && storedPath != source,
            "copy a new import into the library");
        require(first.track.title == metadata.title && first.track.artist == metadata.artist
                && first.track.album == metadata.album && first.track.durationMs > 0,
            "index metadata and audio duration");
        const auto storedMetadata = bang::MetadataService::read(storedPath);
        require(storedMetadata.title == metadata.title && storedMetadata.artist == metadata.artist
                && storedMetadata.album == metadata.album, "write tags to the stored copy");
        require(bang::sha256File(source) == sourceHash, "leave the source audio unchanged");

        const auto second = importer.importFile(source, metadata, "local", {});
        const auto renamed = directory.path / "renamed.mp3";
        fs::copy_file(source, renamed);
        const auto third = importer.importFile(renamed, metadata, "local", {});
        require(!second.isNew && !third.isNew
                && second.track.id == first.track.id && third.track.id == first.track.id,
            "deduplicate repeated content across source paths");
        require(catalog.allTracks().size() == 1
                && std::distance(fs::directory_iterator(storedPath.parent_path()),
                       fs::directory_iterator {}) == 1,
            "retain one track row and one stored audio file");

        const auto playlist = store.createPlaylist("Repeated import");
        const auto m3u = directory.path / "source.m3u8";
        {
            std::ofstream file(m3u, std::ios::binary);
            file << "\xEF\xBB\xBF#EXTM3U\r\n# comment\r\nsource.mp3\r\n"
                    "renamed.mp3\r\nhttps://example.test/remote.mp3\r\nmissing.mp3\r\n";
            require(file.good(), "write M3U fixture");
        }
        bang::PlaylistService playlists(store, importer);
        const auto imported = playlists.importM3u(m3u, playlist);
        require(imported.imported == 1 && imported.duplicates == 1 && imported.failed == 1,
            "import local M3U entries and count duplicates and missing files");
        const auto repeated = playlists.importM3u(m3u, playlist);
        require(repeated.imported == 0 && repeated.duplicates == 2 && repeated.failed == 1,
            "repeat an M3U import without adding duplicate tracks");
        const auto members = catalog.playlistTracks(playlist);
        require(members.size() == 1 && members[0].track.id == first.track.id
                && catalog.allTracks().size() == 1, "reuse library tracks in playlists");

        const auto exported = directory.path / "exported.m3u8";
        require(playlists.exportM3u(playlist, exported), "export an existing playlist");
        std::ifstream file(exported);
        const std::string contents((std::istreambuf_iterator<char>(file)), {});
        require(contents == "#EXTM3U\n" + storedPath.string() + "\n",
            "export the stored audio path");
        std::cout << "PASS duplicate audio and playlist imports\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
