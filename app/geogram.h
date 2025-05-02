#ifndef __GEORGRAM_H__
#define __GEORGRAM_H__

#include <stdint.h>

void GEORGRAM_ProcessAudio(void);
char GEORGRAM_DetectDTMF(const int16_t *samples, uint16_t size);

#endif