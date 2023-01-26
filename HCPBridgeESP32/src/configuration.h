//WIFI
const char* AP_PASSWD = "admin";
const char HOSTNAME[] = "HCPBridge";

//MQTT
const char *MQTTSERVER = "192.168.1.100";
const int MQTTPORT = 1883;
#define FTOPIC "home/garage/door"
#define AVAILABILITY_TOPIC FTOPIC "/availability"
#define STATE_TOPIC FTOPIC "/state"
#define CMD_TOPIC FTOPIC "/command"
#define LAMP_TOPIC FTOPIC "/command/lamp"
#define DOOR_TOPIC FTOPIC "/command/door"
#define SENSOR_TOPIC FTOPIC "/sensor"

#define HA_DISCOVERY_BIN_SENSOR "homeassistant/binary_sensor/garage/%s/config"
#define HA_DISCOVERY_AV_SENSOR "homeassistant/sensor/garage/available/config"
#define HA_DISCOVERY_SENSOR "homeassistant/sensor/garage/%s/config"
#define HA_DISCOVERY_SWITCH "homeassistant/switch/garage/%s/config"
#define HA_DISCOVERY_COVER "homeassistant/cover/garage/%s/config"

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

const char *HA_VERSION = "0.0.2.0";

//OTA Update
const char* OTA_USERNAME = "admin";
const char* OTA_PASSWD = "admin";

//MODBUS
const int READ_DELAY = 6000;           // intervall (ms) to read modbus for new status

// switch relay sync to the lamp
// e.g. the Wifi Relay Board U4648
//#define USERELAY              //enable/disable relay
// Relay Board parameters
#define ESP8266_GPIO2 2 // Blue LED.
#define ESP8266_GPIO4 4 // Relay control.
#define ESP8266_GPIO5 5 // Optocoupler input.
#define LED_PIN ESP8266_GPIO2

// use alternative uart pins
//#define SWAPUART

#define SENSORS              //Uncomment to globally enable sensors
#define SENSE_PERIOD (5*60*1000L)  //read interval of all defined sensors

#define temp_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
#define hum_threshold 2    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
#define pres_threshold 2    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status

//DS18X20
//#define USE_DS18X20         //Uncomment to use a DS18X20 Sensor
const int oneWireBus = 4;     //GPIO where the DS18B20 is connected to

//BME280                      //Uncomment to use a I2C BME280 Sensor
#define USE_BME             //Uncomment to use a DS18X20 Sensor
#define I2C_SDA 21
#define I2C_SCL 22