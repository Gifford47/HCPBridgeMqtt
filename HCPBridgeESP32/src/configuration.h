#ifndef CONFIGURATION_H_
    #define CONFIGURATION_H_

    // ============================================================================
    // Debug logging - controlled by WebUI preference "debug_enabled"
    // ============================================================================
    inline bool debugEnabled = false;

    #define DBG_PRINT(...)    do { if (debugEnabled) Serial.print(__VA_ARGS__); } while(0)
    #define DBG_PRINTLN(...)  do { if (debugEnabled) Serial.println(__VA_ARGS__); } while(0)
    #define DBG_PRINTF(...)   do { if (debugEnabled) Serial.printf(__VA_ARGS__); } while(0)

    // Please change on every new firmware builds!
    inline const char *HA_VERSION = "1.0.0";

    // WIFI Hostname
    inline const char HOSTNAME[]   = "HCPBRIDGE";

    // Station -> set AP_ACTIF to false if you wanna use password from config file
    inline const bool AP_ACTIF = (bool)true;
    inline const char* STA_SSID   = "";
    inline const char* STA_PASSWD = "";
    inline const char* AP_PASSWD = "gifford47";
    inline const char* WWW_USER = "admin";
    inline const char* WWW_PASSWD = "";

    //OTA Update
    inline const char* OTA_USERNAME = "admin";
    inline const char* OTA_PASSWD = "admin";

    #if defined(HCP_Giffordv2) || defined(HCP_Giffordv3)
    #define IS_HCP_BOARD
    #endif

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
        #define PIN_TXD 19
        #define PIN_RXD 18
    #else
        #define PIN_TXD 17 // UART 2 TXT - G17
        #define PIN_RXD 16 // UART 2 RXD - G16
    #endif

    // MQTT
    #define DEVICE_ID "hcpbridge"
    // Max device_id length - limited by char[64] buffers in MQTT discovery topics
    // Longest: "homeassistant/binary_sensor/{id}/{key}/config" with key "gas_alarm"
    inline const char ID_LENGTH = 14;
    inline const char DEVICENAME[] = "Garage Door";
    inline const char *MQTTSERVER = "192.168.1.100";
    inline const int MQTTPORT = 1883;
    inline const char MQTTUSER[] = "mqtt";
    inline const char MQTTPASSWORD[] = "password";
    inline const char GD_AVAIL[]= "Available";
    inline const char GD_LIGHT[] = "Light";
    inline const char GD_VENT[] = "Vent";
    inline const char GD_HALF[] = "Half Open";
    inline const char GD_TOGGLE[] = "Toggle";
    inline const char GD_STATUS[] = "Status";
    inline const char GD_DET_STATUS[] = "Detailed Status";
    inline const char GD_POSITIOM[] = "Position";
    inline const char GS_TEMP[] = "Temperature";
    inline const char GS_HUM[] = "Humidity";
    inline const char GS_PRES[] = "Ambient pressure";
    inline const char GS_FREE_DIST[] = "Free distance";
    inline const char GS_PARK_AVAIL[] = "Park available";
    inline const char GS_MOTION[] = "Motion";
    inline const char GS_GAS[] = "Gas Level";
    inline const char GS_GAS_ALARM[] = "Gas Alarm";
    inline const char GD_DEBUG[] = "Debug";
    inline const char GD_DEBUG_RESTART[] = "Restart Reason";

    //OpenHab as SmartHome if uncommented. Comment for homeassistant
    //#define AlignToOpenHab

    // MQTT
    inline const int READ_DELAY = 2000;           // intervall (ms) to update status on mqtt

    #define SENSE_PERIOD 120  //read interval in Seconds of all defined sensors in seconds

    #define temp_threshold 0.5    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
    #define hum_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
    #define pres_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
    #define prox_treshold 10    //only send mqtt msg when distance change this treshold. Set 0 to send every status
    #define gas_threshold 50    //only send mqtt msg when gas analog value changes by this threshold

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
    #define SR501PIN 34
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
        #define MQ4_ANALOG_PIN 36
        #define MQ4_DIGITAL_PIN 15

    #elif defined(HCP_Giffordv3)
        #define LED1 13
        #define INPUT1 12
        #define INPUT2 14
        #define OUTPUT1 25
        #define OUTPUT2 22
        #define MQ4_ANALOG_PIN 35
        #define MQ4_DIGITAL_PIN 15
    #else
        #define MQ4_ANALOG_PIN 0
        #define MQ4_DIGITAL_PIN 0
    #endif

    // MQTT strings
    #define HA_DISCOVERY_BIN_SENSOR "homeassistant/binary_sensor/%s/%s/config"
    #define HA_DISCOVERY_AV_SENSOR "homeassistant/sensor/%s/available/config"
    #define HA_DISCOVERY_SENSOR "homeassistant/sensor/%s/%s/config"
    #define HA_DISCOVERY_SWITCH "homeassistant/switch/%s/%s/config"
    #define HA_DISCOVERY_COVER "homeassistant/cover/%s/%s/config"
    #define HA_DISCOVERY_LIGHT "homeassistant/light/%s/%s/config"
    #define HA_DISCOVERY_BUTTON "homeassistant/button/%s/%s/config"

    // DEBUG
    //#define DEBUG

    // HA Topics
    #ifndef AlignToOpenHab
        inline const char *HA_ON = "true";
        inline const char *HA_OFF = "false";
        inline const char *HA_OPEN = "open";
        inline const char *HA_CLOSE = "close";
        inline const char *HA_STEP = "step";
        inline const char *HA_HALF = "half";
        inline const char *HA_HALFOPEN = "open h";
        inline const char *HA_VENT = "venting";
        inline const char *HA_STOP = "stop";
        inline const char *HA_OPENING = "opening";
        inline const char *HA_CLOSING = "closing";
        inline const char *HA_CLOSED = "closed";
        inline const char *HA_OPENED = "open";
        inline const char *HA_ONLINE = "online";
        inline const char *HA_OFFLINE = "offline";
    #else
        inline const char *HA_ON = "true";
        inline const char *HA_OFF = "false";
        inline const char *HA_OPEN = "open";
        inline const char *HA_CLOSE = "close";
        inline const char *HA_STEP = "step";
        inline const char *HA_HALF = "half";
        inline const char *HA_VENT = "venting";
        inline const char *HA_STOP = "STOP";
        inline const char *HA_OPENING = "UP";
        inline const char *HA_CLOSING = "DOWN";
        inline const char *HA_CLOSED = "DOWN";
        inline const char *HA_OPENED = "UP";
        inline const char *HA_ONLINE = "online";
        inline const char *HA_OFFLINE = "offline";
    #endif

#endif
