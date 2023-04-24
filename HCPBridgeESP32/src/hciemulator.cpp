#include "hciemulator.h"
#define CHECKCHANGEDSET(Target,Value,Flag) if((Target)!=(Value)){Target=Value;Flag=true;}
int hciloglevel = DEFAULTLOGLEVEL;

#ifdef SOFTSERIAL
#define Log(Level,Message) LogCore(Level,Message)
#define Log3(Level,Message,Buffer, Len) LogCore(Level,Message,Buffer,Len)
//LOGLEVEL
void LogCore(int Level, const char* msg, const unsigned char * data=NULL, size_t datalen=0){
    if(Level>hciloglevel){
        return;
    }
    if(data!=NULL && datalen>0){ 
        String newmsg(msg);
        char str[4];
        for (size_t i = 0; i < datalen; i++){
            snprintf(str,sizeof(str),"%02x ", data[i]);
            newmsg+=str;
        }
        Serial.println(newmsg);        
    }else{
        Serial.println(msg);
    }
}
#else 
    #define Log(Level,Message) 
    #define Log3(Level,Message,Buffer, Len) 
#endif


int HCIEmulator::getLogLevel(){
    return hciloglevel;
    
}
void HCIEmulator::setLogLevel(int level){
    hciloglevel=level;
}

//modbus crc calculation borrowed from:
//https://github.com/yaacov/ArduinoModbusSlave
#define MODBUS_CRC_LENGTH 2
#define readCRC(arr, length) word(arr[(length - MODBUS_CRC_LENGTH) + 1], arr[length - MODBUS_CRC_LENGTH])
#define readUInt16(arr, index) word(arr[index], arr[index + 1])
/**
 * Calculate the CRC of the passed byte array from zero up to the passed length.
 *
 * @param buffer The byte array containing the data.
 * @param length The length of the byte array.
 *
 * @return The calculated CRC as an unsigned 16 bit integer.
 * 
 * Calculate and add the CRC.
 *       uint16_t crc = Modbus::calculateCRC(_responseBuffer, _responseBufferLength - MODBUS_CRC_LENGTH);
 *       _responseBuffer[_responseBufferLength - MODBUS_CRC_LENGTH] = crc & 0xFF;
 *       _responseBuffer[(_responseBufferLength - MODBUS_CRC_LENGTH) + 1] = crc >> 8;
 * 
 * 
 * #define MODBUS_FRAME_SIZE 4
 * #define MODBUS_CRC_LENGTH 2
 * uint16_t crc = readCRC(_requestBuffer, _requestBufferLength);
 * #define readUInt16(arr, index) word(arr[index], arr[index + 1])
 * #define readCRC(arr, length) word(arr[(length - MODBUS_CRC_LENGTH) + 1], arr[length - MODBUS_CRC_LENGTH])
 */
uint16_t calculateCRC(uint8_t *buffer, int length)
{
    int i, j;
    uint16_t crc = 0xFFFF;
    uint16_t tmp;

    // Calculate the CRC.
    for (i = 0; i < length; i++)
    {
        crc = crc ^ buffer[i];
        for (j = 0; j < 8; j++)
        {
            tmp = crc & 0x0001;
            crc = crc >> 1;
            if (tmp)
            {
                crc = crc ^ 0xA001;
            }
        }
    }
    return crc;
}

HCIEmulator::HCIEmulator(Stream * port) { 
    m_state.valid = false; 
    m_statemachine=WAITING;
    m_rxlen = m_txlen = 0;
    m_recvTime=m_lastStateTime=0;
    m_skipFrame=false;
    m_port = port;
    m_statusCallback = NULL;
    setLogLevel(DEFAULTLOGLEVEL);
};

