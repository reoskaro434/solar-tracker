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
LiquidCrystal lcd(LCD_RS, LCD_E, D4, D5, D6, D7); // Obiekt klasy odpowiedzialny za operacje na wyświetlaczu.
Adafruit_INA219 ina219; // Obiekt klasy odpowiedzialny za sterowanie miernikiem prądu/napięcia.
int servoState = 0; // Przechowuje aktualną pozycję serwa, maksymalnie 7
int motorState = 0; // Przechowuje aktualną pozycje silnika krokowego, maksymalnie 25
bool succedScan = false; // Zmienna określająca czy ostatni skan obszaru okazał się sukcesem.
float maxBatteryVoltage = 13.8;// Maksymalne napięcie jakie może osiągnąć akumulator.
float minBatteryVoltage = 12.3;// Minimalne napięcie jakie może mieć akumulator.
int waitingTime = 20;// Czas bezczynności jakie urządzenie musi odczekać po nieudanym skanie otoczenia.
int blinkingDelay = 250;// Czas po jakim wyświetlacz zapala się lub gaśnie gdy urządzenie przekazuje użytkownikowi informacje.
int motorSpeed = 13; // Czas mówiący ile milisekund musi upłynąć by sygnał STEP w sterowniku silnika zmienił się ze
                     // stanu wysokiego na niski lub odwrotnie.
