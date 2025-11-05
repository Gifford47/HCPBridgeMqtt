# Getting started with prebuilt PCBs

If you bought a prebuilt PCB, you can follow this step-by-step guide to set up your HCP-Bridge with your Hörmann garage door.  
These instructions focus on the **Gifford47 PCB** version, but the process is very similar for all other board versions.

---

## 🔌 Installing Sensors

If you want to connect **temperature sensors** or any other sensors, follow these steps carefully:

1. **Solder pin headers** to the dedicated sensor pads on the **Gifford47 PCB**.  
   Make sure the pins are properly aligned and the solder joints are clean and solid.
2. **Attach the sensors** to the corresponding pin headers according to the pinout.
3. **Reinstall the PCB** into your **SupraMatic** unit.
4. **Upload the correct firmware** to your board:

   - Without sensors → `HCP_Giffordv2a.bin` or `HCP_Giffordv3a.bin`  
   - With sensors (any type) → `HCP_Giffordv2b.bin` or `HCP_Giffordv3b.bin`

⚠️ **Note:** The “b” firmware variant is required whenever sensors are present — even if only one is connected.

## What you need

* Prebuild PCB with HCPBridge Firmware installed (installed out of the box on gifford PCBs)
* USB-C power supply (PCB-Revision >=3)
* 3.3V power Supply (min. 300mA)

## Initial configuration

### 1. Connect the PCB to power via the USB-C connector/ 3.3V power supply and wait a few seconds. the LED will be lit.
### 2. Search and connect to the WIFI Network called "HCPBRIDGE" the PCB creates with a phone or pc.
### 3. When connected, open a webbrowser and go to the WEBUI under the following url : http://192.168.4.1

![Initial WebUI](Images/webui_initial_ui.png)

### 4. Open the "Basic Configuration" tab and enter your WIFI and MQTT credentials, then click save.

> Note: The wifi password has to be ASCII chars between ASCII 32-126.

> Tip: To disable the wifi AP just uncheck the "Enable wifi AP" checkbox and save

![basic config](Images/webui_basic_config.png)

### 5. The PCB should now be connected to your wifi. You can now check if it is reachable from your home network and if it connected to your MQTT Server.

## Installation

### 1. connect the PCB with a RJ12 cable to the BUS port on your Hörmann garage door motor (image below, see green arrow).

![Garage Motor](Images/antrieb-min.png)
   
### 2. Figure out how to execute a bus scan on your model, see info below or have a look in your motors ueer manual.

**ProMatic Serie 4**

Use the bottom dip switch (see image above, blue arrow) and toggle it, this will start the buss can.

**SupraMatic E/P Serie 4**

Use the Buttons to navigate to the menu 37 and excecute the Bus Scan, see here: [Tor7.de - Supramatic Bus Scan](https://www.tor7.de/news/bus-scan-beim-supramatic-serie-4-fehlercode-04-vermeiden)
  
### 3. execute a bus scan on your garage door motor, this should make the PCB light up (3V3) and blink rapidly (RS485 module)
### 4. if the bus scan was succesfully you can connect to the WEBUI and control your garage door motor, it should look like in the image below.

![Installation done](Images/webui_ready_and_installed.png)