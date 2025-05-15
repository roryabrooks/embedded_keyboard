#ifndef KNOB_H
#define KNOB_H

#include <bitset>
#include <STM32FreeRTOS.h>

class Knob {
    private:
        uint8_t lowerLimit = 0;
        uint8_t upperLimit = 8;
        uint8_t rotation;
        bool pressed;
        bool loaded = false;
        bool rotationDir = false;
        std::bitset<2> currentAB;
        SemaphoreHandle_t mutex;

    public:
        Knob();

        void init(uint8_t knobNumber);

        bool isLoaded() const;

        bool isPressed() const;

        uint8_t getRotation() const;

        void setPressed(bool press);

        void setRotation(int rotVal);

        void setLowerLimit(int lower);

        void setUpperLimit(int upper);

        void updateRotation(bool A, bool B);
};

#endif