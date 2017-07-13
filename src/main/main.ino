/*
 * Written by Tyler Martin
 * 
 */

#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>

#include <CountUpDownTimer.h>

#include <Adafruit_LEDBackpack.h> // For Control Panel Displays
#include <LedControl.h>           // For Shower Room Displays
#include <LiquidCrystal.h>

const float version  = 0.7;

// Define Inputs
const int PBPins[] = {
                       26,27,28,29, //Timer 1 Inc,Dec,Reset,XX
                       22,23,24,25, //Timer 2 Inc,Dec,Reset,XX
                       34,24,     //Start Room 1, Start Room 2,
                       30,25,     //Start Shower 1, Start Shower 2,
};
const int PBcount = 12;

// Define Outputs

const int ColdWater1Relay = 4;
const int HotWater1Relay  = 3;
const int ColdWater2Relay = 2;
const int HotWater2Relay  = 5;

const int Room1Buzzer     = 36;
const int Room2Buzzer     = 7;

// Define Relay State
const int WaterFlowing    = HIGH;
const int WaterBlocked    = LOW;

// Define displays
Adafruit_7segment Room1TimerCPDis = Adafruit_7segment();
Adafruit_7segment Room2TimerCPDis = Adafruit_7segment();
Adafruit_7segment Water1TimerCPDis = Adafruit_7segment();
Adafruit_7segment Water2TimerCPDis = Adafruit_7segment();

//LedControl VarName = LedControl(DATA PIN, CLK PIN, CS PIN);
LedControl Room1TimerDis = LedControl(51,52,53,2);
LedControl Room2TimerDis = LedControl(51,52,53,2);

const int BlinkRate = 500;
int BlinkTime = 0;

enum TimerDisplay {
  none,
  room,
  water
};

TimerDisplay SelecedTimer1Display = none;
TimerDisplay SelecedTimer2Display = none;

// Define timers
CountUpDownTimer Room1Timer(DOWN);
CountUpDownTimer Water1Timer(DOWN);
CountUpDownTimer Room2Timer(DOWN);
CountUpDownTimer Water2Timer(DOWN);
CountUpDownTimer Room1OverTimer(UP);
CountUpDownTimer Room2OverTimer(UP);

// Default Times
int RoomTimeMinutes   = 15;
int ShowerTimeMinutes = 7;

//Define Button Debounce
const int debounceDelay   = 250; //milliseconds
const int debounceWindow = 100; //milliseconds

long PBTimes[] = {
                       0,0,0,0, //Timer 1 Inc,Dec,Reset,XX
                       0,0,0,0, //Timer 2 Inc,Dec,Reset,XX
                       0,0,     //Start Room 1, Start Room 2,
                       0,0      //Start Shower 1, Start Shower 2,
};

int PBLastStates[] = {
                       0,0,0,0, //Timer 1 Inc,Dec,Reset,XX
                       0,0,0,0, //Timer 2 Inc,Dec,Reset,XX
                       0,0,     //Start Room 1, Start Room 2,
                       0,0      //Start Shower 1, Start Shower 2,
};

int PBStates[] = {
                       LOW,LOW,LOW,LOW, //Timer 1 Inc,Dec,Reset,XX
                       LOW,LOW,LOW,LOW, //Timer 2 Inc,Dec,Reset,XX
                       LOW,LOW,         //Start Room 1, Start Room 2,
                       LOW,LOW          //Start Shower 1, Start Shower 2,
};

bool PBOneShots[] = {
                       false,false,false,false, //Timer 1 Inc,Dec,Reset,XX
                       false,false,false,false, //Timer 2 Inc,Dec,Reset,XX
                       false,false,             //Start Room 1, Start Room 2,
                       false,false              //Start Shower 1, Start Shower 2,
};


//define flags
bool ForceTimerUpdate    = false;

bool Room1TimerRunning   = false;
bool Room2TimerRunning   = false;

bool Shower1HasNotRun    = true;
bool Shower2HasNotRun    = true;

