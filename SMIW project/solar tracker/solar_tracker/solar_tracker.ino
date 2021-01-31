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
bool succedScan = false;
float maxBatteryVoltage = 13.8;//V
float minBatteryVoltage = 12.1;//V
int waitingTime = 60;//One minute to next scanning.
int blinkingDelay = 200;//Time in ms before alert turns on or off backlight.
int motorSpeed = 13; //Lowest = faster
int minimalPower = 10; //Minimal power which panel needs to generate.
int measureTime = 50;
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
  {
    digitalWrite(LCD_A, HIGH);
    lcd.display();
  }
  else
  {
    digitalWrite(LCD_A, LOW);
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
    delay(blinkingDelay);
    turnBackgroundLight(false);
    delay(blinkingDelay);
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
      } // Can't use tracker if one of the ina219 is not working.
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
float measurePower()
{
  float resoult = 0;
  for (int i = 0; i < 5; i++)
  {
    resoult += measureCurrent() * measureLoadVoltage();
    delay(measureTime);
  }


  return  resoult / 5;
}
void measureAll()
{
  TCA9548A(0);
  delay(100);
  String panel = "P:" + String(measureLoadVoltage()) + "V " + String(measurePower()) + "mW";
  TCA9548A(1);
  delay(100);
  float currentVoltage = measureLoadVoltage();
  float deltaMaxMin = maxBatteryVoltage - minBatteryVoltage;
  float currentDelta = currentVoltage - minBatteryVoltage;
  float resoult =  currentDelta / deltaMaxMin;
  resoult *= 100;
  int finalResoult = (int)resoult;
  String battery = "Battery: " + String(finalResoult) + "%";
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
    alert(2);
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
  disableMotor();
  delay(100);
  servo.attach(SERVO_PIN);
  delay(100);
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
  delay(100);
  enableMotor();
}
//#################################################################
//####################     MOTOR     ##############################
//#################################################################
void initMotor()
{
  pinMode(EN, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(STEP, OUTPUT);
  disableMotor(); // Prevents the a4988 for overheating.
}
void enableMotor()
{
  digitalWrite(EN, LOW);
}
void disableMotor()
{
  digitalWrite(EN, HIGH);
}
void moveRight(int stepsCount)
{
  digitalWrite(DIR, LOW);

  for (int i = 0; i < stepsCount; i++)
  {
    delay(motorSpeed);
    digitalWrite(STEP, LOW);
    delay(motorSpeed);
    digitalWrite(STEP, HIGH);
  }
}
void moveLeft(int stepsCount)
{
  digitalWrite(DIR, HIGH);

  for (int i = 0; i < stepsCount; i++)
  {
    delay(motorSpeed);
    digitalWrite(STEP, LOW);
    delay(motorSpeed);
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
  float servoPowerArray[7];
  float maxPower = 0;
  int bestServoState = 0;

  setServoState(0);

  for (int i = 0; i < 7; i++)
  {
    setServoState(i);
    servoPowerArray[i] = measurePower();
    if (maxPower < servoPowerArray[i])
    {
      maxPower = servoPowerArray[i];
      bestServoState = i;
    }
    else
      break;
  }
  writeOnLCD("best state:" + String(bestServoState), "mW:" + String(maxPower));
  setServoState(bestServoState);
  delay(2000);

  writeOnLCD("", "");
  turnBackgroundLight(false);
}
void trackSunRight(float lastPower)
{
  float currentPower;
  if (motorState + 1 < 25)
    setMotorState(motorState + 1);
  else
    setMotorState(0);

  currentPower =  measurePower();
  currentPower = roundNumber(currentPower);

  if (lastPower < currentPower)
    trackSunRight(currentPower);
  else
  {
    if (motorState > 0)
      setMotorState(motorState - 1);
    else
      setMotorState(25);

    trackVertical();
  }
}
void trackSunLeft(float lastPower)
{
  float currentPower;
  if (motorState - 1 > 0)
    setMotorState(motorState - 1);
  else
    setMotorState(25);

  currentPower =  measurePower();
  currentPower = roundNumber(currentPower);

  if (lastPower < currentPower)
    trackSunLeft(currentPower);
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
  setServoState(1);
  enableMotor();

  float powerArray[25];
  int bestMotorState = 0;
  int progres = 0;
  float maxPower = 0;

  //Scanning.
  TCA9548A(0);

  for (int i = 0; i < 25; i++)
  {
    setMotorState(i);
    powerArray[i] = measurePower();
    if (maxPower < powerArray[i])
    {
      maxPower = powerArray[i];
      bestMotorState = i;
    }
    float val = i + 1;
    val /= 25;
    val *= 100;
    progres = val;

    writeOnLCD("area scan: ", String(progres) + "% mW:" + String(powerArray[i]));
    delay(300);
  }
  if (maxPower > minimalPower * 0.1)
  {
    //Going to best place.
    writeOnLCD("best state:" + String(bestMotorState), "mW:" + String(maxPower));
    delay(2000);
    setMotorState(bestMotorState);
    writeOnLCD("", "");
    turnBackgroundLight(false);

    trackVertical();
  }
  if (measurePower() < minimalPower)// Restarting tracker
  {
    delay(4000);

    setServoState(0);

    delay(4000);

    setMotorState(0);

    disableMotor();
    return false;
  }
  disableMotor();
  return true;
}

void trackSun()
{
  float leftPower = 0;
  float rightPower = 0;
  float middlePower = 0;
  setServoState(1);
  enableMotor();
  //Scanning.
  TCA9548A(0);
  
  //Checking current voltage.
  middlePower = measurePower();
  middlePower = roundNumber(middlePower);
 
  //Checking right side.
  if (motorState < 25)
    setMotorState(motorState + 1);
  else
    setMotorState(0);

  rightPower = measurePower();
  rightPower = roundNumber(rightPower);

  //Checking left side.

  if (motorState > 1)
    setMotorState(motorState - 2);
  else
    setMotorState(25);

  leftPower = measurePower();
  leftPower = roundNumber(leftPower);

  //Middle state.
  if (motorState < 25)
    setMotorState(motorState + 1);
  else
    setMotorState(0);
    
  if (leftPower > rightPower)
  {
    if (middlePower < leftPower)
    {
      trackSunLeft(leftPower);

      disableMotor();
      return;
    }
  }
  else //right voltage is higher than left
  {
    if (middlePower < rightPower)
    {
      trackSunRight(rightPower);

      disableMotor();
      return;
    }
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
    while (1)
    {
      if (measureLoadVoltage() < 13.4)
      {
        writeOnLCD("", "");
        turnBackgroundLight(false);
        break;
      }
    }
  }
  else
    chargingON();

  if (succedScan)
  {
    TCA9548A(0);
    if (measurePower() < minimalPower)
    {
      trackSun();

      if (measurePower() < minimalPower)
        succedScan = findSun();
      writeOnLCD("", "");
      turnBackgroundLight(false);
    }

  }
  if (buttonPressed() and succedScan)
  {
    showData();
  }
  if (!succedScan and waitingTime > 0)
  {
    writeOnLCD("waiting for", "restart: " + String(waitingTime));
    delay(1000);
    waitingTime--;
  }

  if (!succedScan and waitingTime <= 0)
  {
    writeOnLCD("", "");
    turnBackgroundLight(false);
    waitingTime = 60;
    succedScan = findSun();
  }
}
void showData()
{
  measureAll();
  delay(6000);
  writeOnLCD("", "");
  turnBackgroundLight(false);
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
  chargingON();
  succedScan = findSun();
  writeOnLCD("", "");
  turnBackgroundLight(false);

}
//*****************************************************************
//#################################################################
//##################### MAIN LOOP #################################
//#################################################################
void loop() {
  controlCharging();
}
