/**************************************************************************/
/*!
    @file     dump_damper.ino
    @author   Brian Moore
    @license  MIT
 
    A little device that helps cover up the sounds of, well, shit. 
    
    @section  HISTORY
 
    v1.0  - First release 2/3/2016 
*/
/**************************************************************************/
// INCLUDES

#include <SoftwareSerial.h>
#include "Adafruit_Soundboard.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RBD_Timer.h>
#include "bitmaps.h" // poops live here

// PIN DEFINITIONS
/**************************************************************************/

  // OLED SCREEN

    // If using software SPI (the default case):
    #define OLED_MOSI   9
    #define OLED_CLK   10
    #define OLED_DC    11
    #define OLED_CS    12
    #define OLED_RESET 13
    Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

  // AUDIO FX BOARD
  
    // Choose any two pins that can be used with SoftwareSerial to RX & TX
    #define SFX_TX 8
    #define SFX_RX 6
    
    // Connect to the RST pin on the Sound Board
    #define SFX_RST 14
  
    // Shut down that amp (technically not the audio fx board, but hey)
    #define AMP_SHUTDOWN 18

  // ROTARY ENCODER
  
    #define PIN_ENCODER_A      3
    #define PIN_ENCODER_B      5
    #define TRINKET_PINx       PIND
    #define PIN_ENCODER_SWITCH 4

  // PIR SENSOR

    #define PIR_PIN 17


// VARS SETUP
/**************************************************************************/

  // ROTARY SETUP
   
    static uint8_t enc_prev_pos   = 0;
    static uint8_t enc_flags      = 0;
    static char    sw_was_pressed = 0;
  
  // VFX SETUP
  
    // we'll be using software serial
    SoftwareSerial ss = SoftwareSerial(SFX_TX, SFX_RX);
    
    // pass the software serial to Adafruit_soundboard, the second
    // argument is the debug port (not used really) and the third 
    // arg is the reset pin
    Adafruit_Soundboard sfx = Adafruit_Soundboard(&ss, NULL, SFX_RST);
    // can also try hardware serial with
    // Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, NULL, SFX_RST);
    
    uint8_t currentTrack = 1;
    uint8_t alertTrack = 3;
    uint8_t numTracks, files;
    int trackToChange = 0;
  
  // VOLUME SETUP
  
    int volume = 50;
    int volumeToChange = 0;
    boolean volume_mode = false;
  
  // UI SETUP
  
    boolean UIactive = false;
  
  // PIR SETUP
  
    uint8_t pirState = LOW;
  
  // TIMER SETUP
  
    RBD::Timer timer;
    RBD::Timer knobTimer;
    RBD::Timer animationTimer;
    RBD::Timer UITimer;
    boolean hasStarted = false;
  
  // ANIMATION SETUP
  
    int animationStage = 0;

void knobHandler() {
  int8_t enc_action = 0; // 1 or -1 if moved, sign is direction
 
  // note: for better performance, the code will use
  // direct port access techniques
  // http://www.arduino.cc/en/Reference/PortManipulation
  uint8_t enc_cur_pos = 0;
  // read in the encoder state first
  if (bit_is_clear(TRINKET_PINx, PIN_ENCODER_A)) {
    enc_cur_pos |= (1 << 0);
  }
  if (bit_is_clear(TRINKET_PINx, PIN_ENCODER_B)) {
    enc_cur_pos |= (1 << 1);
  }
 
  // if any rotation at all
  if (enc_cur_pos != enc_prev_pos)
  {
    if (enc_prev_pos == 0x00)
    {
      // this is the first edge
      if (enc_cur_pos == 0x01) {
        enc_flags |= (1 << 0);
      }
      else if (enc_cur_pos == 0x02) {
        enc_flags |= (1 << 1);
      }
    }
 
    if (enc_cur_pos == 0x03)
    {
      // this is when the encoder is in the middle of a "step"
      enc_flags |= (1 << 4);
    }
    else if (enc_cur_pos == 0x00)
    {
      // this is the final edge
      if (enc_prev_pos == 0x02) {
        enc_flags |= (1 << 2);
      }
      else if (enc_prev_pos == 0x01) {
        enc_flags |= (1 << 3);
      }
 
      // check the first and last edge
      // or maybe one edge is missing, if missing then require the middle state
      // this will reject bounces and false movements
      if (bit_is_set(enc_flags, 0) && (bit_is_set(enc_flags, 2) || bit_is_set(enc_flags, 4))) {
        enc_action = 1;
      }
      else if (bit_is_set(enc_flags, 2) && (bit_is_set(enc_flags, 0) || bit_is_set(enc_flags, 4))) {
        enc_action = 1;
      }
      else if (bit_is_set(enc_flags, 1) && (bit_is_set(enc_flags, 3) || bit_is_set(enc_flags, 4))) {
        enc_action = -1;
      }
      else if (bit_is_set(enc_flags, 3) && (bit_is_set(enc_flags, 1) || bit_is_set(enc_flags, 4))) {
        enc_action = -1;
      }
 
      enc_flags = 0; // reset for next time
    }
  }
 
  enc_prev_pos = enc_cur_pos;
 
  if (enc_action > 0) { // moving clockwise
    if (volume_mode) {
      volumeController(1);
    } else {
      trackController(1);
    }
  }
  else if (enc_action < 0) { // moving counter clockwise
    if (volume_mode) {
      volumeController(0);
    } else {
      trackController(0);
    }
  }

  if (bit_is_clear(TRINKET_PINx, PIN_ENCODER_SWITCH)) 
  {
    if (sw_was_pressed == 0) // only on initial press, so the keystroke is not repeated while the button is held down
    {
      if(volume_mode) { // Encoder pushed down, toggle volume mode/track mode
        volume_mode = false;
        debugScreen("Track Mode");
      } else {
        volume_mode = true;
        debugScreen("Volume Mode");
      }
      delay(5); // debounce delay
    }
    sw_was_pressed = 1;
  }
  else
  {
    if (sw_was_pressed != 0) {
      delay(5); // debounce delay
    }
    sw_was_pressed = 0;
  }
}

