This prototype runs on QNX SDP 8.0 in a VM. The sensor is simulated, and the Raspberry Pi 5 hardware was not validated during the hackathon.

## What it does
Adaptive traffic signal controller with a 4-way (NS/EW) intersection. Green time adapts to vehicle density using an EWMA analytics task. Emergency vehicles trigger an override with arbitration and an 8-second timeout. Built with POSIX threads and message queues on QNX.

## Build (QNX SDP 8.0, x86_64)
qcc -Vgcc_ntox86_64 -o traffic src/common.c src/emergency_task.c src/gpio_mock.c src/logger.c src/main.c src/signal_controller.c src/state_machine.c src/stress_task.c src/traffic_analytics.c src/vehicle_detection.c -Iinclude

Run the program inside the QNX VM as ./traffic. Type e0 or e1 and press Enter to trigger an emergency on NS or EW.

## Measured results (QNX VM)
- Uncontested emergency detect-to-green: 0 ms (millisecond resolution)
- Queued emergency detect-to-switch: 2479 ms, including the wait for the first override to end
- Override timeout: about 8 s, then the normal cycle resumes
- Deadline misses: 0 in the test runs
- Adaptive green times: 3 to 9 s, driven by a simulated sensor

## Limitations
- The sensor is simulated (src/gpio_mock.c). No real hardware run.
- A Raspberry Pi 5 QNX image was installed and written to an SD card. The board did not boot during the hackathon, so all results come from the QNX VM.