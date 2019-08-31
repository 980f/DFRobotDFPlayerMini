/*!
   @file DFRobotDFPlayerMini.cpp
   @brief DFPlayer - An Arduino Mini MP3 Player From DFRobot
   @n Header file for DFRobot's DFPlayer

   @copyright	[DFRobot]( http://www.dfrobot.com ), 2016
   @copyright	GNU Lesser General Public License

   @author [Angelo](Angelo.qiao@dfrobot.com)
   @version  V1.0.3
   @date  2016-12-07
*/

#include "DFRobotDFPlayerMini.h"

#define DF_DEBUG Serial

#ifdef DF_DEBUG
#include "chainprinter.h"
static ChainPrinter dbg(DF_DEBUG, false);
#else
#define dbg(...)
#endif


bool DFRobotDFPlayerMini::sendCommand(uint8_t ccode, uint16_t param ) {
  if (!ready) {  //if the ack mode is on wait until the last transmition
    return false;
  }
  outgoing.fill(ccode, param);
  return shipit();
}

bool DFRobotDFPlayerMini::sendCommand(uint8_t ccode, uint8_t param1 , uint8_t param2) {
  if (!ready) {  //if the ack mode is on wait until the last transmition
    return false;
  }
  outgoing.fill(ccode, param1, param2);
  return shipit();
}

bool DFRobotDFPlayerMini::shipit() {
  auto pun = reinterpret_cast<uint8_t*>(&outgoing);
  auto len = sizeof(outgoing);

  dbg(F("\nsending:"), BLOCK(pun, len, ' '));

  _serial->write(pun, len);

  responseExpected = 500;

  switch (outgoing.opcode) {
    case 0x0C://reset
      ready = 2000; //or 1500 or 3000 depending upon who you believe.
      responseExpected = 3000;
      break;
    case 0x09://change device
      ready = 200;//time to make filelist.
      break;
    default:
      if (!outgoing.ackPlease) { //if the ack mode is off wait 10 ms after one transmission.
        ready = 10;
      } else {
        ready = 0;
      }
      break;
  }

}

void DFRobotDFPlayerMini::onMilli(Handler handler) {
  for (auto few = _serial->available(); few-- > 0;) { //ignoring bytes coming in while we are processing others in order to not hang if we fall behind on our processing.
    if (r(_serial->read())) {
      //check cs
      //check if relevant to us
      //if user request return true.
      handler(r.incoming.opcode,r.incoming.param());
    }
  }
}

void DFRobotDFPlayerMini::begin(Stream &stream, bool requestACK, bool doReset) {
  _serial = &stream;
  ACK(requestACK);

  if (doReset) {
    reset();//sets timers that will block actions until responses have been noticed.
  }

}


//bool DFRobotDFPlayerMini::parseStack() {
//  uint8_t handleCommand = _received[Stack::Command];
//  if (handleCommand == 0x41) { //handle the 0x41 ack feedback as a special case, in case the corruption of _handleCommand, _handleParameter, and _handleType.
//    _isSending = false;
//    return;
//  }
//
//  _handleCommand = handleCommand;
//  _handleParameter =  arrayToUint16(&_received[Stack::Parameter]);
//
//  switch (_handleCommand) {
//    case 0x3D:
//      handleMessage(Event::PlayFinished, _handleParameter);
//      break;
//    case 0x3F:
//      if (_handleParameter & 0x01) {
//        handleMessage(Event::USBOnline, _handleParameter);
//      }
//      else if (_handleParameter & 0x02) {
//        handleMessage(Event::CardOnline, _handleParameter);
//      }
//      else if (_handleParameter & 0x03) {
//        handleMessage(Event::CardUSBOnline, _handleParameter);
//      }
//      break;
//    case 0x3A:
//      if (_handleParameter & 0x01) {
//        handleMessage(Event::USBInserted, _handleParameter);
//      }
//      else if (_handleParameter & 0x02) {
//        handleMessage(Event::CardInserted, _handleParameter);
//      }
//      break;
//    case 0x3B:
//      if (_handleParameter & 0x01) {
//        handleMessage(Event::USBRemoved, _handleParameter);
//      }
//      else if (_handleParameter & 0x02) {
//        handleMessage(Event::CardRemoved, _handleParameter);
//      }
//      break;
//    case 0x40:
//      handleMessage(Event::Error, _handleParameter);
//      break;
//    case 0x3C:
//    case 0x3E:
//    case 0x42:
//    case 0x43:
//    case 0x44:
//    case 0x45:
//    case 0x46:
//    case 0x47:
//    case 0x48:
//    case 0x49:
//    case 0x4B:
//    case 0x4C:
//    case 0x4D:
//    case 0x4E:
//    case 0x4F:
//      handleMessage(Event::FeedBack, _handleParameter);
//      break;
//    default:
//      handleError(WrongStack);
//      break;
//  }
//}

bool DFRobotDFPlayerMini::readFileThing(uint8_t code, Medium device) {
  if (device == PriorChoice) {
    device = this->device;
  }
 
  switch (device) {
    case Medium::U_DISK:
      break;
    case Medium::SDcard:
      ++code;
      break;
    case Medium::FLASH:
      code += 2;
      break;
    default:
      return false;
  }
  return sendCommand(code);
}
