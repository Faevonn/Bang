#include "bang/DownloadService.hpp"
#include "bang/LibraryCatalog.hpp"
#include "bang/ProcessRunner.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>

namespace fs = std::filesystem;

namespace {

bool check(bool condition, const std::string& name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << std::endl;
    return condition;
}

int runDownloader(const std::string& mode)
{
    for (int index = 1; index <= 3; ++index) {
        const auto file = fs::current_path() / ("track" + std::to_string(index) + ".mp3");
        fs::copy_file(fs::path(std::getenv("BANG_TEST_FIXTURES")) / file.filename(), file);
        if (mode == "missing-second" && index == 2) {
            fs::remove(file);
        }
        std::cout << "BANGDONE|" << file.string() << "|Track " << index
                  << "|Test|1" << std::endl;
    }
    if (mode == "exit-error") {
        std::cerr << "ERROR: another playlist entry is unavailable\n";
        return 1;
    }
    return 0;
}

bool runCase(const fs::path& root, const std::string& mode, std::size_t expectedCount)
{
    bang::LibraryStore store(root / mode / "data");
    bang::TrackImporter importer(store);
    bang::LibraryCatalog catalog(store);
    std::mutex mutex;
    std::condition_variable signal;
    bool finished = false;
    bang::DownloadService::Job result;
    bang::DownloadService downloads(store, importer, root / mode / "tmp");
    downloads.setListener([&] {
        const auto jobs = downloads.snapshot();
        if (!jobs.empty() && (jobs[0].state == bang::DownloadService::State::Failed
                                || jobs[0].state == bang::DownloadService::State::Completed)) {
            std::lock_guard lock(mutex);
            result = jobs[0];
            finished = true;
            signal.notify_one();
        }
    });
    downloads.enqueue({ mode, bang::DownloadService::Backend::YtDlp });
    {
        std::unique_lock lock(mutex);
        if (!signal.wait_for(lock, std::chrono::seconds(5), [&] { return finished; })) {
            std::cerr << "FAIL " << mode << " did not finish\n";
            std::_Exit(1);
        }
    }
    downloads.setListener({});
    bool ok = check(catalog.allTracks().size() == expectedCount, mode + ": imported tracks");
    const bool failed = mode != "all-tracks";
    ok &= check(result.state == (failed ? bang::DownloadService::State::Failed
                                      : bang::DownloadService::State::Completed),
        mode + ": status");
    if (failed) {
        ok &= check(result.message.find(mode == "missing-second" ? "missing file" : "unavailable")
                != std::string::npos,
            mode + ": retains failure reason");
        ok &= check(result.message.find(std::to_string(expectedCount) + " tracks imported")
                != std::string::npos,
            mode + ": reports partial success");
    }
    const auto history = catalog.recentDownloads();
    ok &= check(history.size() == 1 && history[0].message == result.message,
        mode + ": persists result");
    return ok;
}

} // namespace

int main(int argc, char** argv)
{
    if (fs::path(argv[0]).filename() == "yt-dlp") {
        return runDownloader(argv[1]);
    }
    char directory[] = "/tmp/bang-download-test-XXXXXX";
    const char* temporary = ::mkdtemp(directory);
    if (temporary == nullptr) {
        return 1;
    }
    const fs::path root(temporary);
    fs::create_directories(root / "bin");
    fs::create_symlink(fs::canonical(argv[0]), root / "bin/yt-dlp");
    ::setenv("BANG_TEST_FIXTURES", root.c_str(), 1);
    ::setenv("XDG_DATA_HOME", root.c_str(), 1);
    const auto ffmpeg = bang::ProcessRunner::findExecutable("ffmpeg");
    if (!ffmpeg) {
        std::cerr << "ffmpeg is required to generate the download test audio\n";
        fs::remove_all(root);
        return 1;
    }
    for (int index = 1; index <= 3; ++index) {
        bang::RunOptions options;
        options.program = *ffmpeg;
        options.arguments = { "-v", "error", "-f", "lavfi", "-i",
            "sine=frequency=" + std::to_string(index * 440) + ":duration=0.1",
            "-c:a", "libmp3lame", (root / ("track" + std::to_string(index) + ".mp3")).string() };
        if (!bang::ProcessRunner::run(options).succeeded()) {
            fs::remove_all(root);
            return 1;
        }
    }
    const std::string path = (root / "bin").string() + ":" + std::getenv("PATH");
    ::setenv("PATH", path.c_str(), 1);
    bool ok = runCase(root, "all-tracks", 3);
    ok &= runCase(root, "missing-second", 2);
    ok &= runCase(root, "exit-error", 3);
    fs::remove_all(root);
    return ok ? 0 : 1;
}
