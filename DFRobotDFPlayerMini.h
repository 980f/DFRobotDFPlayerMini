#pragma once
//by us not defining the old header guard you can detect which version you have included.

/*! Heaviy altered from:
   @file DFRobotDFPlayerMini.h
   @brief DFPlayer - An Arduino Mini MP3 Player From DFRobot
   @n Header file for DFRobot's DFPlayer

   @copyright	[DFRobot]( http://www.dfrobot.com ), 2016
   @copyright	GNU Lesser General Public License

   @author [Angelo](Angelo.qiao@dfrobot.com)
   @version  V1.0.3
   @date  2016-12-07

  980f comments:
  'stack' is used for the communications packet, the thing that gets passed through a network stack.
  I replaced #defines with enums, and then reworked function calls to use the enum's rather than any old byte.
    Many of the enumerated values are never used.
  I replaced *( buffer + offset) with buffer[offset].
  I used the formal means of getting the correct type for use with millisecond timer.
  I replaced delay(0) with a #define of YIELD to make it easier to search for these when I switch this to event driven rather than co-process. I have ESP8266's in hand.

*/

#include "Arduino.h"  //helps some IDE's
#include "index.h"

enum Equalizer {
  NORMAL = 0,
  POP, ROCK, JAZZ, CLASSIC, BASS
};


/** parameter for command 08 */
enum LoopMode {
  Repeat = 0,
  FolderRepeat,
  SingleRepeat,
  Random
};

/** parameter for command 09 */
enum Medium : uint8_t { //#typed because we track which one we last requested
  PriorChoice = 255,
  U_DISK = 0,
  SDcard,  //SD symbol is taken by Arduino.h, caused weird coloring in arduino IDE
  AUX,
  SLEEP,
  FLASH,
};



enum Event : uint8_t {
  TimeOut = 0, WrongStack, CardInserted, CardRemoved , CardOnline , PlayFinished, Error, USBInserted , USBRemoved, USBOnline, CardUSBOnline, FeedBack
};

enum Issue : uint8_t {
  Busy = 1, Sleeping , SerialWrongStack , CheckSumNotMatch , FileIndexOut , FileMismatch , Advertise
};

//__attribute__ ((packed))
struct Packet {
  const uint8_t sm = 0x7E;
  const uint8_t ver = 0xFF;
  const uint8_t len = 6; //theoretically could dynamically switch to 4 but none of the examples do so
  uint8_t opcode;//typeless so that user can send ones we don't define for them
  uint8_t ackPlease = 1; //not const as we want user to be able to run open loop
  uint8_t parmhigh;
  uint8_t parmlow;
  uint8_t cshigh;
  uint8_t cslow;
  const uint8_t em = 0xEF;

  using CsType = uint16_t;
  CsType csinit() const {
    return  sm + ver + len ; //compiler should compute this and do a simple assignment of 0x105;
  }

  uint16_t param() const {
    return parmhigh << 8 | parmlow;
  }

  void fill(uint8_t opcode, uint8_t parmlow, uint8_t parmhigh) {
    CsType sum = csinit();
    sum += this-> opcode = opcode;
    sum += ackPlease;
    sum += this->parmhigh = parmhigh;
    sum += this->parmlow = parmlow;
    sum = -sum;
    cshigh = sum >> 8;
    cslow = sum;
  }

  void fill(uint8_t opcode, uint16_t parm = 0) {
    fill(opcode, parm, parm >> 8);
  }




};


#include "millievent.h"

class DFRobotDFPlayerMini {
  protected://changed access mode as will be extending in order to make it fully event driven without ripping out polled mode.
    Stream* _serial;
    /**retain last selection for user requesting resend */
    uint16_t lastTrack = 1; //until we verify 0.WAV is a valid filename feed a 1.
    /**retain last selection for user requesting stats */
    Medium device = Medium::PriorChoice;//guaranteed to cause a message to be sent to host when a device is actively chosen.

    MonoStable responseExpected;
    //    Tick _timeOutDuration = 500;

    Packet outgoing;
    struct Receiver {
      /** named indexes into message. Messages are curiously referred to as a stack.*/
      enum Index {
        Header = 0,
        Version, Length , Command , ACK,
        Parameter, ParameterLow,
        CheckSum, CheckSumLow, End,
        Allocation
      };
      Packet incoming;
      unsigned idx;
      Packet::CsType sum;
      bool csok;
      bool complete;
      unsigned framingErrors = 0;

      Receiver() {
        prepare();
      }

      void prepare() {
        sum = incoming.sm + incoming.ver + incoming.len ; //compiler should compute this and do a simple assignment of 0x105;
        csok = false;
        idx = Header;
        complete = false;
      }

      void accept(uint8_t bite) {
        reinterpret_cast<uint8_t*>(&incoming)[idx] = bite;
        sum += bite;
        ++idx;
      }

      bool verify(uint8_t bite) {
        uint8_t cf =  reinterpret_cast<uint8_t*>(&incoming)[idx];
        if (cf == bite) {
          ++idx;
          return true;
        } else {
          ++framingErrors;
          idx = incoming.sm == bite ? Version : Header;
          return false;
        }
      }

