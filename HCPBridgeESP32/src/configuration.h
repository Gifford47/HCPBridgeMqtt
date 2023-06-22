#ifndef CONFIGURATION_H_
    #define CONFIGURATION_H_

    // WIFI Accesspoint
    #define AP_PASSWD  "password"
    #define HOSTNAME   "HCPBRIDGE"

    // MQTT
    #define MQTTSERVER  "192.168.1.100"
    #define MQTTPORT   1883
    #define MQTTUSER  "mqtt"
    #define MQTTPASSWORD  "password"

    //OpenHab as SmartHome
    #define AlignToOpenHab true

    //OTA Update
    #define OTA_USERNAME  "admin"
    #define OTA_PASSWD  "admin"

    // Serial port
    #define RS485 Serial2
    #define PIN_TXD 17      // UART 2 TXT - G17
    #define PIN_RXD 16      // UART 2 RXD - G16
    //#define SWAPUART

    // MQTT
    #define READ_DELAY   2000           // intervall (ms) to update status on mqtt

    //#define SENSORS              //Uncomment to globally enable sensors
    #define SENSE_PERIOD (2*60*1000L)  //read interval of all defined sensors

    #define temp_threshold 0.5    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
    #define hum_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
    #define pres_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status

    //DS18X20
    //#define USE_DS18X20         //Uncomment to use a DS18X20 Sensor (only use one sensor!)
    #define oneWireBus  4     //GPIO where the DS18B20 is connected to

    // NOTICE: Breadboards should have 2k2 or 3k3 PullUp resistor between SCL and SDA! If not: interferences
    //BME280                      //Uncomment to use a I2C BME280 Sensor
    //#define USE_BME             //Uncomment to use a DS18X20 Sensor (only use one sensor!)
    #define I2C_ON_OFF  4       // switches I2C On and Off: connect VDD to this GPIO! (due to interferences on i2c bus while door actions (UP/DOWN ...))
    #define I2C_SDA 21
    #define I2C_SCL 22


    // MQTT strings
    #define FTOPIC "hormann/garage_door" 
    #define AVAILABILITY_TOPIC FTOPIC "/availability"
    #define STATE_TOPIC FTOPIC "/state"
    #define CMD_TOPIC FTOPIC "/command"
    #define POS_TOPIC FTOPIC "/position"
    #define SETPOS_TOPIC CMD_TOPIC "/set_position"
    #define LAMP_TOPIC FTOPIC "/command/lamp"
    #define DOOR_TOPIC FTOPIC "/command/door"
    #define SENSOR_TOPIC FTOPIC "/sensor"

    #define HA_DISCOVERY_BIN_SENSOR "homeassistant/binary_sensor/garage_door/%s/config"
    #define HA_DISCOVERY_AV_SENSOR "homeassistant/sensor/garage_door/available/config"
    #define HA_DISCOVERY_SENSOR "homeassistant/sensor/garage_door/%s/config"
    #define HA_DISCOVERY_SWITCH "homeassistant/switch/garage_door/%s/config"
    #define HA_DISCOVERY_COVER "homeassistant/cover/garage_door/%s/config"

    // DEBUG
    //#define DEBUG
    #define DEBUGTOPIC FTOPIC "/DEBUG"
    void sendDebug(char *key, String value);
    
    // HA Topics
    #define HA_ON  "true"
    #define HA_OFF  "false"
    #define HA_OPEN  "open"
    #define HA_CLOSE  "close"
    #define HA_HALF  "half"
    #define HA_STOP  "stop"
    #define HA_OPENING  "opening"
    #define HA_CLOSING  "closing"
    #define HA_CLOSED  "closed"
    #define HA_OPENED  "open"
    #define HA_ONLINE  "online"
    #define HA_OFFLINE  "offline"

    #define HA_VERSION  "0.0.5.1"

#endif