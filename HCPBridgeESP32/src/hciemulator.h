#ifndef __hciemulator_h
#define __hciemulator_h

#include <Arduino.h>
#include <Stream.h>
#include <SoftwareSerial.h>

#define LL_OFF 0
#define LL_ERROR 1
#define LL_WARN 2
#define LL_INFO 3
#define LL_DEBUG 4

#define DEFAULTLOGLEVEL LL_WARN

#define DEVICEID 0x02
#define BROADCASTID 0x00
#define SIMULATEKEYPRESSDELAYMS 100 


// Modbus states that a baud rate higher than 19200 must use a fixed 750 us 
// for inter character time out and 1.75 ms for a frame delay.
// For baud rates below 19200 the timeing is more critical and has to be calculated.
// E.g. 9600 baud in a 10 bit packet is 960 characters per second
// In milliseconds this will be 960characters per 1000ms. So for 1 character
// 1000ms/960characters is 1.04167ms per character and finaly modbus states an
// intercharacter must be 1.5T or 1.5 times longer than a normal character and thus
// 1.5T = 1.04167ms * 1.5 = 1.5625ms. A frame delay is 3.5T.  
#define T1_5 750
#define T3_5  4800 //1750  

//workaround as my Supramatic did not Report the Status 0x0A when it's en vent Position
//When the door is at position 0x08 and not moving Status get changed to Ventig.
#define VENT_POS 0x08

enum DoorState : uint8_t {    
    DOOR_OPEN_POSITION =  0x20,    
    DOOR_CLOSE_POSITION = 0x40,
    DOOR_HALF_POSITION =  0x80,
    DOOR_MOVE_CLOSEPOSITION = 0x02,
    DOOR_MOVE_OPENPOSITION = 0x01,
    DOOR_MOVE_VENTPOSITION = 0x09,
    DOOR_MOVE_HALFPOSITION = 0x05,
    DOOR_VENT_POSITION = 0x0A,
    DOOR_STOPPED = 0x00
};

struct SHCIState{
    bool valid;
    bool lampOn;
    uint8_t doorState; // see DoorState
    uint8_t doorCurrentPosition;
    uint8_t doorTargetPosition;
    uint8_t reserved;
    uint8_t gotoPosition;
};

enum StateMachine: uint8_t{
    WAITING,

    OPEN_DOOR,
    OPEN_DOOR_RELEASE,

    OPEN_DOOR_HALF,
    OPEN_DOOR_HALF_RELEASE,

    CLOSE_DOOR,
    CLOSE_DOOR_RELEASE,

    STOP_DOOR,
    STOP_DOOR_RELEASE,

    TOGGLE_LAMP,
    TOGGLE_LAMP_RELEASE,

    VENTPOSITION,
    VENTPOSITION_RELEASE,

    SET_POSITION_OPEN,
    SET_POSITION_OPEN_RELEASE,
    SET_POSITION_OPEN_PROGRESS,

    SET_POSITION_CLOSE,
    SET_POSITION_CLOSE_RELEASE,
    SET_POSITION_CLOSE_PROGRESS
};

class HCIEmulator {
public:
    typedef std::function<void(const SHCIState&)> callback_function_t;

    HCIEmulator(Stream * port);

    void poll();

    void openDoor();
    void openDoorHalf();
    void closeDoor();
    void stopDoor();
    void setPosition(uint8_t);
    void toggleLamp();
    void ventilationPosition();

    const SHCIState& getState() {   
        if(micros()-m_recvTime > 2000000){
            // 2 sec without statusmessage 
            m_state.valid = false;
        }
        return m_state;
    };
    
    unsigned long getMessageAge(){
        return micros()-m_recvTime;
    }

    int getLogLevel();
    void setLogLevel(int level);

    void onStatusChanged(callback_function_t handler);
    StateMachine getStatemachine();

protected:
    void processFrame();
    void processDeviceStatusFrame();
    void processDeviceBusScanFrame();
    void processBroadcastStatusFrame();

private:
    callback_function_t m_statusCallback;    
    Stream *m_port;
    SHCIState m_state;
    StateMachine m_statemachine;
    StateMachine m_statemachine_backlog;

    unsigned long m_recvTime;
    unsigned long m_lastStateTime;
    unsigned long m_lastSendTime;

    size_t m_rxlen;
    size_t m_txlen;

    unsigned char m_rxbuffer[255];
    unsigned char m_txbuffer[255];

    bool m_skipFrame;
};


#endif