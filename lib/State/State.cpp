#include <State.h>

SysState::SysState() { mutex = xSemaphoreCreateMutex(); }

bool SysState::isLooping() const {
    return __atomic_load_n(&looping,__ATOMIC_RELAXED);
}

bool SysState::isReceiver() const {
    return __atomic_load_n(&receiver,__ATOMIC_RELAXED);
}

std::bitset<28> SysState::getInputs() const {
    std::bitset<28> inputsTmp;
    xSemaphoreTake(mutex, portMAX_DELAY);
    inputsTmp = inputs;
    xSemaphoreGive(mutex);
    return inputsTmp;
}

uint8_t SysState::getWaveform() const {
    return __atomic_load_n(&waveform,__ATOMIC_RELAXED);
}

uint8_t SysState::getVolume() const {
    return __atomic_load_n(&volume,__ATOMIC_RELAXED);
}

uint8_t SysState::getConns() const {
    return __atomic_load_n(&connections,__ATOMIC_RELAXED);
}

uint8_t SysState::getOctave() const {
    return __atomic_load_n(&octave,__ATOMIC_RELAXED);
}

uint8_t SysState::getLowestOctave() const {
    return __atomic_load_n(&lowestOctave,__ATOMIC_RELAXED);
}

uint8_t SysState::getHighestOctave() const {
    return __atomic_load_n(&highestOctave,__ATOMIC_RELAXED);
}

uint8_t SysState::getReceiverOctave() const {
    return __atomic_load_n(&receiverOctave,__ATOMIC_RELAXED);
}

void SysState::setReceiver(bool rec) {
    __atomic_store_n(&receiver,rec,__ATOMIC_RELAXED);
}

void SysState::setLooping(bool loop) {
    __atomic_store_n(&looping,loop,__ATOMIC_RELAXED);
}

void SysState::setInputs(std::bitset<28> in) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    inputs = in;
    xSemaphoreGive(mutex);
}

void SysState::setWaveform(uint8_t wave) {
    __atomic_store_n(&waveform,wave,__ATOMIC_RELAXED);
}

void SysState::setVolume(uint8_t vol) {
    __atomic_store_n(&volume,vol,__ATOMIC_RELAXED);
}

void SysState::setConns(uint8_t conns) {
    __atomic_store_n(&connections,conns,__ATOMIC_RELAXED);
}

void SysState::setOctave(uint8_t oct) {
    __atomic_store_n(&octave,oct,__ATOMIC_RELAXED);
}

void SysState::setLowestOctave(uint8_t lowOct) {
    __atomic_store_n(&lowestOctave,lowOct,__ATOMIC_RELAXED);
}

void SysState::setHighestOctave(uint8_t highOct) {
    __atomic_store_n(&highestOctave,highOct,__ATOMIC_RELAXED);
}

void SysState::setReceiverOctave(uint8_t recOct) {
    __atomic_store_n(&receiverOctave,recOct,__ATOMIC_RELAXED);
}