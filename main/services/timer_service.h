#pragma once

#include "app_state.h"
#include <cstdint>

namespace papers3 {

class TimerService {
public:
    void init(AppState& state);
    bool update(AppState& state);
    void startTimer(AppState& state, std::size_t index);
    void pauseTimer(AppState& state, std::size_t index);
    void resetTimer(AppState& state, std::size_t index);
    void startPomodoro(AppState& state);
    void pausePomodoro(AppState& state);
    void resetPomodoro(AppState& state);
    void switchPomodoroMode(AppState& state, bool focusMode);

private:
    static std::int64_t epochSeconds();
};

}  // namespace papers3
