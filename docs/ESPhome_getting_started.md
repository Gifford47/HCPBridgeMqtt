# Getting started with the ESPHome version

Our boards from tynet.eu ship with the normal Arduino based Firmware that is simple to use out of the box.
Sadly this software that was developed over the past years got some minor inconveniences that are not simple to fix.
If you are one of the few people that have an issue with th software or just wan't to use ESPHome instead you can follow this guide

ESPHome uses a native Home Assistant API instead of MQTT and is simple to setup, we also porivde configuration files as well as ready to flash bin files for our boards.

## Setup ESPHome in Home Assistant

You can follow the [official guide](https://esphome.io/guides/getting_started_hassio) to install the ESPHome Add-on for Home Assistant.

The main steps are:

1. Open your Home Assitant UI
2. Go to Settings->Add-Ons->Add-on Store and search for ESPHome
3. Install and activate the ESPHome addon

## Upgrade from non ESPHome firmware

Our boards all ship with a non ESPHome based firmware, it can be upgraded really simple.

1. Ensure your board is powered over USB-C 
2. Download ESPHome based firmware file: [hcpbridge.ota.bin](https://github.com/Tysonpower/HCPBridgeMqtt_tynet/raw/refs/heads/main/hcpbridge.ota.bin)
3. Connect to your board and open the OTA webui: [http://192.168.4.1/update](http://192.168.4.1/update)
4. If asked use the following credentials to login: `user: admin pass: admin`
5. Click "Select File", navigate to the downloaded ota file from step 1, 

## Board Setup

### Using USB (recommended)

1. Ensure your board is connected to your PC over USB-C
2. Open [web.ESPHome.io](https://web.esphome.io), click connect, select the ESP32-S3 device from the list
3. Click the 3 point menu and select Configure Wifi, select or anter your wifi name and password
<img width="659" alt="image" src="https://github.com/user-attachments/assets/789d1c11-b7df-4c48-9421-5397f1cb5cc1">

4. After entering your wifi credentials click connect
<img width="419" alt="image" src="https://github.com/user-attachments/assets/e22cbe8f-8ec8-42d8-8ec0-0d80b87ba629">

### Using WiFi

1. Ensure your board is powered over USB-C
2. Search and connect to the wifi called `hcpbridge`
3. Open http://192.168.4.1 when connected, login is `user: admin pass: tynet.eu`
4. Connect to your wifi network over the UI
____

Now your board should be connected to your home network over wifi.

Open home Assistant, go to Devices, here the garage door should be visible under discovered devices, just click "Add".

![image](https://github.com/user-attachments/assets/278d3c7b-f70f-47df-ab5e-c5430bf2f175)


## Change preflashed config (Optional)

1. open home assistant and go to the ESPHome UI under Add-Ons
2. You should see the dicovered device, press take control. if you like, change the name
![image](https://github.com/user-attachments/assets/04714647-a317-4312-9d11-c7755bf53faf)

4. it will ask to update the device with encryption, click install, it will take a wile to build the new firmware
![image](https://github.com/user-attachments/assets/38ce6137-78ff-4f72-aea1-fe9cc9cb473d)

5. When it finished uploading you will see the following log output, if this is the case you can press close and you are done
![image](https://github.com/user-attachments/assets/21efb5b9-3eea-4ca7-b932-d811f7a52831)

In some cases you need to set your wifi credentials again afterwards like before


## Setup ESPHome in Home Assistant from scratch

1. Open ESPHome in Home Assistant
2. Connect your board only via usb to your PC
3. Click "New Device" and enter a name
<img width="409" alt="image" src="https://github.com/user-attachments/assets/c01adb14-b041-4001-88d8-c1de9aa4d283">

4. Click ESP32-S3
<img width="412" alt="image" src="https://github.com/user-attachments/assets/2f96c7cd-9611-45e3-ac7b-9eef8695aa58">

5. Copy and save the displayed encryption key and click skip
<img width="412" alt="image" src="https://github.com/user-attachments/assets/0601f882-b53e-445d-bc7c-b402f84b355b">

6. Click on the secrets button on the top right, create the following keys and enter your wifi credentials as well as the secret api key you just copied, hit save

```
wifi_ssid: "myWifi"
wifi_password: "myWifiPassword"
api_key: "myApiSecretWeCopiedAfewMinutesAgo"
web_username: "admin"
web_password: "tynet.eu"
hcp_wifi_ap_password: "tynet.eu"
```

7. Click Edit on the device card
<img width="404" alt="image" src="https://github.com/user-attachments/assets/0c3b92ee-bd92-488f-8b1d-f8b9932b61c1">

8. Copy paste the content of the ESPHome.yaml file from this repo into the editor, click save
<img width="857" alt="image" src="https://github.com/user-attachments/assets/d6283427-480f-4128-8c11-9a84b51699a7">


9. Click install, then select Manual download, wait for the firmware to be build, select factory format to get the.bin file
<img width="1070" alt="image" src="https://github.com/user-attachments/assets/bc316b5e-8e84-47a2-a0fb-bd4f69eaa117">

<img width="1070" alt="image" src="https://github.com/user-attachments/assets/efff77d5-2ae1-4152-be38-c851fb496f2b">

10. Then go to [web.ESPHome.io](https://web.esphome.io/), click connect and select your ESP from the device list, click INSTALL, select the .bin file you just downloaded and click Install.
If it doesen't work, try it multiple times
<img width="1070" alt="image" src="https://github.com/user-attachments/assets/5d1d382a-e449-4071-835c-67cbf3130ed4">

11. go back to home assistant, open settings->devices->ESPHome, here you should be able to add the device, if not try to powercycle the board
<img width="1056" alt="image" src="https://github.com/user-attachments/assets/4d591352-ec6e-4184-b109-da8ef8a03a5a">

Click done and you can now use the Device
<img width="1056" alt="image" src="https://github.com/user-attachments/assets/f918b4ef-936f-4581-8b93-9a31abca524a">

