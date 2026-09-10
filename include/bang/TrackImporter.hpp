// Content-addressed import: hashes the source file (SHA-256), copies it
// into the library under tracks/<hash>.<ext> if that hash isn't already
// stored, tags it (skipped for .mod files, see MetadataService), and
// inserts/updates the DB row. Safe to call twice with the same file, the
// second call is a no-op copy (isNew == false) but still returns the track.
#pragma once

#include "bang/LibraryStore.hpp"
#include "bang/MetadataService.hpp"
#include "bang/Track.hpp"

#include <filesystem>
#include <string>

namespace bang {

class TrackImporter {
public:
    struct Result {
        Track track;
        bool isNew = false;
    };

    explicit TrackImporter(LibraryStore& store);

    [[nodiscard]] Result importFile(const std::filesystem::path& sourceFile,
        const AudioMetadata& overrides, const std::string& source,
        const std::string& sourceUrl) const;

    static void deleteStoredFiles(
        const LibraryStore& store, const std::string& contentHash);

private:
    LibraryStore* store_;
};

} // namespace bang
