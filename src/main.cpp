#include <Arduino.h>
#include <U8g2lib.h>
#include <constants.h>
#include <globals.h>
#include <Knob.h>
#include <State.h>
#include <ES_CAN.h>
#include <ES_IO.h>
#include <waveforms.h>

// #define DISABLE_THREADS
// #define DISABLE_SOUND
// #define DISABLE_CAN
// #define SHOW_STACK_WATERMARKS
// #define SOLO_BOARD
// #define TEST_HANDSHAKE
// #define TEST_KEYS
// #define TEST_PLAYNOTES
// #define TEST_JOYSTICK
// #define TEST_DISPLAY
// #define TEST_DECODE
// #define TEST_TRANSMIT

//Interrupt Service Routine - Sets audio voltage
void sampleISR() {
    static uint32_t phaseAcc[ACCUMULATORS] = {0, 0, 0, 0};

    uint8_t volume = sysState.getVolume();
    uint8_t waveform = sysState.getWaveform();
    uint8_t notes = 0;
    int Vout = 0;
    for (int i = 0; i < ACCUMULATORS; i++) {
        uint32_t thisStepSize = __atomic_load_n(&currentStepSize[i], __ATOMIC_RELAXED);
        uint32_t thisOctave = __atomic_load_n(&notesPlaying[i][1], __ATOMIC_RELAXED);
        uint32_t thisNote = __atomic_load_n(&notesPlaying[i][0], __ATOMIC_RELAXED);

        if (thisNote != 999) {
            //implement octave select
            if(thisOctave > 4){ thisStepSize <<= (thisOctave - 4); }
            else{ thisStepSize >>= (4 - thisOctave); }
            phaseAcc[i] += thisStepSize;

            Vout += waveformGenerator(phaseAcc[i], waveform) << 3; // scaling for audibility
        }
    }

    Vout = Vout >> (8 - volume);
    Vout = Vout / ACCUMULATORS; // only one divide is computationally nicer

    analogWrite(OUTL_PIN, std::min(std::max(Vout + 128, 0), 255)); // try average each note pressed corresponding voltage - separate phase accumulator for each.
    analogWrite(OUTR_PIN, std::min(std::max(Vout + 128, 0), 255)); // try average each note pressed corresponding voltage - separate phase accumulator for each.
}


//Interrupt Service Routine - CAN Reciever
void CAN_RX_ISR (void) {
    uint8_t RX_Message_ISR[8];
    uint32_t ID;
    CAN_RX(ID, RX_Message_ISR);
    xQueueSendFromISR(msgInQ, RX_Message_ISR, NULL); // sends data from ISR to msgInQ
}

//Interrupt Service Routine - CAN Transmitter
void CAN_TX_ISR (void) {
    xSemaphoreGiveFromISR(CAN_TX_Semaphore, NULL);
}

//Function to send message to out queue with default arguments
void sendMsg (uint8_t msgChar, uint8_t octOrPos = 0,
              uint8_t noteOrAssign = 0, uint8_t vol = 0,
              uint8_t conns = 0, /* add more if needed */
              xQueueHandle msgQ = msgOutQ
) {
    uint8_t msgOut[8] = {0};
    msgOut[0] = msgChar;
    msgOut[1] = octOrPos;
    msgOut[2] = noteOrAssign;
    msgOut[3] = vol;
    msgOut[4] = conns;
    // msgOut[5] = 0; add if needed
    // msgOut[6] = 0; add if needed
    // msgOut[7] = 0; add if needed
    xQueueSend(msgQ, msgOut, portMAX_DELAY);
}

//Function to reset output and returns number of connections
uint8_t resetConnsRead() {
    setOutMuxBit(HKOE_BIT, HIGH);
    setOutMuxBit(HKOW_BIT, HIGH);
    std::bitset<2> invHandShake = 0b00; // {west, east}
    invHandShake[1] = !readKey(5, 3);
    invHandShake[0] = !readKey(6, 3);
    uint8_t conns = invHandShake.to_ulong();
    sysState.setConns(conns);
    #ifndef TEST_DECODE
    return conns;
    #else
    return 1;
    #endif
}

