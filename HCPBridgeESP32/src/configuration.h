//WIFI
const char* SSID = "MyWLANSID";
const char* PASSWORD = "MYPASSWORD";

//MQTT
const char* MQTTSERVER = "192.168.1.10";
const int MQTTPORT = 1883;

//OTA Update
const char* OTA_USERNAME = "admin";
const char* OTA_PASSWD = "admin";

//DS18X20
//#define USE_DS18X20         //Uncomment to use a DS18X20 Sensor
const int oneWireBus = 4;   //GPIO where the DS18B20 is connected to
#define SENSE_PERIOD (2*60*1000L)  //read interval of ds18x20
#define temp_threshold 1    //only send mqtt msg when rising this threshold. set 0 to send every status
