// YouTube search via `yt-dlp --flat-playlist ytsearchN:query`, parsed out
// of tab-separated stdout in DownloadParsing.cpp. This is a blocking call
// (up to 60s timeout), main.cpp runs it on a worker thread, see the search
// dialog code.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bang {

class SearchService {
public:
    struct Hit {
        std::string videoId;
        std::string title;
        std::string uploader;
        std::int64_t durationSec = 0;
    };

    [[nodiscard]] std::vector<Hit> searchYouTube(
        const std::string& query, std::size_t limit = 20) const;
};

} // namespace bang
