#include <Knob.h>
#include <ES_IO.h>

Knob::Knob() { mutex = xSemaphoreCreateMutex(); }

void Knob::init(uint8_t knobNumber) {
    std::bitset<2> AB;
    uint8_t row = 4 - (knobNumber / 2);
    uint8_t col = 2 * (1 - (knobNumber % 2));
    AB[1] = readKey(row,col);
    AB[0] = readKey(row,col+1);
    xSemaphoreTake(mutex, portMAX_DELAY);
    currentAB = AB;
    loaded = true;
    xSemaphoreGive(mutex);
}

bool Knob::isLoaded() const {
    return __atomic_load_n(&loaded,__ATOMIC_RELAXED);
}

bool Knob::isPressed() const {
    return __atomic_load_n(&pressed,__ATOMIC_RELAXED);
}

uint8_t Knob::getRotation() const {
    return __atomic_load_n(&rotation,__ATOMIC_RELAXED);
}

void Knob::setRotation(int rotVal) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (rotVal >= lowerLimit && rotVal <= upperLimit) { rotation = rotVal; }
    else if (rotVal < lowerLimit) { rotation = lowerLimit; }
    else if (rotVal > upperLimit) { rotation = upperLimit; }
    xSemaphoreGive(mutex);
}

void Knob::setPressed(bool press) {
    __atomic_store_n(&pressed,press,__ATOMIC_RELAXED);
}

void Knob::setLowerLimit(int lower) {
    __atomic_store_n(&lowerLimit,lower,__ATOMIC_RELAXED);
}

void Knob::setUpperLimit(int upper) {
    __atomic_store_n(&upperLimit,upper,__ATOMIC_RELAXED);
}

void Knob::updateRotation(bool A, bool B) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    std::bitset<2> prevAB = currentAB;
    currentAB[1] = A;
    currentAB[0] = B;

    if (prevAB[0] != currentAB[0] || prevAB[1] != currentAB[1]) { // going to update
        if ((prevAB ^ currentAB).to_ulong() == 2) {
            if (currentAB[0] == currentAB[1]) { rotationDir = false; }
            else { rotationDir = true; }
        }

        if ((prevAB ^ currentAB).to_ulong() == 2 || (prevAB ^ currentAB).to_ulong() == 3) { // missed state
            if (rotationDir && rotation < upperLimit) { rotation++; }
            else if (!rotationDir && rotation > lowerLimit) { rotation--; }
        }
    }
    xSemaphoreGive(mutex);
}