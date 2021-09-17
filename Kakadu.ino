
// TEST



#include <virtuabotixRTC.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Stepper.h>
#include <TEA5767Radio.h>


#define CLK A2
#define DT A1
#define SW A0



//Display
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

//RTC
virtuabotixRTC myRTC(6, 7, 8); //CLK, DAT,RST
boolean timeChanged;







//Rotary Encoder
int currentCounter = 127;
int oldCounter;
int volumeCounter = 0;
int currentStateCLK;
int lastStateCLK;
String currentDir = "";
int buttonState;
boolean Push = false;
boolean longPush = false;
unsigned long timeButtonPressed = 0;
unsigned long timeButtonReleased = 0;
boolean buttonPressed;

//NeoPixel
#define LED_COUNT 60
#define STRIP_PIN 5
Adafruit_NeoPixel strip(LED_COUNT, STRIP_PIN, NEO_GRB + NEO_KHZ800);

//Stepper
const int stepsPerRevolution = 32;
const int gearReduction = 64;
const float stepsPerOutputRotation = stepsPerRevolution * gearReduction;
Stepper myStepper(stepsPerRevolution, 9, 11, 10, 12);
byte hourHand = 1;
byte pointToHour;

//Calibration
#define CAL_PIN A3      //think i killed pin A5
boolean calibrated;

//Wecker
boolean Alarm1State = false;
boolean Alarm1Triggered = false;
boolean Alarm2State = false;
boolean Alarm2Triggered = false;
boolean KuckuckState = true;
byte Alarm1Weekday = 1;
byte Alarm1Hour = 0;
byte Alarm1Minute = 0;
byte Alarm1RadioPreset = 0;
byte Alarm2Weekday = 1;
byte Alarm2Hour = 0;
byte Alarm2Minute = 0;
byte Alarm2RadioPreset = 0;
boolean alarmLight = false;

//Radio
TEA5767Radio radio = TEA5767Radio();
float frequencyPreset1;
float frequencyPreset2;
float frequencyChoice;
boolean radioState = false;
#define JAMS 13                       //JAMS instead of RADIO as "RADIO" is already taken
boolean preset1Saved = false;
boolean preset2Saved = false;


//Kuckuck
//Servo

unsigned long currentTime;
#define SERVO_PIN 3
byte KuckuckCounter = 0;
boolean KuckuckCheck = false;
boolean KuckuckUp = false;;

//LED
#define  LED_PIN 4
boolean kuckuckLight = false;

//DCF77

/**
 * Where is the DCF receiver connected?
 */
#define DCF77PIN 2
/**
 * Where is the LED connected?
 *
 * Number of milliseconds to elapse before we assume a "1",
 * if we receive a falling flank before - its a 0.
 */
#define DCF_split_millis 140
/**
 * There is no signal in second 59 - detect the beginning of 
 * a new minute.
 */
#define DCF_sync_millis 1200
/**
 * Definitions for the timer interrupt 2 handler:
 * The Arduino runs at 16 Mhz, we use a prescaler of 64 -> We need to 
 * initialize the counter with 6. This way, we have 1000 interrupts per second.
 * We use tick_counter to count the interrupts.
 */
#define INIT_TIMER_COUNT 6
#define RESET_TIMER2 TCNT2 = INIT_TIMER_COUNT
int tick_counter = 0;
int TIMSK;
int TCCR2;
int OCIE2;
/**
 * DCF time format struct
 */
struct DCF77Buffer {
  unsigned long long prefix  :21;
  unsigned long long Min  :7; // minutes
  unsigned long long P1   :1; // parity minutes
  unsigned long long Hour :6; // hours
  unsigned long long P2   :1; // parity hours
  unsigned long long Day  :6; // day
  unsigned long long Weekday  :3; // day of week
  unsigned long long Month  :5; // month
  unsigned long long Year :8; // year (5 -> 2005)
  unsigned long long P3   :1; // parity
};
struct {
  unsigned char parity_flag :1;
  unsigned char parity_min    :1;
  unsigned char parity_hour :1;
  unsigned char parity_date :1;
} flags;
/**
 * Clock variables 
 */
volatile unsigned char DCFSignalState = 0;  
unsigned char previousSignalState;
int previousFlankTime;
int bufferPosition;
unsigned long long dcf_rx_buffer;
/**
 * time vars: the time is stored here!
 */
volatile unsigned char ss;
volatile unsigned char mm;
volatile unsigned char hh;
volatile unsigned char day;
volatile unsigned char weekday;
volatile unsigned char mon;
volatile unsigned int year;
    

/**
 * used in main loop: detect a new second...
 */
unsigned char previousSecond;
unsigned long startDCF77;
boolean timeUpdatedDCF77 = false;





//Menu Stuff
boolean refresh = false;
byte menu = 1;
byte subMenuWecker;
byte subMenuRadio;

byte level;
byte oldLevel = 1;
byte timesPressed = 0;
boolean editing = false;
int myYear = 2021;
byte myMonth = 1;
byte myDay = 1;
byte myWeekday = 1;
byte myHour = 0;
byte myMinute = 0;
byte mySecond = 0;

boolean waiting = false;
unsigned long startWait = 0;



//NodeRed
boolean bannerInput = false;
boolean timeInput = false;
boolean dateInput = false;
String nodeRedBannerl1 = "Banner";
String nodeRedBannerl2 = " ";
int date[3];
int times[3];
boolean bufferSet;
byte bannerBuffer;





boolean bouncer = false; // bouncer is used to stop things happening more than once

void setup()
{
  Wire.begin();
  Serial.begin(9600);
  lcd.begin (16, 2);
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);
  myRTC.setDS1302Time(30, 20, 10, 5, 10, 1, 2021); //sek,min,stunden, wochen tag, monats tag, monat, jahr
  level = 1;
  strip.begin();
  strip.show();
  myStepper.setSpeed(500);
  pinMode(SERVO_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(CAL_PIN, INPUT_PULLUP);          
  pinMode(DCF77PIN, INPUT);          
  pinMode(JAMS, OUTPUT);
  DCF77Init();
  calibrated = false;

}


