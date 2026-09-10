// M3U/M3U8 import and export. importM3u only pulls in local files, remote
// URLs listed in a playlist are silently skipped (use the download flow for
// those instead). Imported files are deduped by content hash the same way
// TrackImporter dedupes everywhere else, so importing the same playlist
// twice adds zero duplicate tracks, just re-links existing ones.
#pragma once

#include "bang/LibraryStore.hpp"
#include "bang/TrackImporter.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace bang {

class PlaylistService {
public:
    struct ImportResult {
        std::size_t imported = 0;
        std::size_t duplicates = 0;
        std::size_t failed = 0;
    };

    PlaylistService(LibraryStore& store, TrackImporter& importer);

    // Imports an M3U/M3U8 playlist into an existing playlist. Relative entries
    // are resolved relative to the playlist file. Comments and remote URLs are
    // ignored, while local audio files are imported into the library and added
    // in playlist order.
    [[nodiscard]] ImportResult importM3u(
        const std::filesystem::path& playlistPath,
        std::int64_t playlistId) const;

    // Exports the stored files in playlist order as an M3U8 playlist.
    [[nodiscard]] bool exportM3u(
        std::int64_t playlistId,
        const std::filesystem::path& destination) const;

private:
    LibraryStore* store_;
    TrackImporter* importer_;
};

} // namespace bang
