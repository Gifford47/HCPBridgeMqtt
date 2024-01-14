# Getting started with prebuild PCBs

If you bought a prebuild PCB from either tynet.eu or someone else you can follow thos setp by step guide to setup your HCP-Bridge with your Hörmann Garage Door.

## What you need

* Prebuild PCB with HCPBridge Firmware installed (installed out of the box on tynet.eu PCBs)
* USB-C power supply

## Initial configuration

1. Connect the PCB to power via the USB-C connector and wait a few seconds. On tynet.eu boards the 3V3 LED will be lit.
2. Search and connect to the WIFI Network called "HCPBRIDGE" the PCB creates with a phone or pc.
3. When connected, open a webbrowser and go to the WEBUI under the following url : http://192.168.4.1
4. Open the "Basic Configuration" tab and enter your WIFI and MQTT credentials, thn click save.

## Installation

1. connect the PCB with a RJ12 cable to the BUS port on your Hörmann garage door motor.
2. Take a look at your motors user manual and figure out how to execute a bus scan.
3. execute a bus scan on your garage door motor, this should make the PCB lit up and maybe blink rapidly
4. if the bus scan was succesfully you can connect to the WEBUI and control your garage door motor
