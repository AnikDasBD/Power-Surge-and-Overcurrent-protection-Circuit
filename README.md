# Arduino-Based AC Voltage and Overcurrent Protection System

## Overview
This project simulates an Arduino Uno-based protection system in Proteus 8. It monitors AC voltage and load current and disconnects the load through a relay when overvoltage, undervoltage, or overcurrent conditions are detected.

## Features
- Overvoltage, undervoltage, and overcurrent protection
- LCD display for measured values and protection status
- Configurable protection thresholds through the Proteus Virtual Terminal
- Manual reset button for restoring operation after a fault
- Relay-based load disconnection and LED fault indication

## Default Thresholds

| Parameter | Default Value | Adjustable Range |
|---|---:|---:|
| Overvoltage (OV) | 270 V | 270–300 V |
| Undervoltage (UV) | 207 V | 180–230 V |
| Overcurrent (OC) | 2.5 A | 1.5–4.0 A |

## Repository Structure
- `src/` — Arduino firmware source code
- `schematics/` — Circuit schematic and block diagram
- `test_evidence/` — Simulation screenshots and test evidence
- `project_files/` — Proteus simulation files
- `design_document.pdf` — One-page project design document

## How to Run
1. Install Proteus 8 and open the project file from `project_files/`.
2. Load the compiled Arduino HEX file into the Simulino Arduino Uno model if it is not already configured.
3. Run the simulation and observe the LCD, relay, LED, and Virtual Terminal.
4. Enter commands in the Virtual Terminal to change protection thresholds, for example:
   - `OV=280`
   - `UV=200`
   - `OC=2.5`
   - `STATUS` — Display the current settings and status
   - `HELP` — Display available commands

The project has been tested in simulation only; physical hardware performance has not been validated.