void knobActionHandler() {
  if (volumeToChange != 0) {
    changeVolume(volumeToChange);
    volumeToChange = 0;
  } else if (trackToChange != 0) {
    // changeTrack(trackToChange);
    trackToChange = 0;
  }
}

void volumeController (int change) {
  UITimer.restart(); // hold for UI changes
  if (change == 1) { // vol up
    if ((volume + volumeToChange) < 50) {
      volumeToChange++;
    }
  } else if (change == 0) { // vol down
    if ((volume + volumeToChange) > 0) {
      volumeToChange--;
    }
  }
  knobTimer.restart();
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Volume:");
  display.println(volume + volumeToChange);
  display.display();
}

void changeVolume(int volChange) {
  stopPlayback();
  if (volChange > 0) { // volume up!
    for (int i = 0; i <= volChange; i++) {
      sfx.volUp();
    }
  } else {
    for (int i = 0; i >= volChange; i--) {
      sfx.volDown();
    }
  }
  playCurrentTrack();
  volume += volumeToChange;
}

void trackController (int change) {
  if (change == 1) { // track up
    trackUp();
  } else if (change == 0) { // track down
    trackDown();
  }  
   display.clearDisplay();
   display.setCursor(0,0);
   display.println("Playing");
   display.println(currentTrack);
   display.println(sfx.fileName(currentTrack));
   display.display();
   playCurrentTrack();
}

void trackUp() {
  stopPlayback();
  if (currentTrack == files) {
    currentTrack = 1; // ignore the first one
  } else if (currentTrack == alertTrack-1) {
    currentTrack+=2;
  } else {
    currentTrack++;
  }
}

void trackDown() {
  stopPlayback();
  if (currentTrack == 1) {
    currentTrack = files; // loop around to top
  } else if (currentTrack == alertTrack+1) {
    currentTrack-=2;
  } else {
    currentTrack--;
  }
}

void playCurrentTrack() {
  if (hasStarted) sfx.playTrack(currentTrack);
}

boolean currentlyPlaying() {
  if (digitalRead(15)==LOW) {
    return true;
  } else {
    return false;
  }
}

void stopPlayback() {
  if (currentlyPlaying()) sfx.stop();
}

void pausePlayback() {
  if (currentlyPlaying()) sfx.pause();
}

void unpausePlayback() {
  sfx.unpause();
}

void goIdle() {
  if (currentlyPlaying()) stopPlayback();
  if (hasStarted) { // only run once when idle starts
    showPoop(idlePoop0);
    animationStage = 1;
    hasStarted = false;
    animationTimer.restart();
  }
  // if (!hasStarted) animationTimer.restart();
  ampOff();
}

boolean currentlyMotion() {
  int sense = digitalRead(PIR_PIN);  // Read PIR Sensor
  if(sense == HIGH) {     //  currently motion
    return true;
  } else { // currently no motion
    return false;
  }
}

