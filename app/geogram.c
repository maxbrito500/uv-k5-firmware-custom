#include <stdint.h>
#include "driver/bk4819.h"
#include "ui/helper.h"  // UI_PrintStringSmallNormal

// Track time by counting hook executions (10ms per call)
static uint32_t hookCounter = 0;
static const uint32_t UPDATE_INTERVAL = 3; // 3 * 10ms = 30ms

// Fast hex conversion (no snprintf)
static inline void uint16ToHex(char *buf, uint16_t value) {
    const char hexDigits[] = "0123456789ABCDEF";
    buf[0] = hexDigits[(value >> 12) & 0xF];
    buf[1] = hexDigits[(value >> 8) & 0xF];
    buf[2] = hexDigits[(value >> 4) & 0xF];
    buf[3] = hexDigits[value & 0xF];
}

void GEOGRAM_Hook(void) {
    hookCounter++;
    
    if (hookCounter % UPDATE_INTERVAL != 0) {
        return; // Skip if not enough time passed
    }

    uint16_t sample = BK4819_ReadRegister(0x7F) >> 4;

    // Format directly into a buffer
    char buffer[6] = "A:____"; // Pre-filled with underscores for visibility
    uint16ToHex(&buffer[2], sample); // Write hex at position 2

    UI_PrintStringSmallNormal(buffer, 0, 128, 0);
}