#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include "hciemulator.h"
#include "index_html.h"

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


// Hörmann HCP2 based on modbus rtu @57.6kB 8E1
HCIEmulator emulator(&RS485);

// webserver on port 80
AsyncWebServer server(80);

// called by ESPAsyncTCP-esphome:SyncClient.cpp (see patch) instead of delay to avoid connection breaks
void DelayHandler(void){
    emulator.poll();
}

// switch GPIO4 und GPIO2 sync to the lamp
void onStatusChanged(const SHCIState& state){
  //see https://ucexperiment.wordpress.com/2016/12/18/yunshan-esp8266-250v-15a-acdc-network-wifi-relay-module/
  //Setting GPIO4 high, causes the relay to close the NO contact with
  if(state.valid){    
      digitalWrite( ESP8266_GPIO4, state.lampOn ); 
      digitalWrite(LED_PIN, state.lampOn);
  }else
  {
      digitalWrite( ESP8266_GPIO4, false ); 
      digitalWrite(LED_PIN, false);
  }
}

// toggle lamp to expected state
void switchLamp(bool on){
  bool toggle = (on && !emulator.getState().lampOn) || (!on && emulator.getState().lampOn);
  if(toggle){
    emulator.toggleLamp();
  }    
}

volatile unsigned long lastCall = 0;
volatile unsigned long maxPeriod = 0;

void modBusPolling( void * parameter) {
  while(true){
      if(lastCall>0){
          maxPeriod = _max(micros()-lastCall,maxPeriod);
      }
      lastCall=micros();
      emulator.poll();  
      vTaskDelay(1);    
  }
  vTaskDelete(NULL);
}


TaskHandle_t modBusTask;

// setup mcu
void setup(){
  
  //setup modbus
  RS485.begin(57600,SERIAL_8E1);
  #ifdef SWAPUART
  RS485.swap();  
  #endif  


  xTaskCreatePinnedToCore(
      modBusPolling, /* Function to implement the task */
      "ModBusTask", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      //1,  /* Priority of the task */
      configMAX_PRIORITIES -1,
      &modBusTask,  /* Task handle. */
      1); /* Core where the task should run */


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
    const SHCIState& doorstate = emulator.getState();
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument root(1024);
    root["valid"] = doorstate.valid;
    root["doorstate"] = doorstate.doorState;
    root["doorposition"] = doorstate.doorCurrentPosition;
    root["doortarget"] = doorstate.doorTargetPosition;
    root["lamp"] = doorstate.lampOn;
    root["debug"] = doorstate.reserved;
    root["lastresponse"] = emulator.getMessageAge()/1000;    
    root["looptime"] = maxPeriod;    

    lastCall = maxPeriod = 0;
    
    serializeJson(root,*response);
    request->send(response);
  }); 

  server.on("/command", HTTP_GET, [] (AsyncWebServerRequest *request) {    
    if (request->hasParam("action")) {
      int actionid = request->getParam("action")->value().toInt();
      switch (actionid){
      case 0:
          emulator.closeDoor();
        break;
      case 1:
          emulator.openDoor();
          break;
      case 2:
          emulator.stopDoor();
          break;
      case 3:
          emulator.ventilationPosition();
          break;
      case 4:
          emulator.openDoorHalf();
          break;
      case 5:
          emulator.toggleLamp();
          break;      
      default:
        break;
      }
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {    
    if (request->hasParam("channel") && request->hasParam("state")) {
      String channel = request->getParam("channel")->value();
      String state = request->getParam("state")->value();
      if(channel.equals("door")){
        if(state=="1"){
          emulator.openDoor();
        }else{
          emulator.closeDoor();
        }          
      }
      if(channel.equals("light")){
        switchLamp(state=="1");
      }      
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();

  //setup relay board
#ifdef USERELAY
  pinMode( ESP8266_GPIO4, OUTPUT );       // Relay control pin.
  pinMode( ESP8266_GPIO5, INPUT_PULLUP ); // Input pin.
  pinMode( LED_PIN, OUTPUT );             // ESP8266 module blue L
  digitalWrite( ESP8266_GPIO4, 0 );
  digitalWrite(LED_PIN,0);
  emulator.onStatusChanged(onStatusChanged);
#endif
  
}

// mainloop
void loop(){     
}