#ifndef DFRobotDFPlayerMini_cpp
#define DFRobotDFPlayerMini_cpp

/*!
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


enum DFPLAYER_EQ : uint8_t {
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
enum DFPLAYER_DEVICE : uint8_t {
	PriorChoice=255,
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

/** named indexes into message. Messages are curiously referred to as a stack.*/
enum Stack {
  Header = 0,
  Version, Length , Command , ACK,
  Parameter, ParameterLow,
  CheckSum, CheckSumLow, End,
  Allocation  //DFPLAYER_RECEIVED_LENGTH  DFPLAYER_SEND_LENGTH
};

using Tick = decltype(millis());

class DFRobotDFPlayerMini {
  protected://changed access mode as will be extending in order to make it fully event driven without ripping out polled mode.
    Stream* _serial;

    Tick _timeOutTimer;
    Tick _timeOutDuration = 500;

    uint8_t _received[Stack::Allocation];
    uint8_t _sending[Stack::Allocation] = {0x7E, 0xFF, 06, 00, 01, 00, 00, 00, 00, 0xEF};

    uint8_t _receivedIndex = 0;

    void sendStack();
    void sendStack(uint8_t command);
    void sendStack(uint8_t command, uint16_t argument);

    void sendStack(uint8_t command, uint8_t argumentHigh, uint8_t argumentLow) {
      sendStack(command, argumentHigh << 8 | argumentLow); //gcc knows how to inline this code, saving bytes
    }
    /** ACK means 'please send response to all commands, not just data queries' */
    void enableACK();
    void disableACK();

    /** bigendian value to message, moved here because compiler can efficiently inline it.*/
    void uint16ToArray(uint16_t value, uint8_t *array) {
      *array++ = (uint8_t)(value >> 8);
      *array = (uint8_t)(value);
    }

    /** receive bigendian value, moved here because compiler can efficiently inline it.*/
    uint16_t arrayToUint16(uint8_t *array) {
      uint16_t value = *array++;//# if you eliminate this variable you are giving the compiler permission to swap bytes as it sees fit :)
      return value << 8 | *array;
    }

    uint16_t calculateCheckSum(uint8_t *buffer);

    void parseStack();
    bool validateStack();

    DFPLAYER_DEVICE device = DFPLAYER_DEVICE::PriorChoice;//guaranteed to cause a message to be sent to host when a device is actively chosen. 

  public:

    Event _handleType;
    /** unenumerated command codes, refer to manual*/
    uint8_t _handleCommand;
    uint16_t _handleParameter;

    bool _isAvailable = false;

    bool _isSending = false;

    bool handleMessage(Event type, uint16_t parameter = 0);
    bool handleError(Event type, uint16_t parameter = 0);

    uint8_t readCommand();
    /** setup call. @param requestACK is whether you desire to get acknowledgement messages for your commands. @param doReset blocks for enough time for device to do its scan of the storage media. */
    bool begin(Stream& stream, bool requestACK = true, bool doReset = true);

  /** ACK means 'please send response to all commands, not just data queries' */
    bool ACK(bool on);


    bool waitAvailable(unsigned long duration = 0);

    bool available();

    Event readType();

    uint16_t read();

    void setTimeOut(unsigned long timeOutDuration);

/** you are on your own for the most part.
@returns delay before you should send another command */
    Tick sendCommand(uint8_t ccode, uint16_t param=0);

    void next();

    void previous();

    void play(unsigned filenumber = 1);

    void volumeUp();

    void volumeDown();

    void volume(uint8_t volume);//1..31

    void EQ(DFPLAYER_EQ eq);

    void loop(LoopMode play);

		/** @returns number of millis you shoud wait before trying to play a track. */
    unsigned outputDevice(DFPLAYER_DEVICE device);

    void sleep();

    void reset();

    void start();

    void pause();

    void playFolder(uint8_t folderNumber, uint8_t fileNumber);

    void outputSetting(bool enable, uint8_t gain);

    void enableLoopAll();

    void disableLoopAll();

    void playMp3Folder(unsigned fileNumber);

    void advertise(unsigned fileNumber);

    void playLargeFolder(uint8_t folderNumber, uint16_t fileNumber);

    void stopAdvertise();

    void stop();

    void loopFolder(unsigned folderNumber);

    void randomAll();

    void enableLoop();

    void disableLoop();

    void enableDAC();

    void disableDAC();

    int readState();

    int readVolume();

    int readEQ();

    int readFileCounts(DFPLAYER_DEVICE device);

    int readCurrentFileNumber(DFPLAYER_DEVICE device);

    int readFileCountsInFolder(unsigned folderNumber);

    int readFileCounts();

    int readFolderCounts();

    int readCurrentFileNumber();

};

#endif
