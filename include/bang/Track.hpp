// Plain data shapes for a library track and a track shown in a listing.
// contentHash is the SHA-256 used to find the file on disk (see LibraryStore
// trackFilePath), so it must stay in sync with whatever TrackImporter wrote.
#pragma once

#include <cstdint>
#include <string>

namespace bang {

struct Track {
    std::int64_t id = 0;
    std::string contentHash;
    std::string storedExtension = ".mp3";
    std::string title;
    std::string artist;
    std::string album;
    std::int64_t durationMs = 0;
    std::string source;
    std::string sourceUrl;
    std::int64_t addedAtMs = 0;
};

struct TrackListing {
    Track track;
    bool favorite = false;
};

} // namespace bang
