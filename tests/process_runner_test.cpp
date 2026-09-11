#include "bang/ProcessRunner.hpp"

#include <unistd.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

bool check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << std::endl;
    return condition;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 2) {
        const std::string mode = argv[1];
        if (mode == "both-streams") {
            std::cout << "ready" << std::endl;
            std::cerr << std::string(1024 * 1024, 'x') << std::flush;
            std::cout << "done";
        } else {
            if (mode == "closed-streams") {
                ::close(STDOUT_FILENO);
                ::close(STDERR_FILENO);
            } else {
                std::cout << "ready" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        return 0;
    }

    bang::RunOptions options;
    options.program = std::filesystem::canonical(argv[0]).string();
    options.arguments = { "both-streams" };
    options.timeout = std::chrono::seconds(2);
    std::vector<std::string> lines;
    const auto result = bang::ProcessRunner::runStreaming(options,
        [&](std::string_view line) { lines.emplace_back(line); });
    bool ok = check(result.succeeded(), "child can fill stderr while stdout stays open");
    ok &= check(result.errorOutput == std::string(1024 * 1024, 'x'),
        "capture all stderr");
    ok &= check(lines == std::vector<std::string> { "ready", "done" },
        "stream complete lines and final unterminated line");

    for (const char* mode : { "open-streams", "closed-streams" }) {
        options.arguments = { mode };
        options.timeout = std::chrono::milliseconds(100);
        const auto start = std::chrono::steady_clock::now();
        const auto timed = bang::ProcessRunner::run(options);
        ok &= check(timed.timedOut && !timed.succeeded(), mode);
        ok &= check(std::chrono::steady_clock::now() - start < std::chrono::seconds(2),
            "timeout stops and reaps the child promptly");
    }
    return ok ? 0 : 1;
}