void HCIEmulator::poll(){
    
    if(m_port==NULL) return;

    // receive Data
    if(m_port->available() >0)
    {
        m_rxlen+= m_port->readBytes((char*)(m_rxbuffer+m_rxlen), _min((int)(255-m_rxlen),m_port->available()));
        if(m_rxlen > 254)
        {
            Log(LL_ERROR,"RX Bufferoverflow, skip next Frame");   
            Log3(LL_DEBUG,"Buffer Data: ", m_rxbuffer, m_rxlen);
            m_rxlen=0;               
            m_skipFrame = true;
        }
        m_recvTime = micros();             
    }        
    

    // check frame, process frame
    if(m_rxlen>0 && (micros()-m_recvTime > T3_5)) 
    {
        // check last action timeout -> reset > then 2sec
        if( ((m_statemachine != WAITING) && (m_statemachine != SET_POSITION_OPEN_PROGRESS) && (m_statemachine != SET_POSITION_CLOSE_PROGRESS)) && m_lastStateTime+2000<millis()){
            m_statemachine = WAITING;
        }

        if(!m_skipFrame){

            processFrame();

            // send response
            if(m_txlen > 0){
                
                // fix crc
                uint16_t crc = calculateCRC(m_txbuffer, m_txlen - MODBUS_CRC_LENGTH);
                m_txbuffer[m_txlen - MODBUS_CRC_LENGTH] = crc & 0xFF;
                m_txbuffer[(m_txlen - MODBUS_CRC_LENGTH) + 1] = crc >> 8;                

                // send data
                m_lastSendTime = micros()-m_recvTime;

                //Log(LL_DEBUG, ("ST:"+String(m_lastSendTime)).c_str());
                
                m_port->write(m_txbuffer, m_txlen);
                Log3(LL_DEBUG,"Response: ", m_txbuffer, m_txlen);
                m_txlen = 0;
            }                
        }

        m_skipFrame = false;
        m_rxlen=0;            
    }    
}

void HCIEmulator::processFrame(){
    m_txlen = 0;    // clear send buffer

    if(m_rxlen<5) {
        Log(LL_ERROR,"Frame skipped, invalid frame len");      
        Log3(LL_ERROR,"Data:", m_rxbuffer,m_rxlen);
        return;
    }

    // check device id, pass only device id 2 and 0 (broadcast)
    if(m_rxbuffer[0] != BROADCASTID && m_rxbuffer[0] != DEVICEID){      
        Log(LL_DEBUG,"Frame skipped, unsupported device id");      
        Log3(LL_DEBUG,"Data:", m_rxbuffer,m_rxlen);
        return;
    }

    // check crc
    uint16_t crc = readCRC(m_rxbuffer, m_rxlen);        
    if(crc != calculateCRC(m_rxbuffer,m_rxlen-MODBUS_CRC_LENGTH)){
        Log3(LL_ERROR,"Frame skipped, wrong crc", m_rxbuffer,m_rxlen);
        return;
    }

    Log3(LL_DEBUG,"Incomming Data: ", m_rxbuffer, m_rxlen);

    // dispatch modbus function
    switch(m_rxbuffer[1]){ 
        case 0x10:{  //  Write Multiple registers                            
            if(m_rxlen == 0x1b && m_rxbuffer[0] == BROADCASTID)
            {
                processBroadcastStatusFrame();
                return;
            }  
            break;
        }
        
        case 0x17:{ // Read/Write Multiple registers
            if(m_rxbuffer[0] == DEVICEID){
                switch(m_rxlen){
                    case 0x11:{
                        processDeviceStatusFrame();
                        return;
                    }

                    case 0x13:
                        processDeviceBusScanFrame();
                        return;;
                }
            }
            break;                               
        }
    }
    Log3(LL_ERROR,"Frame skipped, unexpected data: ", m_rxbuffer, m_rxlen);
}