//Updates Connections
void updateConnections(uint8_t newConns) {
    uint8_t msgOut[8] = {0};
    int diff = newConns - sysState.getConns();

    uint8_t thisOct = sysState.getOctave();
    uint8_t lowestOct = sysState.getLowestOctave();
    uint8_t highestOct = sysState.getHighestOctave();
    uint8_t receiverOct = sysState.getReceiverOctave();

    if (diff > 0){ // New Board Connected
        delay(1000); // Account for delay when turning on keyboard
        if (knobs[1].isLoaded()){
            sendMsg('W', 0, 0, knobs[1].getRotation());
        }
        if (knobs[3].isLoaded()) {
            sendMsg('V', 0, 0, knobs[3].getRotation());
        }
    }
    if (abs(diff) == 1) { // East
        uint8_t newHighest = std::min(highestOct + 1, 8); // New Connection
        if (diff < 0) {  // Disconnection from East
            newHighest = highestOct - 1;
            if (receiverOct > thisOct) {
                sendMsg('T', thisOct);
                sysState.setReceiver(true);
            }
        }
        if (thisOct > newHighest) { newHighest = thisOct; } // Bounds Check
        else { sysState.setHighestOctave(newHighest); }
        if (newConns != 0) { // Technically not needed
            sendMsg('L', lowestOct);
            sendMsg('H', newHighest, 1);
        }
    } else if (abs(diff) == 2) { // West
        uint8_t newLowest = std::max(lowestOct - 1, 1); // New Connection
        if (diff < 0) { // Disconnection from West
            newLowest = lowestOct + 1;
            if (receiverOct < thisOct) {
                sendMsg('T', thisOct);
                sysState.setReceiver(true);
            }
        }
        if (thisOct < newLowest) { newLowest = thisOct; } // Bounds Check
        else { sysState.setLowestOctave(newLowest); }
        if (newConns != 0) { // Technically not needed
            sendMsg('H', highestOct);
            sendMsg('L', newLowest, 1);
        }
    }
    sysState.setConns(newConns);
}

//Assigns Octaves based on position from handshake
void assignOctaves(uint8_t max, uint8_t pos) {
    uint8_t half = max / 2; // quotient
    uint8_t even = max % 2; // remainder
    uint8_t octave = std::min(std::max(4 + (pos - half), 1), 7);
    uint8_t lowestOctave = std::max(4 - half, 1);
    uint8_t highestOctave = std::min(4 + half + even, 7);

    sysState.setOctave(octave);
    sysState.setLowestOctave(lowestOctave);
    sysState.setHighestOctave(highestOctave);
    if (octave == 4) { sysState.setReceiver(true); }
    knobs[2].setRotation(octave);

    knobs[1].init(1);
    knobs[2].init(2);
    knobs[3].init(3);
}

//Thread Task - Handshake on startup to assing octaves
void handshakeTask(void* pvParameters) {
    uint8_t msgIn[8] = {0};
    uint8_t msgOut[8] = {0};
    std::bitset<2> handShakePins; // {!west, !east}
    bool firstLoop = true;
    bool eastMost  = false;
    bool disableHSPin = false;

    #ifndef TEST_HANDSHAKE
    delay(500); // Wait for boards to turn on
    while(1)
    #endif
    {
        #ifndef TEST_HANDSHAKE
        delay(50); // wait for CAN messages to be read and variables updated + handshake connections
        handShakePins[1] = readKey(5, 3);
        handShakePins[0] = readKey(6, 3, !disableHSPin);
        #else
        delayMicroseconds(675);
        handShakePins[1] = true;
        handShakePins[0] = true;
        eastMost = true;
        #endif

        // If Only Keyboard - end handshake
        if (handShakePins.all() && firstLoop) {
            assignOctaves(0, 0);
            #ifndef TEST_HANDSHAKE
            vTaskResume(scanKeysHandle);
            #ifndef SHOW_STACK_WATERMARKS
            vTaskDelete(NULL);
            #else
            vTaskSuspend(NULL);
            #endif
            #endif
        } else { firstLoop = false; }

        // Sends Final handshake if eastmost keyboard
        if (!handShakePins[1] && handShakePins[0] && !eastMost) {
            eastMost = true;
        }
        // Only sends Final handshake if all handshakes are complete
        if (handShakePins.all() && eastMost) { // Final handshake
            uint8_t maxPos = sysState.getHighestOctave() + 1;
            sysState.setOctave(maxPos);
            msgOut[0] = 'F';
            msgOut[1] = maxPos;
            xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
            CAN_TX(0x123, msgOut);
            assignOctaves(maxPos, maxPos);
            #ifndef TEST_HANDSHAKE
            vTaskResume(scanKeysHandle);
            #ifndef SHOW_STACK_WATERMARKS
            vTaskDelete(NULL);
            #else
            vTaskSuspend(NULL);
            #endif
            #endif
        // Sends new handshake if next "westmost" keyboard & disables east output pin
        } else if (handShakePins[1] && !disableHSPin) { // New handshake
            uint8_t maxPos = sysState.getHighestOctave() + 1;
            sysState.setOctave(maxPos);
            sysState.setHighestOctave(maxPos);
            msgOut[0] = 'N';
            msgOut[1] = maxPos;
            xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
            CAN_TX(0x123, msgOut);
            disableHSPin = true;
        }
    }
}

