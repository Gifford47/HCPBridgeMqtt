# Bus Scan on SupraMatic Series 4 – Avoiding Error Code 04

![SupraMatic Error 04](Images/bus_scan/BS_1.jpg)

> **Problem:**  
> Error appears after connecting or disconnecting accessories?  
> This can happen with Hörmann drives that use the HCP2 bus connection.  
> No need to worry – here’s how to avoid error 04.

---

## 🔍 04 – Why Does This Error Occur?

With the older **HCP1 bus connection**, accessories could be plugged in or unplugged freely and would work immediately.  
The newer **HCP2 bus connection**, however, works differently:  
It uses a **bus scan** to register and deregister each compatible device.  
This technology prevents malfunctions but requires a specific procedure.

---

## ⚙️ Connecting and Disconnecting Accessories on the HCP2 Bus

Before **connecting or removing** any accessory on the HCP2 bus, a **bus scan** must be performed.  
To do this:

1. **Disconnect the drive from power.**  
2. Plug in or remove the accessory.  
3. **Reconnect power** to the drive.  
4. The bus scan will then start automatically.

---

## 🧭 Performing a Bus Scan on SupraMatic Series 4

The **SupraMatic** and **ProMatic Series 4** drives are equipped with the HCP2 bus.  
In this example, we connect a **HAP1 (Hörmann Adapter Board 1)**.

![HAP1 on HCP2 Bus](Images/bus_scan/BS_2.jpg)

### Registering Accessories (SupraMatic E/P Series 4)

1. **Unplug the drive from power.**  
   Connect the accessory while power is off. Then plug the drive back in.
2. **Open the programming menu.**  
   Hold the `PRG` button until “00” appears on the display.

   ![Menu 00](Images/bus_scan/BS_3.jpg)
3. **Select Menu 37.**  
   Navigate with the arrow keys.

   ![Menu 37](Images/bus_scan/BS_4.jpg)
4. **Open Parameter 01.**

   ![Submenu 01](Images/bus_scan/BS_5.jpg)
5. **Start the bus scan.**  
   Hold the `P` button until **bS** appears on the display, then release.

   ![bS Scan Active](Images/bus_scan/BS_6.jpg)
6. While the scan is running, **bS** will blink.  
   Once the scan is complete, the display briefly shows **1** – the accessory has been detected.

   ![Accessory Detected](Images/bus_scan/BS_7.jpg)

---

### Deregistering Accessories

If accessories are connected or removed without performing a bus scan, the drive will show **Error Code 04**:

![Error Code 04](Images/bus_scan/BS_8.jpg)

To **deregister** an accessory:

1. **Disconnect the drive from power.**  
2. Unplug the accessory.  
3. Reconnect power.  
4. **Perform another bus scan.**

After the scan, the display briefly shows **0** – meaning the accessory has been successfully deregistered.

![Accessory Deregistered](Images/bus_scan/BS_9.jpg)

---

## 🔌 Bus Scan on ProMatic Series 4

For the **ProMatic Series 4**, **DIL switch H (No. 8)** is responsible for the bus scan:

1. Set the switch to **ON** (scan starts).  
2. Then switch it back to **OFF**.

⚠️ A new bus scan must be performed every time you connect or remove accessories.

---

## 💡 What Are HCP Bus Connections For?

**HCP = Hörmann Communication Protocol**

They allow you to connect a wide range of accessories, including:

- Universal adapter boards  
- Option relays  
- Smart home modules

### HCP1 Bus
- Available on **RotaMatic**, **LineaMatic**, **VersaMatic**,  
  and **SupraMatic Series 2 & 3**  
- Supports gateways for **Homematic**, **DeltaDore**, and **Apple HomeKit**

### HCP2 Bus
- Currently used only in **ProMatic Series 4** and **SupraMatic Series 4**
- Required for **KNX gateways**

Adapter boards provide multiple control options:
- Open / Close  
- Stop  
- Partial opening  
- Light  
- Ventilation position

---

## 🔗 More Information

➡️ [Help for Possible Error Messages on SupraMatic Drives](https://www.tor7.de/news/hilfe-zu-den-moeglichen-fehlermeldungen-vom-toroeffner-supramatic)

---

*Source: [tor7.de – Bus Scan on SupraMatic Series 4: Avoid Error Code 04](https://www.tor7.de/news/bus-scan-beim-supramatic-serie-4-fehlercode-04-vermeiden)*
