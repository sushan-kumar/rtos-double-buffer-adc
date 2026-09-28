Removing source code from a README prevents documentation drift, where the embedded code becomes outdated as the actual project files evolve. It also significantly reduces visual clutter, allowing users to focus entirely on system architecture, setup instructions, and feature roadmaps rather than scrolling through hundreds of lines of code.Real-Time Sensor Data Acquisition & FreeRTOS Double-Buffering SystemA production-grade, deterministic embedded firmware architecture for the Arduino Uno R4 (Minima / WiFi) featuring hardware-timer-driven ADC sampling, zero-copy ping-pong double-buffering, ISR-to-Task queue dispatch, and thread-safe serial CLI commands via FreeRTOS primitives and the Renesas FSP Timer.Table of ContentsOverviewKey FeaturesHardware Requirements & SchematicSystem ArchitectureSystem Dataflow DiagramRTOS Task & Synchronization MatrixSoftware PrerequisitesInstallation & SetupSerial CLI InterfaceFuture Roadmap: WiFi Data Logger & Web InterfaceLicenseOverviewHigh-frequency sensor sampling in real-time embedded systems can introduce jitter, CPU starvation, or race conditions if data processing occurs directly inside an Interrupt Service Routine (ISR) or in an unorganized super-loop.This project implements an embedded architecture on the 32-bit ARM Cortex-M4F (Renesas RA4M1) microcontroller. A General Pulse Width Timer (GPT) hardware interrupt periodically samples a Light Dependent Resistor (LDR) at a fixed 10 Hz frequency into a double-buffer system. Once a 10-sample frame is gathered, the ISR yields control to a processing task via a FreeRTOS Queue. Concurrently, an interactive CLI task processes incoming serial commands with thread safety guaranteed by a FreeRTOS Mutex.Key FeaturesDeterministic Hardware Sampling: Uses the Renesas RA4M1 GPT Timer (FspTimer) configured at 10 Hz (100 ms period) to guarantee jitter-free ADC sampling regardless of CPU load.Zero-Tear Double-Buffering: Dual 10-element ping-pong arrays (bufferA and bufferB) ensure the sampling ISR writes to one buffer while the processing task safely reads the other.Low-Latency ISR Dispatch: Employs xQueueSendFromISR with portYIELD_FROM_ISR to trigger immediate context switches to high-priority data processing tasks upon buffer completion.Thread-Safe Shared Memory: Protects global moving averages (global_avg) using a FreeRTOS Mutex (xMutex) to prevent torn multi-byte floating-point reads across task contexts.Non-Blocking Serial CLI: A background serial handling task echoes characters in real time and processes user commands without blocking hardware timer sampling.Hardware Requirements & SchematicBill of MaterialsComponentQuantityDescriptionArduino Uno R41Minima or WiFi variant (Renesas RA4M1 MCU)LDR Sensor1Standard Photoresistor (Light Dependent Resistor)10 kΩ Resistor1Pull-down resistor for analog voltage dividerBreadboard & Jumpers1Prototyping connection wiresCircuit DiagramThe LDR and 10 kΩ resistor form a voltage divider connected to analog input pin A0:             +5V
              |
           [ LDR ]
              |
  A0 <--------+-------- [ 10 kΩ Resistor ] --------> GND
System ArchitectureSystem Dataflow Diagram+-----------------------------------------------------------------------------------+
| HARDWARE INTERRUPT (FspTimer ISR @ 10 Hz / 100 ms)                                |
|  1. analogRead(A0) -> active_buffer[buffer_index]                                 |
|  2. When buffer_index == 10:                                                      |
|     a. Swap active_buffer pointer (bufferA <-> bufferB)                           |
|     b. xQueueSendFromISR(completed_buffer pointer)                                |
|     c. portYIELD_FROM_ISR()                                                       |
+------------------------------------+----------------------------------------------+
                                     |
                                     | (Passes Pointer via FreeRTOS Queue)
                                     v
+------------------------------------+----------------------------------------------+
| Task A: TaskA_ProcessData (Priority 2 - High)                                     |
|  1. Blocks on xQueueReceive(portMAX_DELAY)                                        |
|  2. Calculates mean average across 10 samples                                     |
|  3. xSemaphoreTake(xMutex) -> Update global_avg -> xSemaphoreGive(xMutex)         |
+-----------------------------------------------------------------------------------+