void playNotesTask(void * pvParameters) {
    uint8_t RX_Message_local[8] = {0};
    #ifndef TEST_PLAYNOTES
    while (1)
    #endif
    {
        // Serial.println(notePlayingQ);
        xQueueReceive(notePlayingQ, &RX_Message_local, portMAX_DELAY); // the decoding happens here - feeds in from notePlayingQ to RX_Message
        if (sysState.isReceiver()) {
            if (RX_Message_local[0] == 'P') { // pressed
                xSemaphoreTake(notesPlayingMutex, portMAX_DELAY);
                for (int i = ACCUMULATORS - 1; i > 0; i--) {
                    notesPlaying[i][0] = notesPlaying[i-1][0];
                    notesPlaying[i][1] = notesPlaying[i-1][1];
                }
                notesPlaying[0][0] = RX_Message_local[2]; // note
                notesPlaying[0][1] = RX_Message_local[1]; // octave
                xSemaphoreGive(notesPlayingMutex);
            } else { // released
                xSemaphoreTake(notesPlayingMutex, portMAX_DELAY);
                for (int i = 0; i < ACCUMULATORS; i++) {
                    if (notesPlaying[i][0] == RX_Message_local[2] && notesPlaying[i][1] == RX_Message_local[1]) {
                        // notesPlaying[0][0] = 999; // no note (0 is a note)
                        for (int j = i; j < ACCUMULATORS-1; j++) {
                            notesPlaying[j][0] = notesPlaying[j+1][0];
                            notesPlaying[j][1] = notesPlaying[j+1][1];
                        }
                        notesPlaying[ACCUMULATORS-1][0] = 999;
                        notesPlaying[ACCUMULATORS-1][1] = 4;
                        break;
                    }
                }
                xSemaphoreGive(notesPlayingMutex);
            }
        }
    }
}

