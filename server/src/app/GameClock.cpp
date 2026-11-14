#include "app/GameClock.hpp"
#include <algorithm>

namespace zappy::app {

GameClock::GameClock(int freq, Clock::time_point now) : _freq(freq), _last(now) {}

void GameClock::setFreq(int freq, Clock::time_point now)
{
    _freq = freq;
    _last = now;
}

int GameClock::consumeTicks(Clock::time_point now)
{
    double elapsed = std::chrono::duration<double>(now - _last).count();
    int ticks = static_cast<int>(elapsed / unitSeconds());
    if (ticks > 0)
        _last += std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(ticks * unitSeconds()));
    return ticks;
}

int GameClock::msUntilNextTick(Clock::time_point now) const
{
    double remaining = unitSeconds() - std::chrono::duration<double>(now - _last).count();
    return std::max(1, static_cast<int>(remaining * 1000.0));
}

} // namespace zappy::app