int minimalPower = 200; // Minimalna moc jaką panel musi uzyskać by skan został uznany jako sukces.
int measureTime = 100; // Odstęp w milisekundach między seryjnymi pomiarami prądu.
Servo servo; // Obiekt klasy odpowiedzialny za sterowanie serwomechanizmem.
//#################################################################
//#####################    LCD    #################################
//#################################################################
//Inicjalizuje wyświetlacz.
void initLCD()
{
  pinMode(LCD_A, OUTPUT);
  lcd.begin(16, 2);
}
//Zależnie od argumentu włącza lub wyłącza podświetlenie.
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
//Wyświetla informajce znajdujące się w arg1 i arg2 na wyświetlaczu.
void writeOnLCD(String arg1, String arg2)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(arg1);
  lcd.setCursor(0, 1);
  lcd.print(arg2);
  turnBackgroundLight(true);
}
//Włącza i wyłącza oświetlenie kilkukrotnie, zależnie od argumentu blinkNumber.
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
//Zaokrągla liczbę znajdującą się w agrumencie number do dwóch miejsc
//po przecinku, i zwraca ją.
float roundNumber(float number)
{
  number *= 100;
  int var = (int)number;
  var = var / 10;
  float var2 = (float)var;
  var2 /= 10;
  return var2;
}
//Inicjalizuje oba mierniki INA219.
void initINA219s()
{
  Wire.begin();
  TCA9548A(0);//CURRENT GATHERING
  initINA219();
  TCA9548A(1);//CURRENT USED BY TRACKER
  initINA219();
}
//Umożliwia wybranie odpowiedniego miernika zależnie od potrzeb.
//Dla wartości 0 argumentu bus zostanie wybrany miernik odpowiedzialny
//za prąd i napięcie akumulatora a dla 1 prąd i napięcie ogniwa słonecznego.
void TCA9548A(uint8_t bus)
{
  Wire.beginTransmission(0x70);
  Wire.write(1 << bus);
  Wire.endTransmission();
}
//Inicjalizuje miernik INA219, jeżeli wystąpi błąd, to program zatrzymie się
//w tej funkcji i zgłosi błąd użytkownikowi.
void initINA219()
{
  if (! ina219.begin())
  {
    {
      writeOnLCD("ERROR, CAN'T", "INIT INA219");
      while (1) {
        delay(999999);
      } 
    }
  }
}
//Zwraca bierzące natężenie prądu przepływające przez miernik.
float measureCurrent()
{
  return ina219.getCurrent_mA();
}
//Zwraca całkowite napięcie akumulatora lub ogniwa słonecznego, zależnie od 
//wcześniej wybranego miernika.
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
//Zwraca średnią arytmetyczną z kilku pomiarów mocy.
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
//Funkcja mierząca wszystkie potrzebne wartości, które użytkownik odczytuje //w momencie naciśnięcia przycisku.
void measureAll()
{
  TCA9548A(0);
  delay(100);
  String panel = "P:" + String(measureLoadVoltage()) + "V " + String(measureCurrent()) + "mA";
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
//Inicjalizuje pin odpowiedzialny za załączanie lub odłączanie obwodu.
//Wyłącza domyślnie ładowanie akumulatora.
void initChargerSwitch()
{
  pinMode(MOSFET_GATE, OUTPUT);
  chargingOFF();
}
//Zamyka obwód ładujący akumulator.
void chargingON()
{
  digitalWrite(MOSFET_GATE, LOW);
}
//Otwiera obwód ładujący akumulator.
void chargingOFF()
{
  digitalWrite(MOSFET_GATE, HIGH);
}
//#################################################################
//####################     BUTTON    ##############################
//#################################################################
//Inicjalizuje odpowiedni pin, który będzie odczytywał zachowanie przycisku.
void initButton()
{
  pinMode(BUTTON_PIN, INPUT);
}
// Sprawdza czy przycisk został naciśniety i zwraca odpowiednią wartość. //Jeżeli wartość wynosi true, funkcja uruchamia procedurę alert() //potwierdzającą wciśnięcie przycisku.
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
//Ustawia serwo relatywnie do stanu podanego w argumencie funkcji.
void setServoState(int state)
{
  if (servoState == state)
    return;

 
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
  servo.writeMicroseconds(1500);
  delay(1000);
  servo.detach();
  delay(100);
  enableMotor();
}
//#################################################################
//####################     MOTOR     ##############################
//#################################################################
//Inicjalizuje piny podpięte do sterownika. 
//Domyślnie wyłącza motor w celu ochrony przed przegrzaniem sterownika
//i oszczędzania energii.
void initMotor()
{
  pinMode(EN, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(STEP, OUTPUT);
  disableMotor(); // Prevents the a4988 for overheating.
}
//Włącza możliwośc sterowania silnikiem.
void enableMotor()
{
  digitalWrite(EN, LOW);
}
//Wyłącza możliwość sterowania silnikiem.
void disableMotor()
{
  digitalWrite(EN, HIGH);
}
//Porusza silnikiem w prawo o ilość kroków podanych w argumencie funkcji.
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
//Porusza silnikiem w lewo o ilość kroków podanych w argumencie funkcji.
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
//Ustawia odpowiedni stan motoru.
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
//Ustawia panel przy użyciu serwomechanizmu na pozycję w której ilość //produkowanej energii jest najwyższa.
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
//Funkcja wywoływana rekurencyjnie do momentu znalezienia pozycji w której //produkowana energia jest najwyższa. Obraca panelem w prawo.
void trackSunRight(float lastPower)
{
  float currentPower;
  if (motorState + 1 < 25)
    setMotorState(motorState + 1);
  else
    setMotorState(0);

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
//Funkcja wywoływana rekurencyjnie do momentu znalezienia pozycji w której //produkowana energia jest najwyższa. Obraca panelem w lewo.
void trackSunLeft(float lastPower)
{
  float currentPower;
  if (motorState - 1 > 0)
    setMotorState(motorState - 1);
  else
    setMotorState(25);

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
//Funkcja wywoływana na początku działania urządzenia, jak i w momencie
//w którym nie powiodło się śledzenie słońca. Zwraca true jeżeli źródło 
//światła zostało znalezione.
bool findSun()
{
  setServoState(1);
  enableMotor();

  float powerArray[25];
  int bestMotorState = 0;
  int progres = 0;
  float maxPower = 0;

 
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
   
    writeOnLCD("best state:" + String(bestMotorState), "mW:" + String(maxPower));
    delay(2000);
    setMotorState(bestMotorState);
    writeOnLCD("", "");
    turnBackgroundLight(false);

    trackVertical();
  }
  if (measurePower() < minimalPower)
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
//Po udanym wcześniejszym znalezieniu słońca, gdy ilość generowanej energii //spada, metoda trackSun rozpoczyna śledzenie słońca.
void trackSun()
{
  float leftPower = 0;
  float rightPower = 0;
  float middlePower = 0;
  setServoState(1);
  enableMotor();
 
  TCA9548A(0);
  
  
  middlePower = measurePower();
  middlePower = roundNumber(middlePower);
 
  
  if (motorState < 25)
    setMotorState(motorState + 1);
  else
    setMotorState(0);

  rightPower = measurePower();
  rightPower = roundNumber(rightPower);

  

  if (motorState > 1)
    setMotorState(motorState - 2);
  else
    setMotorState(25);

  leftPower = measurePower();
  leftPower = roundNumber(leftPower);


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
  else
  {
    if (middlePower < rightPower)
    {
      trackSunRight(rightPower);

      disableMotor();
      return;
    }
  }


}
//Sprawdza czy akumulator został naładowany, jeżeli tak
//to wyłącza ładowanie i czeka na rozładowanie akumulatora.
void checkBattery()
{
   TCA9548A(1);
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
}
//Główna funkcja mierząca stan naładowania akumulatora. Wykonuje skanowanie
//obszaru lub śledzenie znalezionego już wcześniej słońca. Usypia również
//urządzenie jeżeli znalezienie słońca jest nie możliwe.
void controlCharging()
{
  checkBattery();
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
    waitingTime = 20;
    succedScan = findSun();
  }
}
void showData()
{
  measureAll();
  delay(8000);
  writeOnLCD("", "");
  turnBackgroundLight(false);
}
//*****************************************************************
//#################################################################
//#####################   SETUP   #################################
//#################################################################
void setup() {
  initLCD();
  initINA219s();
  initMotor();
  initChargerSwitch();
  writeOnLCD("calibrate panel", "and press button");

  while (1)
  {
    if (buttonPressed())
      break;
  }

  checkBattery();
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
