#ifndef ES_IO_H
#define ES_IO_H

#include <Arduino.h>
#include <bitset>

//Set Pin Directions
void setPinDirections();

//Initialise Display & Handshake bits
void initOutMuxBits();

//Sets Demux Row for reading from Key Matrix
void setRow(uint8_t rowIdx);

//Reads Column Keys for selected row in Key Matrix
std::bitset<4> readCols();

//Reads the state of a key in the key matrix
bool readKey(uint8_t rowdx, uint8_t colIdx, uint8_t muxBit = HIGH);

//Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value);

#endif