#include <ctime>

#if defined(_WIN32)
static inline std::tm* localtime_r(const std::time_t* value, std::tm* result)
{
    return localtime_s(result, value) == 0 ? result : nullptr;
}
#endif

#include "../../main/app/app_state.cpp"
#include "../../main/input/input_method.cpp"
#include "../../main/ui/ui_renderer.cpp"