void loop()
{
  if(timeUpdatedDCF77 == false)
  {
    startDCF77 = millis();
  
    while((millis()-startDCF77 <= 600000) && (timeUpdatedDCF77 == false) && (digitalRead(SW) == HIGH))        // 10 minuten nach der uhrzeit suchen
    {
      getTimeDCF77();
    }
    timeUpdatedDCF77 = true;
  }
  if(myRTC.hours == 3)      //jeden morgen um 3 die uhrzeit refreshen
  {
    timeUpdatedDCF77 = false;
  }
  nodeRed();
  if (calibrated == false)
    calibrateHourHand();
  Encoder(currentCounter);
  if ((menu == 1) && (level == 1))
  {
    printTime();
  }
  updateClockface();
  if (myRTC.minutes == 0 )
  {
    myRTC.updateTime();
    if (KuckuckCheck == false)
    {
      Kuckuck();
    }
  }
  if (myRTC.minutes != 0)
  {
    KuckuckCounter = 0;
    KuckuckCheck = false;
  }
  
}

void Encoder(int& i)
{

  // Read the current state of CLK
  currentStateCLK = digitalRead(CLK);

  // If last and current state of CLK are different, then pulse occurred
  // React to only 1 state change to avoid double count
  if (currentStateCLK != lastStateCLK  && currentStateCLK == 1)
  {
    Serial.println(i);
    // If the DT state is different than the CLK state then
    // the encoder is rotating CCW so decrement
    if (digitalRead(DT) != currentStateCLK)
    {
      // Encoder is rotating CW so increment
      i --;
      currentDir = "CCW";

      //Roll round needs to stay in this direction, encoder doesnt like it in the other direction

      //ROLL ROUND SPECIFICATIONS POSITIVE

      if ((i < 128) && (level == 1))
      {
        Serial.println("ROUND");
        i = 131;
      }

      if ((menu == 1) && (level == 3) && (i < 1))
      {
        i = 12;
      }

      if ((menu == 1) && (level == 4) && (i < 1))
      {
        i = 31;
      }

      if ((menu == 1) && (level == 5) && (i < 1))
      {
        i = 7;
      }

      if ((menu == 1) && (level == 6) && (i < 0))
      {
        i = 23;
      }

      if ((menu == 1) && (level == 7) && (i < 0))
      {
        i = 59;
      }

      if ((menu == 2) && (level == 2) && (i < 1))  //Wecker Wheel
      {
        i = 4;
      }

      if ((menu == 2) && (level == 3) && (subMenuWecker == 1) && (editing == true) && (i < 1))  //Alarm 1 Weekday Wheel
      {
        i = 7;
      }

      if ((menu == 2) && (level == 3) && (subMenuWecker == 1) && (editing == false) && (i < 0))  //Alarm 1 State Wheel
      {
        i = 1;
      }

      if ((menu == 2) && (level == 4) && (subMenuWecker == 1) && (editing == true) && (i < 0))  //Alarm 1 Hour Wheel
      {
        i = 23;
      }

      if ((menu == 2) && (level == 5) && (subMenuWecker == 1) && (editing == true) && (i < 0))  //Alarm 1 Minute Wheel
      {
        i = 59;
      }

      if((menu == 2) &&(level ==6) &&(subMenuWecker == 1) && (editing == true) && (i < 0)) //Wake up Mode 1
      {
        i=3;
      }


      if ((menu == 2) && (level == 3) && (subMenuWecker == 2) && (editing == true) && (i < 1))  //Alarm 2 Weekday Wheel
      {
        i = 7;
      }

      if ((menu == 2) && (level == 3) && (subMenuWecker == 2) && (editing == false) && (i < 0))  //Alarm 2 State Wheel
      {
        i = 1;
      }

      if ((menu == 2) && (level == 4) && (subMenuWecker == 2) && (editing == true) && (i < 0))  //Alarm 2 Hour Wheel
      {
        i = 23;
      }

      if ((menu == 2) && (level == 5) && (subMenuWecker == 2) && (editing == true) && (i < 0))  //Alarm 2 Minute Wheel
      {
        i = 59;
      }


      if((menu == 2) &&(level ==6) &&(subMenuWecker == 2) && (editing == true) && (i < 0)) //Wake up Mode 2
      {
        i=3;
      }

      if ((menu == 2) && (level == 3) && (subMenuWecker == 3) && (editing == false) && (i > 1))  //Kuckuck State Wheel
      {
        i = 0;
      }

      if ((menu == 3) && (level == 2) && (i < 1))                                           //Radio Wheel
      {
        i = 4;
      }

      if ((menu == 3) && (level == 3) && (subMenuRadio == 1) && (editing == true) && (i < 1)) //Radio Preset 1 frequency wheel    1 is the value, that when inputed into the formula for the corresponding level outputs 87.5 as frequency
      {
        i = 165;
      }
      
      if((menu == 3) && (level == 3) && (subMenuRadio == 1) && (editing == false) && (i<0))    //Radio Preset 1 State Wheel
      {
        i = 1;
      }
   
      if ((menu == 3) && (level == 3) && (subMenuRadio == 2) && (editing == true) && (i < 1)) //Radio Preset 2 frequency wheel    1 is the value, that when inputed into the formula for the corresponding level outputs 87.5 as frequency
      {
        i = 165;
      }
      
      if((menu == 3) && (level == 3) && (subMenuRadio == 2) && (editing == false) && (i<0))    //Radio Preset 2 State Wheel
      {
        i = 1;
      }
      

      setLines();
      Serial.print("Level: ");
      Serial.println(level);
      Serial.print("Menu: ");
      Serial.println(menu);
      Serial.print("current Counter: ");
      Serial.println(currentCounter);
      Serial.print("old Counter: ");
      Serial.println(oldCounter);
      Serial.print("i: ");
      Serial.println(i);
    }
    else
    {
      i ++;
      currentDir = "CW";

      //ROLL ROUND SPECIFICATIONS NEGATIVE

      if ((i > 131) && (level == 1))
      {
        i = 128;
      }

      if ((menu == 1) && (level == 3) && (i > 12))
      {
        i = 1;
      }

      if ((menu == 1) && (level == 4) && (i > 31))
      {
        i = 1;
      }

      if ((menu == 1) && (level == 5) && (i > 7))
      {
        i = 1;
      }


      if ((menu == 1) && (level == 6) && (i > 23))
      {
        i = 0;
      }

      if ((menu == 1) && (level == 7) && (i > 59))
      {
        i = 0;
      }

      if ((menu == 2) && (level == 2) && (i > 4))  //Wecker Wheel
      {
        i = 1;
      }

      if ((menu == 2) && (level == 3) && (subMenuWecker == 1) && (editing == true) && (i > 7))  //Alarm 1 Weekday Wheel
      {
        i = 1;
      }

      if ((menu == 2) && (level == 3) && (subMenuWecker == 1) && (editing == false) && (i > 1))  //Alarm 1 State Wheel
      {
        i = 0;
      }

      if ((menu == 2) && (level == 4) && (subMenuWecker == 1) && (editing == true) && (i > 23))  //Alarm 1 Hour Wheel
      {
        i = 0;
      }

      if ((menu == 2) && (level == 5) && (subMenuWecker == 1) && (editing == true) && (i > 59))  //Alarm 1 Minute Wheel
      {
        i = 0;
      }


      if ((menu == 2) && (level == 3) && (subMenuWecker == 2) && (editing == true) && (i > 7))  //Alarm 2 Weekday Wheel
      {
        i = 1;
      }

      if ((menu == 2) && (level == 3) && (subMenuWecker == 2) && (editing == false) && (i > 1))  //Alarm 2 State Wheel
      {
        i = 0;
      }

      if ((menu == 2) && (level == 4) && (subMenuWecker == 2) && (editing == true) && (i > 23))  //Alarm 2 Hour Wheel
      {
        i = 0;
      }

      if ((menu == 2) && (level == 5) && (subMenuWecker == 2) && (editing == true) && (i > 59))  //Alarm 2 Minute Wheel
      {
        i = 0;
      }

      if ((menu == 2) && (level == 3) && (subMenuWecker == 3) && (editing == false) && (i > 1))  //Kuckuck State Wheel
      {
        i = 0;
      }

      if ((menu == 3) && (level == 2) && (i > 4))                                           //Radio Wheel
      {
        i = 1;
      }
      
      if((menu == 3) && (level == 3) && (subMenuRadio == 1) && (editing == true) && (i>165)) //Radio Preset 1 Frequency wheel   41 is the value, that when inputed into the formula for the corresponding level outputs 108 as frequency
      {
        i = 1;
      }

      if((menu == 3) && (level == 3) && (subMenuRadio == 1) && (editing == false) && (i>1))    //Radio Preset 1 State Wheel
      {
        i = 0;
      }

      if((menu == 3) && (level == 3) && (subMenuRadio == 2) && (editing == true) && (i>165)) //Radio Preset 2 Frequency wheel   41 is the value, that when inputed into the formula for the corresponding level outputs 108 as frequency
      {
        i = 1;
      }

      if((menu == 3) && (level == 3) && (subMenuRadio == 2) && (editing == false) && (i>1))    //Radio Preset 2 State Wheel
      {
        i = 0;
      }



      //need one for radio presets (level 7)
      

      setLines();
      Serial.print("Level: ");
      Serial.println(level);
      Serial.print("current Counter: ");
      Serial.println(currentCounter);
      Serial.print("old Counter: ");
      Serial.println(oldCounter);
      Serial.print("i: ");
      Serial.println(i);
    }
  }
  lastStateCLK = currentStateCLK;



  if (buttonState != digitalRead(SW) && (timeButtonReleased - timeButtonPressed >= 100)) //State change checker with debouncer
  {
    buttonPressed = true;
  }


  buttonState = digitalRead(SW);
  if (buttonState == HIGH)                //
  {
    timeButtonReleased = millis();        //time of Button press
  }

  if (buttonState == LOW)
  {
    timeButtonPressed = timeButtonReleased;     //time of Button release
  }

  //SHORT PUSH

  if ((buttonPressed == true) && (timeButtonReleased - timeButtonPressed >= 100) && (timeButtonReleased - timeButtonPressed <= 300))
  {
    Serial.println("short");
    Push = true;
    buttonPressed = false;
    oldCounter = currentCounter;
    level++;
    bouncer = false;
    setLines();
  }

  //LONG PUSH

  if ((buttonPressed == true) && (timeButtonReleased - timeButtonPressed >= 2000))
  {
    Serial.println("longPush");
    longPush = true;
    buttonPressed = false;
    oldCounter = currentCounter;
    level++;
    bouncer = false;
    Serial.println(oldCounter);
    setLines();
  }

  if ((buttonState == HIGH))
  {
    longPush = false;
    Push = false;
  }

}









