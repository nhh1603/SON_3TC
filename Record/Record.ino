
#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include "pitchShifter.h"
#include "MyDsp.h"

pitchShifter pitchShifter;
MyDsp myDsp;

// Audio init
AudioInputI2S            in;           //xy=105,63
AudioAnalyzePeak         peak1;          //xy=278,108
AudioRecordQueue         queue1;         //xy=281,63
AudioPlaySdRaw           playRaw1;       //xy=302,157
AudioOutputI2S           out;           //xy=470,120
AudioConnection          patchCord1(in, 0, queue1, 0);
AudioConnection          patchCord2(in, 0, peak1, 0);
AudioConnection          patchCord3(playRaw1, 0, pitchShifter, 0);
AudioConnection          patchCord4(playRaw1, 0, pitchShifter, 1);
AudioConnection          patchCord5(pitchShifter, 0, myDsp, 0);
AudioConnection          patchCord6(pitchShifter, 0, myDsp, 1);
AudioConnection          patchCord7(myDsp, 0, out, 0);
AudioConnection          patchCord8(myDsp, 0, out, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=265,212

// Bounce objects to easily and reliably read the buttons
Bounce buttonRecord = Bounce(0, 8);
// Bounce buttonStop =   Bounce(1, 8);  // 8 = 8 ms debounce time
Bounce buttonPlay =   Bounce(2, 8);
int buttonRecordPress = 0;

// which input on the audio shield will be used?
//const int myInput = AUDIO_INPUT_LINEIN;
const int myInput = AUDIO_INPUT_MIC;


// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

// Remember which mode we're doing
int mode = 0;  // 0=stopped, 1=recording, 2=playing

// The file where data is recorded
File frec;

void setup() {
  // Configure the pushbutton pins
  pinMode(0, INPUT_PULLUP);
  // pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);

  // Audio connections require memory, and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(60);

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.micGain(20);
  sgtl5000_1.volume(0.5);

  // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here if no SD card, but print a message
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
}


void loop() {
  // First, read the buttons
  buttonRecord.update();
  // buttonStop.update();
  buttonPlay.update();

  // Respond to button presses
  if (buttonRecord.fallingEdge()) {
    buttonRecordPress = (buttonRecordPress + 1) % 2;
    if (buttonRecordPress == 1) {
      Serial.println("Record Button Press");
      if (mode == 2) stopPlaying();
      if (mode == 0) startRecording();
    } else if (buttonRecordPress == 0) {
      Serial.println("Stop Button Press");
      if (mode == 1) stopRecording();
      if (mode == 2) stopPlaying();
    }
  }
  if (buttonPlay.fallingEdge()) {
    Serial.println("Play Button Press");
    if (mode == 1) stopRecording();
    if (mode == 0) {
      //pitch shifting 
      float shift = analogRead(A0)/85-6;
      
      //echo
      float echo = analogRead(A1)*10;
      
      startPlaying(shift, echo);
    }
  }

  // If we're playing or recording, carry on...
  if (mode == 1) {
    continueRecording();
  }
  if (mode == 2) {
    continuePlaying();
  }

  // when using a microphone, continuously adjust gain
  // if (myInput == AUDIO_INPUT_MIC) adjustMicLevel();
}


void startRecording() {
  Serial.println("startRecording");
  if (SD.exists("RECORD.RAW")) {
    // The SD library writes new data to the end of the
    // file, so to start a new recording, the old file
    // must be deleted before new data is written.
    SD.remove("RECORD.RAW");
  }
  frec = SD.open("RECORD.RAW", FILE_WRITE);
  if (frec) {
    queue1.begin();
    mode = 1;
  }
}

void continueRecording() {
  if (queue1.available() >= 2) {
    byte buffer[512];
    // Fetch 2 blocks from the audio library and copy
    // into a 512 byte buffer.  The Arduino SD library
    // is most efficient when full 512 byte sector size
    // writes are used.
    memcpy(buffer, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    memcpy(buffer+256, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    // write all 512 bytes to the SD card
    //elapsedMicros usec = 0;
    frec.write(buffer, 512);
    // Uncomment these lines to see how long SD writes
    // are taking.  A pair of audio blocks arrives every
    // 5802 microseconds, so hopefully most of the writes
    // take well under 5802 us.  Some will take more, as
    // the SD library also must write to the FAT tables
    // and the SD card controller manages media erase and
    // wear leveling.  The queue1 object can buffer
    // approximately 301700 us of audio, to allow time
    // for occasional high SD card latency, as long as
    // the average write time is under 5802 us.
    //Serial.print("SD write, us=");
    //Serial.println(usec);
  }
}

void stopRecording() {
  Serial.println("stopRecording");
  queue1.end();
  if (mode == 1) {
    while (queue1.available() > 0) {
      frec.write((byte*)queue1.readBuffer(), 256);
      queue1.freeBuffer();
    }
    frec.close();
  }
  mode = 0;
}


void startPlaying(float s, float e) {
  setShift(s);
  setEcho(e);
  Serial.println("startPlaying");
  playRaw1.play("RECORD.RAW");
  mode = 2;
}

void setShift(float shift) {
  //float shift = shift;
  Serial.print("Shift:");
  Serial.println(shift);
  pitchShifter.setParamValue("shift (semitones)",shift);
}

void setEcho(float echo) {
  //float echo = echo;
  Serial.print("Echo:");
  Serial.println(echo);
  myDsp.echoL.setDel(echo);
  myDsp.echoL.setFeedback(0.5);
  myDsp.echoR.setDel(echo);
  myDsp.echoR.setFeedback(0.5);
}

void continuePlaying() {
  if (!playRaw1.isPlaying()) {
    playRaw1.stop();
    mode = 0;
  }
}

void stopPlaying() {
  Serial.println("stopPlaying");
  if (mode == 2) playRaw1.stop();
  mode = 0;
}

void adjustMicLevel() {
  // TODO: read the peak1 object and adjust sgtl5000_1.micGain()
  // if anyone gets this working, please submit a github pull request :-)
}
