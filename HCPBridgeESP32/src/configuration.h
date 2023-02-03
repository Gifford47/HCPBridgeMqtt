// WIFI
const char* AP_PASSWD = "password";
const char HOSTNAME[] = "HCPBRIDGE";

// MQTT
const char *MQTTSERVER = "192.168.1.100";
const int MQTTPORT = 1883;
const char MQTTUSER[] = "mqtt";
const char MQTTPASSWORD[] = "password";

//OTA Update
const char* OTA_USERNAME = "admin";
const char* OTA_PASSWD = "admin";

// Serial port
#define RS485 Serial2
#define PIN_TXD 17 // UART 2 TXT - G17
#define PIN_RXD 16 // UART 2 RXD - G16
//#define SWAPUART

// MODBUS
const int READ_DELAY = 6000;           // intervall (ms) to update status on mqtt

//#define SENSORS              //Uncomment to globally enable sensors
#define SENSE_PERIOD (5*60*1000L)  //read interval of all defined sensors

#define temp_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
#define hum_threshold 2    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
#define pres_threshold 2    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status

//DS18X20
//#define USE_DS18X20         //Uncomment to use a DS18X20 Sensor (only use one sensor!)
const int oneWireBus = 4;     //GPIO where the DS18B20 is connected to

//BME280                      //Uncomment to use a I2C BME280 Sensor
//#define USE_BME             //Uncomment to use a DS18X20 Sensor (only use one sensor!)
#define I2C_SDA 21
#define I2C_SCL 22


// MQTT strings
#define FTOPIC "hormann/garage_door" 
#define AVAILABILITY_TOPIC FTOPIC "/availability"
#define STATE_TOPIC FTOPIC "/state"
#define CMD_TOPIC FTOPIC "/command"
#define POS_TOPIC FTOPIC "/position"
#define LAMP_TOPIC FTOPIC "/command/lamp"
#define DOOR_TOPIC FTOPIC "/command/door"
#define SENSOR_TOPIC FTOPIC "/sensor"

#define HA_DISCOVERY_BIN_SENSOR "homeassistant/binary_sensor/garage_door/%s/config"
#define HA_DISCOVERY_AV_SENSOR "homeassistant/sensor/garage_door/available/config"
#define HA_DISCOVERY_SENSOR "homeassistant/sensor/garage_door/%s/config"
#define HA_DISCOVERY_SWITCH "homeassistant/switch/garage_door/%s/config"
#define HA_DISCOVERY_COVER "homeassistant/cover/garage_door/%s/config"

const char *HA_ON = "true";
const char *HA_OFF = "false";
const char *HA_OPEN = "open";
const char *HA_CLOSE = "close";
const char *HA_HALF = "half";
const char *HA_STOP = "stop";
const char *HA_OPENING = "opening";
const char *HA_CLOSING = "closing";
const char *HA_CLOSED = "closed";
const char *HA_OPENED = "open";
const char *HA_ONLINE = "online";
const char *HA_OFFLINE = "offline";

const char *HA_VERSION = "0.0.4.0";
