#ifndef HOERMANN_H_
    #define HOERMANN_H_
        #define MODBUSRTU_DEBUG 1

        #include <Arduino.h>
        #include <Stream.h>

        #include "ArduinoJson.h"
        #include "ModbusRTU.h"

        #define SLAVE_ID 2
        #define SIMULATEKEYPRESSDELAYMS 100
        #define DEADREPORTTIMEOUT 60000

        #define RS485 Serial2
        #ifdef CONFIG_IDF_TARGET_ESP32S3
            #define PIN_TXD 17
            #define PIN_RXD 18
        #else
            #define PIN_TXD 17 // UART 2 TXT - G17
            #define PIN_RXD 16 // UART 2 RXD - G16
        #endif

        // workaround as my Supramatic did not Report the Status 0x0A when it's en vent Position
        // When the door is at position 0x08 and not moving Status get changed to Ventig.
        #define VENT_POS 0x08

        static const char *TAG_HCI = "HCI-BUS";

        TaskHandle_t modBusTask;
        void modbusServeTask(void *parameter);

        class HoermannCommand {
        public:
            static const HoermannCommand STARTOPENDOOR;
            static const HoermannCommand STARTCLOSEDOOR;
            static const HoermannCommand STARTSTOPDOOR;
            static const HoermannCommand STARTOPENDOORHALF;
            static const HoermannCommand STARTVENTPOSITION;
            static const HoermannCommand STARTTOGGLELAMP;
            static const HoermannCommand WAITING;

        public:
            uint16_t commandRegPlus2Value;
            uint16_t commandEndPlus2Value;
            uint16_t commandRegPlus3Value;
            uint16_t commandEndPlus3Value;

        private:
            HoermannCommand(
                uint16_t commandRegPlus2Value,
                uint16_t commandEndPlus2Value,
                uint16_t commandRegPlus3Value,
                uint16_t commandEndPlus3Value) {
                this->commandRegPlus2Value = commandRegPlus2Value;
                this->commandEndPlus2Value = commandEndPlus2Value;
                this->commandRegPlus3Value = commandRegPlus3Value;
                this->commandEndPlus3Value = commandEndPlus3Value;
            }
        };

        // arg1,arg4> command start value
        // arg2,arg5> command end   value
        const HoermannCommand HoermannCommand::STARTOPENDOOR = HoermannCommand(0x0210, 0x0110, 0x0000, 0x0000);  // Typo 0201
        const HoermannCommand HoermannCommand::STARTCLOSEDOOR = HoermannCommand(0x0220, 0x0120, 0x0000, 0x0000);
        const HoermannCommand HoermannCommand::STARTSTOPDOOR = HoermannCommand(0x0240, 0x0140, 0x0000, 0x0000);
        const HoermannCommand HoermannCommand::STARTOPENDOORHALF = HoermannCommand(0x0200, 0x0100, 0x0400, 0x0400);
        const HoermannCommand HoermannCommand::STARTVENTPOSITION = HoermannCommand(0x0200, 0x0100, 0x4000, 0x4000);
        const HoermannCommand HoermannCommand::STARTTOGGLELAMP = HoermannCommand(0x0100, 0x0800, 0x0200, 0x0200);
        const HoermannCommand HoermannCommand::WAITING = HoermannCommand(0x0000, 0x0000, 0x0000, 0x0000);

        class HoermannState {
        public:
            enum State {
                OPEN,
                OPENING,
                CLOSED,
                CLOSING,
                HALFOPEN,
                MOVE_VENTING,
                VENT,
                MOVE_HALF,
                STOPPED
            };
            float targetPosition = 0;
            float currentPosition = 0;
            bool lightOn = false;
            State state = CLOSED;
            String translatedState = "closed";
            String coverState = "closed";
            String debugMessage = "initial";
            unsigned long lastModbusRespone = 0;
            bool changed = false;
            bool debMessage = false;
            float gotoPosition = 0.0f;
            bool valid = false;

            void setTargetPosition(float targetPosition) {
                this->targetPosition = targetPosition;
                this->changed = true;
            }
            void setGotoPosition(float setPosition) {
                this->gotoPosition = setPosition;
                this->changed = true;
            }
            void setCurrentPosition(float currentPosition) {
                this->currentPosition = currentPosition;
                this->changed = true;
            }
            void setLigthOn(bool lightOn) {
                this->lightOn = lightOn;
                this->changed = true;
            }
            void recordModbusResponse() {
                this->lastModbusRespone = millis();
            }
            void clearChanged() {
                this->changed = false;
            }
            void clearDebug() {
                this->debMessage = false;
                this->debugMessage = "Initial";
            }
            long responseAge() {
                if (this->lastModbusRespone == 0) {
                    return -1;
                }
                long diff = millis() - lastModbusRespone;
                if (diff < 0) {
                    return -2;
                }
                return diff/1000;
            }
            void setState(State state) {
                this->state = state;
                this->translatedState = translateState(state);
                this->coverState = translateCoverState(state);
                this->changed = true;
            }
            void setValid(bool isValid) {
                this->valid = isValid;
            }

            //Function not used not clear if it make sense to use.
            String isValid() {
                // is valid if age between 0 and 2 second
                if ((this->responseAge() > 0 && this->responseAge() < 2)) {
                    return "true";
                } else {
                    return "false";
                }
            }

            String toStatusJson() {
                DynamicJsonDocument root(1024);
                root["valid"] = this->isValid();
                root["targetPosition"] = (int)(this->targetPosition*100);
                root["currentPosition"] = (int)(this->currentPosition*100);
                root["light"] = lightOn;
                root["state"] = translateState(this->state);
                root["busResponseAge"] = this->responseAge();

                String output;
                serializeJson(root, output);
                return output;
            }

        private:
            static String translateState(State stateCode) {
                switch (stateCode) {
                    case State::OPENING:
                        return "opening";
                    case State::CLOSING:
                        return "closing";
                    case State::OPEN:
                        return "open";
                    case State::CLOSED:
                        return "closed";
                    case State::HALFOPEN:
                        return "open h";
                    case State::MOVE_VENTING:
                        return "opening v";
                    case State::MOVE_HALF:
                        return "opening h";
                    case State::VENT:
                        return "venting";
                    default:
                        return "stopped";
                }
            }
            String translateCoverState(State stateCode) {
                switch (stateCode) {
                    case State::OPENING:
                        return "opening";
                    case State::CLOSING:
                        return "closing";
                    case State::OPEN:
                        return "open";
                    case State::CLOSED:
                        return "closed";
                    case State::HALFOPEN:
                        return "open";
                    case State::MOVE_VENTING:
                        return "opening";
                    case State::MOVE_HALF:
                        return "opening";
                    case State::VENT:
                        return "open";
                    default:
                        if(this->currentPosition == this->targetPosition && this->currentPosition == 0.0f) {
                            return "closed";
                        }else if (this->currentPosition == this->targetPosition && this->currentPosition > 0.0f)
                        {
                            return "open";
                        }else{
                            return "stopped";
                        }
                }
            }
        };

        class HoermannGarageEngine {
        public:
            HoermannState *state = new HoermannState();
            HoermannGarageEngine(){};

            void setup() {
                RS485.begin(57600, SERIAL_8E1, PIN_RXD, PIN_TXD);
                mb.begin(&RS485);
                mb.slave(SLAVE_ID);

                xTaskCreatePinnedToCore(
                    modbusServeTask, /* Function to implement the task */
                    "ModBusTask",    /* Name of the task */
                    10000,           /* Stack size in words */
                    NULL,            /* Task input parameter */
                    // 1,  /* Priority of the task */
                    configMAX_PRIORITIES - 1,
                    &modBusTask, /* Task handle. */
                    1);          /* Core where the task should run */

                // Required for Write
                mb.addHreg(0x9C41, 0, 0x03);  // Commands
                mb.addHreg(0x9D31, 0, 0x09);  // Broadcast
                // For Read
                mb.addHreg(0x9CB9, 0, 0x08);  // Internal State (e.g. Button Pressed)

                // on Request - Clear/Calc Response data
                mb.onRequest([this](Modbus::FunctionCode fc, const Modbus::RequestData data) -> Modbus::ResultCode { return this->onRequest(fc, data); });

                // On Write (update counter in Read Registers)
                mb.onSet(
                    HREG(0x9C41 + 0), [this](TRegister *reg, uint16_t val) -> uint16_t { return this->onCounterWrite(reg, val); },
                    0x01);
                // Broadcast Changes
                mb.onSet(
                    HREG(0x9D31 + 2), [this](TRegister *reg, uint16_t val) -> uint16_t { return this->onCurrentStateChanged(reg, val); },
                    0x01);
                mb.onSet(
                    HREG(0x9D31 + 1), [this](TRegister *reg, uint16_t val) -> uint16_t { return this->onDoorPositonChanged(reg, val); },
                    0x01);
                mb.onSet(
                    HREG(0x9D31 + 6), [this](TRegister *reg, uint16_t val) -> uint16_t { return this->onLampState(reg, val); },
                    0x01);
            }

            void handleModbus() {
                mb.task();
            }

            /**
             * Initialize Response Registers per Request
             */
            Modbus::ResultCode onRequest(Modbus::FunctionCode fc, const Modbus::RequestData data) {
                this->state->recordModbusResponse();

                // Command Requst (Internal State representation)
                if (fc == Modbus::FC_READWRITE_REGS && data.regWrite.address == 0x9C41 && data.regWriteCount == 0x02 && data.regRead.address == 0x9CB9 && data.regReadCount == 0x08) {
                    mb.Reg(HREG(0x9CB9 + 0), (uint16_t)0x0000);
                    mb.Reg(HREG(0x9CB9 + 1), (uint16_t)0x0001);
                    setCommandValuesToRead();
                    // mb.Reg(HREG(0x9CB9+2),(uint16_t)0x0000);
                    // mb.Reg(HREG(0x9CB9+3),(uint16_t)0x0000);
                    mb.Reg(HREG(0x9CB9 + 4), (uint16_t)0x0000);
                    mb.Reg(HREG(0x9CB9 + 5), (uint16_t)0x0000);
                    mb.Reg(HREG(0x9CB9 + 6), (uint16_t)0x0000);
                    mb.Reg(HREG(0x9CB9 + 7), (uint16_t)0x0000);
                }
                // Empty Command Requst
                else if (fc == Modbus::FC_READWRITE_REGS && data.regWrite.address == 0x9C41 && data.regWriteCount == 0x02 && data.regRead.address == 0x9CB9 && data.regReadCount == 0x02) {
                    mb.Reg(HREG(0x9CB9 + 0), (uint16_t)0x0004);
                    mb.Reg(HREG(0x9CB9 + 1), (uint16_t)0x0000);
                    ESP_LOGD(TAG_HCI, "executing empty command");
                }
                // BusScan
                else if (fc == Modbus::FC_READWRITE_REGS && data.regWrite.address == 0x9C41 && data.regWriteCount == 0x03 && data.regRead.address == 0x9CB9 && data.regReadCount == 0x05) {
                    ESP_LOGD(TAG_HCI, "executing busscan");
                    mb.Reg(HREG(0x9CB9 + 0), (uint16_t)0x0000);
                    mb.Reg(HREG(0x9CB9 + 1), (uint16_t)0x0005);
                    mb.Reg(HREG(0x9CB9 + 2), (uint16_t)0x0430);
                    mb.Reg(HREG(0x9CB9 + 3), (uint16_t)0x10ff);
                    mb.Reg(HREG(0x9CB9 + 4), (uint16_t)0xa845);
                } else if (fc == Modbus::FC_WRITE_REGS && data.reg.address == 0x9D31) {
                    // ESP_LOGD("ON_REQ", "on Status Update (cnt: %d)",data.regCount);
                } else {
                    this->state->debugMessage = "unknown function code fc=" + fc;
                    this->state->debMessage = true;
                    ESP_LOGW(TAG_HCI, "unknown function code fc=%x", fc);
                }
                this->state->setValid(true);
                return Modbus::EX_SUCCESS;
            }

            /**
             * Set the values of the next command to be read from registers
             */
            void setCommandValuesToRead() {
                uint16_t regPlug2Value = 0x0000;
                uint16_t regPlug3Value = 0x0000;

                // Command was set
                if (nextCommand != nullptr) {
                    // But not yet sent
                    if (commandWrittenOn == 0) {
                        // Send it
                        regPlug2Value = nextCommand->commandRegPlus2Value;
                        regPlug3Value = nextCommand->commandRegPlus3Value;
                        ESP_LOGI(TAG_HCI, "command start %x %x", regPlug2Value, regPlug3Value);
                        commandWrittenOn = millis();
                        // It was written and it can be cleared
                    } else if (commandWrittenOn != 0 && (commandWrittenOn + SIMULATEKEYPRESSDELAYMS) < millis()) {
                        regPlug2Value = nextCommand->commandEndPlus2Value;
                        regPlug3Value = nextCommand->commandEndPlus3Value;
                        ESP_LOGI(TAG_HCI, "command dispose %x %x", regPlug2Value, regPlug3Value);
                        // Reset Variables
                        commandWrittenOn = 0;
                        nextCommand = nullptr;
                    }
                }
                mb.Reg(HREG(0x9CB9 + 2), regPlug2Value);
                mb.Reg(HREG(0x9CB9 + 3), regPlug3Value);
            }

            /**
             * Write on 0x9D31+1 , byte1: target, byte2: current
             */
            uint16_t onDoorPositonChanged(TRegister *reg, uint16_t val) {
                // on First Byte changed (current)
                if ((reg->value & 0x00FF) != (val & 0x00FF)) {
                    this->state->setCurrentPosition((float)(val & 0x00FF)/200.0f);
                    if ((this->state->gotoPosition > 0.0f && this->state->state == HoermannState::State::CLOSING && this->state->gotoPosition >= this->state->currentPosition) ||
                        (this->state->gotoPosition > 0.0f && this->state->state == HoermannState::State::OPENING && this->state->gotoPosition <= this->state->currentPosition)) {
                        this->stopDoor();
                        this->state->setGotoPosition(0.0f);
                    }
                }
                // on Second Byte changed (target)
                if ((reg->value & 0xFF00) != (val & 0xFF00)) {
                    this->state->setTargetPosition((float)((val & 0xFF00) >> 8)/200.0f);
                }

                return val;
            }
            /**
             * Write on 0x9D31+2 , byte1: current state
             */
            uint16_t onCurrentStateChanged(TRegister *reg, uint16_t val) {
                // on First Byte changed
                if (((reg->value & 0xFF00) != (val & 0xFF00))) {
                    ESP_LOGI(TAG_HCI, "onCurrentStateChanged. address=%x, value=%x (actual: %x)", reg->address.address, val, (val & 0xFF00) >> 8);

                    switch ((val & 0xFF00) >> 8) {
                        case 0x1:
                            this->state->setState(HoermannState::State::OPENING);
                            break;
                        case 0x2:
                            this->state->setState(HoermannState::State::CLOSING);
                            break;
                        case 0x20:
                            this->state->setState(HoermannState::State::OPEN);
                            break;
                        case 0x40:
                            this->state->setState(HoermannState::State::CLOSED);
                            break;
                        case 0x80:
                            this->state->setState(HoermannState::State::HALFOPEN);
                            break;
                        case 0x09:
                            this->state->setState(HoermannState::State::MOVE_VENTING);
                            break;
                        case 0x05:
                            this->state->setState(HoermannState::State::MOVE_HALF);
                            break;
                        case 0x0A:
                            this->state->setState(HoermannState::State::VENT);
                            break;
                        case 0x00:
                            if (this->state->currentPosition == this->state->targetPosition && (int)(this->state->currentPosition*200) == VENT_POS) {
                                this->state->setState(HoermannState::State::VENT);
                            } else {
                                this->state->setState(HoermannState::State::STOPPED);
                            }
                            break;
                        default:
                            ESP_LOGW(TAG_HCI, "unknown State %x", (val & 0xFF00) >> 8);
                    }
                }
                return val;
            }

            /**
             * Write on 0x9D31+6 , byte2: Lamp State
             */
            uint16_t onLampState(TRegister *reg, uint16_t val) {
                // On second byte changed
                if ((reg->value & 0x00FF) != (val & 0x00FF)) {
                    ESP_LOGI(TAG_HCI, "onLampState. address=%x, value=%x", reg->address.address, val);
                    // 14 .. from docs (a indicator for automatic state maby?)
                    // 10 .. on after turn on
                    // 04 .. shut down after inactivy
                    // 00 .. off after turn off
                    this->state->setLigthOn((val & 0x00FF) == 0x14 || (val & 0x00FF) == 0x10);
                }
                return val;
            }

            /**
             * Write on 0x9C41 , byte1: counter, byte2: command
             */
            uint16_t onCounterWrite(TRegister *reg, uint16_t val) {
                uint16_t counter = val & 0xFF00;
                uint16_t command = (val & 0x00FF) << 8;
                mb.Reg(HREG(0x9CB9 + 0), mb.Reg(HREG(0x9CB9 + 0)) | counter);
                mb.Reg(HREG(0x9CB9 + 1), mb.Reg(HREG(0x9CB9 + 1)) | command);
                return val;
            }

            /**
             * Helper to set next Command and *not* skip Current Command before end was sent
             */
            void setCommand(bool cond, const HoermannCommand *command) {
                if (cond) {
                    if (nextCommand != nullptr) {
                        ESP_LOGW(TAG_HCI, "Last Command was not yet fetched by modbus!");
                    } else {
                        nextCommand = command;
                    }
                }
            }

            /**
             * Control Functions
             */
            void stopDoor() {
                setCommand(this->state->state == HoermannState::State::CLOSING || this->state->state == HoermannState::State::OPENING, &HoermannCommand::STARTSTOPDOOR);
            }
            void closeDoor() {
                setCommand(true, &HoermannCommand::STARTCLOSEDOOR);
            }
            void openDoor() {
                setCommand(true, &HoermannCommand::STARTOPENDOOR);
            }
            void toogleDoor() {
                setCommand(this->state->currentPosition < 1, &HoermannCommand::STARTOPENDOOR);
                setCommand(this->state->currentPosition >= 1, &HoermannCommand::STARTCLOSEDOOR);
            }
            void halfPositionDoor() {
                setCommand(true, &HoermannCommand::STARTOPENDOORHALF);
            }
            void ventilationPositionDoor() {
                setCommand(true, &HoermannCommand::STARTVENTPOSITION);
            }
            void turnLight(bool on) {
                setCommand((on && !this->state->lightOn) || (!on && this->state->lightOn), &HoermannCommand::STARTTOGGLELAMP);
            }
            void toogleLight() {
                setCommand(true, &HoermannCommand::STARTTOGGLELAMP);
            }
            void setPosition(int setPosition) {
                // First and last movement segments seem a bit inconsistent on Promatic4, so it's better to leave it to fully open or close.
                if (setPosition <= 5)
                    closeDoor();
                else if (setPosition >= 95)
                    openDoor();
                else if ((setPosition > 5) && (setPosition < 95)) {
                    this->state->setGotoPosition(static_cast<float>(setPosition)/100.0f);
                    setCommand(this->state->currentPosition < this->state->gotoPosition, &HoermannCommand::STARTOPENDOOR);
                    setCommand(this->state->currentPosition > this->state->gotoPosition, &HoermannCommand::STARTCLOSEDOOR);
                }
            }

        private:
            ModbusRTU mb;                                  // ModbusRTU instance, the man behind the curtain
            const HoermannCommand *nextCommand = nullptr;  // Next Command to transmit
            unsigned long commandWrittenOn = 0;            // When was last command written (wait 100ms before end of command is transmitted)
        };

        HoermannGarageEngine *hoermannEngine = new HoermannGarageEngine();

        void DelayHandler(void) {
            hoermannEngine->handleModbus();
        }

        void modbusServeTask(void *parameter) {
            while (true) {
                hoermannEngine->handleModbus();
                vTaskDelay(1);
            }
            vTaskDelete(NULL);
        }
#endif