const unsigned char ResponseTemplate_Fcn17_Cmd03_L08 []= {0x02,0x17,0x10,0x3E,0x00,0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x74,0x1B};
const unsigned char ResponseTemplate_Fcn17_Cmd04_L02 []= {0x02,0x17,0x04,0x0F,0x00,0x04,0xFD,0x0A,0x72};
void HCIEmulator::processDeviceStatusFrame(){
    if(m_rxlen==0x11){  
        unsigned char counter = m_rxbuffer[11];
        unsigned char cmd = m_rxbuffer[12];
        if(m_rxbuffer[5] == 0x08){
                // expose internal state
            //      00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
            //0011: 02 17 9C B9 00 08 9C 41 00 02 04 3E 03 00 00 EB CC
            //res=> 02 17 10 3E 00 03 01 00 00 00 00 00 00 00 00 00 00 00 00 74 1B                 
            memcpy(m_txbuffer, ResponseTemplate_Fcn17_Cmd03_L08, sizeof(ResponseTemplate_Fcn17_Cmd03_L08));  
            m_txbuffer[0] = m_rxbuffer[0];
            m_txbuffer[3] = counter;
            m_txbuffer[5] = cmd;
            m_txlen = sizeof(ResponseTemplate_Fcn17_Cmd03_L08);   

            // if no action on statemachine, process last backlog
            if (m_statemachine == WAITING && m_statemachine_backlog != WAITING){
                m_statemachine == m_statemachine_backlog;   // process backlog ...
                m_statemachine_backlog == WAITING;          // reset backlog
            }
            
            switch(m_statemachine)
            {
                // Open Door
                case OPEN_DOOR:
                    m_txbuffer[7]= 0x02;
                    m_txbuffer[8]= 0x10;
                    m_statemachine = OPEN_DOOR_RELEASE;
                    m_lastStateTime = millis();
                    break;
                case OPEN_DOOR_RELEASE:
                    if(m_lastStateTime+SIMULATEKEYPRESSDELAYMS<millis())
                    {
                        m_txbuffer[7]= 0x01;
                        m_txbuffer[8]= 0x10;
                        m_statemachine = WAITING; 
                    }
                    break;

                // Close Door
                case CLOSE_DOOR:
                    m_txbuffer[7]= 0x02;
                    m_txbuffer[8]= 0x20;
                    m_statemachine = CLOSE_DOOR_RELEASE;
                    m_lastStateTime = millis();
                    break;
                case CLOSE_DOOR_RELEASE:
                    if(m_lastStateTime+SIMULATEKEYPRESSDELAYMS<millis())
                    {
                        m_txbuffer[7]= 0x01;
                        m_txbuffer[8]= 0x20;
                        m_statemachine = WAITING; 
                    }
                    break;   

                // Stop Door
                // Not really a "stop", but a "stop/invert" command, as it moves the door in the opposite direction of previous movement if it is currently not moving.
                case STOP_DOOR:
                    
                    // Avoid to send a "stop" command if door is already open or closed. It would actually move the door... unsafe!
                    if ( (m_state.doorCurrentPosition == 0) || ((m_state.doorCurrentPosition == 100)) )
                    {
                        m_statemachine = WAITING;
                    }
                    else {
                        
                        m_txbuffer[7]= 0x02;
                        m_txbuffer[8]= 0x40;
                        m_statemachine = STOP_DOOR_RELEASE;
                        m_lastStateTime = millis();
                    }
                    break;
                case STOP_DOOR_RELEASE:
                    if(m_lastStateTime+SIMULATEKEYPRESSDELAYMS<millis())
                    {
                        m_txbuffer[7]= 0x01;
                        m_txbuffer[8]= 0x40;
                        m_statemachine = WAITING; 
                    }
                    break;

                // Set Position
                case SET_POSITION_OPEN:
                    m_txbuffer[7]= 0x02;
                    m_txbuffer[8]= 0x10;
                    m_statemachine = SET_POSITION_OPEN_RELEASE;
                    m_lastStateTime = millis();
                    break;
                case SET_POSITION_OPEN_RELEASE:
                    if(m_lastStateTime+SIMULATEKEYPRESSDELAYMS<millis())
                    {
                        m_txbuffer[7]= 0x01;
                        m_txbuffer[8]= 0x10;
                        m_statemachine = SET_POSITION_OPEN_PROGRESS; 
                    }
                    break;
                case SET_POSITION_OPEN_PROGRESS:
                    if(m_state.doorCurrentPosition >= m_state.gotoPosition)
                    {
                        m_lastStateTime = millis();
                        m_statemachine = STOP_DOOR; 
                    }
                    break;
                case SET_POSITION_CLOSE:
                    m_txbuffer[7]= 0x02;
                    m_txbuffer[8]= 0x20;
                    m_statemachine = SET_POSITION_CLOSE_RELEASE;
                    m_lastStateTime = millis();
                    break;
                case SET_POSITION_CLOSE_RELEASE:
                    if(m_lastStateTime+SIMULATEKEYPRESSDELAYMS<millis())
                    {
                        m_txbuffer[7]= 0x01;
                        m_txbuffer[8]= 0x20;
                        m_statemachine = SET_POSITION_CLOSE_PROGRESS; 
                    }
                    break; 
                case SET_POSITION_CLOSE_PROGRESS:
                    if(m_state.doorCurrentPosition <= m_state.gotoPosition)
                    {
                        m_lastStateTime = millis();
                        m_statemachine = STOP_DOOR; 
                    }
                    break;

                // Ventilation
                case VENTPOSITION:
                    m_txbuffer[7]= 0x02;
                    m_txbuffer[9]= 0x40;
                    m_statemachine = VENTPOSITION_RELEASE;
                    m_lastStateTime = millis();
                    break;
                case VENTPOSITION_RELEASE:
                    if(m_lastStateTime+SIMULATEKEYPRESSDELAYMS<millis())
                    {
                        m_txbuffer[7]= 0x01;
                        m_txbuffer[9]= 0x40;
                        m_statemachine = WAITING; 
                    }
                    break;        


                // Half Position
                case OPEN_DOOR_HALF:
                    m_txbuffer[7]= 0x02;
                    m_txbuffer[9]= 0x04;
                    m_statemachine = OPEN_DOOR_HALF_RELEASE;
                    m_lastStateTime = millis();
                    break;

                case OPEN_DOOR_HALF_RELEASE:
                    if(m_lastStateTime+SIMULATEKEYPRESSDELAYMS<millis())
                    {
                        m_txbuffer[7]= 0x01;
                        m_txbuffer[9]= 0x04;
                        m_statemachine = WAITING; 
                    }
                    break;                                          

                // Toggle Lamp
                case TOGGLE_LAMP:
                    m_txbuffer[7]= 0x10;
                    m_txbuffer[9]= 0x02;
                    m_statemachine = TOGGLE_LAMP_RELEASE;
                    m_lastStateTime = millis();
                    break;
                case TOGGLE_LAMP_RELEASE:
                    if(m_lastStateTime+SIMULATEKEYPRESSDELAYMS<millis())
                    {
                        m_txbuffer[7]= 0x08;
                        m_txbuffer[9]= 0x02;
                        m_statemachine = WAITING; 
                    }
                    break;  
                    
                case WAITING:
                    break;            
            }                                 
            return;             
        }
        else if(m_rxbuffer[5] == 0x02){
            //      00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
            //0011: 02 17 9C B9 00 02 9C 41 00 02 04 0F 04 17 00 7B 21
            //res=> 02 17 04 0F 00 04 FD 0A 72     
            memcpy(m_txbuffer, ResponseTemplate_Fcn17_Cmd04_L02, sizeof(ResponseTemplate_Fcn17_Cmd04_L02));  
            m_txbuffer[0] = m_rxbuffer[0];
            m_txbuffer[3] = counter;
            m_txbuffer[5] = cmd;
            m_txlen = sizeof(ResponseTemplate_Fcn17_Cmd04_L02);   
            return;
        }
    }

    Log3(LL_ERROR,"Frame skipped, unexpected data: ", m_rxbuffer, m_rxlen); 
}

