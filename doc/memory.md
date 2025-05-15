# Memory safety

Care was taken to ensure that shared data structures can be accessed asychronously across many threads, whilst maintaining memory safety and avoiding blocking dependencies.

### Shared Data Structures

Listed here are all of the shared data structures, with the methods used to ensure their use is safe.

#### **Queue handles:**
**msgInQ**
*Used by*: CAN_RX_ISR, decodeMessageTask
**msgOutQ** 
*Used by*: transmitMessageTask, scanKeysTask
**notePlayingQ** 
*Used by*: playNotesTask, scanKeysTask, decodeMessageTask

*Safety*: all Queue related functions in FreeRTOS are inherently thread-safe (xQueueReceive, xQueueSend, etc.)

#### **Global variables:**
Mutex vs atomic operations: atomic operations are faster than taking and giving a mutex, so are used in most places. Mutex's are only used when accessing arrays, as it is important that the state of the array is kept constant through the process of reading / writing to it, else there may be undefined behaviour.
An exception to this is that the notesPlaying array is read via atomic operation in the ISR that generates sound, as taking a mutex is too costly a task to fit within the ISR frequency. The array is purely read from in this case rather than written to, so the effects of another task changing the array whilst it is being accessed here is minimal.
**sysState**
*Purpose*: contains system info for each board (volume, octave, etc)
*Used by*: scanKeysTask, displayUpdateTask, decodeMessageTask, transmitMessageTask
*Safety*: all getters/setters use atomic operations, or a mutex
**knobs[]**
*Purpose*: an array of objects of knob class, containing information for each knob on the board.
*Used by*: scanKeysTask, handshakeTask, decodeMessageTask
*Safety*: all getters/setters use atomic operations, or a mutex depending on if they are getting/settings a variable or an array of variables within the object respectively.
**currentStepSize[]**
*Purpose*: an array containing the phase accumulator step sizes for each of the notes currently playing.
*Used by*: sampleISR, joystickUpdateTask
*Safety*: all accesses use atomic operations (safe as only one thread can write to this, else a mutex would be used)
**notesPlaying[]**
*Purpose*: an array containing the note index and octave number of each note currently being played.
*Used by*: sampleISR, playNotesTask, displayUpdateTask, joystickUpdateTask
*Safety*: all accesses use a mutex or atomic operation (only ISR)

### Dependencies
All tasks have tried to maintain local variables when possible to ensure that all tasks can be stopped and started without risk of entering deadlock. However, when this is not possible, two different protocols (applied when applicable) were used to ensure that there is no possible way for the processor to enter deadlock. The first step taken was to ensure that any tasks that needed to be run in immediate succession were grouped together into the same function.

For example, the scanKeysTask on the surface is much longer than other tasks, however it contains tasks that must be run in immediate succession. Thus, rather than creating a large dependancy tree, it was decided to keep it all within a single thread. The second protocol used to ensure the system could never enter deadlock was to ensure that none of the mutex/semaphores had any circular dependancies, meaning every single mutex/semaphore can be taken without a circular conflict generating.


An example of this is the knob class which was implemented in such a way that the mutex protects the entire class, not just the function being run. This means that regardless of any interrupts or task switching, the program is guaranteed to complete. There may be some stalls, however no task may block CPU operation, thus the program is guaranteed to complete.
