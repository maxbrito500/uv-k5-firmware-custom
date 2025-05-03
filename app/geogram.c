#include "geogram.h"
#include "driver/bk4819.h"
#include "ui/ui.h"

#define SAMPLE_BUFFER_SIZE 128
#define SAMPLE_RATE 8000
#define DTMF_THRESHOLD 1e6


// Coeficientes Goertzel pré-calculados: 2 * cos(2πf / Fs)
static const float GOERTZEL_COEFFS[] = {
    1.624,  // 697 Hz
    1.506,  // 770 Hz
    1.389,  // 852 Hz
    1.260,  // 941 Hz
    0.875,  // 1209 Hz
    0.658,  // 1336 Hz
    0.446   // 1477 Hz
};

// Dígito detectado
static char dtmf_display = ' ';

// Acesso externo ao último dígito
char GEOGRAM_GetLastDigit(void) {
    return dtmf_display;
}

// Implementação do algoritmo de Goertzel
static float goertzel(const int16_t *samples, float coeff) {
    float q0, q1 = 0, q2 = 0;

    for (uint16_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        q0 = coeff * q1 - q2 + samples[i];
        q2 = q1;
        q1 = q0;
    }
    return q1*q1 + q2*q2 - coeff*q1*q2;
}

// Execução periódica no main loop
void GEOGRAM_ProcessAudio(void) {
    int16_t sample_buffer[SAMPLE_BUFFER_SIZE];
    static char last_digit = '\0';
    static uint8_t digit_count = 0;

    // Captura de amostras de áudio
    for (uint16_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        sample_buffer[i] = BK4819_ReadRegister(0x7F) >> 4;
    }

    // Potência nas 7 frequências
    float row_power[4], col_power[3];
    for (uint8_t i = 0; i < 4; i++) {
        row_power[i] = goertzel(sample_buffer, GOERTZEL_COEFFS[i]);
    }
    for (uint8_t i = 0; i < 3; i++) {
        col_power[i] = goertzel(sample_buffer, GOERTZEL_COEFFS[4 + i]);
    }

    // Maior potência em linha/coluna
    uint8_t row_max = 0, col_max = 0;
    for (uint8_t i = 1; i < 4; i++) {
        if (row_power[i] > row_power[row_max]) row_max = i;
    }
    for (uint8_t i = 1; i < 3; i++) {
        if (col_power[i] > col_power[col_max]) col_max = i;
    }

    // Verificação de limiar
    if (row_power[row_max] > DTMF_THRESHOLD &&
        col_power[col_max] > DTMF_THRESHOLD) {

        const char digits[4][3] = {
            {'1','2','3'},
            {'4','5','6'},
            {'7','8','9'},
            {'*','0','#'}
        };

        char current = digits[row_max][col_max];

        // Detecção com debounce simples
        if (current == last_digit) {
            if (++digit_count == 2) {
                dtmf_display = current;
                // Ação customizada pode ser adicionada aqui
            }
        } else {
            digit_count = 0;
            last_digit = current;
        }
    }
}
