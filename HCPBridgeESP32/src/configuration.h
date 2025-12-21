#ifndef CONFIGURATION_H_
    #define CONFIGURATION_H_
    
    // Please change on every new firmware builds!
    const char *HA_VERSION = "0.9";

    // WIFI Hostname
    const char HOSTNAME[]   = "HCPBRIDGE";

    // Station -> set AP_ACTIF to false if you wanna use password from config file
    const bool AP_ACTIF = (bool)true;
    const char* STA_SSID   = "";
    const char* STA_PASSWD = "";
    const char* AP_PASSWD = "gifford47";
    const char* WWW_USER = "admin";
    const char* WWW_PASSWD = "";

    //OTA Update
    const char* OTA_USERNAME = "admin";
    const char* OTA_PASSWD = "admin";

    //RS485 pins
    #ifdef CONFIG_IDF_TARGET_ESP32S3
        #ifdef M5STACK
            #define PIN_TXD 13
            #define PIN_RXD 15          
        #else
            #define PIN_TXD 17
            #define PIN_RXD 18
        #endif
    #elif defined(HCP_Giffordv3)
        #define PIN_TXD 16  // Richtige Pins für HCP_Giffordv3?
        #define PIN_RXD 17
    #else
        #define PIN_TXD 17 // UART 2 TXT - G17
        #define PIN_RXD 16 // UART 2 RXD - G16
    #endif

    // MQTT
    #define DEVICE_ID "hcpbridge"
    const char ID_LENGTH = 16;
    const char DEVICENAME[] = "Garage Door";
    const char *MQTTSERVER = "192.168.1.100";
    const int MQTTPORT = 1883;
    const char MQTTUSER[] = "mqtt";
    const char MQTTPASSWORD[] = "password";
    const char GD_AVAIL[]= "Available";
    const char GD_LIGHT[] = "Light";
    const char GD_VENT[] = "Vent";
    const char GD_HALF[] = "Half Open";
    const char GD_STEP[] = "Step";
    const char GD_STATUS[] = "Status";
    const char GD_DET_STATUS[] = "Detailed Status";
    const char GD_POSITIOM[] = "Position";
    const char GS_TEMP[] = "Temperature";
    const char GS_HUM[] = "Humidity";
    const char GS_PRES[] = "Ambient pressure";
    const char GS_FREE_DIST[] = "Free distance";
    const char GS_PARK_AVAIL[] = "Park available";
    const char GS_MOTION[] = "Motion";
    const char GD_DEBUG[] = "Debug";
    const char GD_DEBUG_RESTART[] = "Restart Reason";

    //OpenHab as SmartHome if uncommented. Comment for homeassistant
    //#define AlignToOpenHab
    
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
    #if defined(HCP_Giffordv2)
        #define I2C_SDA 21
        #define I2C_SCL 33
    #elif defined(HCP_Giffordv3)
        #define I2C_SDA 21
        #define I2C_SCL 33
    #else
        #define I2C_SDA 21
        #define I2C_SCL 22
    #endif

    //HC-SR04
    #if defined(HCP_Giffordv2)
        #define SR04_TRIGPIN 5
        #define SR04_ECHOPIN 48
    #elif defined(HCP_Giffordv3)
        #define SR04_TRIGPIN 5
        #define SR04_ECHOPIN 27
    #else
        #define SR04_TRIGPIN 5
        #define SR04_ECHOPIN 18
    #endif
    #define SR04_MAXDISTANCECM 150
    #define SOUND_SPEED 0.034   //define sound speed in cm/uS

    // DHT22
    #if defined(HCP_Giffordv3)
        #define DHTPIN 26
    #else
        #define DHTPIN 27
    #endif
    #define DHTTYPE DHT22

    //HC-SR501
    #if defined(HCP_Giffordv2)
    #define SR501PIN 23
    #elif defined(HCP_Giffordv3)
        #define SR501PIN 32
    #else
        #define SR501PIN 34
    #endif

    //digital in- and outputs
    #if defined(HCP_Giffordv2)
        #define LED1 13
        #define INPUT1 12
        #define INPUT2 14
        #define OUTPUT1 37
        #define OUTPUT2 35
        #define MQ2_ANALOG 36
        #define MQ2_DIG 15

    #elif defined(HCP_Giffordv3)
        #define LED1 13
        #define INPUT1 12
        #define INPUT2 14
        #define OUTPUT1 25
        #define OUTPUT2 22
        #define MQ2_ANALOG 2
        #define MQ2_DIG 15
    #endif

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
        const char *HA_STEP = "step";
        const char *HA_HALF = "half";
        const char *HA_HALFOPEN = "open h";
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
        const char *HA_STEP = "step";
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

#endif
