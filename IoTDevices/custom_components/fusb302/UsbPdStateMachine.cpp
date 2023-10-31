#include "UsbPd.h"
#include "UsbPdStateMachine.h"


UsbPdStateMachine::UsbPdState UsbPdStateMachine::update() {
  if (state_ > kEnableTransceiver) {  // poll to detect COMP VBus low
    uint8_t compResult;
    if (!readComp(compResult)) {
      if (compResult == 0) {
        compLowTimer_.start();
      } else {
        compLowTimer_.reset();
        compLowTimer_.stop();
      }
    } else {
      ESP_LOGW("UsbPdStateMachine", "processInterrupt(): ICompChng readComp failed");
    }
    startStopDelay();
    if (compLowTimer_.read_ms() >= kCompLowResetTimeMs) {
      ESP_LOGW("UsbPdStateMachine", "update(): Comp low reset");
      reset();
    }
  }

  switch (state_) {
    case kStart:
    default:
      if (!init()) {
        measuringCcPin_ = -1;
        savedCcMeasureLevel_ = -1;
        state_ = kDetectCc;
        stateExpire_ = millis() + kMeasureTimeMs;
      } else {
        ESP_LOGW("UsbPdStateMachine", "update(): Start init failed");
      }
      break;
    case kDetectCc:
      if ((measuringCcPin_ == 1 || measuringCcPin_ == 2) && millis() >= stateExpire_) {  // measurerment ready
        uint8_t measureLevel;
        if (!readMeasure(measureLevel)) {
          if (savedCcMeasureLevel_ != -1 && measureLevel != savedCcMeasureLevel_) {
            // last measurement on other pin was valid, and one is higher
            if (measureLevel > savedCcMeasureLevel_) {  // this measurement higher, use this CC pin
              ccPin_ = measuringCcPin_;
            } else {  // other measurement higher, use other rCC pin
              ccPin_ = measuringCcPin_ == 1 ? 2 : 1;
            }
            state_ = kEnableTransceiver;
          } else {  // save this measurement and swap measurement pins
            uint8_t nextMeasureCcPin = measuringCcPin_ == 1 ? 2 : 1;
            if (!setMeasure(nextMeasureCcPin)) {
              compLowTimer_.reset();
              compLowTimer_.start();
              savedCcMeasureLevel_ = measureLevel;
              measuringCcPin_ = nextMeasureCcPin;
              stateExpire_ = millis() + kMeasureTimeMs;
            } else {
              ESP_LOGW("UsbPdStateMachine", "update(): DetectCc setMeasure failed");
            }
          }
        } else {
          ESP_LOGW("UsbPdStateMachine", "update(): DetectCc readMeasure failed");
        }
      } else if (measuringCcPin_ != 1 && measuringCcPin_ != 2) {  // state entry, invalid measurement pin
        if (!setMeasure(1)) {
          ESP_LOGW("UsbPdStateMachine", "update(): DetectCc setMeasure failed");
        }
        timer_.reset();
        measuringCcPin_ = 1; 
      }
      break;
    case kEnableTransceiver:
      if (!enablePdTrasceiver(ccPin_)) {
        state_ = kWaitSourceCapabilities;
        stateExpire_ = millis() + UsbPdTiming::tTypeCSendSourceCapMsMax;
      } else {
        ESP_LOGW("UsbPdStateMachine", "update(): EnableTransceiver enablePdTransceiver failed");
      }
      break;
    case kWaitSourceCapabilities:
      if (sourceCapabilitiesLen_ > 0) {
        state_ = kConnected;
      } else if (millis() >= stateExpire_) {
        state_ = kEnableTransceiver;
      }
      break;
    case kConnected:
      break;
  }

  return state_;
}

// void UsbPdStateMachine::processInterrupt() {
//   uint8_t intVal;
//   if (!fusb_.readRegister(Fusb302::Register::kInterrupt, intVal)) {
//     ESP_LOGW("UsbPdStateMachine", "processInterrupt(): readRegister(Interrupt) failed");
//     return;
//   }
//   startStopDelay();
//   if (intVal & Fusb302::kInterrupt::kICrcChk) {
//     processRxMessages();
//   }
// }

int UsbPdStateMachine::getCapabilities(UsbPd::Capability::Unpacked capabilities[]) {
  for (uint8_t i=0; i<sourceCapabilitiesLen_; i++) {
    capabilities[i] = UsbPd::Capability::unpack(sourceCapabilitiesObjects_[i]);
  }
  return sourceCapabilitiesLen_;
}

