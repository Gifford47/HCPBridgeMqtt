#ifndef CONFIGURATION_H_
    #define CONFIGURATION_H_
    // WIFI Hostname

    const char HOSTNAME[]   = "HCPBRIDGE";

    // Station -> set AP_ACTIF to false if you wanna use password from config file
    const bool AP_ACTIF = (bool)true;
    const char* STA_SSID   = "";
    const char* STA_PASSWD = "";

    //RS485 pins
    #ifdef CONFIG_IDF_TARGET_ESP32S3
        #ifdef M5STACK
            #define PIN_TXD 13
            #define PIN_RXD 15          
        #else
            #define PIN_TXD 17
            #define PIN_RXD 18
        #endif
    #else
        #define PIN_TXD 17 // UART 2 TXT - G17
        #define PIN_RXD 16 // UART 2 RXD - G16
    #endif

    // MQTT
    #define DEVICE_ID "hcpbridge"
    const char DEVICENAME[] = "Garage Door";
    const char *MQTTSERVER = "192.168.1.100";
    const int MQTTPORT = 1883;
    const char MQTTUSER[] = "mqtt";
    const char MQTTPASSWORD[] = "password";
    const char GD_AVAIL[]= "Garage Door Available";
    const char GD_LIGHT[] = "Garage Door Light";
    const char GD_VENT[] = "Garage Door Vent";
    const char GD_STATUS[] = "Garage Door Status";
    const char GD_DET_STATUS[] = "Garage Door detailed Status";
    const char GD_POSITIOM[] = "Garage Door Position";
    const char GD_MOTION[] = "Garage Door Motion";
    const char GS_TEMP[] = "Garage Temperature";
    const char GS_HUM[] = "Garage Humidity";
    const char GS_PRES[] = "Garage ambient pressure";
    const char GS_FREE_DIST[] = "Garage Free distance";
    const char GS_PARK_AVAIL[] = "Garage park available";
    const char GD_DEBUG[] = "Garage Door Debug";
    const char GD_DEBUG_RESTART[] = "Garage Restart Reason";

    //OpenHab as SmartHome if uncommented. Comment for homeassistant
    //#define AlignToOpenHab

    //OTA Update
    const char* OTA_USERNAME = "admin";
    const char* OTA_PASSWD = "admin";
    
    // MQTT
    const int READ_DELAY = 2000;           // intervall (ms) to update status on mqtt

    #define SENSE_PERIOD 120  //read interval in Seconds of all defined sensors in seconds

    #define temp_threshold 0.5    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
    #define hum_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
    #define pres_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
    #define prox_treshold 10    //only send mqtt msg when distance change this treshold. Set 0 to send every status

    //DS18X20
    #define oneWireBus 4     //GPIO where the DS18B20 is connected to

    // NOTICE: Breadboards should have 2k2 or 3k3 PullUp resistor between SCL and SDA! If not: interferences
    //BME280                     
    #define I2C_ON_OFF  4       // switches I2C On and Off: connect VDD to this GPIO! (due to interferences on i2c bus while door actions (UP/DOWN ...))
    #define I2C_SDA 21
    #define I2C_SCL 22

    //HC-SR04                   
    #define SR04_TRIGPIN 5
    #define SR04_ECHOPIN 18
    #define SR04_MAXDISTANCECM 150
    #define SOUND_SPEED 0.034   //define sound speed in cm/uS

    // DHT22
    #define DHT_VCC_PIN 27
    #define DHTTYPE DHT22

    //HC-SR501
    #define SR501PIN 23

    // MQTT strings
    #define HA_DISCOVERY_BIN_SENSOR "homeassistant/binary_sensor/%s/%s/config"
    #define HA_DISCOVERY_AV_SENSOR "homeassistant/sensor/%s/available/config"
    #define HA_DISCOVERY_SENSOR "homeassistant/sensor/%s/%s/config"
    #define HA_DISCOVERY_SWITCH "homeassistant/switch/%s/%s/config"
    #define HA_DISCOVERY_COVER "homeassistant/cover/%s/%s/config"
    #define HA_DISCOVERY_LIGHT "homeassistant/light/%s/%s/config"
    #define HA_DISCOVERY_TEXT "homeassistant/text/%s/%s/config"

    // DEBUG
    //#define DEBUG

    // HA Topics
    #ifndef AlignToOpenHab
        const char *HA_ON = "true";
        const char *HA_OFF = "false";
        const char *HA_OPEN = "open";
        const char *HA_CLOSE = "close";
        const char *HA_HALF = "half";
        const char *HA_VENT = "venting";
        const char *HA_STOP = "stop";
        const char *HA_OPENING = "opening";
        const char *HA_CLOSING = "closing";
        const char *HA_CLOSED = "closed";
        const char *HA_OPENED = "open";
        const char *HA_ONLINE = "online";
        const char *HA_OFFLINE = "offline";
    #else
        const char *HA_ON = "true";
        const char *HA_OFF = "false";
        const char *HA_OPEN = "open";
        const char *HA_CLOSE = "close";
        const char *HA_HALF = "half";
        const char *HA_VENT = "venting";
        const char *HA_STOP = "STOP";
        const char *HA_OPENING = "UP";
        const char *HA_CLOSING = "DOWN";
        const char *HA_CLOSED = "DOWN";
        const char *HA_OPENED = "UP";
        const char *HA_ONLINE = "online";
        const char *HA_OFFLINE = "offline";
    #endif
    const char *HA_VERSION = "0.0.6.0";

#endif
