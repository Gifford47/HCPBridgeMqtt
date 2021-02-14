# HCPBridge
emuliert ein Hörmann HAP 1 HCP auf dem ESP8622.<br/>
## Funktionen:
- Abrufen des aktuellen Status (Tor, Licht)
- Aulösen der Aktionen (Licht an/aus, Tor öffen, schließen, stoppen sowie Lüftungsstellung
- WebInterface
- WebService
- Schalten eines Relay mit der Beleuchtung

## WebInterface:
![alt text](https://github.com/hkiam/HCPBridge/raw/master/Images/webinterface.PNG)

## WebService:
### Aktion ausführen
http://<deviceip>/command?action=<id>
| Action | Beschreibung |
| --- | --- |
| 0 | schließe Tor |
| 1 | öffne Tor |
| 2 | stoppe Tor |
| 3 | Lüftungsstellung |
| 4 | 1/2 öffnen |
| 5 | Lampe an/an |  
  

### Status abfragen:
http://<deviceip>/status

Response:
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