uint8_t UsbPdStateMachine::currentCapability() {
  return currentCapability_;
}

bool UsbPdStateMachine::requestCapability(uint8_t capability, uint16_t currentMa) {
  return sendRequestCapability(capability, currentMa);
}

void UsbPdStateMachine::reset() {
  state_ = kStart;
  ccPin_ = 0;
  nextMessageId_ = 0;

  sourceCapabilitiesLen_ = 0;

  requestedCapability_ = 0;
  currentCapability_ = 0;
  powerStable_ = false;
}

bool UsbPdStateMachine::init() {
  if (!fusb_.writeRegister(Fusb302::Register::kReset, 0x03)) {  // reset everything
    ESP_LOGW("UsbPdStateMachine", "init(): reset failed");
    return false;
  }
  startStopDelay();

  if (!fusb_.writeRegister(Fusb302::Register::kPower, 0x0f)) {  // power up everything
    ESP_LOGW("UsbPdStateMachine", "init(): power failed");
    return false;
  }
  startStopDelay();
  if (!fusb_.writeRegister(Fusb302::Register::kMeasure, 0x40 | (kCompVBusThresholdMv/42))) {  // MEAS_VBUS
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): Measure failed");
    return false;
  }
  startStopDelay();  

  return true;
}

bool UsbPdStateMachine::enablePdTrasceiver(int ccPin) {
  uint8_t switches0Val = 0x03;  // PDWN1/2
  uint8_t switches1Val = 0x24;  // Revision 2.0, auto-CRC
  if (ccPin == 1) {
    switches0Val |= 0x04;
    switches1Val |= 0x01;
  } else if (ccPin == 2) {
    switches0Val |= 0x08;
    switches1Val |= 0x02;
  } else {
    ESP_LOGE("UsbPdStateMachine", "invalid ccPin = %i", ccPin);
    return false;
  }

  if (!fusb_.writeRegister(Fusb302::Register::kSwitches0, switches0Val)) {  // PDWN1/2
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): switches0 failed");
    return false;
  }
  startStopDelay();
  if (!fusb_.writeRegister(Fusb302::Register::kSwitches1, switches1Val)) {
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): switches1 failed");
    return false;
  }
  startStopDelay();
  if (fusb_.writeRegister(Fusb302::Register::kControl3, 0x07)) {  // enable auto-retry
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): control3 failed");
    return false;
  }
  startStopDelay();
  if (!fusb_.writeRegister(Fusb302::Register::kMask, 0xef)) {  // mask interupts
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): mask failed");
    return false;
  }
  startStopDelay();
  if (!fusb_.writeRegister(Fusb302::Register::kMaska, 0xff)) {  // mask interupts
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): maska failed");
    return false;
  }
  startStopDelay();
  if (!fusb_.writeRegister(Fusb302::Register::kMaskb, 0x01)) {  // mask interupts
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): maskb failed");
    return false;
  }
  startStopDelay();
  if (!fusb_.writeRegister(Fusb302::Register::kControl0, 0x04)) {  // unmask global interrupt
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): control0 failed");
    return false;
  }
  startStopDelay();

  if (!fusb_.writeRegister(Fusb302::Register::kReset, 0x02)) {  // reset PD logic
    ESP_LOGW("UsbPdStateMachine", "enablePdTransceiver(): reset failed");
    return false;
  }
  startStopDelay();

  return true;
}

bool UsbPdStateMachine::setMeasure(int ccPin) {
  uint8_t switches0Val = 0x03;  // PDWN1/2
  if (ccPin == 1) {
    switches0Val |= 0x04;
  } else if (ccPin == 2) {
    switches0Val |= 0x08;
  } else {
    ESP_LOGW("UsbPdStateMachine", "setMeasure(): invalid ccPin arg = %i", ccPin);
    return false;
  }
  if (!fusb_.writeRegister(Fusb302::Register::kSwitches0, switches0Val)) {
    ESP_LOGW("UsbPdStateMachine", "setMeasure(): switches0 failed");
    return false;
  }
  startStopDelay();

  return true;
}

bool UsbPdStateMachine::readMeasure(uint8_t& result) {
  uint8_t regVal;
  if (!fusb_.readRegister(Fusb302::Register::kStatus0, regVal)) {
    ESP_LOGW("UsbPdStateMachine", "readMeasure(): status0 failed", );
    return false;
  }
  startStopDelay();

  result = regVal & 0x03;  // take BC_LVL only
  return true;
}

