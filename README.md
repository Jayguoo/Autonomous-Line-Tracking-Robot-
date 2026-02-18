# Autonomous Line-Tracking Robot

An autonomous two-wheel robot built on a Digilent Arty A7 FPGA, running FreeRTOS on a MicroBlaze soft processor. The robot follows reflective tape using IR proximity sensors and avoids obstacles using a MAXSONAR ultrasonic sensor — achieving 90% track completion accuracy.

## Features

- **Line following** — Dual IR proximity sensors detect reflective tape, adjusting motor speeds for left/right corrections
- **Obstacle avoidance** — Pmod MAXSONAR ultrasonic sensor detects objects within 15 cm, stops the robot, and executes a right turn
- **FreeRTOS task management** — Main control loop runs as a FreeRTOS task with 50ms polling cycle
- **PWM motor control** — Pmod DHB1 dual H-bridge driver with configurable speed (0–100%) and direction per motor
- **Standalone boot** — Custom SREC SPI bootloader flashed to 16 MB Quad-SPI flash, enabling boot without a host PC

## System Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Arty A7 FPGA                          │
│                                                          │
│  ┌──────────────┐    ┌──────────────┐                   │
│  │  MicroBlaze   │◄──►│   AXI GPIO   │◄── IR Sensors    │
│  │  Soft CPU     │    └──────────────┘    (Left/Right)   │
│  │               │    ┌──────────────┐                   │
│  │  FreeRTOS     │◄──►│  Pmod DHB1   │──► Motor 1 (R)   │
│  │               │    │  (GPIO+PWM)  │──► Motor 2 (L)   │
│  │               │    └──────────────┘                   │
│  │               │    ┌──────────────┐                   │
│  │               │◄──►│ Pmod MAXSONAR│◄── Ultrasonic     │
│  └──────────────┘    └──────────────┘    Sensor          │
│                                                          │
│  ┌──────────────┐    ┌──────────────┐                   │
│  │  16 MB QSPI  │    │    UART      │──► Serial Debug   │
│  │  Flash Boot   │    │   115200     │                   │
│  └──────────────┘    └──────────────┘                   │
└──────────────────────────────────────────────────────────┘
```

## Control Logic

The main task runs a sensor-driven decision loop every 50ms:

```
                    ┌─────────────┐
                    │ Read Sensors│
                    │ IR + Sonar  │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐     Yes    ┌───────────┐
                    │ Obstacle    │────────────►│   STOP    │
                    │ ≤ 15 cm?   │             │ Turn Right│
                    └──────┬──────┘             └───────────┘
                           │ No
                    ┌──────▼──────┐
                    │ IR Readings │
                    └──┬──┬──┬───┘
                       │  │  │
            ┌──────────┘  │  └──────────┐
            ▼             ▼             ▼
     ┌────────────┐ ┌──────────┐ ┌────────────┐
     │ Left only  │ │  Both    │ │ Right only │
     │ Turn Right │ │  STOP    │ │ Turn Left  │
     │ R:70 L:35  │ │ R:0 L:0 │ │ R:35 L:70  │
     └────────────┘ └──────────┘ └────────────┘
            │             │             │
            └─────────────┼─────────────┘
                          ▼
                   ┌────────────┐
                   │  Neither   │
                   │  Forward   │
                   │  R:70 L:70 │
                   └────────────┘
```

## Hardware Components

| Component | Module | Description |
|-----------|--------|-------------|
| **Arty A7 FPGA** | Digilent | Xilinx Artix-7, hosts MicroBlaze SoC |
| **Pmod DHB1** | Digilent | Dual H-bridge motor driver, GPIO direction + PWM speed |
| **Pmod MAXSONAR** | Digilent | Ultrasonic distance sensor, reads clock edges via AXI |
| **IR Proximity Sensors** | Generic | 2x sensors on AXI GPIO, detect reflective tape |
| **16 MB Quad-SPI Flash** | On-board | Stores SREC bootloader + application for standalone boot |

## Memory Map

| Peripheral | Base Address | Interface |
|------------|-------------|-----------|
| DHB1 GPIO (motor direction) | `0x44A00000` | AXI GPIO |
| DHB1 PWM (motor speed) | `0x44A20000` | AXI PWM |
| IR Sensor GPIO | `0x40020000` | AXI GPIO |
| Pmod MAXSONAR | `XPAR_PMOD_DUAL_MAXSONAR_0` | AXI |

## Configuration

Key parameters in `main.c`:

```c
#define PWM_PERIOD_MS           2       // PWM period for motors
#define BASE_SPEED              70      // Default motor speed (0-100%)
#define OBSTACLE_DISTANCE_CM    15      // Sonar trigger distance
#define MAXSONAR_CHANNEL        1       // Sonar input channel
#define TASK_STACK_SIZE         2048    // FreeRTOS task stack
```

## Building & Flashing

### Prerequisites
- Xilinx Vivado 2025.1
- Xilinx Vitis 2025.1
- Digilent Arty A7 board

### Steps

1. **Open hardware design** — Import `base_soc_wrapper.xsa` in Vitis to generate the BSP
2. **Build application** — Create a new application project with the source files
3. **Run on FPGA** — Program the bitstream and run via JTAG for debugging
4. **Flash for standalone boot**:
   - In Vitis, select *Convert ELF to bootloadable SREC format*
   - Program the SREC bootloader + application to Quad-SPI flash
   - Power cycle — robot boots independently without a host PC

> **Note:** In Vitis 2025.1, remember to select "Convert ELF to bootloadable SREC format" — it's easy to miss.

## File Structure

```
autonomous-line-tracking-robot/
├── main.c                 # FreeRTOS main task — sensor reading + motor control
├── Pmod_DHB1.c            # DHB1 motor driver (direction, speed, enable/disable)
├── Pmod_DHB1.h            # DHB1 driver header
├── PWM.c                  # PWM IP driver (period, duty cycle, enable)
├── PWM.h                  # PWM driver header
├── MotorFeedback.c        # Motor encoder feedback driver (speed, position)
├── MotorFeedback.h        # Motor feedback header
├── base_soc_wrapper.xsa   # Vivado hardware export (MicroBlaze SoC definition)
└── README.md
```

## Built With

- **C** — Application firmware
- **FreeRTOS** — Real-time task scheduling
- **Xilinx Vivado** — FPGA SoC design (MicroBlaze, AXI interconnect)
- **Xilinx Vitis** — Embedded software development and flash programming
- **Verilog** — Custom SoC in Vivado block design