void setup()
{
  Serial.begin(9600);
  Serial.println("Exit 0 Shower System");
  Serial.println("Written by Tyler Martin");
  Serial.print("Firmware Version: ");
  Serial.println(version);
  
  //Init I2C device addresses
  Room1TimerCPDis.begin(0x72);  //Set address for room timer 1 control panel display
  Water1TimerCPDis.begin(0x73); //Set address for water timer 1 control panel display

  Room2TimerCPDis.begin(0x70);  //Set address for room timer 2 control panel display
  Water2TimerCPDis.begin(0x71); //Set address for water timer 2 control panel display

  //Init Timers
  Room1Timer.SetTimer(0,RoomTimeMinutes,0); 
  Water1Timer.SetTimer(0,ShowerTimeMinutes,0);

  Room2Timer.SetTimer(0,RoomTimeMinutes,0);
  Water2Timer.SetTimer(0,ShowerTimeMinutes,0);

  Room1OverTimer.SetTimer(0,0,0);
  Room2OverTimer.SetTimer(0,0,0);

  // I/O Setup
  for (int thisPB = 0; thisPB < PBcount; thisPB++)
  {
    pinMode(PBPins[thisPB], INPUT);
  }
  
  pinMode(ColdWater1Relay, OUTPUT);
  pinMode(HotWater1Relay, OUTPUT);
  pinMode(ColdWater2Relay, OUTPUT);
  pinMode(HotWater2Relay, OUTPUT);

  pinMode(Room1Buzzer, OUTPUT);
  pinMode(Room2Buzzer, OUTPUT);

  //Set Outputs
  digitalWrite(ColdWater1Relay, WaterBlocked);
  digitalWrite(HotWater1Relay, WaterBlocked);
  digitalWrite(ColdWater2Relay, WaterBlocked);
  digitalWrite(HotWater2Relay, WaterBlocked);

  digitalWrite(Room1Buzzer, LOW);
  digitalWrite(Room2Buzzer, LOW);
  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  Room1TimerDis.shutdown(0,false);
  Room1TimerDis.shutdown(1,false);
  /* Set the brightness to a medium values */
  Room1TimerDis.setIntensity(0,15);
  Room1TimerDis.setIntensity(1,15);
  /* and clear the display */
  Room1TimerDis.clearDisplay(0);
  Room1TimerDis.clearDisplay(1);

  Room1TimerDis.setScanLimit(1,5);

  Room2TimerDis.shutdown(0,false);
  Room2TimerDis.shutdown(1,false);
  /* Set the brightness to a medium values */
  Room2TimerDis.setIntensity(0,15);
  Room2TimerDis.setIntensity(1,15);
  /* and clear the display */
  Room2TimerDis.clearDisplay(0);
  Room2TimerDis.clearDisplay(1);

  Room2TimerDis.setScanLimit(1,5);

 delay(1500);
 ForceTimerUpdate = true;
}