const unsigned char ResponseTemplate_Fcn17_Cmd02_L05 []= {0x02,0x17,0x0a,0x00,0x00,0x02,0x05,0x04,0x30,0x10,0xff,0xa8,0x45,0x0e,0xdf};
void HCIEmulator::processDeviceBusScanFrame(){
    //      00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
    //0013: 02 17 9C B9 00 05 9C 41 00 03 06 00 02 00 00 01 02 f8 35
    //res=> 02 17 0a 00 00 02 05 04 30 10 ff a8 45 0e df
    unsigned char counter = m_rxbuffer[11];
    unsigned char cmd = m_rxbuffer[12];
    memcpy(m_txbuffer, ResponseTemplate_Fcn17_Cmd02_L05, sizeof(ResponseTemplate_Fcn17_Cmd02_L05));  
    m_txbuffer[0] = m_rxbuffer[0];
    m_txbuffer[3] = counter;
    m_txbuffer[5] = cmd;
    m_txlen = sizeof(ResponseTemplate_Fcn17_Cmd02_L05); 

    Log(LL_INFO,"Busscan received");  
}

void HCIEmulator::processBroadcastStatusFrame(){
    //001B: 00 10 9D 31 00 09 12 64 00 00 00 40 60 00 00 00 00 00 00 00 00 00 01 00 00 CA 22
    bool hasChanged = false;
    CHECKCHANGEDSET(m_state.lampOn,m_rxbuffer[20] == 0x10,hasChanged);      // Light starting
    if(!m_state.lampOn){CHECKCHANGEDSET(m_state.lampOn,m_rxbuffer[20] == 0x14,hasChanged);}     // Light on
    CHECKCHANGEDSET(m_state.doorCurrentPosition,m_rxbuffer[10],hasChanged);
    CHECKCHANGEDSET(m_state.doorTargetPosition, m_rxbuffer[9],hasChanged);
    //hack to get vent Status as my Supramatic did report state 0x00 instead of 0x0A when is in venting state.
    if(m_rxbuffer[11] == 0x00 && m_state.doorTargetPosition == VENT_POS && m_state.doorTargetPosition == m_state.doorCurrentPosition){
        m_rxbuffer[11] = DOOR_VENT_POSITION;
    }
    CHECKCHANGEDSET(m_state.doorState, m_rxbuffer[11],hasChanged);
    CHECKCHANGEDSET(m_state.reserved, m_rxbuffer[17],hasChanged);
    CHECKCHANGEDSET(m_state.valid, true,hasChanged);   
    
    if(hasChanged){
        Log3(LL_INFO,"New State: ",m_rxbuffer,m_rxlen);
        if(m_statusCallback != NULL){
            m_statusCallback(m_state);
        }  
    }             
}    

