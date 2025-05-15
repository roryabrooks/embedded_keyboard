#include <ES_IO.h>
#include <constants.h>

void setPinDirections() {
    //Input Pins
    pinMode(C0_PIN, INPUT);
    pinMode(C1_PIN, INPUT);
    pinMode(C2_PIN, INPUT);
    pinMode(C3_PIN, INPUT);
    pinMode(JOYX_PIN, INPUT);
    pinMode(JOYY_PIN, INPUT);

    //Output Pins
    pinMode(RA0_PIN, OUTPUT);
    pinMode(RA1_PIN, OUTPUT);
    pinMode(RA2_PIN, OUTPUT);
    pinMode(REN_PIN, OUTPUT);
    pinMode(OUT_PIN, OUTPUT);
    pinMode(OUTL_PIN, OUTPUT);
    pinMode(OUTR_PIN, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
}

void initOutMuxBits() {
    setOutMuxBit(DRST_BIT, LOW);  //Assert display logic reset
    delayMicroseconds(2);
    setOutMuxBit(DRST_BIT, HIGH); //Release display logic reset
    setOutMuxBit(DEN_BIT, HIGH);  //Enable display power supply
    setOutMuxBit(HKOW_BIT, HIGH);
    setOutMuxBit(HKOE_BIT, HIGH);
}

void setRow(uint8_t rowIdx) {
    digitalWrite(REN_PIN, LOW);
    digitalWrite(RA0_PIN, rowIdx & 0x1);
    digitalWrite(RA1_PIN, rowIdx & 0x2);
    digitalWrite(RA2_PIN, rowIdx & 0x4);
}

std::bitset<4> readCols() {
	std::bitset<4> result;
    result[0] = digitalRead(C0_PIN);
	result[1] = digitalRead(C1_PIN);
	result[2] = digitalRead(C2_PIN);
	result[3] = digitalRead(C3_PIN);
    digitalWrite(REN_PIN, LOW);
	return result;
}

bool readKey(uint8_t rowdx, uint8_t colIdx, uint8_t muxBit) {
    bool result;
    uint8_t colPin = (colIdx%2) ? ((colIdx/2) ? C3_PIN : C1_PIN) :
                                  ((colIdx/2) ? C2_PIN : C0_PIN);
    setRow(rowdx);
    setOutMuxBit(rowdx, muxBit);
    digitalWrite(REN_PIN, HIGH);
    delayMicroseconds(3);
    result = digitalRead(colPin);
    digitalWrite(REN_PIN, LOW);
    return result;
}

void setOutMuxBit(const uint8_t bitIdx, const bool value) {
    digitalWrite(REN_PIN,LOW);
    digitalWrite(RA0_PIN, bitIdx & 0x01);
    digitalWrite(RA1_PIN, bitIdx & 0x02);
    digitalWrite(RA2_PIN, bitIdx & 0x04);
    digitalWrite(OUT_PIN,value);
    digitalWrite(REN_PIN,HIGH);
    delayMicroseconds(2);
    digitalWrite(REN_PIN,LOW);
}