//Thread Task - Updates Globals from Input Keys - SEPERATE WHILE LOOP FUNCTIONS
void scanKeysTask(void * pvParameters) {
    #ifndef DISABLE_THREADS
    vTaskSuspend(NULL);
    #endif

    #ifndef TEST_KEYS
    const TickType_t xFrequency = 20/portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    setOutMuxBit(HKOE_BIT, HIGH);
    setOutMuxBit(HKOW_BIT, HIGH);
    #endif

    std::bitset<28> inputs;
    std::bitset<28> prevInputs;
    std::bitset<12> prevNotes;
    uint8_t prevConns;

    bool connsChanged = false;
    TickType_t connsChangeTime = xTaskGetTickCount();

    std::vector<std::bitset<28>> looped {};
    bool lastButton = 1; //button starts not-pressed
    int loopPointer = 0;
    int loopLength = 0;

    bool firstScan = true;

    #ifndef TEST_KEYS
    while (1)
    #endif
    {
        #ifndef TEST_KEYS
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        #endif
        prevInputs = sysState.getInputs();

        // Gets Inputs
        prevInputs = sysState.getInputs();
        for (uint8_t i = 6; i != UINT8_MAX; i--) {
            inputs <<= 4;
            setRow(i);
            digitalWrite(REN_PIN, HIGH);
            delayMicroseconds(3);
            inputs |= readCols().to_ulong();
        }

        std::bitset<12> noteInputs = std::bitset<12>((inputs & NOTE_MASK).to_ulong());
        uint8_t connections = (!inputs[23] << 1) | !inputs[27]; // {west, east}

        // Debouncing due to east and west reads being volatile during connection changes
        // i.e 1 -> 2 is actually 1 -> 0 -> 2 or 1 -> 0 -> 2 -> 0 -> 2 etc.
        // Not first scan to readjust out bits from handshake + init connections
        if (firstScan) { // initialising connections
            firstScan = false;
            connections = resetConnsRead();
        }
        else if (connections != prevConns) {
            connsChangeTime = xTaskGetTickCount();
            connsChanged = true;
        }
        // Update connections after debounce time if changed
        if (connsChanged && ((xTaskGetTickCount() - connsChangeTime) >= CONN_TIME)) {
            updateConnections(connections);
            connsChanged = false;
        }

        // Loading global variables for knob updates
        int knob0rotation = knobs[0].getRotation(); // UNUSED
        int knob1rotation = knobs[1].getRotation();
        int knob2rotation = knobs[2].getRotation(); // UNUSED
        int knob3rotation = knobs[3].getRotation();

        uint8_t octave   = sysState.getOctave();
        uint8_t volume   = sysState.getVolume();
        uint8_t waveform = sysState.getWaveform();

        // Knob Updates
        if (prevInputs != inputs) {
            for (uint8_t i = 0; i < 4; i++) {
                uint8_t rotIdx = 18 - (2 * i);
                uint8_t pressIdx = 20 + (4 * (1 - i/2)) + (i % 2);
                knobs[i].updateRotation(inputs[rotIdx], inputs[rotIdx+1]);
                knobs[i].setPressed(inputs[pressIdx]);
            }
        }

        // Knob 1 - Change Waveform (Rotate)
        if (waveform != knob1rotation && knobs[1].isLoaded()) {
            sysState.setWaveform(knob1rotation);
            sendMsg('W', 0, 0, knob1rotation);
        }
        // Knob 2 - Change Octave (Rotate)
        if (octave != knob2rotation && knobs[2].isLoaded()) {
            sysState.setOctave(knob2rotation);
        };

        // Knob 3 - Change Volume (Rotate)
        if (volume != knob3rotation && knobs[3].isLoaded()) {
            sysState.setVolume(knob3rotation);
            sendMsg('V', 0, 0, knob3rotation);
        }
        // Knob 3 - Set Transmitter (Press)
        if (!inputs[21]) {
            sysState.setReceiver(true);
            uint8_t msgOut[8] = {0};
            sendMsg('T', octave);
        }

        // Loading global variables for looping
        bool looping = sysState.isLooping();
        bool recordingNotes =  !inputs[24];

        // Loop Playback
        if (looping) {
            inputs &= looped[loopPointer];
            if (loopPointer == loopLength - 1) { loopPointer = 0; }
            else { loopPointer ++; }
        }

        // Loop Recording
        if (recordingNotes && !looping) { looped.push_back(inputs); }

        // Loop Start
        if (!recordingNotes && !looping && !lastButton) {
            //knob has been released and need to replay sound
            sysState.setLooping(true);
            loopPointer = 0;
            loopLength = looped.size();
            if (0) {
                for (std::bitset<28> set : looped) {
                    std::string setstring = set.to_string();
                }
            }
        }

        // Loop Stop
        if (!inputs[25]) {
            sysState.setLooping(false);
            looped = {};
        }

        // key change for CAN communication
        for (int i = 0; i < 12; i++) {
            if (prevInputs[i] != inputs[i]) {
                uint8_t msgChar = inputs[i] ? 'R' : 'P';
                xQueueHandle msgQ = sysState.isReceiver() ? notePlayingQ : msgOutQ;
                sendMsg(msgChar, octave, i, volume, 0, msgQ);
            }
        }

        // Update previous values to current values
        lastButton = inputs[24];
        prevInputs = inputs;
        prevNotes = noteInputs;
        prevConns = connections;
        sysState.setInputs(inputs);
    }
}