void debugScreen(String message) {
   display.clearDisplay();
   display.setCursor(0,0);
   display.println(message);
   display.display();
}

void startAudio() {
  hasStarted = true;
  ampOn();
  showPoop(happyPoop);
  sfx.playTrack(3);
  while (currentlyPlaying()) {
    delay(10);
  }
  showPoop(soundPoop);
  playCurrentTrack();
}

void showPoop(const unsigned char * poop) {
  display.clearDisplay();
  display.drawBitmap(0, 0, poop, 128, 32, WHITE);
  display.display();
}

void idlePoopHandler() {
  if(!hasStarted) {
    if (!UIactive) {
      switch (animationStage) {
        case 0: {
          showPoop(idlePoop0);
          animationTimer.setTimeout(500);
          break;
        }
        case 1: {
          showPoop(idlePoop1);
          animationTimer.setTimeout(500);
          break;
        }
        case 2: {
          showPoop(idlePoop2);
          animationTimer.setTimeout(500);
          break;
        }
        case 3: {
          showPoop(idlePoop3);
          animationTimer.setTimeout(1500);
          break;
        }
        break;
      }
      if (animationStage < 3) {
        animationStage++;
      } else {
        animationStage = 0;
      }
    }
    animationTimer.restart();
  }
}

void ampOff() {
  digitalWrite(AMP_SHUTDOWN, LOW);
  pinMode(     AMP_SHUTDOWN, OUTPUT);
}

void ampOn() {
  pinMode(AMP_SHUTDOWN, INPUT_PULLUP);
}


void setup()
{
  // ROTARY ENCODER SETUP 
  
    // set pins as input with internal pull-up resistors enabled
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    pinMode(PIN_ENCODER_SWITCH, INPUT_PULLUP);
   
   
    // get an initial reading on the encoder pins
    if (digitalRead(PIN_ENCODER_A) == LOW) {
      enc_prev_pos |= (1 << 0);
    }
    if (digitalRead(PIN_ENCODER_B) == LOW) {
      enc_prev_pos |= (1 << 1);
    }


  // OLED DISPLAY SETUP
  
    // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
    display.begin(SSD1306_SWITCHCAPVCC);
    // init done
    
    // Show image buffer on the display hardware.
    // Since the buffer is intialized with an Adafruit splashscreen
    // internally, this will display the splashscreen.
    display.display();
    delay(2000);
  
    // Clear the buffer.
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("Hello, world!");
    display.display();

  //SFX SETUP
  
    ss.begin(9600);

    if (!sfx.reset()) {
      display.clearDisplay();
      display.println("Not found");
      display.display();
      while (1);
    }
    display.println("SFX Board Ready");
    display.display();
    delay(1000);
    
    pinMode(15, INPUT); // ACT pin on SFX
    files = sfx.listFiles(); // Get list of files
    numTracks = files-1;
    
    ampOff(); // set Amp off to start

 
  // PIR SETUP
  
    pinMode(PIR_PIN, INPUT); // Initial state is low

  // TIMER SETUP
  
    timer.setTimeout(180000); // MAIN TIMEOUT FOR SOUND EFFECTS
    knobTimer.setTimeout(1200); // Knob interaction timeout
    UITimer.setTimeout(1200); // UI overlay timeout
  
    showPoop(happyPoop);
    delay(1000);

  // GO DIRECT TO IDLE, DO NOT COLLECT $200
  
    goIdle();
    
}
 
void loop() {
 knobHandler(); // deal with the rotary encoder
 
 if(currentlyMotion()) { // when motion detected...
  if(!hasStarted){ // ...and haven't already started playing audio...
   startAudio(); // ...play audio.
  }
  timer.restart(); // (and everytime you detect motion, restart the timer)
 }

 if(timer.onRestart()) { // go idle if no motion detected
   goIdle();
 }

 if(knobTimer.onExpired()) { // take action after you've knobbed.
  knobActionHandler();
 }

 if(animationTimer.onExpired()) { // idle animation initiate
  idlePoopHandler();
 }

 if(UITimer.onActive()) { // Note when the UI overlay is active
  UIactive = true;
 }

 if(UITimer.onExpired()) { // Note when the UI overlay is inactive
  UIactive = false;
 }

 if(hasStarted && ! currentlyPlaying()) { // Loop the audio if we're still poopin'
   sfx.playTrack(currentTrack);
 }

}