void loop() {
  // Reset Vars on loop
  boolean drawDots = false;
  int tensmin,onesmin,tenssec,onessec = 0;
  int PBCurrStates[] = {
                       0,0,0,0, //Timer 1 Inc,Dec,Sel,Save
                       0,0,0,0, //Timer 2 Inc,Dec,Sel,Save
                       0,0,     //Start Room 1, Start Room 2,
                       0,0,     //Start Shower 1, Start Shower 2,
  };
  // Read Button States
  for (int thisPB = 0; thisPB < PBcount; thisPB++)
  {
    PBCurrStates[thisPB]  = digitalRead(PBPins[thisPB]);
  }
  
  // Check to see if Button State has changed 
  for (int thisPB = 0; thisPB < PBcount; thisPB++)
  {
    if (PBCurrStates[thisPB] != PBLastStates[thisPB])
    {
      PBTimes[thisPB] = millis(); 
      PBLastStates[thisPB] = PBCurrStates[thisPB]; 
    }
  }

  // Check to see if debounce timer has expired
  for (int thisPB = 0; thisPB < PBcount; thisPB++)
  {
    if ((((millis() - PBTimes[thisPB]) > debounceDelay)) && (((millis() - PBTimes[thisPB]) < (debounceDelay + debounceWindow))))
    {
      PBTimes[thisPB] = 0;
      PBStates[thisPB] = PBCurrStates[thisPB];
      PBOneShots[thisPB] = true;
    }
  }
  
  //Do something based on button state
  
  //Array Pos 0
  //Timer 1 Increment
  if ((PBStates[0] == HIGH) && (PBOneShots[0] == true))
  {
    PBOneShots[0] = false;
    Serial.println("INC 1 Pressed");
    if(Water1Timer.ShowMinutes() <= 58)
    {
      Water1Timer.SetTimer(0,Water1Timer.ShowMinutes()+1,Water1Timer.ShowSeconds()); 
      ForceTimerUpdate = true;
    }
  }
  else if (PBStates[0] == LOW && PBOneShots[0] == true)
  {
    PBOneShots[0] = false;
    Serial.println("INC 1 Released");
  }

  //Array Pos 1
  //Timer 1 Decrement
  if ((PBStates[1] == HIGH) && (PBOneShots[1] == true))
  {
    PBOneShots[1] = false;
    Serial.println("DEC 1 Pressed");
    if(Water1Timer.ShowMinutes() > 0)
    {
      Water1Timer.SetTimer(0,Water1Timer.ShowMinutes()-1,Water1Timer.ShowSeconds()); 
      ForceTimerUpdate = true;
    }
  }
  else if (PBStates[1] == LOW && PBOneShots[1] == true)
  {
    PBOneShots[1] = false;
    Serial.println("DEC 1 Released");
  }

  //Array Pos 2
  //Timer 1 XX
  
  
  //Array Pos 3
  //Timer 1 Reset
  if ((PBStates[3] == HIGH) && (PBOneShots[3] == true))
  {
    PBOneShots[3] = false;
    Serial.println("Reset 1 Pressed");
    Room1Timer.StopTimer();
    Water1Timer.StopTimer();
    Room1Timer.SetTimer(0,RoomTimeMinutes,0); 
    Water1Timer.SetTimer(0,ShowerTimeMinutes,0);
    Room1TimerRunning = false;
    Shower1HasNotRun = true;
    digitalWrite(Room1Buzzer, LOW);
    digitalWrite(ColdWater1Relay, WaterBlocked);
    digitalWrite(HotWater1Relay, WaterBlocked);
    
  }
  else if (PBStates[3] == LOW && PBOneShots[3] == true)
  {
    PBOneShots[3] = false;
    Serial.println("Reset 1 Released");
  }
  //Array Pos 4
  //Timer 2 Increment
  if ((PBStates[4] == HIGH) && (PBOneShots[4] == true))
  {
    PBOneShots[4] = false;
    Serial.println("INC 2 Pressed");
    if(Room2Timer.ShowMinutes() <= 58)
    {
      Room2Timer.SetTimer(0,Room2Timer.ShowMinutes()+1,Room2Timer.ShowSeconds()); 
      ForceTimerUpdate = true;
    }
  }
  else if (PBStates[4] == LOW && PBOneShots[4] == true)
  {
    PBOneShots[4] = false;
    Serial.println("INC 2 Released");
  }
  
  //Array Pos 5
  //Timer 1 Decrement
  if ((PBStates[5] == HIGH) && (PBOneShots[5] == true))
  {
    PBOneShots[5] = false;
    Serial.println("DEC 2 Pressed");
    if(Room2Timer.ShowMinutes() > 0)
    {
      Room2Timer.SetTimer(0,Room2Timer.ShowMinutes()-1,Room2Timer.ShowSeconds()); 
      ForceTimerUpdate = true;
    }
  }
  else if (PBStates[5] == LOW && PBOneShots[5] == true)
  {
    PBOneShots[5] = false;
    Serial.println("DEC 2 Released");
  }
  
  //Array Pos 6
  //Room 2 Reset
  if ((PBStates[6] == HIGH) && (PBOneShots[6] == true))
  {
    PBOneShots[6] = false;
    Serial.println("Reset 2 Pressed");
    Room2Timer.StopTimer();
    Water2Timer.StopTimer();
    Room2Timer.SetTimer(0,RoomTimeMinutes,0); 
    Water2Timer.SetTimer(0,ShowerTimeMinutes,0);
    Room2TimerRunning = false;
    Shower2HasNotRun = true;
    digitalWrite(Room2Buzzer, LOW);
    digitalWrite(ColdWater2Relay, WaterBlocked);
    digitalWrite(HotWater2Relay, WaterBlocked);
  }
  else if (PBStates[6] == LOW && PBOneShots[6] == true)
  {
    PBOneShots[6] = false;
    Serial.println("Reset 2 Released");
  }
  
  //Array Pos 7
  //Room 2 XX
  
  //Array Pos 8
  //Start Room 1 Timer
  if ((PBStates[8] == HIGH) && (PBOneShots[8] == true))
  {
    PBOneShots[8] = false;
    Serial.println("Room 1 Pressed");
    Room1Timer.StartTimer();
    Room1TimerRunning = true;
  }
  else if (PBStates[8] == LOW && PBOneShots[8] == true)
  {
    PBOneShots[8] = false;
    Serial.println("Room 1 Released");
  }
  
  //Array Pos 9
  //Start Room 2 Timer
  if ((PBStates[9] == HIGH) && (PBOneShots[9] == true))
  {
    PBOneShots[9] = false;
    Serial.println("Room 2 Pressed");
    Room2Timer.StartTimer();
    Room2TimerRunning = true;
  }
  else if (PBStates[9] == LOW && PBOneShots[9] == true)
  {
    PBOneShots[9] = false;
    Serial.println("Room 2 Released");
  }
  
  //Array Pos 10
  //Start Room 1 Shower Timer
  if ((PBStates[10] == HIGH) && (PBOneShots[10] == true))
  {
    PBOneShots[10] = false;
    Serial.println("Shower 1 Pressed");
    if(Room1TimerRunning && Shower1HasNotRun)
    {
      Serial.println("Shower 1 Timer Started");
      Water1Timer.StartTimer();
      digitalWrite(ColdWater1Relay, WaterFlowing);
      digitalWrite(HotWater1Relay, WaterFlowing);
      Shower1HasNotRun = false; 
    }
    
  }
  else if (PBStates[10] == LOW && PBOneShots[10] == true)
  {
    PBOneShots[10] = false;
    Serial.println("Shower 1 Released");
  }

  //Array Pos 11
  //Start Room 2 Shower Timer
  if ((PBStates[11] == HIGH) && (PBOneShots[11] == true))
  {
    PBOneShots[11] = false;
    Serial.println("Shower 2 Pressed");
    if(Room2TimerRunning && Shower1HasNotRun)
    {
      Serial.println("Shower 2 Timer Started");
      Water2Timer.StartTimer();
      digitalWrite(ColdWater2Relay, WaterFlowing);
      digitalWrite(HotWater2Relay, WaterFlowing); 
      Shower2HasNotRun = false;
    }
  }
  else if (PBStates[11] == LOW && PBOneShots[11] == true)
  {
    PBOneShots[11] = false;
    Serial.println("Shower 2 Released");
  }


  // Update timer information
  Room1Timer.Timer();
  Water1Timer.Timer();
  Room2Timer.Timer();
  Water2Timer.Timer();
  Room1OverTimer.Timer();
  Room2OverTimer.Timer();
  
  // Update display if timer has changed 
  if (Room1Timer.TimeHasChanged() || ForceTimerUpdate )
  {
    tensmin = Room1Timer.ShowMinutes()/10;
    onesmin = Room1Timer.ShowMinutes()%10;
    tenssec = Room1Timer.ShowSeconds()/10;
    onessec = Room1Timer.ShowSeconds()%10;
    if(onessec%2){drawDots=true;}
    else {drawDots=false;}
    Room1TimerCPDis.writeDigitNum(0,tensmin);
    Room1TimerCPDis.writeDigitNum(1,onesmin);
    Room1TimerCPDis.drawColon(drawDots);
    Room1TimerCPDis.writeDigitNum(3,tenssec);
    Room1TimerCPDis.writeDigitNum(4,onessec);
    Room1TimerCPDis.writeDisplay();

    Room1TimerDis.setDigit(1,0,tensmin,0);
    Room1TimerDis.setDigit(1,1,onesmin,0);
    Room1TimerDis.setDigit(1,4,0,drawDots);
    Room1TimerDis.setDigit(1,2,tenssec,0);
    Room1TimerDis.setDigit(1,3,onessec,0);
    
  }

  if (Water1Timer.TimeHasChanged() || ForceTimerUpdate)
  {
    tensmin = Water1Timer.ShowMinutes()/10;
    onesmin = Water1Timer.ShowMinutes()%10;
    tenssec = Water1Timer.ShowSeconds()/10;
    onessec = Water1Timer.ShowSeconds()%10;
    if(onessec%2){drawDots=true;}
    else {drawDots=false;}
    Water1TimerCPDis.writeDigitNum(0,tensmin);
    Water1TimerCPDis.writeDigitNum(1,onesmin);
    Water1TimerCPDis.drawColon(drawDots);
    Water1TimerCPDis.writeDigitNum(3,tenssec);
    Water1TimerCPDis.writeDigitNum(4,onessec);
    Water1TimerCPDis.writeDisplay();
    if(tensmin)
    {
      SetRoom1Digit(0,tensmin);
    }
    else
    {
      SetRoom1Digit(0,10);// Blank Digit
    }
    SetRoom1Digit(1,onesmin);
    SetRoom1Colon(drawDots);
    SetRoom1Digit(2,tenssec);
    SetRoom1Digit(3,onessec);
  }

  if (Room2Timer.TimeHasChanged() || ForceTimerUpdate)
  {
    tensmin = Room2Timer.ShowMinutes()/10;
    onesmin = Room2Timer.ShowMinutes()%10;
    tenssec = Room2Timer.ShowSeconds()/10;
    onessec = Room2Timer.ShowSeconds()%10;
    if(onessec%2){drawDots=true;}
    else {drawDots=false;}
    Room2TimerCPDis.writeDigitNum(0,tensmin);
    Room2TimerCPDis.writeDigitNum(1,onesmin);
    Room2TimerCPDis.drawColon(drawDots);
    Room2TimerCPDis.writeDigitNum(3,tenssec);
    Room2TimerCPDis.writeDigitNum(4,onessec);
    Room2TimerCPDis.writeDisplay();
  }

  if (Water2Timer.TimeHasChanged() || ForceTimerUpdate )
  {
    tensmin = Water2Timer.ShowMinutes()/10;
    onesmin = Water2Timer.ShowMinutes()%10;
    tenssec = Water2Timer.ShowSeconds()/10;
    onessec = Water2Timer.ShowSeconds()%10;
    if(onessec%2){drawDots=true;}
    else {drawDots=false;}
    Water2TimerCPDis.writeDigitNum(0,tensmin);
    Water2TimerCPDis.writeDigitNum(1,onesmin);
    Water2TimerCPDis.drawColon(drawDots);
    Water2TimerCPDis.writeDigitNum(3,tenssec);
    Water2TimerCPDis.writeDigitNum(4,onessec);
    Water2TimerCPDis.writeDisplay();
  }

  //Check to see if buzzer needs to fire
  
//Room 1
  if (Water1Timer.TimeCheck(0,1,0))
  {
    digitalWrite(Room1Buzzer, HIGH);
  }

  if (Water1Timer.TimeCheck(0,0,59))
  {
   digitalWrite(Room1Buzzer, LOW); 
  }

  if (Water1Timer.TimeCheck(0,0,50))
  {
    digitalWrite(Room1Buzzer, HIGH);
  }

  if (Water1Timer.TimeCheck(0,0,49))
  {
   digitalWrite(Room1Buzzer, LOW); 
  }

  if (Water1Timer.TimeCheck(0,0,40))
  {
    digitalWrite(Room1Buzzer, HIGH);
  }

  if (Water1Timer.TimeCheck(0,0,39))
  {
   digitalWrite(Room1Buzzer, LOW); 
  }

  if (Water1Timer.TimeCheck(0,0,30))
  {
    digitalWrite(Room1Buzzer, HIGH);
  }

  if (Water1Timer.TimeCheck(0,0,29))
  {
   digitalWrite(Room1Buzzer, LOW); 
  }

  if (Water1Timer.TimeCheck(0,0,20))
  {
    digitalWrite(Room1Buzzer, HIGH);
  }

  if (Water1Timer.TimeCheck(0,0,19))
  {
   digitalWrite(Room1Buzzer, LOW); 
  }
  
  if (Water1Timer.TimeCheck(0,0,10))
  {
    digitalWrite(Room1Buzzer, HIGH);
  }

  if (Water1Timer.TimeCheck(0,0,9))
  {
   digitalWrite(Room1Buzzer, LOW); 
  }

  if (Water1Timer.TimeCheck(0,0,2))
  {
    digitalWrite(Room1Buzzer, HIGH);
  }

  if (Water1Timer.TimeCheck(0,0,0))
  {
   digitalWrite(Room1Buzzer, LOW); 
  }
  
  if (Water1Timer.TimeCheck(0,0,0))
  {
   digitalWrite(ColdWater1Relay, WaterBlocked);
   digitalWrite(HotWater1Relay, WaterBlocked); 
  }
  
//Room 2
  if (Water2Timer.TimeCheck(0,1,0))
  {
    digitalWrite(Room2Buzzer, HIGH);
  }

  if (Water2Timer.TimeCheck(0,0,59))
  {
   digitalWrite(Room2Buzzer, LOW); 
  }

  if (Water2Timer.TimeCheck(0,0,50))
  {
    digitalWrite(Room2Buzzer, HIGH);
  }

  if (Water2Timer.TimeCheck(0,0,49))
  {
   digitalWrite(Room2Buzzer, LOW); 
  }

  if (Water2Timer.TimeCheck(0,0,40))
  {
    digitalWrite(Room2Buzzer, HIGH);
  }

  if (Water2Timer.TimeCheck(0,0,39))
  {
   digitalWrite(Room2Buzzer, LOW); 
  }

  if (Water2Timer.TimeCheck(0,0,30))
  {
    digitalWrite(Room2Buzzer, HIGH);
  }

  if (Water2Timer.TimeCheck(0,0,29))
  {
   digitalWrite(Room2Buzzer, LOW); 
  }

  if (Water2Timer.TimeCheck(0,0,20))
  {
    digitalWrite(Room2Buzzer, HIGH);
  }

  if (Water2Timer.TimeCheck(0,0,19))
  {
   digitalWrite(Room2Buzzer, LOW); 
  }

  if (Water2Timer.TimeCheck(0,0,10))
  {
    digitalWrite(Room2Buzzer, HIGH);
  }

  if (Water2Timer.TimeCheck(0,0,9))
  {
   digitalWrite(Room2Buzzer, LOW); 
  }

  if (Water2Timer.TimeCheck(0,0,2))
  {
    digitalWrite(Room2Buzzer, HIGH);
  }

  if (Water2Timer.TimeCheck(0,0,0))
  {
   digitalWrite(Room2Buzzer, LOW); 
  }
  
  if (Water2Timer.TimeCheck(0,0,0))
  {
   digitalWrite(ColdWater2Relay, WaterBlocked);
   digitalWrite(HotWater2Relay, WaterBlocked); 
  }

//Check to see if Shower timers have started

  if(Room1Timer.TimeCheck(0,ShowerTimeMinutes,1) && Shower1HasNotRun)
  {
    Serial.println("Shower 1 Timer Started");
    Water1Timer.StartTimer();
    digitalWrite(ColdWater1Relay, WaterFlowing);
    digitalWrite(HotWater1Relay, WaterFlowing);
    Shower1HasNotRun = false;
  }

  if(Room2Timer.TimeCheck(0,ShowerTimeMinutes,1) && Shower2HasNotRun)
  {
    Serial.println("Shower 2 Timer Started");
    Water2Timer.StartTimer();
    digitalWrite(ColdWater2Relay, WaterFlowing);
    digitalWrite(HotWater2Relay, WaterFlowing);
    Shower2HasNotRun = false;
  }
  
  ForceTimerUpdate = false;
  
}//END MAIN

