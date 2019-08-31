/*!Heavily altered from:
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

const char *Packet::prefix = "\x7E\xFF\06";//{ 0x7E,  0xFF, 6}; //theoretically could dynamically switch to 4 but none of the examples do so

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

bool DFRobotDFPlayerMini::sendCommand(uint8_t opcode, uint8_t param1 , uint8_t param2) {
  dbg("\nSC:", opcode, ',', param1, ',', param2);
  if (!ready) {  //if the ack mode is on wait until the last transmition
    return false;
  }
  outgoing.fill(opcode, param1, param2);
  return shipit();
}

bool DFRobotDFPlayerMini::shipit() {
  auto pun = reinterpret_cast<uint8_t*>(&outgoing);
  auto len = sizeof(outgoing);

  dbg("\nsending:", BLOCK(pun, len, ' '));//the F() here took more bytes than it wrapped! Netted 32 bytes removing it.

  _serial->write(Packet::prefix, 3);
  _serial->write(pun, len);
  _serial->write(Packet::em);

  responseOverdue = 500;

  switch (outgoing.opcode) {
    case 0x0C://reset
      ready = 2000; //or 1500 or 3000 depending upon who you believe.
      responseOverdue = 3000;
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
    if (r(_serial->read())) {//true on well framed packet
      //check cs
      if (r.csok) {
        auto opcode = r.incoming.opcode;
        auto param = r.incoming.param();
        //check if relevant to us
        switch (opcode) {
          case 0x3F:
            ready = 200; //it needs more time after returning that before accepting another command, time to scan the disk.
            break;
          case 0x40://packet rejected
            dbg("\nrejected", param);
            break;
        }
        if(r.receptions<10)
        handler(opcode, param);
      } else {
        //who do we report errors to?
        dbg("\nchecksum error");
      }
    }
    r.prepare();//ready for next
  }
  if (responseOverdue) {
    handler(0, MilliTick(responseOverdue));
  }
}

void DFRobotDFPlayerMini::begin(Stream &stream, bool requestACK, bool doReset) {
  _serial = &stream;
  ACK(requestACK);

  if (doReset) {
    reset();//sets timers that will block actions until responses have been noticed.
  } else {
    ready = 1234; //give the thing some time to power up.
  }

}


void DFRobotDFPlayerMini::Receiver::prepare() {
  sum = Packet::csinit ; //compiler should compute this and do a simple assignment of 0x105;
  csok = false;
  idx = Header;
  complete = false;
}

void DFRobotDFPlayerMini::Receiver::accept(uint8_t bite) {
  reinterpret_cast<uint8_t*>(&incoming)[idx] = bite;
  sum += bite;
  ++idx;
}

bool DFRobotDFPlayerMini::Receiver::verify(uint8_t bite) {
  uint8_t cf =  Packet::prefix[idx];
  if (cf == bite) {
    ++idx;
    return true;
  } else {
    ++framingErrors;
    idx = Packet::prefix[0] == bite ? Version : Header;
    return false;
  }
}

bool DFRobotDFPlayerMini::Receiver::operator()(uint8_t bite) {
  ++receptus;
  switch (idx) {
    case Header:
    case Version:
    case Length:
      verify(bite);
      break;
    case Command:
    case ACK:
    case Parameter:
    case ParameterLow:
      accept(bite);
      break;
    case CheckSum:
      sum += bite << 8;
      ++idx;
      break;
    case CheckSumLow:
      sum += bite;
      csok = sum == 0;
      ++idx;
      break;
    case End:
      complete = Packet::em == bite; //but cs might be bad.
      if (complete) {
        ++receptions;
      } else {
        ++framingErrors;
      }
      break;
    default:
      prepare();
      break;
  }
  return complete;
}

void Packet::fill(uint8_t opcode, uint8_t parmlow, uint8_t parmhigh) {

  CsType sum = csinit;
  sum += this-> opcode = opcode;
  sum += ackPlease;
  sum += this->parmhigh = parmhigh;
  sum += this->parmlow = parmlow;
  sum = -sum;
  cshigh = sum >> 8;
  cslow = sum;
}

#if DF_Include_Helpers
//helper helpers go here

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

#endif