bool UsbPdStateMachine::readComp(uint8_t& result) {
  uint8_t regVal;
  if ((!fusb_.readRegister(Fusb302::Register::kStatus0, regVal))) {
    ESP_LOGW("UsbPdStateMachine", "readMeasure(): status0 failed");
    return false;
  }
  startStopDelay();

  result = regVal & 0x20;  // take COMP only
  return true;
}

bool UsbPdStateMachine::processRxMessages() {
  while (true) {
    uint8_t status1Val;
    if (!fusb_.readRegister(Fusb302::Register::kStatus1, status1Val)) {
      ESP_LOGW("UsbPdStateMachine", "processRxMessages(): readRegister(Status1) failed");
      return false;  // exit on error condition
    }
    startStopDelay();

    bool rxEmpty = status1Val & Fusb302::kStatus1::kRxEmpty;
    if (rxEmpty) {
      return true;
    }

    uint8_t rxData[30];
    if (!fusb_.readNextRxFifo(rxData)) {
      ESP_LOGW("UsbPdStateMachine", "processRxMessages(): readNextRxFifo failed");
      return false;  // exit on error condition
    }
    startStopDelay();

    uint16_t header = UsbPd::unpackUint16(rxData + 0);
    uint8_t messageType = UsbPd::MessageHeader::unpackMessageType(header);
    uint8_t messageNumDataObjects = UsbPd::MessageHeader::unpackNumDataObjects(header);
    if (messageNumDataObjects > 0) {  // data message
      ESP_LOGI("UsbPdStateMachine", "processRxMessages(): data message: id=%i, type=%03x, numData=%i", 
          messageId, messageType, messageNumDataObjects);
      switch (messageType) {
        case UsbPd::MessageHeader::DataType::kSourceCapabilities: {
          bool isFirstMessage = sourceCapabilitiesLen_ == 0;
          processRxSourceCapabilities(messageNumDataObjects, rxData);
          if (isFirstMessage && sourceCapabilitiesLen_ > 0) {
            UsbPd::Capability::Unpacked v5vCapability = UsbPd::Capability::unpack(sourceCapabilitiesObjects_[0]);
            sendRequestCapability(0, v5vCapability.maxCurrentMa);  // request the vSafe5v capability
          } else {
            // TODO this should be an error
          }
        } break;
        default:  // ignore
          break;
      }
    } else {  // command message
      ESP_LOGI("UsbPdStateMachine", "processRxMessages(): command message: id=%i, type=%03x", 
          messageId, messageType);
      switch (messageType) {
        case UsbPd::MessageHeader::ControlType::kAccept:
          currentCapability_ = requestedCapability_;
          break;
        case UsbPd::MessageHeader::ControlType::kReject:
          requestedCapability_ = currentCapability_;
          break;
        case UsbPd::MessageHeader::ControlType::kPsRdy:
          powerStable_ = true;
          break;
        case UsbPd::MessageHeader::ControlType::kGoodCrc:
        default:  // ignore
          break;
      }
    }
  }
}

void UsbPdStateMachine::processRxSourceCapabilities(uint8_t numDataObjects, uint8_t rxData[]) {
  for (uint8_t i=0; i<numDataObjects; i++) {
    sourceCapabilitiesObjects_[i] = UsbPd::unpackUint32(rxData + 2 + 4*i);
    UsbPd::Capability::Unpacked capability = UsbPd::Capability::unpack(sourceCapabilitiesObjects_[i]);
  }
  sourceCapabilitiesLen_ = numDataObjects;
}

bool UsbPdStateMachine::sendRequestCapability(uint8_t capability, uint16_t currentMa) {
  uint16_t header = UsbPd::MessageHeader::pack(UsbPd::MessageHeader::DataType::kRequest, 1, nextMessageId_);

  uint32_t requestData = UsbPd::maskAndShift(capability, 3, 28) |
      UsbPd::maskAndShift(1, 10, 24) |  // no USB suspend
      UsbPd::maskAndShift(currentMa / 10, 10, 10) |
      UsbPd::maskAndShift(currentMa / 10, 10, 0);
  if (fusb_.writeFifoMessage(header, 1, &requestData)) {
    requestedCapability_ = capability;
    powerStable_ = false;
    ESP_LOGI("UsbPdStateMachine", "requestCapability(): writeFifoMessage(Request(%i), %i)", capability, nextMessageId_);
  } else {
    ESP_LOGW("UsbPdStateMachine", "requestCapability(): writeFifoMessage(Request(%i), %i) failed", capability, nextMessageId_);
    return false;
  }
  nextMessageId_ = (nextMessageId_ + 1) % 8;
  return true;
}