//***************************************************************************************************
//***************************************************************************************************
//***************************************************************************************************
void SetRoom1Colon(bool state)
{
  Room1TimerDis.setLed(0,7,3,state);
}

void SetRoom2Colon(bool state)
{
  Room2TimerDis.setLed(0,7,3,state);
}

void SetRoom1Digit(int digit, int value)
{
  //digit is 0 based
  //a value of 10 will blank the digit
  switch(value)
  {
    case 0:
      Room1TimerDis.setLed(0,0,(digit + 1),1);//a
      Room1TimerDis.setLed(0,1,(digit + 1),1);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),1);//d
      Room1TimerDis.setLed(0,4,(digit + 1),1);//e
      Room1TimerDis.setLed(0,5,(digit + 1),1);//f
      Room1TimerDis.setLed(0,6,(digit + 1),0);//g
      break;
    case 1:
      Room1TimerDis.setLed(0,0,(digit + 1),0);//a
      Room1TimerDis.setLed(0,1,(digit + 1),1);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),0);//d
      Room1TimerDis.setLed(0,4,(digit + 1),0);//e
      Room1TimerDis.setLed(0,5,(digit + 1),0);//f
      Room1TimerDis.setLed(0,6,(digit + 1),0);//g
      break;
    case 2:
      Room1TimerDis.setLed(0,0,(digit + 1),1);//a
      Room1TimerDis.setLed(0,1,(digit + 1),1);//b
      Room1TimerDis.setLed(0,2,(digit + 1),0);//c
      Room1TimerDis.setLed(0,3,(digit + 1),1);//d
      Room1TimerDis.setLed(0,4,(digit + 1),1);//e
      Room1TimerDis.setLed(0,5,(digit + 1),0);//f
      Room1TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 3:
      Room1TimerDis.setLed(0,0,(digit + 1),1);//a
      Room1TimerDis.setLed(0,1,(digit + 1),1);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),1);//d
      Room1TimerDis.setLed(0,4,(digit + 1),0);//e
      Room1TimerDis.setLed(0,5,(digit + 1),0);//f
      Room1TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 4:
      Room1TimerDis.setLed(0,0,(digit + 1),0);//a
      Room1TimerDis.setLed(0,1,(digit + 1),1);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),0);//d
      Room1TimerDis.setLed(0,4,(digit + 1),0);//e
      Room1TimerDis.setLed(0,5,(digit + 1),1);//f
      Room1TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 5:
      Room1TimerDis.setLed(0,0,(digit + 1),1);//a
      Room1TimerDis.setLed(0,1,(digit + 1),0);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),1);//d
      Room1TimerDis.setLed(0,4,(digit + 1),0);//e
      Room1TimerDis.setLed(0,5,(digit + 1),1);//f
      Room1TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 6:
      Room1TimerDis.setLed(0,0,(digit + 1),1);//a
      Room1TimerDis.setLed(0,1,(digit + 1),0);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),1);//d
      Room1TimerDis.setLed(0,4,(digit + 1),1);//e
      Room1TimerDis.setLed(0,5,(digit + 1),1);//f
      Room1TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 7:
      Room1TimerDis.setLed(0,0,(digit + 1),1);//a
      Room1TimerDis.setLed(0,1,(digit + 1),1);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),0);//d
      Room1TimerDis.setLed(0,4,(digit + 1),0);//e
      Room1TimerDis.setLed(0,5,(digit + 1),0);//f
      Room1TimerDis.setLed(0,6,(digit + 1),0);//g
      break;
    case 8:
      Room1TimerDis.setLed(0,0,(digit + 1),1);//a
      Room1TimerDis.setLed(0,1,(digit + 1),1);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),1);//d
      Room1TimerDis.setLed(0,4,(digit + 1),1);//e
      Room1TimerDis.setLed(0,5,(digit + 1),1);//f
      Room1TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 9:
      Room1TimerDis.setLed(0,0,(digit + 1),1);//a
      Room1TimerDis.setLed(0,1,(digit + 1),1);//b
      Room1TimerDis.setLed(0,2,(digit + 1),1);//c
      Room1TimerDis.setLed(0,3,(digit + 1),1);//d
      Room1TimerDis.setLed(0,4,(digit + 1),0);//e
      Room1TimerDis.setLed(0,5,(digit + 1),1);//f
      Room1TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 10:
      Room1TimerDis.setLed(0,0,(digit + 1),0);//a
      Room1TimerDis.setLed(0,1,(digit + 1),0);//b
      Room1TimerDis.setLed(0,2,(digit + 1),0);//c
      Room1TimerDis.setLed(0,3,(digit + 1),0);//d
      Room1TimerDis.setLed(0,4,(digit + 1),0);//e
      Room1TimerDis.setLed(0,5,(digit + 1),0);//f
      Room1TimerDis.setLed(0,6,(digit + 1),0);//g
      break;
    default:
      Room1TimerDis.setLed(0,0,(digit + 1),0);//a
      Room1TimerDis.setLed(0,1,(digit + 1),0);//b
      Room1TimerDis.setLed(0,2,(digit + 1),0);//c
      Room1TimerDis.setLed(0,3,(digit + 1),0);//d
      Room1TimerDis.setLed(0,4,(digit + 1),0);//e
      Room1TimerDis.setLed(0,5,(digit + 1),0);//f
      Room1TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
  }
}

