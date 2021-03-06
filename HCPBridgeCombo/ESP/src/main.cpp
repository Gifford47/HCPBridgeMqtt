#include <Arduino.h>
#include "EspSaveCrash.h"
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "index_html.h"
#include "../../Arduino/src/i2cshare.h"


/* create this file and add your wlan credentials
  const char* ssid = "MyWLANSID";
  const char* password = "MYPASSWORD";
*/
#include "../../../../private/credentials.h"


// switch relay sync to the lamp
// e.g. the Wifi Relay Board U4648
#define USERELAY


// Relay Board parameters
#define ESP8266_GPIO2    2 // Blue LED.
#define ESP8266_GPIO4    4 // Relay control.
#define ESP8266_GPIO5    5 // Optocoupler input.
#define LED_PIN          ESP8266_GPIO2

EspSaveCrash SaveCrash;
// the buffer to put the Crash log to
char *_debugOutputBuffer;


SHCIState lastState;

// webserver on port 80
AsyncWebServer server(80);

unsigned long nextStateCall = millis();
unsigned long lasti2cmsgtime = millis();


// switch GPIO4 und GPIO2 sync to the lamp
void onStatusChanged(){
  //see https://ucexperiment.wordpress.com/2016/12/18/yunshan-esp8266-250v-15a-acdc-network-wifi-relay-module/
  //Setting GPIO4 high, causes the relay to close the NO contact with
#ifdef USERELAY  
  bool lamp = lastState.valid && lastState.lampOn;  
  digitalWrite( ESP8266_GPIO4, lamp ); 
  digitalWrite(LED_PIN, lamp);  
#endif
}




// setup mcu
void setup(){

  lastState.valid = false;
  lastState.reserved = 0xAB;

  // Setup I2C Master on UART Pins to use relayboard
  //ESP 8266 PIN 1(TX0) and PIN 3 (RX0)
  Wire.begin(1,3);

  _debugOutputBuffer = (char *) calloc(2048, sizeof(char));

  //setup wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  // setup http server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P( 200, "text/html", index_html,sizeof(index_html));
    response->addHeader("Content-Encoding","deflate");    
    request->send(response);
  }); 

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument root(1024);
    root["valid"] = lastState.valid;
    root["doorstate"] = lastState.doorState;
    root["doorposition"] = lastState.doorCurrentPosition;
    root["doortarget"] = lastState.doorTargetPosition;
    root["lamp"] = lastState.lampOn;
    root["debug"] = lastState.reserved;
    root["cc"] = lastState.cc;
    root["lastresponse"] = millis()-lasti2cmsgtime;
    
    serializeJson(root,*response);
    request->send(response);
  }); 

  server.on("/command", HTTP_GET, [] (AsyncWebServerRequest *request) {    
    if (request->hasParam("action")) {
      int actionid = request->getParam("action")->value().toInt();
      switch (actionid){
        case I2C_CMD_CLOSEDOOR:
        case I2C_CMD_OPENDOOR:
        case I2C_CMD_OPENDOORHALF:
        case I2C_CMD_STOPDOOR:
        case I2C_CMD_VENTPOS:
        case I2C_CMD_TOGGLELAMP:
          Wire.beginTransmission(I2CADDR);
          Wire.write((unsigned char)actionid);
          Wire.endTransmission();      
          break;

        default:
          break;
      }
    }
    request->send(200, "text/plain", "OK");
  });
  
  server.on("/crashinfo", HTTP_GET, [] (AsyncWebServerRequest *request){    
    strcpy(_debugOutputBuffer, "");    
    SaveCrash.print(_debugOutputBuffer,2048);
    if (request->hasParam("clear")) {
      SaveCrash.clear();
    }
    request->send(200, "text/plain", _debugOutputBuffer);
  });

  server.on("/sysinfo", HTTP_GET, [] (AsyncWebServerRequest *request) {      
    char buffer[150];
    rst_info* rinfo = ESP.getResetInfoPtr();
  
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument root(1024);
    root["freemem"] = ESP.getFreeHeap();    
    root["hostname"] = WiFi.hostname();
    root["ip"] = WiFi.localIP().toString();
    root["ssid"] = String(ssid);
    root["wifistatus"] = WiFi.status();
    root["resetreason"] =ESP.getResetReason();
    root["errors"] =  rinfo->exccause;
    
    //The	address	of	the	last	crash	is	printed,	which	is	used	to    
    sprintf(buffer, "epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x, exccause=0x%x, reason=0x%x",
      rinfo->epc1,	rinfo->epc2,	rinfo->epc3,	rinfo->excvaddr,	rinfo->depc, rinfo->exccause, rinfo->reason); 
    root["rstinfo"] =  buffer;
    serializeJson(root,*response);

    request->send(response);    
  });

  AsyncElegantOTA.begin(&server);
  server.begin();

  //setup relay board
#ifdef USERELAY
  pinMode( ESP8266_GPIO4, OUTPUT );       // Relay control pin.
  pinMode( ESP8266_GPIO5, INPUT_PULLUP ); // Input pin.
  pinMode( LED_PIN, OUTPUT );             // ESP8266 module blue L
  digitalWrite( ESP8266_GPIO4, 0 );
  digitalWrite(LED_PIN,0);  
#endif
  
}

void loop(){   
   if(nextStateCall < millis()){
      Wire.requestFrom(I2CADDR, sizeof(SHCIState));
      if(sizeof(SHCIState) != Wire.readBytes((unsigned char*) &lastState, sizeof(SHCIState))){
        lasti2cmsgtime = millis();
        onStatusChanged();
      }
      nextStateCall = millis()+200;      
   }
   AsyncElegantOTA.loop();
}