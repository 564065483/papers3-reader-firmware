#include "timer_service.h"

#include <algorithm>
#include <ctime>

namespace papers3 {

std::int64_t TimerService::epochSeconds() { return static_cast<std::int64_t>(std::time(nullptr)); }

void TimerService::init(AppState& state)
{
    if (state.timers.empty()) {
        state.timers.push_back({"tea", "泡茶", 180, 180, false, 0});
        state.timers.push_back({"rest", "休息", 600, 600, false, 0});
    }
}

bool TimerService::update(AppState& state)
{
    bool completed = false;
    const auto now = epochSeconds();
    for (auto& timer : state.timers) {
        if (!timer.running || timer.endsAtEpoch <= 0) continue;
        const auto remaining = std::max<std::int64_t>(0, timer.endsAtEpoch - now);
        timer.remainingSeconds = static_cast<std::uint32_t>(remaining);
        if (!remaining) { timer.running = false; timer.endsAtEpoch = 0; completed = true; }
    }
    auto& pomodoro = state.pomodoro;
    if (pomodoro.running && pomodoro.endsAtEpoch > 0) {
        const auto remaining = std::max<std::int64_t>(0, pomodoro.endsAtEpoch - now);
        pomodoro.remainingSeconds = static_cast<std::uint32_t>(remaining);
        if (!remaining) {
            pomodoro.running = false;
            pomodoro.endsAtEpoch = 0;
            if (pomodoro.focusMode) ++pomodoro.cycles;
            completed = true;
        }
    }
    return completed;
}

void TimerService::startTimer(AppState& state, std::size_t index)
{
    if (index >= state.timers.size()) return;
    auto& timer = state.timers[index];
    if (!timer.remainingSeconds) timer.remainingSeconds = timer.durationSeconds;
    timer.endsAtEpoch = epochSeconds() + timer.remainingSeconds;
    timer.running = true;
}

void TimerService::pauseTimer(AppState& state, std::size_t index)
{
    if (index >= state.timers.size()) return;
    auto& timer = state.timers[index];
    if (timer.running) timer.remainingSeconds = static_cast<std::uint32_t>(std::max<std::int64_t>(0, timer.endsAtEpoch - epochSeconds()));
    timer.running = false;
    timer.endsAtEpoch = 0;
}

void TimerService::resetTimer(AppState& state, std::size_t index)
{
    if (index >= state.timers.size()) return;
    auto& timer = state.timers[index];
    timer.remainingSeconds = timer.durationSeconds;
    timer.running = false;
    timer.endsAtEpoch = 0;
}

void TimerService::startPomodoro(AppState& state)
{
    auto& timer = state.pomodoro;
    if (!timer.remainingSeconds) timer.remainingSeconds = static_cast<std::uint32_t>((timer.focusMode ? timer.focusMinutes : timer.breakMinutes) * 60);
    timer.endsAtEpoch = epochSeconds() + timer.remainingSeconds;
    timer.running = true;
}

void TimerService::pausePomodoro(AppState& state)
{
    auto& timer = state.pomodoro;
    if (timer.running) timer.remainingSeconds = static_cast<std::uint32_t>(std::max<std::int64_t>(0, timer.endsAtEpoch - epochSeconds()));
    timer.running = false;
    timer.endsAtEpoch = 0;
}

void TimerService::resetPomodoro(AppState& state)
{
    auto& timer = state.pomodoro;
    timer.running = false;
    timer.endsAtEpoch = 0;
    timer.remainingSeconds = static_cast<std::uint32_t>((timer.focusMode ? timer.focusMinutes : timer.breakMinutes) * 60);
}

void TimerService::switchPomodoroMode(AppState& state, bool focusMode)
{
    state.pomodoro.focusMode = focusMode;
    resetPomodoro(state);
}

}  // namespace papers3