//Thread Task - Displays Useful Information/Globals
void displayUpdateTask(void * pvParameters) {
    #ifndef TEST_DISPLAY
    const TickType_t xFrequency = 100/portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    #endif
    {
        #ifndef TEST_DISPLAY
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        #endif

        //Update display
        u8g2.clearBuffer();                 // clear the internal memory
        u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font


        u8g2.setCursor(2, 10);
        if (sysState.isReceiver()) {
            u8g2.print("Main Board");
        } else {
            u8g2.print("4 Blind Men");
        }

        if (sysState.isLooping()) {
            u8g2.setCursor(83, 10);
            u8g2.print("Looping");
        }

        u8g2.setCursor(2, 19);
        std::string noteString = "";

        u8g2.setFont(u8g2_font_5x7_mf);
        xSemaphoreTake(notesPlayingMutex, portMAX_DELAY);
        int nonzero = 0;
        for (int i = ACCUMULATORS - 1; i >= 0; i--) {
            if (notesPlaying[i][0] != 999) {
                noteString = NOTE_NAMES[notesPlaying[i][0]];
                noteString += std::to_string(notesPlaying[i][1]);

                u8g2.setCursor(2 + 13*nonzero, 19);
                u8g2.print(noteString.c_str());
                nonzero++;
            }
        }
        xSemaphoreGive(notesPlayingMutex);

        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setCursor(2, 30);
        switch (sysState.getWaveform()) {
            case 0:
                u8g2.print("Sawtooth");
                break;
            case 1:
                u8g2.print("Sine");
                break;
            case 2:
                u8g2.print("Square");
                break;
            case 3:
                u8g2.print("Triangle");
                break;

            default:
                u8g2.print("Invalid");
                break;
        }

        u8g2.setCursor(54, 30);
        u8g2.print("  Oct: ");
        u8g2.print(sysState.getOctave());
        u8g2.print("  Vol: ");
        u8g2.print(sysState.getVolume());

        //Toggle LED after updating display
        u8g2.sendBuffer(); // transfer internal memory to the display
        digitalToggle(LED_BUILTIN);
    }
}

//Need to create a new task for joystick updates, bc ANALOG READ TAKES TOO LONG TO PUT IN KEYSCAN TASK
void joystickUpdateTask(void * pvParameters) {
    #ifndef TEST_JOYSTICK
    const TickType_t xFrequency = 20/portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    #endif
    {
        #ifndef TEST_JOYSTICK
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        #endif

        // Read Joystick
        int32_t xInput = analogRead(JOYX_PIN); // UNUSED
        int32_t yInput = analogRead(JOYY_PIN);

        // Vibrato
        xSemaphoreTake(notesPlayingMutex, portMAX_DELAY);
        for (int i = 0; i < ACCUMULATORS; i++) {
            if (notesPlaying[i][0] != 999) {
                uint32_t vibratoStepSize = vibratoFunc(stepSizes, notesPlaying[i][0], yInput);
                __atomic_store_n(&currentStepSize[i], vibratoStepSize,__ATOMIC_RELAXED);
            } else { __atomic_store_n(&currentStepSize[i], 0, __ATOMIC_RELAXED); }
        }
        xSemaphoreGive(notesPlayingMutex);
    }
}