+-----------------------------------------------------------------------------------+
| Task B: TaskB_SerialHandler (Priority 1 - Low)                                    |
|  1. Listens on Serial interface and echoes typed characters                       |
|  2. On newline, parses buffer for command "avg"                                   |
|  3. xSemaphoreTake(xMutex) -> Read global_avg -> xSemaphoreGive(xMutex)           |
|  4. Prints formatted telemetry to Serial Monitor                                  |
+-----------------------------------------------------------------------------------+
RTOS Task & Synchronization MatrixNameType / PriorityStack SizeFunction / DescriptionreadDataHardware ISRN/AFills active ping-pong buffer at 10 Hz; swaps buffers and pushes array pointer to queue every 1 second.TaskA_ProcessDataTask (Priority 2)256 wordsUnblocks on queue receipt, calculates moving average, updates global average under Mutex protection.TaskB_SerialHandlerTask (Priority 1)256 wordsParses serial terminal input, echoes keys, safely reads global_avg under Mutex lock.xqueueQueue (Depth 2)sizeof(int*)Passes completed buffer memory addresses from ISR to Task A.xMutexMutexN/AEnsures strict mutual exclusion during read/write access to global_avg.Software PrerequisitesTo build and flash this project, ensure the following are installed:Arduino IDE 2.x or Arduino CLIArduino Renesas UNO R4 Boards Package (renesas_uno v1.2.0 or higher)Arduino_FreeRTOS Library (Included in the Renesas board core package)Installation & SetupAssemble Circuit: Wire the LDR voltage divider to pin A0 on the Arduino Uno R4.Open Project: Load the sketch file (sensor-rtos.ino) in the Arduino IDE.Select Board Target: Select Tools > Board > Arduino UNO R4 Minima (or Arduino UNO R4 WiFi).Compile & Upload: Press Ctrl + U / Cmd + U to build and flash the firmware.Launch Terminal: Open the Serial Monitor (Ctrl + Shift + M), set baud rate to 115200, and set line endings to Both NL & CR.Serial CLI InterfaceUpon startup, the board initializes RTOS primitives and prints a greeting message:PlaintextWelocome to FreeRTOS Serial buffer
Type avg to get the avergae of the ldr value's
Commandsavg — Retrieves the latest thread-safe 10-sample moving average.Example Session:Plaintext> avg
LDR average is: 512.40

> avg
LDR average is: 684.10
Future Roadmap: WiFi Data Logger & Web InterfaceThe system is designed to expand seamlessly onto the Arduino Uno R4 WiFi by leveraging its onboard ESP32-S3 co-processor and WiFiS3 networking library.+-----------------------------------------------------------------------------------+
|                        FREE-RTOS MULTITASKING ENGINE                              |
|                                                                                   |
|  +------------------+     +--------------------+     +-------------------------+  |
|  | Hardware GPT ISR | --> | Task A: Processing | --> | Shared Memory           |  |
|  | (10 Hz Sampling) |     | (Double-Buffer)    |     | (global_avg + RingBuf)  |  |
|  +------------------+     +--------------------+     +------------+------------+  |
|                                                                    |              |
|           +--------------------------------------------------------+              |
|           |                        |                               |              |
|           v                        v                               v              |
|  +------------------+    +-------------------+    +----------------------------+  |
|  | Task B: CLI Task |    | Task C: HTTP Web  |    | Task D: WebSocket Streamer |  |
|  | (Serial Monitor) |    | Server (Port 80)  |    | (Live Telemetry Broadcast) |  |
|  +------------------+    +-------------------+    +----------------------------+  |
+-----------------------------------------------------------------------------------+
Planned FeaturesTask C: HTTP Web Server TaskSpawn a dedicated, low-priority FreeRTOS task (TaskC_WebServer) utilizing WiFiServer on port 80.Serve a lightweight, single-page application (SPA) containing embedded CSS and JavaScript stored directly in microcontroller Flash memory (PROGMEM).RESTful Telemetry API Endpoints/api/telemetry — Returns JSON payloads containing current light intensity averages, minimums, maximums, and uptime metadata:JSON{
  "timestamp_ms": 45200,
  "current_avg": 512.40,
  "unit": "ADC_12BIT",
  "buffer_size": 10
}
Real-Time Web Dashboard (Chart.js)Embed a dynamic HTML5 line chart driven by WebSocket streams or periodic AJAX polling.Render real-time light level curves directly inside any standard web browser without disrupting ADC timer interrupts.Circular Historical Data BufferImplement a lock-free circular ring buffer holding the last 100 calculated averages for plotting continuous historical trends.LicenseThis project is licensed under the MIT License. See the LICENSE file for details.
