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

#include "Arduino.h"

#ifndef DFRobotDFPlayerMini_cpp
#define DFRobotDFPlayerMini_cpp


enum DFPLAYER_EQ : uint8_t {
  NORMAL = 0,
  POP, ROCK, JAZZ, CLASSIC, BASS
};

enum DFPLAYER_DEVICE : uint8_t {
  U_DISK = 1,
  SD, AUX, SLEEP, FLASH
};

//#define _DEBUG

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

    void enableACK();
    void disableACK();

    void uint16ToArray(uint16_t value, uint8_t *array);

    uint16_t arrayToUint16(uint8_t *array);

    uint16_t calculateCheckSum(uint8_t *buffer);

    void parseStack();
    bool validateStack();

    uint8_t device = DFPLAYER_DEVICE::SD;

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
    /** setup call */
    bool begin(Stream& stream, bool isACK = true, bool doReset = true);

    bool waitAvailable(unsigned long duration = 0);

    bool available();

    Event readType();

    uint16_t read();

    void setTimeOut(unsigned long timeOutDuration);

    void next();

    void previous();

    void play(int fileNumber = 1);

    void volumeUp();

    void volumeDown();

    void volume(uint8_t volume);

    void EQ(uint8_t eq);

    void loop(int fileNumber);

    void outputDevice(uint8_t device);

    void sleep();

    void reset();

    void start();

    void pause();

    void playFolder(uint8_t folderNumber, uint8_t fileNumber);

    void outputSetting(bool enable, uint8_t gain);

    void enableLoopAll();

    void disableLoopAll();

    void playMp3Folder(int fileNumber);

    void advertise(int fileNumber);

    void playLargeFolder(uint8_t folderNumber, uint16_t fileNumber);

    void stopAdvertise();

    void stop();

    void loopFolder(int folderNumber);

    void randomAll();

    void enableLoop();

    void disableLoop();

    void enableDAC();

    void disableDAC();

    int readState();

    int readVolume();

    int readEQ();

    int readFileCounts(uint8_t device);

    int readCurrentFileNumber(uint8_t device);

    int readFileCountsInFolder(int folderNumber);

    int readFileCounts();

    int readFolderCounts();

    int readCurrentFileNumber();

};

#endif
