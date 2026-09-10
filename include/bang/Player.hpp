// Thin wrapper around GStreamer's playbin element, playbin handles
// demux/decode/output selection on its own so this class barely needs to
// know about GStreamer internals beyond state changes and bus messages.
// IMPORTANT: poll() has to be called regularly from the main loop (see
// main.cpp's frame loop). It's what drains the GStreamer bus for EOS/error/
// state-changed messages. If nothing calls poll(), end-of-track never
// fires onFinished_ and playback just silently sits there after the file
// ends. gst_init_check is only run once process-wide (the static bool).
#pragma once

#include <filesystem>
#include <functional>
#include <string>

struct _GstElement;
struct _GstBus;

namespace bang {

class Player {
public:
    enum class State { Stopped, Playing, Paused };

    Player();
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    void load(const std::filesystem::path& file);
    void play();
    void pause();
    void toggle();
    void stop();

    void seek(std::int64_t positionMs);
    void setVolume(double volume);

    [[nodiscard]] State state() const { return state_; }
    [[nodiscard]] std::int64_t positionMs();
    [[nodiscard]] std::int64_t durationMs() const;

    using FinishedCallback = std::function<void()>;
    void setOnFinished(FinishedCallback callback);

    bool poll();

private:
    _GstElement* playbin_ = nullptr;
    _GstBus* bus_ = nullptr;
    State state_ = State::Stopped;
    double volume_ = 0.8;
    FinishedCallback onFinished_;
};

} // namespace bang