void SetRoom2Digit(int digit, int value)
{
  //digit is 0 based
  switch(value)
  {
    case 0:
      Room2TimerDis.setLed(0,0,(digit + 1),1);//a
      Room2TimerDis.setLed(0,1,(digit + 1),1);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),1);//d
      Room2TimerDis.setLed(0,4,(digit + 1),1);//e
      Room2TimerDis.setLed(0,5,(digit + 1),1);//f
      Room2TimerDis.setLed(0,6,(digit + 1),0);//g
      break;
    case 1:
      Room2TimerDis.setLed(0,0,(digit + 1),0);//a
      Room2TimerDis.setLed(0,1,(digit + 1),1);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),0);//d
      Room2TimerDis.setLed(0,4,(digit + 1),0);//e
      Room2TimerDis.setLed(0,5,(digit + 1),0);//f
      Room2TimerDis.setLed(0,6,(digit + 1),0);//g
      break;
    case 2:
      Room2TimerDis.setLed(0,0,(digit + 1),1);//a
      Room2TimerDis.setLed(0,1,(digit + 1),1);//b
      Room2TimerDis.setLed(0,2,(digit + 1),0);//c
      Room2TimerDis.setLed(0,3,(digit + 1),1);//d
      Room2TimerDis.setLed(0,4,(digit + 1),1);//e
      Room2TimerDis.setLed(0,5,(digit + 1),0);//f
      Room2TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 3:
      Room2TimerDis.setLed(0,0,(digit + 1),1);//a
      Room2TimerDis.setLed(0,1,(digit + 1),1);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),1);//d
      Room2TimerDis.setLed(0,4,(digit + 1),0);//e
      Room2TimerDis.setLed(0,5,(digit + 1),0);//f
      Room2TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 4:
      Room2TimerDis.setLed(0,0,(digit + 1),0);//a
      Room2TimerDis.setLed(0,1,(digit + 1),1);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),0);//d
      Room2TimerDis.setLed(0,4,(digit + 1),0);//e
      Room2TimerDis.setLed(0,5,(digit + 1),1);//f
      Room2TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 5:
      Room2TimerDis.setLed(0,0,(digit + 1),1);//a
      Room2TimerDis.setLed(0,1,(digit + 1),0);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),1);//d
      Room2TimerDis.setLed(0,4,(digit + 1),0);//e
      Room2TimerDis.setLed(0,5,(digit + 1),1);//f
      Room2TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 6:
      Room2TimerDis.setLed(0,0,(digit + 1),1);//a
      Room2TimerDis.setLed(0,1,(digit + 1),0);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),1);//d
      Room2TimerDis.setLed(0,4,(digit + 1),1);//e
      Room2TimerDis.setLed(0,5,(digit + 1),1);//f
      Room2TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 7:
      Room2TimerDis.setLed(0,0,(digit + 1),1);//a
      Room2TimerDis.setLed(0,1,(digit + 1),1);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),0);//d
      Room2TimerDis.setLed(0,4,(digit + 1),0);//e
      Room2TimerDis.setLed(0,5,(digit + 1),0);//f
      Room2TimerDis.setLed(0,6,(digit + 1),0);//g
      break;
    case 8:
      Room2TimerDis.setLed(0,0,(digit + 1),1);//a
      Room2TimerDis.setLed(0,1,(digit + 1),1);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),1);//d
      Room2TimerDis.setLed(0,4,(digit + 1),1);//e
      Room2TimerDis.setLed(0,5,(digit + 1),1);//f
      Room2TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 9:
      Room2TimerDis.setLed(0,0,(digit + 1),1);//a
      Room2TimerDis.setLed(0,1,(digit + 1),1);//b
      Room2TimerDis.setLed(0,2,(digit + 1),1);//c
      Room2TimerDis.setLed(0,3,(digit + 1),1);//d
      Room2TimerDis.setLed(0,4,(digit + 1),0);//e
      Room2TimerDis.setLed(0,5,(digit + 1),1);//f
      Room2TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
    case 10:
      Room2TimerDis.setLed(0,0,(digit + 1),0);//a
      Room2TimerDis.setLed(0,1,(digit + 1),0);//b
      Room2TimerDis.setLed(0,2,(digit + 1),0);//c
      Room2TimerDis.setLed(0,3,(digit + 1),0);//d
      Room2TimerDis.setLed(0,4,(digit + 1),0);//e
      Room2TimerDis.setLed(0,5,(digit + 1),0);//f
      Room2TimerDis.setLed(0,6,(digit + 1),0);//g
      break;
    default:
      Room2TimerDis.setLed(0,0,(digit + 1),0);//a
      Room2TimerDis.setLed(0,1,(digit + 1),0);//b
      Room2TimerDis.setLed(0,2,(digit + 1),0);//c
      Room2TimerDis.setLed(0,3,(digit + 1),0);//d
      Room2TimerDis.setLed(0,4,(digit + 1),0);//e
      Room2TimerDis.setLed(0,5,(digit + 1),0);//f
      Room2TimerDis.setLed(0,6,(digit + 1),1);//g
      break;
  }
}

