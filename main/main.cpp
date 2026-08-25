#include "app_controller.h"

using papers3::AppController;

extern "C" void app_main(void)
{
    static AppController app;
    app.init();
    while (true) {
        app.update();
    }
}