//Thread Task - Decodes Messages
void decodeMessageTask(void * pvParameters) {
    uint8_t RX_Message_local[8] = {0};
    #ifndef TEST_DECODE
    while (1)
    #endif
    {
        xQueueReceive(msgInQ, &RX_Message_local, portMAX_DELAY); // the decoding happens here - feeds in from msgInQ to RX_Message

        // Decoding Messages
        if (RX_Message_local[0] == 'P' || RX_Message_local[0] == 'R') { // Pressed or Released
           xQueueSend(notePlayingQ, RX_Message_local, portMAX_DELAY);
        } else if (RX_Message_local[0] == 'N') { // New handshake
            if (eTaskGetState(handshakeHandle) == eReady) {
                sysState.setHighestOctave(RX_Message_local[1]);
            }
        } else if (RX_Message_local[0] == 'F') { // Final handshake
            if (eTaskGetState(handshakeHandle) == eReady) {
                #ifndef SHOW_STACK_WATERMARKS
                vTaskDelete(handshakeHandle);
                #else
                vTaskSuspend(handshakeHandle);
                #endif
                assignOctaves(RX_Message_local[1], sysState.getOctave());
                vTaskResume(scanKeysHandle);
            }
        } else if (RX_Message_local[0] == 'V') { // Volume
            sysState.setVolume(RX_Message_local[3]);
            knobs[3].setRotation(RX_Message_local[3]);
            knobs[3].init(3);
        } else if (RX_Message_local[0] == 'W') { // Waveform
            sysState.setWaveform(RX_Message_local[3]);
            knobs[1].setRotation(RX_Message_local[3]);
            knobs[1].init(3);
        } else if (RX_Message_local[0] == 'H') { // Highest octave
            sysState.setHighestOctave(RX_Message_local[1]);
            if (resetConnsRead() == 2 && RX_Message_local[2]) { // if eastmost keyboard and assignment
                #ifndef TEST_DECODE
                #ifndef SHOW_STACK_WATERMARKS
                vTaskDelete(handshakeHandle);
                #else
                vTaskSuspend(handshakeHandle);
                #endif
                vTaskResume(scanKeysHandle);
                #endif
                sysState.setOctave(RX_Message_local[1]);
                knobs[2].setRotation(RX_Message_local[1]);
                knobs[2].init(2);
            }
        } else if (RX_Message_local[0] == 'L') { // Lowest octave
            sysState.setLowestOctave(RX_Message_local[1]);
            if (resetConnsRead() == 1 && RX_Message_local[2]) { // if westmost keyboard and assignment
                #ifndef TEST_DECODE
                #ifndef SHOW_STACK_WATERMARKS
                vTaskDelete(handshakeHandle);
                #else
                vTaskSuspend(handshakeHandle);
                #endif
                vTaskResume(scanKeysHandle);
                #endif
                sysState.setOctave(RX_Message_local[1]);
                knobs[2].setRotation(RX_Message_local[1]);
                knobs[2].init(2);
            }
        } else if (RX_Message_local[0] == 'T') { // Transmitter
            sysState.setReceiver(false);
            sysState.setReceiverOctave(RX_Message_local[1]);
        }
    }
}

//Thread Task - Transmits Messages
void transmitMessageTask (void * pvParameters) {
	uint8_t msgOut[8] = {0};
    #ifndef TEST_TRANSMIT
	while (1)
    #endif
    {
		xQueueReceive(msgOutQ, &msgOut, portMAX_DELAY); // wait until outgoing message
        #ifndef TEST_TRANSMIT
        if (sysState.getConns() > 0)
        #else
        delayMicroseconds(900);
        #endif
        { // only sends if there are connections
            xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY); // wait until semaphore can be taken
            CAN_TX(0x123, msgOut); // send
        }
	}
}

//Audio Interrupt Timer
void setAudioInterrups() {
    TIM_TypeDef *Instance = TIM1;
    HardwareTimer *sampleTimer = new HardwareTimer(Instance);
    sampleTimer->setOverflow(22000, HERTZ_FORMAT);
    sampleTimer->attachInterrupt(sampleISR);
    sampleTimer->resume();
}

//CAN Start with default configuration -- Move to ES_CAN Later
void CAN_ConfigStart() {
    #ifndef DISABLE_CAN
    #if defined(TEST_HANDSHAKE) || defined(TEST_TRANSMIT) || defined(SOLO_BOARD)
    CAN_Init(true); // sets CAN hardware to loopback mode - receives and ACKs its own messages - for testing only
    #else
    CAN_Init(false);
    #endif
    setCANFilter(0x123, 0x7ff); // only accepts messages from address 0x123 (0x7ff is the mask - match all bits)
    CAN_RegisterRX_ISR(CAN_RX_ISR);
    CAN_RegisterTX_ISR(CAN_TX_ISR);
    CAN_Start();
    #endif
}

