# Birdy Num Num
A temperature-calibrated, time-stamped, hummingbird feeder weight logger

The main application is in 'bnn' and the calibration tool is in 'bnn-init'.

## Overview
Continuously monitors the (100g) Load Cell for significant (as defined by a
given threshold value) change in weight of the feeder, and then writes the
new weight, along with a timestamp and temperature reading, to a file on the
uSD card.

At each powerup, the uSD card's (FAT32) file system is read and if a config
file exists, the contents are used to overwrite the default values for the
application's variables.

After this initialization is done, this application creates a new log file
and gives it a name that includes the startup time.

An initial reading of the sensors is made and a log is written to the file.

This application will continue reading the Load Cell in a loop and compare
the weight against the last recorded value.  When the difference between
them exceeds a given amount, then a new log record is written and the last
weight value is updated.

Log records are of the form: `<timestamp>, <temperature>, <loadCell value>`

The timestamp is in ISO format.  The temperature is a float in degrees C.

The Load Cell value is the raw (uncompensated) value read from the HX711.

Log records contain raw values and all compensation and conversion (e.g.,
Load Cell temperature compensation) is done in post-processing.

The uSD file is flushed after each log record is written.  This way the
file is kept in a consistent state so the power can be removed at any time
without compromising the data log.


## Hardware Units

### SparkFun microSD shield SparkFun Arduino Pro

Pin assignments:

| Signal | Pin |
| ------ |:---:|
| MOSI   | 11  |
| MISO   | 12  |
| CLK    | 13  |
| CS     |  8  |

This pin assignment is given by the PCB, so must work around it.

### HX711 Load Cell Amp

Calibration factor can be very positive or very negative. It all depends on
the setup of your scale system and the direction the sensors deflect from
zero state.  Calibration factor will be significantly different but it will
be linearly related to lbs (1 lbs = 0.453592 kg).

Pin assignments:

| Signal | Pin |
| ------ |:---:|
| CLK    | 2   |
| DOUT   | 3   |
| VCC    | 5V  |
| GND    | GND |

Most any pin on the Arduino Uno will be compatible with DOUT/CLK.
The HX711 board can be powered from 2.7V to 5V so the Arduino 5V power
should be fine.

### DS3234 Real-Time Clock

SparkFun DeadOn RTC Breakout - DS3234

Pin assignments:

| Signal | Pin | Library Default |  Notes   |
| ------ |:---:|:---------------:|:-------- |
| SS     | 10  | 10              |          |
| MOSI   | 11  | 11              |          |
| MISO   | 12  | 12              |          |
| CLK    | 13  | 13              |          |
| SQW    | N/C |  2              | optional |
| VCC    | 5V  |                 |          |
| GND    | GND |                 |          |

### One Wire Digital Temperature Sensor - DS18B20

Looking at flat side, leads down, from left to right: GND, Data, VCC
Data needs to be be pulled up to VCC with 4.7K ohm resistor

Pin assignments:

| Signal | Pin | Library Default |
| ------ |:---:|:---------------:|
| GND    | GND |                 |
| Data   | 4   | 2               |
| VCC    | 5V  |                 |


## I/O Pin Map:

| Pin |  Signal | Direction |   Notes   |
|:---:|:------- |:---------:|:--------- |
| A0  | N/C     | In        | Analog In |
| A1  | N/C     | In        | Analog In |
| A2  | N/C     | In        | Analog In |
| A3  | N/C     | In        | Analog In |
| A4  | N/C     | In        | Analog In |
| A5  | N/C     | In        | Analog In |
| D0  | TX      | Out       | Serial    |
| D1  | RX      | In        | Serial    |
| D2  | LCA CLK | Out       | ????      |
| ~D3 | LCA DOUT| Out       | ????      |
| D4  | DTS DATA| In        | ????      |
| ~D5 | N/C     |           |           |
| ~D6 | N/C     |           |           |
| D7  | N/C     |           |           |
| D8  | USDS CS | Out       | SPI - CS  |
| D9  | N/C     |           |           |
| ~D10| RTC SS  | Out       | SPI - CS  |
| ~D11| SPI MOSI| ????      | USDS, RTC |
| D12 | SPI MISO| ????      | USDS, RTC |
| D13 | SPI CLK | ????      | USDS, RTC |
