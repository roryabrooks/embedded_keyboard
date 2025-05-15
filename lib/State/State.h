#ifndef STATE_H
#define STATE_H

#include <bitset>
#include <STM32FreeRTOS.h>

class SysState {
    private:
        bool receiver = false;
        bool looping = false;
        std::bitset<28> inputs = 0xFFFFFFF;
        uint8_t waveform = 0;
        uint8_t volume = 0;
        uint8_t connections = 0;
        uint8_t octave = 0;
        uint8_t lowestOctave = 0;
        uint8_t highestOctave = UINT8_MAX;
        uint8_t receiverOctave = 4;
        SemaphoreHandle_t mutex;

    public:
        SysState();

        bool isReceiver() const;

        bool isLooping() const;

        std::bitset<28> getInputs() const;

        uint8_t getWaveform() const;

        uint8_t getVolume() const;

        uint8_t getConns() const;

        uint8_t getOctave() const;

        uint8_t getLowestOctave() const;

        uint8_t getHighestOctave() const;

        uint8_t getReceiverOctave() const;

        void setReceiver(bool rec);
        
        void setLooping(bool loop);

        void setInputs(std::bitset<28> in);

        void setWaveform(uint8_t wave);

        void setVolume(uint8_t vol);

        void setConns(uint8_t conns);

        void setOctave(uint8_t oct);

        void setLowestOctave(uint8_t lowOct);

        void setHighestOctave(uint8_t highOct);

        void setReceiverOctave(uint8_t recOct);
};

#endif