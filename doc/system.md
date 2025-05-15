# System Overview 

The microcontroller runs seven tasks simultaneously, with different assigned stack sizes and interval delays.

## Tasks Performed By the System
Shown is a high level table of all tasks the microcontroller performs throughout operation

| Task | Priority | Stack Allocation | Interval Delay (Frequency) | Description |
| ---- | -------- | ---------------- | --------- | ----------- |
| Play Notes | 1 | 64 bytes | on queue | Updates which notes are currently being played based on incoming / local messages via a queue |
| Decode Message | 1 | 64 bytes | on queue | Processes incoming messages from other boards. Fed via a queue from an ISR |
| Transmit Message | 1 | 64 bytes | on queue | Transmits messages across the CAN bus. Fed via a queue from other tasks |
| Scan Keys | 2 | 128 bytes | 20 ms | Reads key inputs on the board and updates global variables / sends messages to other boards via queue. Also feeds looped inputs in to be processed again. |
| Update Joystick | 3 | 128 bytes | 20 ms | Calculates step sizes to be fed to the ISR, adding vibrato in the process (determined by an analogue read). | 
| CAN Handshake | 1 | 64 bytes | 50 ms | Performs handshake with other keyboards upon startup if available, to establish connection. |
| Update Display | 4 | 256 bytes | 100 ms | Updates the display with information relevant to the user, based on global variables. |


Critical-instant analysis was performed, showing that timing is met with the initiation intervals shown above. [Timing analysis](timing.md)

Analysis on stack sizes was performed using the uxTaskGetStackHighWaterMark function to assess how much stack remains after running with an oversized stack, to allow for accurate stack allocations. From this it was observed that the tasks had the following peak stack sizes:
* Play notes, 38 bytes
* Decode message, 35 bytes
* Transmit message, 19 bytes
* Scan keys, 64 bytes
* Update joystick, 78 bytes
* Handshake, 54 bytes
* Display update, 127 bytes

From this it was determined to set the stack sizes as shown in the table above, rounding up to the nearest power of 2 for each (with some room for edge cases where more stack size would be needed).

## Task descriptions

### Handshaking (handshakeTask)

The handshake task allows automatic assignment of octaves on startup/power. The board assigned to octave 4 is set as the receiver, with the rest being transmitters. During the handshake two global variables, octave and highest octave, represent the current position of the board and the highest position reached of any board in the system respectively. Global variables are used since they are updated via incoming CAN messages within **decodeMessageTask**. It uses the **assignOctaves** function to set the octave and receiver board based on the current position and maximum position of any board in the system, centering the receiver at octave 4. This function is only run on start up and either deletes itself, or is deleted in **decodeMessageTask** once the final handshake has occurred.

The function itself has a minimum initiation time of 937µs, however in order for all the connected boards to receiver power and initiate their pins, an additional delay of 500ms was introduced on the first cycle of the task. During each task another delay of 50ms was introduced. This was done as once a new handshake is sent, the device turns off its output mux bit. However the connected board will recognize the disconnection of the west handshake before it has received and decoded the updated current and highest position sent in the handshake CAN message. So in order to allow certainty that the position has been updated this delay was introduced in each iteration of the task. These delays will not affect the user experience as they will only occur during the initial power on of the system and is not a noticeable wait time. When adding new keyboards to a system that has already been powered, the handshake will not occur and the configuration of the new board is handled in the **updateConnections** function within **scanKeysTask**.

The handshaking task had to be given a priority of 1 despite its slower initiation interval, to ensure it runs before the scan keys task. This blocking dependency is not ideal, but is not an issue given that the handshake task terminates after the handshakes completes.


### Input Reading (scanKeysTask)

This task is paused on startup, and only resumed after the initial handshaking has been completed.

#### Updating Connections

After the initial handshaking, the the output east handshake pin has been disabled, so on the first loop of the scan keys task both handshaking pins are reset to high and then read. In later cycles, a debounce time is added to the changing of the connections before updating whether a new keyboard has been added. This is because during the physical connection of a new board, the connection pin values can be very unstable. Thus debouncing delay is implemented to stabilise when a new connection is detected. When there has been a change to the connections, either adding or removing a board from the system, **updateConnections** is called, which sends a CAN message to all other boards. When a board is added, the relevant configuration details (volume, new highest octave etc.) are sent to it. When a board is removed it sends a message to all other boards what the new lowest or highest octave in the system is. This function allows for boards to be added or removed one at a time after the initial handshake.

