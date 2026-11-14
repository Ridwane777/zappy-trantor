#pragma once

#include <chrono>

namespace zappy::app {

// Turns wall-clock time into game time units at `freq` units per second.
class GameClock {
public:
    using Clock = std::chrono::steady_clock;

    explicit GameClock(int freq, Clock::time_point now = Clock::now());

    int freq() const { return _freq; }
    void setFreq(int freq, Clock::time_point now = Clock::now());

    // Time units elapsed since the last call (the clock advances by that much).
    int consumeTicks(Clock::time_point now = Clock::now());
    // Milliseconds until the next time unit, at least 1.
    int msUntilNextTick(Clock::time_point now = Clock::now()) const;

private:
    int _freq;
    Clock::time_point _last;
    double unitSeconds() const { return 1.0 / _freq; }
};

} // namespace zappy::app
