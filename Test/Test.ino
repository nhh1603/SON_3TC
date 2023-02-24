#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

File myFile;
const int myInput = AUDIO_INPUT_MIC;

AudioInputI2S          audioInput;         // audio shield: mic or line-in
AudioSynthWaveformSine sinewave;
AudioPlaySdRaw         playRaw1;
AudioMixer4            mixer1;
AudioAnalyzeFFT1024    myFFT;
AudioOutputI2S         audioOutput;        // audio shield: headphones & line-out

// Connect either the live input or synthesized sine wave
//AudioConnection patchCord1(audioInput, 0, myFFT, 0);
//AudioConnection patchCord1(sinewave, 0, myFFT, 0);
//AudioConnection patchCord1(playRaw1, 0, myFFT, 0);
AudioConnection          patchCord1(playRaw1, 0, mixer1, 0);
AudioConnection          patchCord2(playRaw1, 1, mixer1, 1);
AudioConnection          patchCord3(mixer1, myFFT);

AudioControlSGTL5000 audioShield;

void setup() {
  AudioMemory(12);

  // Enable the audio shield and set the output volume.
  audioShield.enable();
  audioShield.inputSelect(myInput);
  // audioShield.micGain(20);
  audioShield.volume(0.5);

  // Configure the window algorithm to use
  myFFT.windowFunction(AudioWindowHanning1024);
  //myFFT.windowFunction(NULL);

  // Create a synthetic sine wave, for testing
  // To use this, edit the connections above
  // sinewave.amplitude(0.8);
  // sinewave.frequency(1034.007);

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

  myFile = SD.open("RECORD.RAW");
  playRaw1.play("RECORD.RAW");
}

void loop() {
  float n;
  int i;
  
  if (myFFT.available()) {
    Serial.print("FFT: ");
    for (i=0; i<40; i++) {
      n = myFFT.read(i);
      if (n >= 0.01) {
        Serial.print(n);
        Serial.print(" ");
      } else {
        Serial.print("  -  "); // don't print "0.00"
      }
    }
    Serial.println();
  } else {
    myFile.close();
  }
}