      void check(uint8_t bite) {
        reinterpret_cast<uint8_t*>(&incoming)[idx] ^= bite;
        ++idx;
      }

      /** call this with each byte received. When it returns true then immediatley interpret the contents then call prepare() */
      bool operator()(uint8_t bite) {
        switch (idx) {
          case Header:
          case Version:
          case Length:
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
            complete = incoming.em == bite; //but cs might be bad.
            if (!complete) {
              ++framingErrors;
            }
            break;
          default:
            prepare();
            break;
        }
        return complete;
      }
    };

    Receiver r;

 bool shipit();
 
    bool waitingOnInit = false; //3F expected
    bool readFileThing(uint8_t code, Medium device);
  public:
    /** certain operations are going to return an error code if you try anything, check this first */
    MonoStable ready;

    using Handler = void (*)(uint8_t, uint16_t);
    /** user must call this frequently, but no more often than every millisecond. Skip a few if you must but hope that serial won't overflow.
        if it returns true then you must inspect the received data before calling this again or it might get corrupted by a spontaneous sending of the device.
    */
    void onMilli(Handler handler) ;

    /** if you call directly you are on your own for the most part.
      @returns whether it did it, which it won't if the interface is busy*/
    bool sendCommand(uint8_t ccode, uint16_t param = 0);

 /** if you call directly you are on your own for the most part.
      @returns whether it did it, which it won't if the interface is busy*/
    bool sendCommand(uint8_t ccode, uint8_t param1 , uint8_t param2);


    /** setup call. @param requestACK is whether you desire to get acknowledgement messages for your commands. @param doReset restarts the device, it will not be ready for a few seconds */
    void begin(Stream& stream, bool requestACK = true, bool doReset = true);

    /** ACK means 'please send response to all commands, not just data queries' */
    void ACK(bool on) {
      outgoing.ackPlease = on;//bool conveniently matches command values
    }

    bool outputDevice(Medium device) {
      if (this->device != device) {
        this->device = device;
        return sendCommand(0x09, device);
      } else {
        return true;
      }
    }

    void Loop(bool on = true) {
      sendCommand(0x19, on ? 0 : 1); //complement of most
    }

    void DAC(bool on) {
      sendCommand(0x1A, on ? 0 : 1); //complement of most
    }

    void LoopAll(bool on = true) {
      sendCommand(0x11, on);
    }


    void play(unsigned fileNumber = 1) {
      sendCommand(0x03, fileNumber);
    }

    void playFolder(uint8_t folderNumber, uint8_t fileNumber) {
      sendCommand(0x0F, folderNumber, fileNumber);
    }
    void loop(LoopMode play, bool andBegin = true);


    void loop(LoopMode play) {
      sendCommand(0x08, play);
    }


    void playLargeFolder(uint8_t folderNumber, uint16_t fileNumber) {
      sendCommand(0x14, (((uint16_t)folderNumber) << 12) | fileNumber);
    }

    void loopFolder(unsigned folderNumber) {
      sendCommand(0x17, folderNumber);
    }

//play from a folder named "MP3"
    void playMp3Folder(unsigned fileNumber) {
      sendCommand(0x12, fileNumber);
    }

    void randomAll() {
      sendCommand(0x18);
    }


    /** advertise interrupts loop play to play given item, returns to loop play,  pass a zero to return sooner or cancel one that hasn't started */
    void advertise(unsigned fileNumber) {
      sendCommand(fileNumber ? 0x13 : 0x15, fileNumber);
    }

    //1..31
    void volume(uint8_t volume) {
      sendCommand(0x06, volume);
    }

    void outputSetting(bool enable, uint8_t gain) {
      sendCommand(0x10, enable, gain);
    }


    void EQ(Equalizer eq) {
      sendCommand(0x07, eq);
    }

    void next() {
      sendCommand(0x01);
    }

    void previous() {
      sendCommand(0x02);
    }

    void volumeUp() {
      sendCommand(0x04);
    }

    void volumeDown() {
      sendCommand(0x05);
    }

    void stop() {
      sendCommand(0x16);
    }

    void sleep() {
      sendCommand(0x0A);
    }

    void reset() {
      sendCommand(0x0C);
    }

    void start() {
      sendCommand(0x0D);
    }

    void pause() {
      sendCommand(0x0E);
    }

    int readState() {
      sendCommand(0x42);
    }

    int readVolume() {
      sendCommand(0x43);
    }

    int readEQ() {
      sendCommand(0x44);
    }

    int readFileCounts(Medium device = PriorChoice) {
      return readFileThing(0x47, device);
    }
    int readCurrentFileNumber(Medium device = PriorChoice) {
      return readFileThing(0x4B, device);
    }

    bool readFileCountsInFolder(unsigned folderNumber) {
      return sendCommand(0x4E, folderNumber);
    }

    int readFolderCounts() {
      sendCommand(0x4F);
    }
};