void setLines()
{

  ////LEVEL 1 OF STATE MACHINE

  if (level == 1)
  {
    switch (currentCounter)
    {
      case 128: //Uhrzeit und Datum
        menu = 1;
        lcd.clear();
        if (longPush == true)
          level = 2;
        break;

      case 129: //Wecker
        menu = 2;
        lcd.clear();
        lcd.print("Wecker");
        break;


      case 130:  //Radio
        menu = 3;
        lcd.clear();
        lcd.print("Radio");
        break;

      case 131: //Banner
        menu = 4;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(nodeRedBannerl1);
        lcd.setCursor(0, 1);
        lcd.print(nodeRedBannerl2);
        break;
    }
  }

  ////LEVEL 2 OF STATE MACHINE

  if (level == 2)
  {
    switch (menu)
    {
      case 1: //Jahr
        if (longPush == true)
        {
          myYear = 2021;
          currentCounter = myYear;
          editing = true;
        }
        if (editing == true)
        {
          lcd.clear();
          lcd.print("Jahr: ");
          lcd.print(currentCounter);
        }
        if (editing == false)
        {
          level = 1;
        }
        break;

      case 2: //Wecker
        if (bouncer == false)
        {
          currentCounter = 1;
          bouncer = true;
        }

        switch (currentCounter)
        {
          case 1:
            subMenuWecker = 1;
            lcd.clear();
            lcd.print("Wecker 1");
            break;

          case 2:
            subMenuWecker = 2;
            lcd.clear();
            lcd.print("Wecker 2");
            break;

          case 3:
            subMenuWecker = 3;
            lcd.clear();
            lcd.print("Kuckuck");
            break;

          case 4:
            subMenuWecker = 4;
            lcd.clear();
            lcd.print("Back");
            break;
        }
        break;

      case 3: //Radio
        if (bouncer == false)
        {
          currentCounter = 1;
          bouncer = true;
        }

        switch (currentCounter)
        {
          case 1:
            subMenuRadio = 1;
            lcd.clear();
            lcd.print("Radio Preset 1");
            break;

          case 2:
            subMenuRadio = 2;
            lcd.clear();
            lcd.print("Radio Preset 2 ");
            break;

          case 3:
            subMenuRadio = 3;
            lcd.clear();
            lcd.print("Frequenzauswahl");
            break;

          case 4:
            subMenuRadio = 4;
            lcd.clear();
            lcd.print("Back");
            break;
        }
        break;

    }
  }


  //LEVEL 3 OF STATE MACHINE
  if (level == 3)
  {
    switch (menu)
    {
      case 1: //Month
        if (editing == true) //stops entry through normal presses
        {
          if (bouncer == false)
          {
            myYear = oldCounter;
            currentCounter = myMonth;
            bouncer = true;
          }
          lcd.clear();
          lcd.print("Monat: ");
          lcd.print(currentCounter);
        }
        else
          level = 1;
        break;


      case 2: //Wecker                                  //problem here goes to case 3 instead
        switch (subMenuWecker)
        {
          case 1:           //Wecker 1
            if (longPush == true)
            {
              editing = true;
            }
            if (editing == true)
            {
              if (bouncer == false)
              {
                currentCounter = Alarm1Weekday;
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Wochentag: ");
              lcd.setCursor(0, 1);
              switch (currentCounter)
              {
                case 1:
                  lcd.print("Montag");
                  break;

                case 2:
                  lcd.print("Dienstag");
                  break;

                case 3:
                  lcd.print("Mittwoch");
                  break;

                case 4:
                  lcd.print("Donnerstag");
                  break;

                case 5:
                  lcd.print("Freitag");
                  break;

                case 6:
                  lcd.print("Samstag");
                  break;

                case 7:
                  lcd.print("Sonntag");
                  break;
              }
            }
            else
            {
              if (bouncer == false)
              {
                currentCounter = Alarm1State;
                bouncer = true;
              }
              lcd.clear();
              if (currentCounter == 0)
                lcd.print("Aus");
              else
                lcd.print("An");
            }
            break;

          case 2:           //Wecker 2
            if (longPush == true)
            {
              editing = true;
            }
            if (editing == true)
            {
              if (bouncer == false)
              {
                currentCounter = Alarm2Weekday;
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Wochentag: ");
              lcd.setCursor(0, 1);
              switch (currentCounter)
              {
                case 1:
                  lcd.print("Montag");
                  break;

                case 2:
                  lcd.print("Dienstag");
                  break;

                case 3:
                  lcd.print("Mittwoch");
                  break;

                case 4:
                  lcd.print("Donnerstag");
                  break;

                case 5:
                  lcd.print("Freitag");
                  break;

                case 6:
                  lcd.print("Samstag");
                  break;

                case 7:
                  lcd.print("Sonntag");
                  break;
              }
            }
            else
            {
              if (bouncer == false)
              {
                currentCounter = Alarm2State;
                bouncer = true;
              }
              lcd.clear();
              if (currentCounter == 0)
                lcd.print("Aus");
              else
                lcd.print("An");
            }
            break;

          case 3: //Kuckuck
            if (bouncer == false)
            {
              currentCounter = KuckuckState;
              bouncer = true;
            }
            lcd.clear();
            if (currentCounter == 0)
              lcd.print("Aus");
            else
              lcd.print("An");
            break;

          case 4: //Back
            currentCounter = 129;
            level = 1;
            lcd.clear();
            lcd.print("Wecker");
            break;

        }
        break;


      case 3: //Radio
        switch (subMenuRadio)
        {
          case 1: //Radio Preset 1
            if (longPush == true)
            {
              editing  = true;
            }
            if (editing == true)
            {
              if (bouncer == false)
              {
                frequencyPreset1 = 97.5;
                currentCounter = 1;          //need this to be a random start value to be able to tell if current number is even or odd (issue with integer function and float data type)
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Radio Preset 1: ");
              lcd.setCursor(0, 1);
              lcd.print(frequencyPreset1 + (0.05 * currentCounter));        //need adjustment here
              if (radioState == true)
              {
                radio.setFrequency(frequencyPreset1 + (0.05 * currentCounter));
              }
            }
            if (radioState == true && editing == false)
            {
              digitalWrite(JAMS, LOW);
              radioState = false;
              currentCounter = 1;
              level = 2;
              break;
            }
            if (radioState == false && editing == false)
            {
              digitalWrite(JAMS, HIGH);
              wait(1000);
              if (waiting == false)
              {
                Serial.println("Frequency: ");   
                Serial.print(frequencyPreset1); 
                radio.setFrequency(frequencyPreset1);     //this doesnt seem to use the saved variable
                radioState = true;
                currentCounter = 1;
                level = 2;
              }
              break;
            }
            break;


          case 2: //Radio Preset 2
            if (longPush == true)
            {
              editing  = true;
            }
            if (editing == true)
            {
              if (bouncer == false)
              {
                frequencyPreset2 = 97.5;
                currentCounter = 1;          //need this to be a random staart value to be able to tell if current number is even or odd (issue with integer function and float data type)
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Radio Preset 2: ");
              lcd.setCursor(0, 1);
              lcd.print(frequencyPreset2 + (0.5 * currentCounter));
              if (radioState == true)
              {
                radio.setFrequency(frequencyPreset2 + (0.5 * currentCounter));
              }

            }
            if (radioState == true)
            {
              digitalWrite(JAMS, LOW);
              radioState = false;
              currentCounter = 2;
              level = 2;
              break;
            }
            if (radioState == false)
            {
              digitalWrite(JAMS, HIGH);                     //radio might need more time after turning on to tune
              wait(1000);
              if(waiting)
              {
              radio.setFrequency(frequencyPreset2);
              radioState = true;
              currentCounter = 2;
              level = 2;
              }
              break;
            }

            break;


          case 3:         //Frequenzauswahl
            editing  = true;
            if (bouncer == false)
            {
              digitalWrite(JAMS, HIGH);
              frequencyChoice = 97.5;
              currentCounter = 1;          //need this to be a random staart value to be able to tell if current number is even or odd (issue with integer function and float data type)
              bouncer = true;
            }
            lcd.clear();
            lcd.print("Frequenz: ");
            lcd.setCursor(0, 1);
            lcd.print(frequencyChoice + (0.05 * currentCounter));
            radio.setFrequency(frequencyChoice + (0.05 * currentCounter));
            break;


          case 4:                 //Back
            currentCounter = 130;
            level = 1;
            lcd.clear();
            lcd.print("Radio");
            break;
        }
    }
  }








  //LEVEL 4 OF STATE MACHINE

  if (level == 4)
  {
    switch (menu)
    {
      case 1: //Day
        if (editing == true)
        {
          if (bouncer == false)
          {
            myMonth = oldCounter;
            currentCounter = myDay;
            bouncer = true;
          }
          lcd.clear();
          lcd.print("Tag: ");
          lcd.print(currentCounter);
        }
        break;

      case 2:   //Wecker
        switch (subMenuWecker)
        {
          case 1: //Wecker 1
            if (editing == true)
            {
              if (bouncer == false)
              {
                Alarm1Weekday = oldCounter;
                currentCounter = Alarm1Hour;
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Stunde: ");
              lcd.print(currentCounter);
            }
            else
            {
              if (bouncer == false)
              {
                Alarm1State = oldCounter;
                currentCounter = 1;
                level = 2;    //back to first level of state machine
                bouncer = true;
                lcd.clear();
                lcd.print("Wecker 1");
              }
            }
            break;


          case 2: //Wecker 2
            if (editing == true)
            {
              if (bouncer == false)
              {
                Alarm2Weekday = oldCounter;
                currentCounter = Alarm2Hour;
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Stunde: ");
              lcd.print(currentCounter);
            }
            else
            {
              if (bouncer == false)
              {
                Alarm2State = oldCounter;
                currentCounter = 2;
                level = 2;    //back to first level of state machine
                bouncer = true;
                lcd.clear();
                lcd.print("Wecker 2");
              }
            }
            break;

          case 3: //Kuckuck
            if (bouncer == false)
            {
              KuckuckState = oldCounter;
              currentCounter = 3;
              level = 2;    //back to first level of state machine
              bouncer = true;
              lcd.clear();
              lcd.print("Kuckuck");
            }
            break;
        }
        break;


      case 3:     //Radio
        switch (subMenuRadio)
        {
          case 1: //Radio Preset 1
            if (editing == true)
            {
              frequencyPreset1 = (frequencyPreset1 + (0.05 * oldCounter)); 
              editing = false;
              preset1Saved = true;
            }
            else
            {
              radioState = oldCounter;
            }
            level = 2;
            lcd.clear();
            break;

          case 2: // Radio Preset 2
            if (editing == true)
            {
              frequencyPreset2 = (frequencyPreset2 + (0.05 * oldCounter));
              editing = false;
              preset2Saved = true;
            }
            else
            {

            }
            level = 2;
            lcd.clear();
            break;

          case 3: //Frequenzauswahl
            digitalWrite(JAMS, LOW);
            currentCounter = 3;
            level = 2;
            lcd.clear();
            lcd.print("Frequenzauswahl");
            break;

        }
    }
  }




  //LEVEL 5 OF STATE MACHINE

  if (level == 5)
  {
    switch (menu)
    {
      case 1: //Weekday
        if (editing == true)
        {
          if (bouncer == false)
          {
            myDay = oldCounter;
            currentCounter = myWeekday;
            bouncer = true;
          }
          lcd.clear();
          lcd.print("Wochentag: ");
          lcd.setCursor(0, 1);
          switch (currentCounter)
          {
            case 1:
              lcd.print("Montag");
              break;

            case 2:
              lcd.print("Dienstag");
              break;

            case 3:
              lcd.print("Mittwoch");
              break;

            case 4:
              lcd.print("Donnerstag");
              break;

            case 5:
              lcd.print("Freitag");
              break;

            case 6:
              lcd.print("Samstag");
              break;

            case 7:
              lcd.print("Sonntag");
              break;
          }
        }
        break;

      case 2: //Wecker
        switch (subMenuWecker)
        {
          case 1: //Wecker 1
            if (editing == true)
            {
              if (bouncer == false)
              {
                Alarm1Hour = oldCounter;
                currentCounter = Alarm1Minute;
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Minute: ");
              lcd.print(currentCounter);
            }


          case 2: //Wecker 2
            if (editing == true)
            {
              if (bouncer == false)
              {
                Alarm2Hour = oldCounter;
                currentCounter = Alarm2Minute;
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Minute: ");
              lcd.print(currentCounter);
            }
        }
    }
  }


  //LEVEL 6 OF STATE MACHINE

  if (level == 6)
  {
    switch (menu)
    {
      case 1: //Hours
        if (editing == true)
        {
          if (bouncer == false)
          {
            myWeekday = oldCounter;
            currentCounter = myHour;
            bouncer = true;
          }
          lcd.clear();
          lcd.print("Stunde: ");
          lcd.print(currentCounter);
        }
        break;

      case 2: //Wecker
        switch (subMenuWecker)
        {
          case 1:
            if (editing == true)
            {
              if (bouncer == false)
              {
                Alarm1Minute = oldCounter;
                currentCounter = Alarm1RadioPreset;
                bouncer = true;
              }
              lcd.clear();
              if(currentCounter != 3)
              {
              lcd.print("Radio Preset: ");                              //NEED TO DO THIS WHEN RADIO IS DONE
              lcd.print(currentCounter);
              }
              if(currentCounter == 3)
              {
                lcd.print("Masochist");
              }

            }
            break;

          case 2:
            if (editing == true)
            {
              if (bouncer == false)
              {
                Alarm2Minute = oldCounter;
                currentCounter = Alarm2RadioPreset;
                bouncer = true;
              }
              lcd.clear();
              lcd.print("Radio Preset: ");                              //NEED TO DO THIS WHEN RADIO IS DONE
              lcd.print(currentCounter);
              if(currentCounter == 3)
              {
                lcd.print("Masochist");
              }
            }
        }

    }



  }


  //LEVEL 7 OF STATE MACHINE

  if (level == 7)
  {
    switch (menu)
    {
      case 1: //Minutes
        if (editing == true)
        {
          if (bouncer == false)
          {
            myHour = oldCounter;
            currentCounter = myMinute;
            bouncer = true;
          }
          lcd.clear();
          lcd.print("Minute: ");
          lcd.print(currentCounter);
        }
        break;

      case 2: //Wecker
        switch(subMenuWecker)
        {
          case 1:
            if(editing == true)
            {
              if(bouncer == false)
              {
                Alarm1RadioPreset = oldCounter;
                editing = false;
                level = 2;
                subMenuWecker = 1;
                bouncer = true;
                lcd.clear();
                lcd.print("Wecker 1");
              }
            }

          case 2:
            if(editing == true)
            {
              if(bouncer == false)
              {
                Alarm2RadioPreset = oldCounter;
                editing = false;
                level = 2;
                subMenuWecker = 2;
                bouncer = true;
                lcd.clear();
                lcd.print("Wecker 2");
              }
            }
        }
    }
  }

  //LEVEL 8 OF STATE MACHINE

  if (level == 8)
  {
    switch (menu)
    {
      case 1: //Go back to clock
        if (editing == true)
        {
          if (bouncer == false)
          {
            myMinute = oldCounter;
            myRTC.setDS1302Time(0, myMinute, myHour, myWeekday, myDay, myMonth, myYear); //sek,min,stunden, wochen tag, monats tag, monat, jahr
            currentCounter = 128;
            level = 1;    //back to first level of state machine
            editing = false;
            timeChanged = true;
            bouncer = true;
            lcd.clear();
          }
        }
    }
  }

}


void refreshTimeVariables()
{
  myRTC.updateTime();
  myHour = myRTC.hours;
  myMinute = myRTC.minutes;

}


void updateClockface()
{
  myRTC.updateTime();
  if (myMinute != myRTC.minutes)
  {
    myMinute = myRTC.minutes;
    Wecker();
  }
  if (myMinute != 0)
  {
    strip.clear();
    if (myMinute >= 31)
    {
     strip.setPixelColor(myMinute - 1, 0, 255, 0);
    }
    else
      strip.setPixelColor(myMinute - 1, 255, 0, 0);
    strip.show();
  }
  if ((myMinute == 0) && (KuckuckState == false))
  {
    strip.clear();
    strip.show();
  }

  if (myRTC.hours > 12)
    pointToHour = myRTC.hours - 12;
  else
    pointToHour = myRTC.hours;
  if (pointToHour == 0)
  {
    pointToHour = 12;
  }
  if (hourHand != pointToHour && editing == false)
  {
    if ((hourHand < pointToHour) && (hourHand != 12))
    {
      myStepper.step(-(stepsPerOutputRotation / 12 * (hourHand - pointToHour)));
    }

    if ((hourHand > pointToHour) && (hourHand != 12))
    {
      pointToHour = pointToHour + 12;
      myStepper.step(-(stepsPerOutputRotation / 12 * (hourHand - pointToHour)));
    }
    if (hourHand == 12)
    {
      if (pointToHour > 6)
      {
        myStepper.step(-(stepsPerOutputRotation / 12 * (12 - pointToHour)));
      }

      if (pointToHour <= 6)
      {
        myStepper.step((stepsPerOutputRotation / 12 * (pointToHour)));
      }
    }
    hourHand = pointToHour;
  }
}



void ServoMove180()
{
  for (byte i = 0; i <= 50; i++)
  {
    //this turns it to about 90 atm

    //it seems the larger smaller the high and the larger the dip the move it moves clockwise and vice versa
    // A pulse each 20ms
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(1300); // Duration of the pusle in microseconds
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(18700); // 20ms - duration of the pusle
    // Pulses duration: 600 - 0deg; 1450 - 90deg; 2300 - 180deg

    //high 1300 and low 19000 is 90 deg

    //high 400 and low 19600 is 180 deg
  }

}

void ServoMove0()
{
  for (byte i = 0; i < 50; i++)
  {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(400); // Duration of the pusle in microseconds
    digitalWrite(SERVO_PIN, LOW);
    delayMicroseconds(19600);
  }
}


void Kuckuck()        //need to make sure servo functions happen a few times
{
  if (KuckuckState == true)
  {
    boolean lightsLoading;
    if (KuckuckUp == false)
    {
      ServoMove180();
      KuckuckUp = true;


      for (int i = 0; i < 60;)
      {
        if (waiting == false)
        {
          strip.setPixelColor(i, strip.Color(255, 0, 0));       //  Set pixel's color (in RAM)
          i++;
        }
        wait(15);
        strip.show();
      }
      lightsLoading = false;
    }



    if (KuckuckCounter <= myRTC.hours && lightsLoading == false)
    {
      if (waiting == false && kuckuckLight == false)
      {
        digitalWrite(LED_PIN, HIGH);
        colorWipe(strip.Color(255,   0,   0)); // Red
        kuckuckLight = true;
      }
      if (kuckuckLight == true)
      {
        wait(1000);
      }
      if (waiting == false && kuckuckLight == true)
      {
        Serial.print("LIGHTS OFF");
        digitalWrite(LED_PIN, LOW);
        strip.clear();
        strip.show();
        kuckuckLight = false;
        KuckuckCounter++;
      }
      if (kuckuckLight == false)
      {
        wait(1000);
      }
    }


    if (KuckuckCounter >= myRTC.hours)
    {
      ServoMove0();
      KuckuckCheck = true;
      KuckuckUp = false;
    }
  }
}

void Wecker()
{

  //Wecker 1 Triggers
  if((myRTC.minutes ==Alarm1Minute) && (myRTC.hours == Alarm1Hour) && (myRTC.dayofweek == Alarm1Weekday) && Alarm1Triggered == false)
  {
    switch(Alarm1RadioPreset)
    {
      case 1:
           while(digitalRead(SW) == HIGH)
           {
             if(alarmLight == false)
             {
              for (int i = 0; i < 60;)
              {
               strip.setPixelColor(i, strip.Color(190,140,10));       //  Set pixel's color (in RAM)
               i++;
              }
              strip.show();
              alarmLight = true;
              }
              digitalWrite(JAMS, HIGH);
              if(Alarm1Triggered == false)
              { 
                  Wire.begin();
                  wait(2000);
                  if(waiting == false)
                  {
                  radio.setFrequency(frequencyPreset1);
                  Alarm1Triggered = true;
                  }
              }
           }
           strip.clear();
           digitalWrite(JAMS,LOW);
        break;

      case 2:
          while(digitalRead(SW) == HIGH)
          {
              if(alarmLight == false)
              {
                for (int i = 0; i < 60;)
                {
                  strip.setPixelColor(i, strip.Color(190,140,10));       //  Set pixel's color (in RAM)
                  i++;
                }
                strip.show();
                alarmLight = true;
              }
              digitalWrite(JAMS,HIGH);
              if(Alarm1Triggered == false)
                {
                  Wire.begin();
                  wait(2000);
                  if(waiting == false)
                  {
                  radio.setFrequency(frequencyPreset2);
                  Alarm1Triggered = true;
                  }
               }
          }
          strip.clear();
          digitalWrite(JAMS,LOW);
        break;
      
      

      case 3:   
      while(digitalRead(SW) == HIGH)
      {
        digitalWrite(LED_PIN,HIGH);
        ServoMove180();
        KuckuckUp = true;
        if(waiting == false)
        {
          for (int i = 0; i < 60;)
          {
           strip.setPixelColor(i, strip.Color(random(0,255),random(0,255),random(0,255)));       //  Set pixel's color (in RAM)
           i++;
          }
        }
        strip.show();
        wait(1000);
        Alarm1Triggered = true;
      }
      strip.clear();
      digitalWrite(LED_PIN, LOW);
      ServoMove0();
      KuckuckUp = false;
     break;
    }
   if(myRTC.hours >= 23 && myRTC.minutes >= 59 && Alarm1Triggered == true)
   {
     Alarm1Triggered = false;
   }
  
  }
  


  //Wecker 2 Triggers
  if((myRTC.minutes ==Alarm2Minute) && (myRTC.hours == Alarm2Hour) && (myRTC.dayofweek == Alarm2Weekday)  && ((Alarm2Triggered == false)))
  {
    switch(Alarm2RadioPreset)
    {
      case 1:
           while(digitalRead(SW) == HIGH)
           {
             if(alarmLight == false)
             {
              for (int i = 0; i < 60;)
              {
               strip.setPixelColor(i, strip.Color(190,140,10));       //  Set pixel's color (in RAM)
               i++;
              }
              strip.show();
              alarmLight = true;
              }
              digitalWrite(JAMS, HIGH);
              if(Alarm1Triggered == false)
              { 
                  Wire.begin();
                  wait(2000);
                  if(waiting == false)
                  {
                  radio.setFrequency(frequencyPreset1);
                  Alarm2Triggered = true;
                  }
              }
           }
           strip.clear();
           digitalWrite(JAMS,LOW);
        break;

      case 2:
          while(digitalRead(SW) == HIGH)
          {
              if(alarmLight == false)
              {
                for (int i = 0; i < 60;)
                {
                  strip.setPixelColor(i, strip.Color(190,140,10));       //  Set pixel's color (in RAM)
                  i++;
                }
                strip.show();
                alarmLight = true;
              }
              digitalWrite(JAMS,HIGH);
              if(Alarm1Triggered == false)
                {
                  Wire.begin();
                  wait(2000);
                  if(waiting == false)
                  {
                  radio.setFrequency(frequencyPreset2);
                  Alarm2Triggered = true;
                  }
               }
          }
          strip.clear();
          digitalWrite(JAMS,LOW);
        break;
      
      

      case 3:   
      while(digitalRead(SW) == HIGH)
      {
        digitalWrite(LED_PIN,HIGH);
        ServoMove180();
        KuckuckUp = true;
        if(waiting == false)
        {
          for (int i = 0; i < 60;)
          {
           strip.setPixelColor(i, strip.Color(random(0,255),random(0,255),random(0,255)));       //  Set pixel's color (in RAM)
           i++;
          }
        }
        strip.show();
        wait(1000);
        Alarm2Triggered = true;
      }
      strip.clear();
      digitalWrite(LED_PIN, LOW);
      ServoMove0();
      KuckuckUp = false;
     break;
   }

   if(myRTC.hours >= 23 && myRTC.minutes >= 59 && myRTC.dayofweek ==Alarm2Weekday && Alarm2Triggered == true)
   {
     Alarm2Triggered = false;
   }

   
  }
  
}



void colorWipe(uint32_t color)
{
  for (int i = 0; i < 60; i++)
  {
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
  }
  strip.show();
}


void wait(int waitTime)
{
  if (waiting == false)
  {
    Serial.println("Wait started");
    startWait = millis();
    waiting = true;
  }
  if (waiting == true)
  {
    Serial.println("Waiting...");
    if (millis() - startWait >= waitTime)
    {
      Serial.print("Wait ended");
      waiting = false;
    }
  }
}


void calibrateHourHand()                        //add this to date and time as automatic and also add manual option
{
  if (calibrated == false)
  {
    if (digitalRead(CAL_PIN) == HIGH)              //high because of pullup
    {
      myStepper.step(-(stepsPerOutputRotation / 50));
      Serial.println("spinning");
      Serial.println(digitalRead(CAL_PIN));
    }
    else
    {
      hourHand = 12;
      calibrated = true;
    }
  }
}



void nodeRed()
{

  //Function for Date Input

  if (Serial.available() > 0)
  {

    switch (Serial.read())
    {
      case 'd':
        for (int i = 0; i <= 2; i++)
        {
          date[i] = Serial.parseInt();
          if (i == 2 && date[0] != 0)
            dateInput = true;
          if(date[0] == 0)
            dateInput = false;
        }
        Serial.print("year");
        Serial.print(date[0]);
        Serial.print("month");
        Serial.print(date[1]);
        Serial.print("day");
        Serial.print(date[2]);
        if (dateInput == true)
        {
          myRTC.setDS1302Time(myRTC.seconds, myRTC.minutes, myRTC.hours, myRTC.dayofweek, date[2], date[1], date[0]);
        }
        wait(10000);
        break;



      case 't':
        for (int i = 0; i <= 2; i++)
        {
          times[i] = Serial.parseInt();
          if (i == 2 && times[2] != 0)
            timeInput = true;
        }
        Serial.print("hour");
        Serial.print(times[0]);
        Serial.print("minutes");
        Serial.print(times[1]);
        Serial.print("seconds");
        Serial.print(times[2]);
        if (timeInput == true)
        {
          myRTC.setDS1302Time(times[2], times[1], times[0], myRTC.dayofweek, myRTC.dayofmonth, myRTC.month, myRTC.year);
        }
        wait(100);
        break;

      case 'b':
        nodeRedBannerl1 = Serial.readString();
        nodeRedBannerl2 = nodeRedBannerl1.substring(15);
        bannerInput = true;
        wait(100);
        break;

    }

    if (waiting == false)
    {
      dateInput = false;
    }
    if (waiting == false)
    {
      timeInput = false;
    }
    if (waiting == false)
    {
      bannerInput = false;
    }
  }
}










void printTime()
{
  myRTC.updateTime();
  lcd.setCursor(0, 0);
  if (myRTC.hours < 10)
  {
    lcd.print("0");
  }
  lcd.print(myRTC.hours);
  lcd.print(":");
  if (myRTC.minutes < 10)
  {
    lcd.setCursor(3, 0);
    lcd.print("0");
  }
  lcd.print(myRTC.minutes);
  lcd.setCursor(0, 1);
  if (myRTC.dayofmonth < 10)
  {
    lcd.print("0");
  }
  lcd.print(myRTC.dayofmonth);
  lcd.print("/");
  if (myRTC.month < 10)
  {
    lcd.setCursor(3, 1);
    lcd.print("0");
  }
  lcd.print(myRTC.month);
  lcd.print("/");

  lcd.print(myRTC.year);
}





//////////////////////////////////////////////DCF/////////////////////////////////////////////////////////////////////








    
/**
 * Initialize the DCF77 routines: initialize the variables,
 * configure the interrupt behaviour.
 */
void DCF77Init() {
  previousSignalState=0;
  previousFlankTime=0;
  bufferPosition=0;
  dcf_rx_buffer=0;
  ss=mm=hh=day=weekday=mon=year=0;
  pinMode(DCF77PIN, INPUT);

  //Timer2 Settings: Timer Prescaler /64, 
  TCCR2 |= (1<<CS22);    // turn on CS22 bit
  TCCR2 &= ~((1<<CS21) | (1<<CS20));    // turn off CS21 and CS20 bits   
  // Use normal mode
  TCCR2 &= ~((1<<WGM21) | (1<<WGM20));   // turn off WGM21 and WGM20 bits 
  // Use internal clock - external clock not used in Arduino
  ASSR |= (0<<AS2);
  TIMSK |= (1<<TOIE2) | (0<<OCIE2);        //Timer2 Overflow Interrupt Enable  
  RESET_TIMER2; 
  attachInterrupt(0, int0handler, CHANGE);
}

/**
 * Append a signal to the dcf_rx_buffer. Argument can be 1 or 0. An internal
 * counter shifts the writing position within the buffer. If position > 59,
 * a new minute begins -> time to call finalizeBuffer().
 */
void appendSignal(unsigned char signal) 
{
  dcf_rx_buffer = dcf_rx_buffer | ((unsigned long long) signal << bufferPosition);
  // Update the parity bits. First: Reset when minute, hour or date starts.
  if (bufferPosition ==  21 || bufferPosition ==  29 || bufferPosition ==  36) {
  flags.parity_flag = 0;
  }
  // save the parity when the corresponding segment ends
  if (bufferPosition ==  28) {flags.parity_min = flags.parity_flag;};
  if (bufferPosition ==  35) {flags.parity_hour = flags.parity_flag;};
  if (bufferPosition ==  58) {flags.parity_date = flags.parity_flag;};
  // When we received a 1, toggle the parity flag
  if (signal == 1) 
  {
    flags.parity_flag = flags.parity_flag ^ 1;
  }
  bufferPosition++;
  if (bufferPosition > 59) 
  {
    finalizeBuffer();
  }
}

/**
 * Evaluates the information stored in the buffer. This is where the DCF77
 * signal is decoded and the internal clock is updated.
 */
void finalizeBuffer(void) 
{
  if (bufferPosition == 59) 
  {
    struct DCF77Buffer *rx_buffer;
    rx_buffer = (struct DCF77Buffer *)(unsigned long long)&dcf_rx_buffer;
    if (flags.parity_min == rx_buffer->P1  && flags.parity_hour == rx_buffer->P2  && flags.parity_date == rx_buffer->P3) 
    { 

      //convert the received bits from BCD
      mm = rx_buffer->Min-((rx_buffer->Min/16)*6);
      hh = rx_buffer->Hour-((rx_buffer->Hour/16)*6);
      day= rx_buffer->Day-((rx_buffer->Day/16)*6); 
      weekday = rx_buffer->Weekday-((rx_buffer->Weekday/16)*6); 
      mon= rx_buffer->Month-((rx_buffer->Month/16)*6);
      year= 2000 + rx_buffer->Year-((rx_buffer->Year/16)*6);
    }
  } 
  // reset stuff
  ss = 0;
  bufferPosition = 0;
  dcf_rx_buffer=0;
}

/**
 * Dump the time to the serial line.
 */
void updateTimeVariablesformDCF77(void)
{
  /*
  lcd.setCursor(0,0);
  lcd.print("Time: ");
  lcd.print(hh);
  lcd.print(":");
  lcd.print(mm);
  lcd.print(":");
  lcd.print(ss);
  lcd.print("T");
  lcd.print(weekday);
  lcd.setCursor(0,1);
  lcd.print(" Date: ");
  lcd.print(day);
  lcd.print(".");
  lcd.print(mon);
  lcd.print(".");
  lcd.println(year);
*/
if(hh!=0 && mm!= 0)
{
  myRTC.setDS1302Time(ss, mm, hh, weekday, day, mon, year); //sek,min,stunden, wochen tag, monats tag, monat, jahr
  timeUpdatedDCF77 = true;
}
 
}

/**
 * Evaluates the signal as it is received. Decides whether we received
 * a "1" or a "0" based on the 
 */
void scanSignal(void){
    if (DCFSignalState == 1) {
      int thisFlankTime=millis();
      lcd.setCursor(0,1);             //insert searching here
      lcd.print("Suchen...");
      if (thisFlankTime - previousFlankTime > DCF_sync_millis) 
      {
        updateTimeVariablesformDCF77();
        finalizeBuffer();
      }
      previousFlankTime=thisFlankTime;



    } 
    else {
      /* or a falling flank */
      int difference=millis() - previousFlankTime;

      if (difference < DCF_split_millis) {
        appendSignal(0);
      } 
      else {
        appendSignal(1);
      }
    }
}


//The interrupt routine for counting seconds - increment hh:mm:ss.

ISR(TIMER2_OVF_vect) {
  RESET_TIMER2;
  tick_counter += 1;
  if (tick_counter == 1000) {
    ss++;
    if (ss==60) {
      ss=0;
      mm++;
      if (mm==60) {
        mm=0;
        hh++;
        if (hh==24) 
          hh=0;
      }
    }
    tick_counter = 0;
  }
};


void int0handler() {
  // check the value again - since it takes some time to
  // activate the interrupt routine, we get a clear signal.
  DCFSignalState = digitalRead(DCF77PIN);
}


void getTimeDCF77(void) {
  if (ss != previousSecond) {
    updateTimeVariablesformDCF77();
    previousSecond = ss;
  }
  if (DCFSignalState != previousSignalState) 
  {   
    scanSignal();
    if (DCFSignalState)
    {
      strip.setPixelColor(27, strip.Color(0,0,10));
      strip.show();
    } 
    else 
    {
      strip.clear();
      strip.show();
    }
    previousSignalState = DCFSignalState;
  }
}
