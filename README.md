# ODIn (Optical Density Instrument)

Repo for documentation related to the Optical Density Instrument at the Stahl lab

## Download repository

After setting up [ssh keys](https://help.github.com/articles/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent/) connected to your github account from command line, run the following command

`git clone git@github.com:jmicrobe/odin.git`

## Introduction

A common technique in microbiology is the use of optical density to measure the growth of liquid microbial culture over time. As the number of cells in a culture grows, the optical density, or the amount of light that is scattered when passed through the culture tube, will increase. These measurements can be taken by hand using a spectrophotometer or by using a high-throughput well plate reader device. Plate readers are effective for studying growth in the presence of oxygen, however for microbes requiring anaerobic conditions plate readers are sub-optimal.

The ODIn is designed to take continuous measurements of growing anaerobic cultures using an infrared feedback loop. During an experiment samples are shaken on a platform that holds racks containing up to 64 anaerobic "Balch-type" culture tubes. Measurements of the infrared signal strength are reported in milliamperes and automatically sent to a database for easy access and observation. In addition to the ODIn's capability for high-throughput measurements over an extended time, the larger culture volume capacity of the ODIn compared to traditional well plate readers allows for more flexibility in chemical testing of the samples.

While data is logged to an on-board SD card, the ODIn also sends data to a remote server, so that data can be monitored remotely, and interface with the FileMaker Pro database software developed for the project. The code for this server exists in a separate repository, located [here](https://github.com/BeckResearchLab/ODIn_dataServer).

## Apparatus Environment Implications
> Ex: necessary temperature to operate. LEDs, phototransistor, microprocessing etc

## Setup Arduino

> TODO need to make sure this is the right model

This project is designed with the [Arduino Mega 2560](https://store.arduino.cc/arduino-mega-2560-rev3)

### Shields

#### Data Logger

> TODO describe more about data logger specifically how it is used in project. issue #10

Logs data to be prepared to be uploaded to the database. [Data Logger from Adafruit](https://www.adafruit.com/product/1141?gclid=EAIaIQobChMImqmPr9Hv1wIVE2t-Ch1ZOwSYEAAYASAAEgI-BPD_BwE)

#### Ethernet

> TODO more information of how and where data is uploaded. issue #11

Sends data to the database. [Ethernet Shield](https://store.arduino.cc/arduino-ethernet-shield-2)

### Libraries

> TODO look at code, try to aggregate libraries issue #7

## Circuits

### Express PCB

> TODO find out how to do this. Issue #1

### Resistor selections

> TODO this appeared to be described in the original docs, so here's a placeholder

## Bill of Materials

> TODO issue #9

## References

> TODO need to gather any information that helped with the creation of the project

## Credits

> TODO Get every name that deserves love!
