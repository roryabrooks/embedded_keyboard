#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <bitset>
#include <cx_math.h>
#include <Arduino.h>
#include <STM32FreeRTOS.h>

//Pin Definitions
//Row select and enable
const int RA0_PIN = D3;
const int RA1_PIN = D6;
const int RA2_PIN = D12;
const int REN_PIN = A5;

//Matrix input and output
const int C0_PIN = A2;
const int C1_PIN = D9;
const int C2_PIN = A6;
const int C3_PIN = D1;
const int OUT_PIN = D11;

//Audio analogue out
const int OUTL_PIN = A4;
const int OUTR_PIN = A3;

//Joystick analogue in
const int JOYY_PIN = A0;
const int JOYX_PIN = A1;

//Output multiplexer bits
const int DEN_BIT = 3;
const int DRST_BIT = 4;
const int HKOW_BIT = 5;
const int HKOE_BIT = 6;

//Constants
//New Connection Stabilisation Time
const TickType_t CONN_TIME = pdMS_TO_TICKS(10);

//Accumulators - number of playable concurrent notes (10 fingers)
const int ACCUMULATORS = 10;

//Stack Sizes
const int HANDSHAKE_SIZE = 64;
const int SCANKEYS_SIZE  = 128;
const int PLAYNOTES_SIZE = 64;
const int JOYSTICK_SIZE  = 128;
const int DISPLAY_SIZE   = 256;
const int DECODE_SIZE    = 64;
const int TRANSMIT_SIZE  = 32;

//Notes
const std::bitset<28> NOTE_MASK = 0xFFF;
const char NOTE_NAMES[12][3] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};

//Step Sizes
const int INDEX_A = 10;
const uint32_t FREQ_A = 440; //frequency of the "a" note in octave 4
const uint32_t SAMPLE_RATE = 22000;
constexpr uint32_t constructStepSizes(int index) {
    uint32_t frequency = (uint32_t)(FREQ_A * cx::pow(2,(index - INDEX_A) / 12.0)); // Shift by 12 (divide by 2, 12 times)
    uint32_t scalar = cx::pow(2,32) / SAMPLE_RATE;
    return scalar * frequency;
}
const uint32_t stepSizes[12] = {
    /*
    NOTE: This function had an error when calling the "pow" function in constructStepSizes() from math.h as is not a constexpr.
    We have tried to use cmath.h std::pow which uses constexpr but PlatformIO still compiled the code using math.h
    In order to fix this we have included a cx_math.h file with its source and license commented in /include/cx_math.h
    The code was still able to run fully functionally with the error but has been fixed in order to ensure compliance with
    constexpr requirements and maintain compatibility with all platforms and toolchains
    */
    constructStepSizes(0),
    constructStepSizes(1),
    constructStepSizes(2),
    constructStepSizes(3),
    constructStepSizes(4),
    constructStepSizes(5),
    constructStepSizes(6),
    constructStepSizes(7),
    constructStepSizes(8),
    constructStepSizes(9),
    constructStepSizes(10),
    constructStepSizes(11)
};

#endif