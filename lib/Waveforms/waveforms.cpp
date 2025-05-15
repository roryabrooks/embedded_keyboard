#include <waveforms.h>
#include <math.h>

const int lut[256] = {
   0,   3,   6,   9,  12,  16,  19,  22,  25,  28,
  31,  34,  37,  40,  43,  46,  49,  51,  54,  57,
  60,  63,  65,  68,  71,  73,  76,  78,  81,  83,
  85,  88,  90,  92,  94,  96,  98, 100, 102, 104,
 106, 107, 109, 111, 112, 113, 115, 116, 117, 118,
 120, 121, 122, 122, 123, 124, 125, 125, 126, 126,
 126, 127, 127, 127, 127, 127, 127, 127, 126, 126,
 126, 125, 125, 124, 123, 122, 122, 121, 120, 118,
 117, 116, 115, 113, 112, 111, 109, 107, 106, 104,
 102, 100,  98,  96,  94,  92,  90,  88,  85,  83,
  81,  78,  76,  73,  71,  68,  65,  63,  60,  57,
  54,  51,  49,  46,  43,  40,  37,  34,  31,  28,
  25,  22,  19,  16,  12,   9,   6,   3,   0,  -3,
  -6,  -9, -12, -16, -19, -22, -25, -28, -31, -34,
 -37, -40, -43, -46, -49, -51, -54, -57, -60, -63,
 -65, -68, -71, -73, -76, -78, -81, -83, -85, -88,
 -90, -92, -94, -96, -98,-100,-102,-104,-106,-107,
-109,-111,-112,-113,-115,-116,-117,-118,-120,-121,
-122,-122,-123,-124,-125,-125,-126,-126,-126,-127,
-127,-127,-127,-127,-127,-127,-126,-126,-126,-125,
-125,-124,-123,-122,-122,-121,-120,-118,-117,-116,
-115,-113,-112,-111,-109,-107,-106,-104,-102,-100,
 -98, -96, -94, -92, -90, -88, -85, -83, -81, -78,
 -76, -73, -71, -68, -65, -63, -60, -57, -54, -51,
 -49, -46, -43, -40, -37, -34, -31, -28, -25, -22,
 -19, -16, -12,  -9,  -6,  -3 };

int32_t Vout;

int32_t sawtoothGen(int32_t scaledPhase) {
    Vout = scaledPhase;
    return Vout;
}

int32_t sinGen(int32_t scaledPhase) {
    uint8_t index = scaledPhase + 128; //scaledPhase becomes index to look up table
    Vout = lut[index];                 //look up table
    return Vout;
}

int32_t squareGen(int32_t scaledPhase) {
    Vout = scaledPhase < 0 ? -128 : 127;
    return Vout;
}

int32_t triangleGen(int32_t scaledPhase) {
    if (scaledPhase <= 0) { Vout = 2 * (scaledPhase + 64); }
    else { Vout = 2 * (64 - scaledPhase); }
    return Vout;
}

int32_t waveformGenerator(uint32_t phaseAcc, int waveSelect) {
    int32_t scaledPhase = (phaseAcc >> 24) - 128;
    if (waveSelect == 0) { return sawtoothGen(scaledPhase); }
    else if (waveSelect == 1) { return sinGen(scaledPhase); }
    else if (waveSelect == 2) { return squareGen(scaledPhase); }
    else if (waveSelect == 3) { return triangleGen(scaledPhase); }
    else { return 0; }
}

//vibrato using joystick Y - this is over a range of 2 keys (whole tone) each direction
uint32_t vibratoFunc(const uint32_t stepSizes[12], int firstKeyPressed, int32_t joyYInput){
    uint32_t noteBelow;
    uint32_t noteAbove;
    uint32_t thisNote = stepSizes[firstKeyPressed];
    uint32_t bendStepSize;
    joyYInput = joyYInput - 512;

    if (firstKeyPressed == 0 || firstKeyPressed == 1) {
        noteAbove = stepSizes[firstKeyPressed + 2];
        noteBelow = stepSizes[firstKeyPressed + 10] / 2;
    } else if (firstKeyPressed == 11 || firstKeyPressed == 10) {
        noteAbove = stepSizes[firstKeyPressed - 10] * 2;
        noteBelow = stepSizes[firstKeyPressed - 2];
    } else {
        noteAbove = stepSizes[firstKeyPressed + 2];
        noteBelow = stepSizes[firstKeyPressed - 2];
    }

    if (joyYInput > 0) {
        bendStepSize = thisNote + (joyYInput / 512.0) * (noteAbove - thisNote);
    } else {
        bendStepSize = thisNote + (joyYInput / 512.0) * (thisNote - noteBelow);
    }

    return bendStepSize;
}