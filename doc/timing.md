# Timing Analysis
Each task in the system was analysed to find its worst-case execution time, and minimum initiation interval. This was done by conditionally compiling the code, then isolating each of the tasks, and feeding in worst-case inputs to simulate the worst execution time theoretically possible for each task. Each task was run 32 times sequentially, to get a good idea of the initialisation time required over a period of time. For most tasks the minimum initiation interval is the same as tje exection time, as these tasks can be called instantly after finishing and run correctly. The handshake and transmit tasks have a longer minimum initiation interval than this as the CAN_TX operation failed to operate correctly otherwise. Timing was performed using the micros() function.

<center>

| Task | Worst-case execution time | Minimum initialisation interval | Initialisation interval for CIA |
| ---- | ------------------- | --------------- | -- |
| Play Note | 12 &mu;s | 12 &mu;s | 0.238 ms |
| Decode | 95 &mu;s | 95 &mu;s | 0.22 ms |
| Transmit | 12 &mu;s |  912 &mu;s | 1.33 ms |
| Scan Keys | 160 &mu;s | 160 &mu;s | 20 ms |
| Joystick Update | 318 &mu;s | 318 &mu;s | 20ms |
| Handshake | 262 &mu;s | 937 &mu;s | 50 ms |
| Update Display | 16.63 ms | 16.63 ms | 100 ms |

</center>
    
## Critical Instant Analysis
The longest initiation interval is 100ms, which will be used for this analysis. The transmit, decode, and play notes tasks initiation interval's are determined based on the maximum rate at which messages need to be transmitted / received on the board. The maximum rate at which messages need to be transmit, is the maximum of the following 2: 15 within the scan keys task (12 notes, plus octave, volume, waveform changes), or 8 messages in the handshaking task. This is because these two tasks are exclusive - the scan keys task only resumes once the handshake task terminates. The maximum of these is the scan keys task, requiring 15 messages to be sent in 20ms, leading to a minimum initiation interval of 1.33ms for the transmit task. For the decode task, we use the knowledge that there can be at most 7 boards connected together at once (7 octaves), hence the minimum initiation interval for the decode task is 0.22ms, 1/6th that of the transmit task. For the okay notes task, there can be at most 12 notes played from each board at once, leading to 84 notes needing to be played in the scan keys initiation interval, leading to a 0.238ms initiation interval for the play notes task.

This leads to the critical instant analysis below: <br /> <center>

$`\begin{split} L_n = 16630 + {100\over50} \times 262 + {100\over20} \times 318 + {100\over20}\times 160 \\ + {100\over1.33} \times 12 + {100\over0.22} \times 95 + {100\over0.238} \times 12 = 68670 \mu s \end{split}`$

<br /> </center>
The produced result is 68670 < 100000 (100ms), therefore the system passes critical instant analysis, meaning that all tasks can execute within the initiation interval of the lowest priority task. With this, it can be seen that the CPU is at ~69% usage.

Timing analysis was not directly performed for the ISR's given the nature of interrupt service routines. However, they are included in the analysis for the threads, given that the CAN_RX and CAN_TX ISR's are active for their corresponding tasks. The note generation was not active, but since the CPU is at ~69% usage, it can be safely assumed that the usage of this ISR will fit within the remaining CPU capacity.