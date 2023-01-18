//WIFI
const char* SSID = "MyWLANSID";
const char* PASSWORD = "MYPASSWORD";

//MQTT
const char* MQTTSERVER = "192.168.1.10";
const int MQTTPORT = 1883;

//OTA Update
const char* OTA_USERNAME = "admin";
const char* OTA_PASSWD = "admin";

// switch relay sync to the lamp
// e.g. the Wifi Relay Board U4648
//#define USERELAY              //enable/disable relay
#define RELAY_PIN  ESP8266_GPIO4       // Relay control pin.
#define RELAY_INPUT  ESP8266_GPIO5       // Input pin.

// use alternative uart pins
//#define SWAPUART

#define SENSORS              //Uncomment to globally enable sensors
#define SENSE_PERIOD (2*60*1000L)  //read interval of all defined sensors

#define temp_threshold 1    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
#define hum_threshold 2    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status
#define pres_threshold 2    //only send mqtt msg when temp,pressure or humidity rises this threshold. set 0 to send every status

//DS18X20
//#define USE_DS18X20         //Uncomment to use a DS18X20 Sensor
const int oneWireBus = 4;     //GPIO where the DS18B20 is connected to

//BME280                      //Uncomment to use a I2C BME280 Sensor
//#define USE_BME             //Uncomment to use a DS18X20 Sensor
#define I2C_SDA 21
#define I2C_SCL 22