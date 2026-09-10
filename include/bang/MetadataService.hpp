// TagLib wrapper for reading/writing ID3v2 tags and cover art. Only speaks
// MPEG/.mp3 (TagLib::MPEG::File), on purpose: .mod tracker files are never
// passed through here, see TrackImporter which skips tagging entirely for
// those. If MP4/FLAC/etc support is ever needed, this needs a real
// TagLib::FileRef dispatch instead of hardcoding MPEG::File.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace bang {

struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::int64_t durationMs = 0;
    std::vector<std::byte> artwork;
};

class MetadataService {
public:
    static AudioMetadata read(const std::filesystem::path& file);
    static void write(const std::filesystem::path& file,
        const AudioMetadata& fields);
    static void embedArtwork(
        const std::filesystem::path& file, const std::vector<std::byte>& image);
};

} // namespace bang
