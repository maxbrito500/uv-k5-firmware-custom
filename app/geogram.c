#include "geogram.h"
#include "driver/bk4819.h"
#include "ui/helper.h"

static uint16_t gLastMicLevel = 0;
static uint32_t gUpdateCounter = 0;

void GEOGRAM_ForceMicPath(void) {
    // Critical registers to enable microphone monitoring
    BK4819_WriteRegister(0x19, 0x0000);  // Enable MIC AGC
    BK4819_WriteRegister(0x30, 0xC1FE);  // Enable RX DSP path
    BK4819_WriteRegister(0x47, 0x6040);  // Configure AF path
    BK4819_WriteRegister(0x48, 0xB3A8);  // Set AF RX gains
}

void GEOGRAM_Init(void) {
    // Initial setup
    GEOGRAM_ForceMicPath();  // Use our new function
}

void GEOGRAM_EnableRXMonitoring(void) {
    // RX-specific configuration
    GEOGRAM_ForceMicPath();  // Reuse the same function
}

void GEOGRAM_Hook(void) {
    if (++gUpdateCounter < 3) return; // Update every ~30ms
    gUpdateCounter = 0;
    
    GEOGRAM_ForceMicPath();  // Continually reinforce the path
    
    gLastMicLevel = BK4819_ReadRegister(0x64) & 0x7FFF;
    
    char buffer[8];
    buffer[0] = 'M';
    buffer[1] = ':';
    buffer[2] = '0' + (gLastMicLevel / 1000) % 10;
    buffer[3] = '0' + (gLastMicLevel / 100) % 10;
    buffer[4] = '0' + (gLastMicLevel / 10) % 10;
    buffer[5] = '0' + gLastMicLevel % 10;
    buffer[6] = '\0';
    
    UI_PrintStringSmallNormal(buffer, 0, 127, false);
}