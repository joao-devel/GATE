# GATE
LoRa to Ethernet Gateway

## Features
- Gateway for Sensorium LoRa devices

## Required Hardware
- Board: Arduino MKR WAN 1310
- Modules: Ethernet shield

## Installation
1. Clone the repository: `git clone https://github.com/joao-devel/GATE.git`
2. Open the `.ino` file with Arduino IDE.
3. Install the necessary libraries via Library Manager (Arduino IDE > Sketch > Include Library > Manage Libraries):
- Search for and install: RTCZero, LoRa, Ethernet, ArduinoLowPower, etc.

## Usage
The code runs as is, without any special tuning. All parameter settings are managed by the web app.

## Dependencies (Third-Party Libraries)
This project uses the following external libraries. Please respect their licenses when using or modifying the code.

| Library              | Author/Repository                              | License       | Link |
|-----------------------|------------------------------------------------|---------------|------|
| LoRa                 | Sandeep Mistry                                 | MIT           | [github.com/sandeepmistry/arduino-LoRa](https://github.com/sandeepmistry/arduino-LoRa) |
| RTCZero              | Arduino LLC                                    | LGPL-2.1      | [github.com/arduino-libraries/RTCZero](https://github.com/arduino-libraries/RTCZero) |
| Ethernet             | Arduino                                        | LGPL-2.1      | [github.com/arduino-libraries/Ethernet](https://github.com/arduino-libraries/Ethernet) |
| HttpClient           | Arduino                                        | Apache-2.0    | [github.com/arduino-libraries/ArduinoHttpClient](https://github.com/arduino-libraries/ArduinoHttpClient) |
| ArduinoLowPower      | Arduino                                        | LGPL-2.1      | [github.com/arduino-libraries/ArduinoLowPower](https://github.com/arduino-libraries/ArduinoLowPower) |
| FlashAsEEPROM (FlashStorage) | Arduino LLC                             | LGPL-2.1      | [github.com/cmaglie/FlashStorage](https://github.com/cmaglie/FlashStorage) |
| Arduino_PMIC         | Arduino (per Power Management)                 | LGPL-2.1      | Parte del core Arduino per board specifiche |
| SPI                  | Arduino Core                                   | LGPL-2.1      | Inclusa nel core Arduino |

Note: The LoRa library is copyright "(c) 2016 Sandeep Mistry." For the full text of the licenses, see the original repositories.

## License
This project (the original code written by you) is released under the [MIT License](LICENSE).

Third-party libraries have their own licenses as indicated above.

## Contributions
Pull requests and issues are welcome, as is active participation in the project.

Author: GB
Date: January 2026
