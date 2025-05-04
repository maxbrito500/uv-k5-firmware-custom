#include <stdint.h>
#include <ui/helper.h>
#include "driver/st7565.h"


void GEOGRAM_Hook(void) {
    UI_DisplayClear();
    UI_DisplayPopup("TEST");
    ST7565_BlitFullScreen();
}
