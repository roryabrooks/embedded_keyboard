#ifndef GLOBALS_H
#define GLOBALS_H

#include <U8g2lib.h>
#include <STM32FreeRTOS.h>
#include <Knob.h>
#include <State.h>
#include <constants.h>

//Globals
//Display driver object
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0);

//Task Handles
TaskHandle_t handshakeHandle = NULL;
TaskHandle_t scanKeysHandle = NULL;
TaskHandle_t playNotesHandle = NULL;
TaskHandle_t joystickUpdateHandle = NULL;
TaskHandle_t displayUpdateHandle = NULL;
TaskHandle_t decodeMessageHandle = NULL;
TaskHandle_t transmitMessageHandle = NULL;

//System State
// Use malloc if stack too large - requires ~State() destructor
// e.g. State* sysState = new State(); in setup
// State->setOctave() to change octave
SysState sysState;

//Knob Array
// Use malloc if stack too large - requires ~Knob() destructor
// e.g. Knob* knobs[4]; then knob[i] = new Knob(); in setup
// Knob->getRotation() to get rotation
// Button: knob[0] = loop record, knob[1] = loop stop, knob[2] = ?       knob[3] = set receiver
// Rotate: knob[0] = ?,           knob[1] = waveform,  knob[2] = octave, knob[3] = volume
Knob knobs[4];

//CAN Communication
// 0 - (P)ressed, (R)eleased, (N)ew HS, (F)inish HS, (V)olume, (W)aveform, (H)ighest Octave, (L)owest Octave, (T)ransmitter
// 1 - Octave(1-7) / Position(0-255) on startup
// 2 - Note number(0-11) / Assign(1/0) on octave change
// 3 - Volume(0-8) / Waveform (0-3)
// 4 - Connections None(0b00), East(0b01), West(0b10), Both(0b11)
QueueHandle_t msgInQ, msgOutQ, notePlayingQ; // CAN message queues
SemaphoreHandle_t CAN_TX_Semaphore;

//Step Sizes
volatile uint32_t currentStepSize[ACCUMULATORS] = {0};
SemaphoreHandle_t stepSizeMutex;

//Notes & Octaves Playing
volatile uint32_t notesPlaying[ACCUMULATORS][2] = {{999,4}, {999,4}, {999,4}, {999,4}, {999,4}, {999,4}, {999,4}, {999,4}, {999,4}, {999,4}};
SemaphoreHandle_t notesPlayingMutex;

#endif