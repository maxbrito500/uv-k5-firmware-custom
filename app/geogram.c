#include <math.h>
#include "geogram.h"
#include "driver/bk4819.h"
#include "ui/ui.h"

// DTMF frequency pairs (Hz)
static const float ROW_FREQ[] = {697, 770, 852, 941};
static const float COL_FREQ[] = {1209, 1336, 1477};
#define SAMPLE_RATE 8000
#define SAMPLE_BUFFER_SIZE 128  // 16ms buffer

// Detection thresholds (adjust empirically)
#define ROW_THRESHOLD 1e6
#define COL_THRESHOLD 1e6

static char last_digit = '\0';
static uint8_t digit_counter = 0;

// Goertzel algorithm for a single frequency
static float goertzel(const int16_t *samples, uint16_t size, float freq) {
    float coeff = 2 * cos(2 * M_PI * freq / SAMPLE_RATE);
    float q0, q1 = 0, q2 = 0;
    
    for (uint16_t i = 0; i < size; i++) {
        q0 = coeff * q1 - q2 + samples[i];
        q2 = q1;
        q1 = q0;
    }
    return q1*q1 + q2*q2 - coeff*q1*q2;
}

// Main DTMF detection function
char GEORGRAM_DetectDTMF(const int16_t *samples, uint16_t size) {
    float row_power[4], col_power[3];
    
    // Check row frequencies
    for (uint8_t i = 0; i < 4; i++) {
        row_power[i] = goertzel(samples, size, ROW_FREQ[i]);
    }
    
    // Check column frequencies
    for (uint8_t i = 0; i < 3; i++) {
        col_power[i] = goertzel(samples, size, COL_FREQ[i]);
    }
    
    // Find strongest row/column
    uint8_t row_max = 0, col_max = 0;
    for (uint8_t i = 1; i < 4; i++) {
        if (row_power[i] > row_power[row_max]) row_max = i;
    }
    for (uint8_t i = 1; i < 3; i++) {
        if (col_power[i] > col_power[col_max]) col_max = i;
    }
    
    // Validate against thresholds
    if (row_power[row_max] > ROW_THRESHOLD && 
        col_power[col_max] > COL_THRESHOLD) {
        
        const char digits[4][3] = {
            {'1', '2', '3'},
            {'4', '5', '6'},
            {'7', '8', '9'},
            {'*', '0', '#'}
        };
        
        char current = digits[row_max][col_max];
        
        // Simple debounce - require 2 consecutive detections
        if (current == last_digit) {
            if (++digit_counter > 1) {
                digit_counter = 0;
                return current;
            }
        } else {
            digit_counter = 0;
            last_digit = current;
        }
    }
    
    return '\0';
}

// Audio capture and processing wrapper
void GEORGRAM_ProcessAudio() {
    static int16_t sample_buffer[SAMPLE_BUFFER_SIZE];
    
    // Read audio from BK4819 FIFO
    BK4819_ReadRegisterBlock(BK4819_REG_7F, sample_buffer, SAMPLE_BUFFER_SIZE);
    
    // Detect and handle DTMF
    char digit = GEORGRAM_DetectDTMF(sample_buffer, SAMPLE_BUFFER_SIZE);
    if (digit != '\0') {
        UI_DisplayDTMF(digit);  // Show on screen
        // Add custom actions here if needed
    }
}