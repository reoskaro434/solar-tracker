//V 0.9
//#################################################################
//#####################  LIBRARIES  ###############################
//#################################################################
#include <LiquidCrystal.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

//#################################################################
//#####################  DEFINES  #################################
//#################################################################

//############### LCD
#define LCD_RS 2
#define LCD_E 3
#define D4 4
#define D5 5
#define D6 6
#define D7 7
#define LCD_A 8
//################ MOTOR
#define DIR 16
#define STEP 15
#define EN 14
//################ LOGIC LEVEL MOSFET
#define MOSFET_GATE 9
//################ SERVO
#define SERVO_PIN 10
//################ BUTTON
#define BUTTON_PIN 0

//#################################################################
//#####################  GLOBAL  ##################################
//#################################################################
LiquidCrystal lcd(LCD_RS, LCD_E, D4, D5, D6, D7);
Adafruit_INA219 ina219;
int servoState = 0; // 1,2,3,4,5.. states
int motorState = 0; // 0...400 states 25 states if we moving by 16 steps.
int roundedCurrentVoltage = 0;
bool succedScan = false;
float maxBatteryVoltage = 13.8;//V
float minBatteryVoltage = 12.55;//V
float minPanelCurrent = 10; //mA
int waitingTime = 60;//One minute to next scanning.
Servo servo;
//#################################################################
//#####################    LCD    #################################
//#################################################################
void initLCD()
{
  pinMode(LCD_A, OUTPUT);
  lcd.begin(16, 2);
}
void turnBackgroundLight(bool turnOn)
{
  if (turnOn)
  { digitalWrite(LCD_A, HIGH);
    lcd.display();
  }
  else
  { digitalWrite(LCD_A, LOW);
    lcd.noDisplay();
  }
}
void writeOnLCD(String arg1, String arg2)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(arg1);
  lcd.setCursor(0, 1);
  lcd.print(arg2);
  turnBackgroundLight(true);
}
void alert(int blinkNumber)
{
  writeOnLCD("", "");
  turnBackgroundLight(false);
  for ( int i = 0; i < blinkNumber; i++)
  {
    turnBackgroundLight(true);
    delay(400);
    turnBackgroundLight(false);
    delay(400);
  }
}

//#################################################################
//#####################  INA_MANAGER  #############################
//#################################################################
float roundNumber(float number)
{
  number *= 100;
  int var = (int)number;
  var = var / 10;
  float var2 = (float)var;
  var2 /= 10;
  return var2;
}
void initINA219s()
{
  Wire.begin();
  TCA9548A(0);//CURRENT GATHERING
  initINA219();
  TCA9548A(1);//CURRENT USED BY TRACKER
  initINA219();
}
void TCA9548A(uint8_t bus)
{
  Wire.beginTransmission(0x70);
  Wire.write(1 << bus);
  Wire.endTransmission();
}
void initINA219()
{
  if (! ina219.begin())
  {
    {
      writeOnLCD("ERROR, CAN'T", "INIT INA219");
      while (1) {
        delay(999999);
      } // Can't use tracker if one of the ina219 not working.
    }
  }
}
float measureCurrent()
{
  return ina219.getCurrent_mA();
}
float measureLoadVoltage()
{
  float shuntvoltage = 0;
  float busvoltage = 0;
  float loadvoltage = 0;

  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();

  loadvoltage = busvoltage + (shuntvoltage / 1000);

  return loadvoltage;
}
void measureAll()
{
  TCA9548A(0);
  delay(500);
  String panel = "P:"+String(measureLoadVoltage())+"V "+String(measureCurrent())+"mA";

  TCA9548A(1);
  delay(500);
  float currentVoltage = measureLoadVoltage();
  float deltaMaxMin = maxBatteryVoltage - minBatteryVoltage;
  float currentDelta = currentVoltage - minBatteryVoltage;
  float resoult =  currentDelta/deltaMaxMin;
  resoult *=100;
  int finalResoult = (int)resoult;
  String battery = "Battery: " + String(finalResoult)+"%";
  writeOnLCD(battery, panel);
}
//#################################################################
//#################### CHARGER SWITCH #############################
//#################################################################
void initChargerSwitch()
{
  pinMode(MOSFET_GATE, OUTPUT);
  chargingOFF();
}
void chargingON()
{
  digitalWrite(MOSFET_GATE, LOW);
}
void chargingOFF()
{
  digitalWrite(MOSFET_GATE, HIGH);
}
//#################################################################
//####################     BUTTON    ##############################
//#################################################################
void initButton()
{
  pinMode(BUTTON_PIN, INPUT);
}

