#include <Arduino.h>
#include "EspSaveCrash.h"
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "hciemulator.h"
#include "../../WebUI/index_html.h"


/* create this file and add your wlan credentials
  const char* ssid = "MyWLANSID";
  const char* password = "MYPASSWORD";
*/
#include "../../../private/credentials.h"


// switch relay sync to the lamp
// e.g. the Wifi Relay Board U4648
#define USERELAY

// use alternative uart pins 
//#define SWAPUART

#define RS485 Serial

// Relay Board parameters
#define ESP8266_GPIO2    2 // Blue LED.
#define ESP8266_GPIO4    4 // Relay control.
#define ESP8266_GPIO5    5 // Optocoupler input.
#define LED_PIN          ESP8266_GPIO2

EspSaveCrash SaveCrash;
// the buffer to put the Crash log to
char *_debugOutputBuffer;

// webserver on port 80
AsyncWebServer server(80);

#ifdef USERELAY
// switch GPIO4 und GPIO2 sync to the lamp
void onStatusChanged(SHCIState* state){
  //see https://ucexperiment.wordpress.com/2016/12/18/yunshan-esp8266-250v-15a-acdc-network-wifi-relay-module/
  //Setting GPIO4 high, causes the relay to close the NO contact with
  if(state->valid){    
      digitalWrite( ESP8266_GPIO4, state->lampOn ); 
      digitalWrite(LED_PIN, state->lampOn);
  }else
  {
      digitalWrite( ESP8266_GPIO4, false ); 
      digitalWrite(LED_PIN, false);
  }
}
#else
void onStatusChanged(SHCIState* state){
}
#endif



// toggle lamp to expected state
void switchLamp(bool on){
  int lampon = getHCIState()->lampOn;
  bool toggle = (on && !lampon) || (!on && lampon);
  if(toggle){
    toggleLamp();
  }    
}

// setup mcu
void setup(){
  
  //setup modbus
  // Hörmann HCP2 based on modbus rtu @57.6kB 8E1
  uart0_open(57600,UART_FLAGS_8E1);

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
    SHCIState *doorstate = getHCIState();
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument root(1024);
    root["valid"] = doorstate->valid && (getMessageAge()/1000)<3000;
    root["doorstate"] = doorstate->doorState;
    root["doorposition"] = doorstate->doorCurrentPosition;
    root["doortarget"] = doorstate->doorTargetPosition;
    root["lamp"] = doorstate->lampOn;
    root["debug"] = doorstate->reserved;
    root["lastresponse"] = getMessageAge()/1000;
    serializeJson(root,*response);
    request->send(response);
  }); 

  server.on("/command", HTTP_GET, [] (AsyncWebServerRequest *request) {    
    if (request->hasParam("action")) {
      int actionid = request->getParam("action")->value().toInt();
      switch (actionid){
      case 0:
          closeDoor();
        break;
      case 1:
          openDoor();
          break;
      case 2:
          stopDoor();
          break;
      case 3:
          ventilationPosition();
          break;
      case 4:
          openDoorHalf();
          break;
      case 5:
          toggleLamp();
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

// mainloop
bool isLampOn = false;
void loop(){     
   bool newisLampOn = getHCIState()->valid && getHCIState()->lampOn;
   if(isLampOn!=newisLampOn){
     onStatusChanged(getHCIState());
     isLampOn = newisLampOn;
   }
   AsyncElegantOTA.loop();
}