//Compatible with the Arduino IDE 1.0
//Library version:1.1
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

const float lowCellVoltage = 2.9; // stop discharge when cellvoltage is lower than this voltage
const float cellDetectVoltage = 2.0;  // detect cell if it has more than this voltage
const float overheatProtectionTemperature = 45;  // stop discharge when temperature hits this celcius, this is generous setting
const bool overheatProtection = false; // enable / disable overheat protection and temperature sensing

int dischargeControlPin[] = {40, 41, 42, 43}; // output to MOSFETS gates
int dischargeButtonPin[] = {30, 31, 32, 33}; // buttons pins
int battPin[] = {4, 5, 6, 7}; // voltage probes to battery + poles
int shuntPin[] = {0, 1, 2, 3}; // voltage probes on the shunt resistors
int tempPin[] = {8, 9, 10, 11}; // LM35 temperature pins
int beepPin = 5; // piezo buzzer pin

// cells data storages
float voltage[] = {0, 0, 0, 0};
float current[] = {0, 0, 0, 0};
float capacity[] = {0, 0, 0, 0};
float temperature[] = {0, 0, 0, 0};
float maxTemperature[] = {0, 0, 0, 0};
int status[] = {0, 0, 0, 0};

unsigned long dischargeStartedAt[] = {0, 0, 0, 0};
unsigned long lastMeasureAt[] = {0, 0, 0, 0};

// status = 0 no battery
// status = 1 idle
// status = 2 dicharging
// status = 3 disconnected
// status = 4 finished

void printData(int cell) {
  int margin = cell * 5;
  if (status[cell] == 0) {
    lcd.setCursor(margin, 0);
    lcd.print("N/A ");
    lcd.setCursor(margin, 1);
    lcd.print("    ");
    lcd.setCursor(margin, 2);
    lcd.print("    ");
    lcd.setCursor(margin, 3);
    if (overheatProtection) {
      lcd.print(temperature[cell], 1);
    } else {
      lcd.print("    ");
    }
  }
  else {
    // 1. line voltage
    lcd.setCursor(margin, 0);
    lcd.print(voltage[cell], 2);
    // 2. line current
    lcd.setCursor(margin, 1);
    if (status[cell] == 2) {
      lcd.print(current[cell], 2);
    } else {
      lcd.print("    ");
    }
    // 3. line capacity
    lcd.setCursor(margin, 2);
    if (status[cell] >= 2) {
      lcd.print(capacity[cell], 2);
    } else {
      lcd.print("    ");
    }
    // 4.line status | temperature
    lcd.setCursor(margin, 3);
    if (overheatProtection) {
      if (status[cell] == 1 || status[cell] == 2) {
        lcd.print(temperature[cell], 1);
      }
      else {
        lcd.print(maxTemperature[cell], 1);
      }
    }
    else {
      if (status[cell] == 1) {
        lcd.print("idle");
      } else if (status[cell] == 2) {
        lcd.print("DIS ");
      } else if (status[cell] == 3) {
        lcd.print("????");
      } else if (status[cell] == 4) {
        lcd.print("done");
      } 
    }
  }
}

void readData(int cell) {
  voltage[cell] = analogRead(battPin[cell]) * (5.0 / 1023.0);
  current[cell] = (voltage[cell] - analogRead(shuntPin[cell]) * (5.0 / 1023.0)) / 5.3;
  if (overheatProtection) {
    temperature[cell] = analogRead(tempPin[cell]) * (500.0 / 1023.0);
  }
  if (current[cell] < 0) {
    current[cell] = 0.0;
  }
  if (status[cell] == 2) {
    capacity[cell] = capacity[cell] + (current[cell] * (millis() - lastMeasureAt[cell]) / 3600000.0 );
    lastMeasureAt[cell] = millis();
  }
}

void processData(int cell) {
  // no battery
  if (status[cell] == 0) {
    // switch to idle if voltage detected
    if (voltage[cell] > cellDetectVoltage) {
      status[cell] = 1;
    }
  }
  else if (status[cell] == 1) {
    if (voltage[cell] < cellDetectVoltage) {
      status[cell] = 0;
    }
    if (digitalRead(dischargeButtonPin[cell]) == LOW) {
      startDischarge(cell);
    }
  }
  else if (status[cell] == 2) {
    if (overheatProtection && temperature[cell] > maxTemperature[cell]) {
      maxTemperature[cell] = temperature[cell];
    }
    if (voltage[cell] < cellDetectVoltage) { // wait for the cell to reconnect
      pauseDischarge(cell);
    } else if (voltage[cell] < lowCellVoltage || (overheatProtection && (temperature[cell] >= overheatProtectionTemperature)) || digitalRead(dischargeButtonPin[cell]) == LOW) {
      stopDischarge(cell);
      delay(200);
    }
  } 
  else if (status[cell] == 3) {
    if (voltage[cell] > cellDetectVoltage) { // cell detected again, continue discharge
      startDischarge(cell); 
    }
  }
  else if (status[cell] = 4) {
    if (digitalRead(dischargeButtonPin[cell]) == LOW) {
      resetValues(cell);
      delay(200);
    }
  }
}

void resetValues(int cell) {
  status[cell] = 0;
  digitalWrite(dischargeControlPin[cell], LOW);
  capacity[cell] = 0;
  maxTemperature[cell] = 0;
}

void startDischarge(int cell) {
  beepLow(500);
  status[cell] = 2;
  digitalWrite(dischargeControlPin[cell], HIGH);
  dischargeStartedAt[cell] = millis();
  lastMeasureAt[cell] = millis();
}

void pauseDischarge(int cell) {
  status[cell] = 3;
  digitalWrite(dischargeControlPin[cell], LOW);
  beepHigh(200);
}

void stopDischarge(int cell) {
  status[cell] = 4;
  digitalWrite(dischargeControlPin[cell], LOW);
  beepHigh(200);
  beepLow(200);
  beepHigh(200);
  beepLow(200);
}

void setupCell(int cell) {
  pinMode(dischargeControlPin[cell], OUTPUT);
  pinMode(dischargeButtonPin[cell], INPUT_PULLUP);
  resetValues(cell);
}

void beepLow(int howLong) {
  tone(beepPin, 500);
  delay(howLong);
  noTone(beepPin);
}

void beepHigh(int howLong) {
  tone(beepPin, 1500);
  delay(howLong);
  noTone(beepPin);
}

void setup()
{
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  int i;
  for (i = 0; i <= 3; i = i + 1) {
    setupCell(i);
  }
  beepLow(100);
  beepHigh(200);
  beepLow(100);
  beepHigh(200);
}

void loop()
{
  int i;
  for (i = 0; i <= 3; i = i + 1) {
    readData(i);
    processData(i);
  }
  for (i = 0; i <= 3; i = i + 1) {
    printData(i);
  }
  delay(300);
}
