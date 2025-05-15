#ifndef WAVEFORMS_H
#define WAVEFORMS_H

#include <Arduino.h>

int32_t waveformGenerator(uint32_t phaseAcc, int waveSelect);

uint32_t vibratoFunc(const uint32_t stepSizes[12], int firstKeyPressed, int32_t joyYInput);

#endif