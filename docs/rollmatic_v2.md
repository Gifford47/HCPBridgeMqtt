# Rollmativ V2

## Hardware part:

* Remove power cord
* Plug in RJ-11 connector
* Plug-in power cord
* Drive the garage door completely up or down once

## Programming:

* Hold "PGR" button until 10 is shown
* Press "Down" button until "37" is shown
* Press "PRG" button once -> Display shows "00" (meaning "no external modules configured")
* Press "Up" button once until 01 is shown (meaning "configure external module")
* Hold "PGR" button until "B.S." is shown
* When "B.S." starts blinking, bus scan is running (RX and TX on RS485 board active)
* If external module was recognized on the bus, the display shows "1" for a couple of seconds
* If external module was not recognized on the bus, the display shows "01" or "00" for a couple of seconds
* Menu goes back to "10"

## Troubleshooting Error "04"
Hörmann Rollmatic v2 uses HCP2-Bus which requires external modules to be registered and unregistered via bus scan. Error "04" shows up when you want to drive the door, but the registered external module was removed without performing a bus scan afterwards. The door will not drive until the issue is solved.

This can be archieved by Powering off the door, re-attach the external module and power on again.
Or by performing a complete factory reset of the Rollmatic v2.

Unfortunately a simple un-registration of a registered (but non working or non-attached module) seems not possible since in my case, Error "04" prevents access to the programming menu after powering on.