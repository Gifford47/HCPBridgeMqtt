# HCPBridge with MQTT + HomeAssistant Support

Emulates Hörmann HAP 1 HCP using an ESP32 and a RS485 converter.<br/>

**Compatible with the following motors (HCP2-Bus - Modbus):**

* SupraMatic E/P Serie 4
* ProMatic Serie 4

Bitte beachten, das Projekt emuliert UAP 1 **HCP** und ist auch **nur** mit der Serie 4 kompatibel! Ältere Antriebe als Serie 4 haben eine andere Pinbelegung und ein komplett anderes Protokoll.<p/>

Eigentlich war das Ziel, die Steuerung komplett nur mit einem ESP8266 zu realisieren, allerdings gibt es durch die WLAN und TCP/IP-Stackumsetzung Timeoutprobleme, die zum Verbindungsabbruch zwischen dem Antrieb und der Steuerung führen können. Durch die ISR-Version konnte das Problem zwar reduziert aber nicht komplett ausgeschlossen werden. Daher gibt es zwei weitere Versionen, die bisher stabil laufen. Eine Variante nutzt den ESP32 statt ESP8266, welcher über 2 Kerne verfügt und so scheinbar besser mit WLAN-Verbindungsproblemen zurecht kommt. Die andere Option ist ein zweiter MCU, der die MODBUS Simulation übernimmt, sodass sich der ESP8266 nur noch um die Netzwerkkommunikation und das WebInterface kümmern muss.

## Funktionen:

* Abrufen des aktuellen Status (Tor, Licht)
* Auslösen der Aktionen (Licht an/aus, Tor öffen, schließen, stoppen sowie Lüftungsstellung
* WebInterface
* WebService
* Schalten eines Relais mit der Beleuchtung
* OTA Update (with username and password)
* AsyncWifiManger hinzugefügt (Hotspot bei getrennter Verbindung)
* DS18X20 Temperatur Sensor (mit Threshold)
* Send only MQTT Message if Door state changed

## Known Bugs:
* Licht kann nicht per WebUI geschaltet werden
* ESP.restart()/ doCommand(6) wird nicht ausgeführt

## WebInterface:

![alt text](Images/webinterface.PNG)

## WebService:

### Aktion ausführen

***http://[deviceip]/command?action=[id]***

| Action | Beschreibung |
|--------|--------------|
| 0 | schließe Tor |
| 1 | öffne Tor |
| 2 | stoppe Tor |
| 3 | Lüftungsstellung |
| 4 | 1/2 öffnen |
| 5 | Lampe an/an |
| 6 | Restart |

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

### Wifi Status:

***http://[deviceip]/sysinfo***

### OTA Firmware update (AsyncElegantOTA):

***http://[deviceip]/update***

## Pinout RS485 (Plug):

![alt text](Images/plug-min.png)

1. GND (Blue)
2. GND (Yellow)
3. B- (Green)
4. A+ (Red)
5. \+24V (Black)
6. \+24V (White)

## RS485 Adapter:

![alt text](Images/rs485board-min.png)  
Pins A+ (Red) und B- (Green) need a 120 Ohm resistor for BUS termination.

## DS18X20 Temperature Sensor

![DS18X20](Images/ds18x20.jpg) <br/>
DS18X20 connected to GPIO4.

## Schaltung

![alt text](Images/esp32.png) <br/>
ESP32 (requires a Step Down Module such as LM2596S DC-DC, but any will do 24VDC ==> 5VDC will do)

## Installation

![alt text](Images/antrieb-min.png)

* Adapter am Bus anschließen (grüner Pfeil) <br/>
* Busscan ausführen: 
  * alte HW-Version: Busscan ausführen (blauer Pfeil auf off und wieder zurück auf off). Der Adapter bekommt erst dann Strom über die 25V Leitung und muss während des Busscans antworten, sonst wird der Strom wieder abgeschaltet. Im Falle eines Fehlers oder wenn der Adapter abgezogen werden soll, einfach die Busscan Prozedur (On/Off) wiederholen.
  * neue HW-Version: Der Busscan wird bei neueren HW-Versionen mittels des LC-Displays im Menü 37 durchgeführt. Weiteres siehe: [Supramatic 4 Busscan](https://www.tor7.de/news/bus-scan-beim-supramatic-serie-4-fehlercode-04-vermeiden)

# HCPBridge MQTT (HomeAssistant topics)

Integrate https://github.com/hkiam/HCPBridge into HomeAssistant through MQTT.

This is just a quick and dirty impmlementation and needs refactoring. However, it works fine for me.

![alt text](Images/HA.png)