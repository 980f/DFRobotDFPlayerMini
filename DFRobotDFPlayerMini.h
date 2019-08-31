#pragma once
//by us not defining the old header guard you can detect which version you have included.

/*! Heavily altered from:
   @file DFRobotDFPlayerMini.h
   @brief DFPlayer - An Arduino Mini MP3 Player From DFRobot
   @n Header file for DFRobot's DFPlayer

   @copyright	[DFRobot]( http://www.dfrobot.com ), 2016
   @copyright	GNU Lesser General Public License

   @author [Angelo](Angelo.qiao@dfrobot.com)
   @version  V1.0.3
   @date  2016-12-07


*/

#include "Arduino.h"  //helps some IDE's
#include "chainprinter.h" //todo: conditional on debug flag

#ifndef DF_Include_Helpers
#pragma warn("#define DF_Include_Helpers to a nonzero value to get convenience wrappers for commands.");
#endif

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

/** parameter for command 09, 0x32 1st byte */
enum Medium : uint8_t { //#typed because we track which one we last requested
  PriorChoice = 255,
  U_DISK = 0,
  SDcard,  //SD symbol is taken by Arduino.h, caused weird coloring in arduino IDE
  AUX,
  SLEEP,
  FLASH, //this guy's command tokens are missing in newer documents.
};


//these were statuses the original code generated
//enum Event : uint8_t {
//  TimeOut = 0, WrongStack, CardInserted, CardRemoved , CardOnline , PlayFinished, Error, USBInserted , USBRemoved, USBOnline, CardUSBOnline, FeedBack
//};

//0x42 2nd byte, 1st is Medium
enum Issue : uint8_t {
  Busy = 1, Sleeping , SerialWrongStack , CheckSumNotMatch , FileIndexOut , FileMismatch , Advertise
};

//__attribute__ ((packed))
struct Packet {
  enum {  em = 0xEF , csinit = 0x105};
  static const char *prefix;
  uint8_t opcode;//typeless so that user can send ones we don't define for them
  uint8_t ackPlease = 1; //not const as we want user to be able to run open loop
  uint8_t parmhigh;
  uint8_t parmlow;
  uint8_t cshigh;
  uint8_t cslow;

  using CsType = uint16_t;

  uint16_t param() const {
    return parmhigh << 8 | parmlow;
  }

  void fill(uint8_t opcode, uint8_t parmlow, uint8_t parmhigh);

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

    MonoStable responseOverdue;//NYI: signal a timeout to handler.

    Packet outgoing;
    struct Receiver {
      /** named indexes into message. Messages are curiously referred to as a stack.*/
      enum Index {
        Header = 0, Version, Length , Command , ACK, Parameter, ParameterLow, CheckSum, CheckSumLow, End, Allocation
      };
      Packet incoming;
      unsigned idx;
      Packet::CsType sum;
      bool csok;
      bool complete;
      unsigned receptus = 0;
      unsigned framingErrors = 0;
      unsigned receptions = 0;

      Receiver() {
        prepare();
      }

      void prepare();

      void accept(uint8_t bite);

      bool verify(uint8_t bite);

      void check(uint8_t bite) {
        reinterpret_cast<uint8_t*>(&incoming)[idx] ^= bite;
        ++idx;
      }

      /** call this with each byte received. When it returns true then immediatley interpret the contents then call prepare() */
      bool operator()(uint8_t bite);
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
    void ACK(bool on) { //this is not a helper, it sets a local protocol option.
      outgoing.ackPlease = on;//bool conveniently matches command values
    }

    bool reset() {//used by begin
      return sendCommand(0x0C);
    }

    void showstate(ChainPrinter &cp) {
      cp("Ready:", bool(ready), " ", MilliTick(ready), ", R:", r.receptions, ", E:", r.framingErrors, ", B:", r.receptus);
      cp("Sent:", HEXLY(outgoing.opcode), "@", outgoing.param());
      cp("Recv:", HEXLY(r.incoming.opcode), "@", r.incoming.param());
    }

#if DF_Include_Helpers
    bool outputDevice(Medium device) {
      if (this->device != device) {
        this->device = device;
        return sendCommand(0x09, device);
      } else {
        return true;
      }
    }

    bool Loop(bool on = true) {
      return sendCommand(0x19, on ? 0 : 1); //complement of most
    }

    bool DAC(bool on) {
      return sendCommand(0x1A, on ? 0 : 1); //complement of most
    }

    bool LoopAll(bool on = true) {
      return sendCommand(0x11, on);
    }

    bool play(unsigned fileNumber = 1) {
      return sendCommand(0x03, fileNumber);
    }

    bool playFolder(uint8_t folderNumber, uint8_t fileNumber) {
      return sendCommand(0x0F, folderNumber, fileNumber);
    }

    bool loop(LoopMode play) {
      return sendCommand(0x08, play);
    }

    bool playLargeFolder(uint8_t folderNumber, uint16_t fileNumber) {
      return sendCommand(0x14, (((uint16_t)folderNumber) << 12) | fileNumber);
    }

    bool loopFolder(unsigned folderNumber) {
      return sendCommand(0x17, folderNumber);
    }

    //play from a folder named "MP3"
    bool playMp3Folder(unsigned fileNumber) {
      return sendCommand(0x12, fileNumber);
    }

    bool randomAll() {
      return sendCommand(0x18);
    }

    /** advertise interrupts loop play to play given item, returns to loop play,  pass a zero to return sooner or cancel one that hasn't started */
    bool advertise(unsigned fileNumber) {
      return sendCommand(fileNumber ? 0x13 : 0x15, fileNumber);
    }

    //1..31
    bool volume(uint8_t volume) {
      return sendCommand(0x06, volume);
    }

    bool outputSetting(bool enable, uint8_t gain) {
      return sendCommand(0x10, enable, gain);
    }

    bool EQ(Equalizer eq) {
      return sendCommand(0x07, eq);
    }

    bool next() {
      return sendCommand(0x01);
    }

    bool previous() {
      return sendCommand(0x02);
    }

    bool volumeUp() {
      return sendCommand(0x04);
    }

    bool volumeDown() {
      return sendCommand(0x05);
    }

    bool stop() {
      return sendCommand(0x16);
    }

    bool sleep() {
      return sendCommand(0x0A);
    }


    bool start() {
      return sendCommand(0x0D);
    }

    bool pause() {
      return sendCommand(0x0E);
    }

    bool readState() {
      return sendCommand(0x42);
    }

    bool readVolume() {
      return sendCommand(0x43);
    }

    bool readEQ() {
      return sendCommand(0x44);
    }

    bool readFileCounts(Medium device = PriorChoice) {
      return readFileThing(0x47, device);
    }
    bool readCurrentFileNumber(Medium device = PriorChoice) {
      return readFileThing(0x4B, device);
    }

    bool readFileCountsInFolder(unsigned folderNumber) {
      return sendCommand(0x4E, folderNumber);
    }

    bool readFolderCounts() {
      return sendCommand(0x4F);
    }
#endif

};