void setup() {
    setPinDirections();

    //Initialise Display
    initOutMuxBits();
    u8g2.begin();

    //Initialise Audio
    #ifndef DISABLE_SOUND
    setAudioInterrups();
    #endif

    //Initialise CAN
    CAN_ConfigStart();

    notesPlayingMutex = xSemaphoreCreateMutex();

    //Initialise UART
    Serial.begin(9600);
    Serial.println("Hello World");

    msgInQ = xQueueCreate(36,8); // CAN incoming message queue - 36 items, 8 bytes each
    msgOutQ = xQueueCreate(36,8);
    CAN_TX_Semaphore = xSemaphoreCreateCounting(3,3); // counting semaphore as can transmit 3 at a time
    notePlayingQ = xQueueCreate(36,8);

    #ifdef TEST_PLAYNOTES
    uint8_t msgOut[8] = {0};
    msgOut[0] = 'P';
    msgOut[1] = 4;
    msgOut[2] = 5;
    for (int i = 0; i < 32; i++) {
        msgOut[0] = (int(i/2) % 2 == 1) ? 'P' : 'R';
        msgOut[1] = int(i/2) % 3 + 3;
        msgOut[2] = int(i/2) % 12;
        xQueueSend(notePlayingQ, msgOut, portMAX_DELAY);
    }
    #endif

    #ifdef TEST_DECODE
    uint8_t msgIn[8] = {0};
    msgIn[0] = 'P';
    msgIn[1] = 4;
    msgIn[2] = 5;
    for (int i = 0; i < 32; i++) {
        msgIn[0] = 'L';
        msgIn[1] = 7;
        msgIn[2] = 1;
        xQueueSend(msgInQ, msgIn, portMAX_DELAY);
    }
    #endif

    #ifdef TEST_TRANSMIT
    uint8_t msgOut[8] = {0};
    msgOut[0] = 'P';
    msgOut[1] = 4;
    msgOut[2] = 5;
    for (int i = 0; i < 32; i++) {
        msgOut[0] = 'L';
        msgOut[1] = 7;
        msgOut[2] = 1;
        xQueueSend(msgOutQ, msgOut, portMAX_DELAY);
    }
    #endif

    knobs[1].setUpperLimit(3);
    knobs[1].setLowerLimit(0);

    knobs[2].setUpperLimit(7); //typical piano spans octaves 1 -> 7 (though we can change this if we want)
    knobs[2].setLowerLimit(1);

    #ifndef DISABLE_THREADS
    xTaskCreate(
        handshakeTask,   /* Function that implements the task */
        "handshake",     /* Text name for the task */
        HANDSHAKE_SIZE,  /* Stack size in bytes */
        NULL,            /* Parameter passed into the task */
        1,	             /* Task priority */
        &handshakeHandle /* Pointer to store the task handle */
    );

    xTaskCreate(
        scanKeysTask,   /* Function that implements the task */
        "scanKeys",     /* Text name for the task */
        SCANKEYS_SIZE,  /* Stack size in bytes */
        NULL,	        /* Parameter passed into the task */
        2,			    /* Task priority */
        &scanKeysHandle /* Pointer to store the task handle */
    );

    xTaskCreate(
        playNotesTask,   /* Function that implements the task */
        "playNotes",     /* Text name for the task */
        PLAYNOTES_SIZE,  /* Stack size in bytes */
        NULL,			 /* Parameter passed into the task */
        1,			     /* Task priority */
        &playNotesHandle /* Pointer to store the task handle */
    );

    xTaskCreate(
        joystickUpdateTask,   /* Function that implements the task */
        "joystickUpdate",     /* Text name for the task */
        JOYSTICK_SIZE,        /* Stack size in bytes */
        NULL,	              /* Parameter passed into the task */
        3,			          /* Task priority */
        &joystickUpdateHandle /* Pointer to store the task handle */
    );

    xTaskCreate(
        displayUpdateTask,   /* Function that implements the task */
        "displayUpdate",     /* Text name for the task */
        DISPLAY_SIZE,        /* Stack size in bytes */
        NULL,			     /* Parameter passed into the task */
        4,			         /* Task priority */
        &displayUpdateHandle /* Pointer to store the task handle */
    );

    xTaskCreate(
        decodeMessageTask,   /* Function that implements the task */
        "decodeMessage",     /* Text name for the task */
        DECODE_SIZE,         /* Stack size in bytes */
        NULL,                /* Parameter passed into the task */
        1,	                 /* Task priority */
        &decodeMessageHandle /* Pointer to store the task handle */
    );

    xTaskCreate(
        transmitMessageTask,   /* Function that implements the task */
        "transmitMessage",     /* Text name for the task */
        TRANSMIT_SIZE,           /* Stack size in bytes */
        NULL,			       /* Parameter passed into the task */
        1,			           /* Task priority */
        &transmitMessageHandle /* Pointer to store the task handle */
    );
    #else
    for (int i = 0; i < 4; i++) { knobs[i].init(i); }
    knobs[2].setRotation(4);
    sysState.setReceiver(true);
    #endif

    vTaskStartScheduler();

}