bool buttonPressed()
{
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    while (digitalRead(BUTTON_PIN) == LOW)
    {}

    return true;
  }
  else
    return false;
}

//#################################################################
//####################     SERVO     ##############################
//#################################################################
void setServoState(int state)
{
  if (servoState == state)
    return;

 // Servo servo;
  digitalWrite(EN, HIGH);
  delay(400);
  servo.attach(SERVO_PIN);
  delay(400);
  if (servoState > state)
  {
    servo.writeMicroseconds(500);
    while (servoState != state)
    {
      delay(1000);
      servoState--;
    }
  }
  else
  {
    servo.writeMicroseconds(2500);
    while (servoState != state)
    {
      delay(1000);
      servoState++;
    }
  }
  servo.writeMicroseconds(1500);//Default stop;
  delay(1000);
  servo.detach();
  delay(400);
  digitalWrite(EN, LOW);
}
//#################################################################
//####################     MOTOR     ##############################
//#################################################################
void initMotor()
{
  pinMode(EN, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(STEP, OUTPUT);
  digitalWrite(EN, HIGH); // Prevents the a4988 for overheating.
}
void moveRight(int stepsCount)
{
  digitalWrite(DIR, LOW);

  for (int i = 0; i < stepsCount; i++)
  {
    delay(20);
    digitalWrite(STEP, LOW);
    delay(20);
    digitalWrite(STEP, HIGH);
  }
}
void moveLeft(int stepsCount)
{
  digitalWrite(DIR, HIGH);

  for (int i = 0; i < stepsCount; i++)
  {
    delay(20);
    digitalWrite(STEP, LOW);
    delay(20);
    digitalWrite(STEP, HIGH);
  }
}
void setMotorState(int state)
{
  if (motorState == state)
    return;



  if (motorState > state)
  {
    while (motorState != state)
    {
      moveLeft(16);
      motorState--;
    }
  }
  else
  {
    while (motorState != state)
    {
      moveRight(16);
      motorState++;
    }
  }


}
//#################################################################
//#####################   TRACKER   ###############################
//#################################################################
void trackVertical()
{   
 
    setServoState(0);
    float servoVoltageTab[7];
    float maxVoltage2 = 0;
    int bestServoState = 0;
    int progres =0;
    for (int i = 0; i < 7; i++)
    {
      setServoState(i);
      servoVoltageTab[i] = measureLoadVoltage();
      if (maxVoltage2 < servoVoltageTab[i])
      {
        maxVoltage2 = servoVoltageTab[i];
        bestServoState = i;
      }
      else
        break;
      float val = i + 1;
      val /= 7;
      val *= 100;
      progres = val;

      writeOnLCD("area scan: ", String(progres) + "% V:" + String(servoVoltageTab[i]));
      delay(1000);


    }
    writeOnLCD("best state:" + String(bestServoState), "V:" + String(maxVoltage2));
    setServoState(bestServoState);
    delay(2000);

    writeOnLCD("","");
    turnBackgroundLight(false);   
}
void trackSunRight(float lastVoltage)
{ 
  float servoVoltageTab[7];
  float currentVoltage;
  if (motorState + 1 < 25)
    setMotorState(motorState + 1);
  else
    setMotorState(0);

  currentVoltage =  measureLoadVoltage();
  currentVoltage = roundNumber(currentVoltage);

  if (lastVoltage < currentVoltage)
    trackSunRight(currentVoltage);
  else
  {
    if (motorState > 0)
      setMotorState(motorState - 1);
    else
      setMotorState(25);
      
  trackVertical();
  }
}
void trackSunLeft(float lastVoltage)
{
  float currentVoltage;
  if (motorState - 1 > 0)
    setMotorState(motorState - 1);
  else
    setMotorState(25);

  currentVoltage =  measureLoadVoltage();
  currentVoltage = roundNumber(currentVoltage);

  if (lastVoltage < currentVoltage)
    trackSunLeft(currentVoltage);
  else
  {
    if (motorState < 25)
      setMotorState(motorState + 1);
    else
      setMotorState(0);
  trackVertical();
  }
}
bool findSun()
{
  digitalWrite(EN, LOW);
  float voltageTab[25];
  float servoVoltageTab[7];
  int bestMotorState = 0;
  int bestServoState = 0;
  int progres = 0;
  float maxVoltage = 0;

  //Scanning.
  chargingOFF();
  TCA9548A(0);

  for (int i = 0; i < 25; i++)
  {
    setMotorState(i);
    voltageTab[i] = measureLoadVoltage();
    if (maxVoltage < voltageTab[i])
    {
      maxVoltage = voltageTab[i];
      bestMotorState = i;
    }
    float val = i + 1;
    val /= 25;
    val *= 100;
    progres = val;

    writeOnLCD("area scan: ", String(progres) + "% V:" + String(voltageTab[i]));
    delay(300);
  }
  if (maxVoltage > 1)
  {
    //Going to best place.
    writeOnLCD("best state:" + String(bestMotorState), "V:" + String(maxVoltage));
    delay(2000);
    setMotorState(bestMotorState);
    writeOnLCD("","");
    turnBackgroundLight(false);   
    
    
    float maxVoltage2 = 0;
    for (int i = 0; i < 7; i++)
    {
      setServoState(i);
      servoVoltageTab[i] = measureLoadVoltage();
      if (maxVoltage2 < servoVoltageTab[i])
      {
        maxVoltage2 = servoVoltageTab[i];
        bestServoState = i;
      }
      else
        break;
      float val = i + 1;
      val /= 7;
      val *= 100;
      progres = val;

      writeOnLCD("area scan: ", String(progres) + "% V:" + String(servoVoltageTab[i]));
      delay(1000);


    }
    writeOnLCD("best state:" + String(bestServoState), "V:" + String(maxVoltage2));



    setServoState(bestServoState);
    delay(2000);
   
    writeOnLCD("","");
    turnBackgroundLight(false);   
  }
  if (measureLoadVoltage() < 16)
  {
    delay(4000);

    setServoState(0);

    delay(4000);

    setMotorState(0);

    chargingOFF();
    digitalWrite(EN, HIGH);
    return false;
  }
  chargingON();
  digitalWrite(EN, HIGH);
  return true;
}

void trackSun()
{
  float leftVoltage = 0;
  float rightVoltage = 0;
  float middleVoltage = 0;

  digitalWrite(EN, LOW);
  //Scanning.
  chargingOFF();
  TCA9548A(0);
  //Checking current voltage.
  setServoState(0);
  middleVoltage = measureLoadVoltage();
  middleVoltage = roundNumber(middleVoltage);
  //Checking right side.
  if (motorState < 25)
    setMotorState(motorState + 1);
  else
    setMotorState(0);

  rightVoltage = measureLoadVoltage();
  rightVoltage = roundNumber(rightVoltage);
  if (middleVoltage < rightVoltage)
  {
    trackSunRight(rightVoltage);

    digitalWrite(EN, HIGH);
    return;
  }

  //If voltage is not enough high checking left side.

  if (motorState > 1)
    setMotorState(motorState - 2);
  else
    setMotorState(25);

  leftVoltage = measureLoadVoltage();
  leftVoltage = roundNumber(leftVoltage);
  if (middleVoltage < leftVoltage)
  {
    trackSunLeft(leftVoltage);

    digitalWrite(EN, HIGH);
    return;
  }
}
void controlCharging()//Charging is ON
{
  TCA9548A(1);//Battery
  if (measureLoadVoltage() > maxBatteryVoltage)
  {
    chargingOFF();
    alert(4);
    writeOnLCD("battery is full", "charging canceled");
    succedScan = false;
    while(1)
    {
      if(measureLoadVoltage()<13.4)
      {
      writeOnLCD("","");
      turnBackgroundLight(false);   
      break;
      }
    }
  }
 
  if(succedScan)
 { 
  TCA9548A(0);//Panel
  if (measureCurrent() < minPanelCurrent)
  {
    chargingOFF();
    TCA9548A(0);
    if(measureLoadVoltage()<16)
    {
      trackSun();
      int voltage = (int)measureLoadVoltage();
      if (voltage < 16)
        succedScan = findSun();
    }
     chargingON();
  }

 }
  if (buttonPressed() and succedScan)
  {
    alert(2);
    measureAll();
    delay(10000);
    writeOnLCD("","");
    turnBackgroundLight(false);   
  }
  if(!succedScan and waitingTime > 0)
  {
    writeOnLCD("waiting for","restart: "+String(waitingTime));
    delay(1000);
    waitingTime--;
  }
  
  if(!succedScan and waitingTime <=0)
    {
      writeOnLCD("","");
      turnBackgroundLight(false);   
      waitingTime = 60;
      succedScan = findSun();
  }
}
//*****************************************************************
//#################################################################
//#####################   SETUP   #################################
//#################################################################
void setup() {
  initLCD();
  initINA219s();//Measures battery and panel's parameters
  initMotor();//default enable pin is turned high to prevent overheating
  initChargerSwitch();//default charging is turned off
  writeOnLCD("calibrate panel", "and press button");

  while (1)
  {
    if (buttonPressed())
      break;
  }
  succedScan = findSun();
}
//*****************************************************************
//#################################################################
//##################### MAIN LOOP #################################
//#################################################################
void loop() {
  controlCharging();
}