void HCIEmulator::openDoor(){
    if(m_statemachine != WAITING){
        m_statemachine_backlog = OPEN_DOOR;
        return;
    }
    m_lastStateTime = millis();
    m_statemachine = OPEN_DOOR;
}

void HCIEmulator::openDoorHalf(){
    if(m_statemachine != WAITING){
        m_statemachine_backlog = OPEN_DOOR_HALF;
        return;
    }
    m_lastStateTime = millis();
    m_statemachine = OPEN_DOOR_HALF;
}

void HCIEmulator::closeDoor(){
    if(m_statemachine != WAITING){
        m_statemachine_backlog = CLOSE_DOOR;
        return;
    }
    m_lastStateTime = millis();    
    m_statemachine = CLOSE_DOOR;
}

void HCIEmulator::setPosition(uint8_t position){
    if(m_statemachine == WAITING)
    {
        // First and last movement segments seem a bit inconsistent on Promatic4, so it's better to leave it to fully open or close.
        if (position <= 5) closeDoor();
        else if (position >= 95) openDoor();  
            
        else if ((position > 5) && (position < 95)) {
            m_state.gotoPosition = position * 2; // HA is 0-100%, Horman/Modbus range is 0-200
            m_lastStateTime = millis();

            if (m_state.gotoPosition > m_state.doorCurrentPosition)
                m_statemachine = SET_POSITION_OPEN;
            else if (m_state.gotoPosition < m_state.doorCurrentPosition)
                m_statemachine = SET_POSITION_CLOSE;
            else // already there
                return;
        }
    }
}

void HCIEmulator::stopDoor(){
    if((m_statemachine == WAITING) || (m_statemachine == SET_POSITION_OPEN_PROGRESS) || (m_statemachine == SET_POSITION_CLOSE_PROGRESS))
    { 
        m_lastStateTime = millis();    
        m_statemachine = STOP_DOOR;
    }else{
        m_statemachine_backlog = STOP_DOOR;
    }
}

void HCIEmulator::toggleLamp(){
    if(m_statemachine != WAITING){
        m_statemachine_backlog = TOGGLE_LAMP;
        return;
    }
    m_lastStateTime = millis();
    m_statemachine = TOGGLE_LAMP;
}

void HCIEmulator::ventilationPosition(){
    if(m_statemachine != WAITING){
        m_statemachine_backlog = VENTPOSITION;
        return;
    }
    m_lastStateTime = millis();
    m_statemachine = VENTPOSITION;
}

void HCIEmulator::onStatusChanged(callback_function_t handler) {
    m_statusCallback = handler;
}

StateMachine HCIEmulator::getStatemachine(){
    return m_statemachine;
}