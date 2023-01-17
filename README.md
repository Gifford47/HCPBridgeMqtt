# HCPBridge @ hkiam with MQTT + HomeAssistant Support
Emuliert ein Hörmann HAP 1 HCP auf dem ESP8622 bzw. ESP32 und Arduino.<br/>

**Kompatible Torantriebe (HCP2-Bus - Modbus):**
- SupraMatic E/P Serie 4
- ProMatic Serie 4

Bitte beachten, das Projekt emuliert UAP 1 **HCP** und ist auch **nur** mit der Serie 4 kompatibel! Ältere Antriebe als Serie 4 haben eine andere Pinbelegung und ein komplett anderes Protokoll.<p/>

Eigentlich war das Ziel, die Steuerung komplett nur mit einem ESP8266 zu realisieren, allerdings gibt es durch die WLAN und TCP/IP-Stackumsetzung Timeoutprobleme, die zum Verbindungsabbruch zwischen dem Antrieb und der Steuerung führen können. Durch die ISR-Version konnte das Problem zwar reduziert aber nicht komplett ausgeschlossen werden. Daher gibt es zwei weitere Versionen, die bisher stabil laufen. Eine Variante nutzt den ESP32 statt ESP8266, welcher über 2 Kerne verfügt und so scheinbar besser mit WLAN-Verbindungsproblemen zurecht kommt. Die andere Option ist ein zweiter MCU, der die MODBUS Simulation übernimmt, sodass sich der ESP8266 nur noch um die Netzwerkkommunikation und das WebInterface kümmern muss.



## Funktionen:
- Abrufen des aktuellen Status (Tor, Licht)
- Auslösen der Aktionen (Licht an/aus, Tor öffen, schließen, stoppen sowie Lüftungsstellung
- WebInterface
- WebService
- Schalten eines Relais mit der Beleuchtung
- OTA Update
- DS18X20 Temperatur Sensor (mit Threshold)

## WebInterface:
![alt text](https://github.com/hkiam/HCPBridge/raw/master/Images/webinterface.PNG)

## WebService:
### Aktion ausführen

***http://[deviceip]/command?action=[id]***

  
| Action | Beschreibung |
| --- | --- |
| 0 | schließe Tor |
| 1 | öffne Tor |
| 2 | stoppe Tor |
| 3 | Lüftungsstellung |
| 4 | 1/2 öffnen |
| 5 | Lampe an/an |  

### Türstatus abfragen:

***http://[deviceip]/status***
  
Response (JSON):
 ```
{
  "valid" : true,
  "doorstate" : 1,
  "doorposition" : 0,
  "doortarget" : 0,
  "lamp" : true,
  "debug" : 0,
  "lastresponse" : 0
}
```

### Wifi Status abfragen:

***http://[deviceip]/sysinfo***

### OTA Firmware update (AsyncElegantOTA):

***http://[deviceip]/update***


## Pinout RS485 (Plug):
![alt text](https://github.com/hkiam/HCPBridge/raw/master/Images/plug-min.png)
1. GND (Blue)
2. GND (Yellow)
3. B- (Green)
4. A+ (Red)
5. +25V (Black)
6. +25V (White)

## RS485 Adapter:
![alt text](https://github.com/hkiam/HCPBridge/raw/master/Images/rs485board-min.png)  
Zwischen A+ (Red) und B- (Green) ist ein 120 Ohm Widerstand zum terminieren des BUS! 

## DS18X20 Temperature Sensor
![DS18X20](Images/ds18x20.jpg) <br/>
DS18X20 connected to GPIO4.

## Schaltung
![alt text](https://github.com/hkiam/HCPBridge/raw/master/Images/combo.png) <br/>
ESP8266 + Arduino Combo (Bisher stabil, benötigt allerdings zwei MCU)

![alt text](https://github.com/hkiam/HCPBridge/raw/master/Images/esp32.png) <br/>
ESP32 (Bisher stabil durch Nutzung beider Kerne, 25V auf 5V durch LM2596S DC-DC Step Down Module)

![alt text](https://github.com/hkiam/HCPBridge/raw/master/Images/schaltung.png) <br/>
ESP8266 ISR (instabil!, WLAN-Probleme können die ModBus-Verbindung beenden, dann ist ein Neuanlernen erforderlich) 

## Installation
![alt text](https://github.com/hkiam/HCPBridge/raw/master/Images/antrieb-min.png)
- Adapter am Bus anschließen (grüner Pfeil)
- Busscan ausführen (blauer Pfeil auf off und wieder zurück auf off). Der Adapter bekommt erst dann Strom über die 25V Leitung und muss während des Busscans antworten, sonst wird der Strom wieder abgeschaltet. Im Falle eines Fehlers oder wenn der Adapter abgezogen werden soll, einfach die Busscan Prozedur (On/Off) wiederholen. 

# HCPBridge MQTT (HomeAssistant topics)
Integrate https://github.com/hkiam/HCPBridge into HomeAssistant through MQTT.

This is just a quick and dirty impmlementation and needs refactoring. However, it works fine for me.

![alt text](Images/HA.png)

## Changelog
06.03.2021: Neue Version mit Arduino Pro Mini als Co MCU
24.02.2021: Neue Version via Interrupt und für ESP32 zur Vermeidung von Timing-Problemen