#### Looping and Playback

While scanning for keypresses, a looping function operates to let the synthesizer playback pre-recorded sequences that can be inputted by the user. By holding down the leftmost knob, any keypresses (and lack of keypresses) will be recorded by the microcontroller into internal memory. Then, upon releasing the knob, the processor will keep replaying these sequences of key presses until the 2nd knob is pressed, to clear the memory. This is done within scan keys since the looping replicates keyboard inputs. Thus, it is essential that this key scanning happens simultaneously with the key presses, to ensure that the timing is kept synchronised.

### Vibrato (joystickUpdateTask)
The joystick-controlled vibrato is **task based** via **joystickUpdateTask**. This task calculates a pitch offset based off the joystick position. This offset is then applied to each note being played (including in the looper) and calculates the corresponding step size. This runs at 20ms to update the step sizes at the same rate the notes being played is updated.

The name 'vibrato' is a misnomer, given that this is actually a ±1 tone pitch bend, though a vibrato effect can be achieved through its use. 


## Note Generation
The processing and playing of notes is dependent on both **playNotesTask** and **joystickUpdateTask**, which are **task based**, and **sampleISR** which is **interrupt based**.

playNotesTask is task based and waits for messages to become available in the notePlayingQ, before updating a shift register of notes being played. This is fed into joystickUpdateTask to update the step sizes as outlined above.

These step sizes are then read in the ISR, and fed through a waveform generator to produce the desired sound.

The ISR is triggered by an instance of timer TIM1 at a frequency of 22kHz, which makes 22kHz the sampling frequency for notes being played on the keyboards.

## Display Updating
The display is updated with the lowest frequency (100ms), hence being given the lowest priority. Faster than this makes no difference as the human eye cannot tell the difference anyway. This task reads global variables and outputs relevant information on the display.

## CAN Communication
Note: for the sake of clarity, receiving/transmitting tasks have been split into 2 parts, these being the **CAN side**, which describes moving data to the CAN bus from a queue (and vice-versa) and the **processing side**, which describes processing data from the queue into note values (and vice-versa).

* **Interrupt based** through **CAN_RX_ISR()** and **CAN_TX_ISR()**
* CAN_RX_ISR happens any time message recieved on CAN bus. It adds the recieved message to the queue
* CAN_TX_ISR gives the CAN_TX_Semaphore. It happens after a message has been successfully tranmsitted on CAN bus.

CAN communication is **interrupt based** via **CAN_RX_ISR** and **CAN_TX_ISR**. CAN_RX_ISR is triggered whenever a message is available on the CAN bus, and moves that message to msgInQ. CAN_TX_ISR is triggered when a message has been successfully sent on the CAN bus (an ACK is recieved).

CAN_TX_ISR doesn't actually transmit anything. This is the job of transmitMessageTask. CAN_TX_ISR simply controls when transmitMessageTask can run via releasing the semaphore after successful transmission, thus controlling the flow of transmitted messages. 

The reading of CAN messages is ISR based so no bytes of data will be missed. An ISR is used to release the semaphore so that the ACK won't be missed. This is because threads cannot run with a high enough frequency to avoid potentially missing some messages.

### Transmission
The transmission of messages from the msgOutQ queue to the CAN bus is **task based** via **transmitMessageTask**. If there is a message available in the msgOutQ queue (fed by other tasks), this will be broadcast on the CAN bus, once the CAN_TX_Semaphore can be taken. The semaphore availability is dependent on the previous CAN message sending successfully and being ACKed. This allows for other tasks to run in parallel to CAN transmission, while filling up the buffer for asynchronous transmission. 

### Recieving
The receipt of messages from the msgInQ is **task based** via **decodeMessageTask**. This task takes messages from msgInQ to decode and process them based on the first element (a char). This char determines what kind of message has been received, and how the task should hence proceed.

The options are to set global variables based on the incoming data, or forward the data to the note playing queue to be processed.