#ifdef SHOW_STACK_WATERMARKS
int hsStackMax, skStackMax, pnStackMax, juStackMax, duStackMax, dmStackMax, tmStackMax;
#endif
void loop() {
    #ifdef SHOW_STACK_WATERMARKS
    int hsStack = uxTaskGetStackHighWaterMark(handshakeHandle);
    hsStackMax = (hsStack > hsStackMax) ? (HANDSHAKE_SIZE - hsStack) : hsStackMax;
    int skStack = uxTaskGetStackHighWaterMark(scanKeysHandle);
    skStackMax = (skStack > skStackMax) ? (SCANKEYS_SIZE - skStack) : skStackMax;
    int pnStack = uxTaskGetStackHighWaterMark(playNotesHandle);
    pnStackMax = (pnStack > pnStackMax) ? (PLAYNOTES_SIZE - pnStack) : pnStackMax;
    int juStack = uxTaskGetStackHighWaterMark(joystickUpdateHandle);
    juStackMax = (juStack > juStackMax) ? (JOYSTICK_SIZE - juStack) : juStackMax;
    int duStack = uxTaskGetStackHighWaterMark(displayUpdateHandle);
    duStackMax = (duStack > duStackMax) ? (DISPLAY_SIZE - duStack) : duStackMax;
    int dmStack = uxTaskGetStackHighWaterMark(decodeMessageHandle);
    dmStackMax = (dmStack > dmStackMax) ? (DECODE_SIZE - dmStack) : dmStackMax;
    int tmStack = uxTaskGetStackHighWaterMark(transmitMessageHandle);
    dmStackMax = (tmStack > tmStackMax) ? (TRANSMIT_SIZE - tmStack) : tmStackMax;

    Serial.print("Highest Stack Size used for handshake:       ");
    Serial.print(hsStackMax);
    Serial.println(" bytes");
    Serial.print("Highest Stack Size used for scanKeys:        ");
    Serial.print(skStackMax);
    Serial.println(" bytes");
    Serial.print("Highest Stack Size used for playNotes:       ");
    Serial.print(pnStackMax);
    Serial.println(" bytes");
    Serial.print("Highest Stack Size used for joystickUpdate:  ");
    Serial.print(juStackMax);
    Serial.println(" bytes");
    Serial.print("Highest Stack Size used for displayUpdate:   ");
    Serial.print(duStackMax);
    Serial.println(" bytes");
    Serial.print("Highest Stack Size used for decodeMessage:   ");
    Serial.print(dmStackMax);
    Serial.println(" bytes");
    Serial.print("Highest Stack Size used for transmitMessage: ");
    Serial.print(tmStackMax);
    Serial.println(" bytes");
    #endif

    #ifdef TEST_HANDSHAKE
        uint32_t startTime = micros();
        for (int iter = 0; iter < 32; iter++) {
            handshakeTask(NULL);
        }
        Serial.println(micros()-startTime);
        while(1);
    #endif

    #ifdef TEST_KEYS
        uint32_t startTime = micros();
        for (int iter = 0; iter < 32; iter++) {
            scanKeysTask(NULL);
        }
        Serial.println(micros()-startTime);
        while(1);
    #endif

    #ifdef TEST_PLAYNOTES
        uint32_t startTime = micros();
        for (int iter = 0; iter < 32; iter++) {
            playNotesTask(NULL);
        }
        Serial.println(micros()-startTime);
        while(1);
    #endif

    #ifdef TEST_JOYSTICK
        uint32_t startTime = micros();
        for (int iter = 0; iter < 32; iter++) {
            joystickUpdateTask(NULL);
        }
        Serial.println(micros()-startTime);
        while(1);
    #endif

    #ifdef TEST_DISPLAY
        uint32_t startTime = micros();
        for (int iter = 0; iter < 32; iter++) {
            displayUpdateTask(NULL);
        }
        Serial.println(micros()-startTime);
        while(1);
    #endif

    #ifdef TEST_DECODE
        uint32_t startTime = micros();
        for (int iter = 0; iter < 32; iter++) {
            decodeMessageTask(NULL);
        }
        Serial.println(micros()-startTime);
        while(1);
    #endif

    #ifdef TEST_TRANSMIT
        uint32_t startTime = micros();
        for (int iter = 0; iter < 32; iter++) {
            transmitMessageTask(NULL);
        }
        Serial.println(micros()-startTime);
        while(1);
    #endif
}