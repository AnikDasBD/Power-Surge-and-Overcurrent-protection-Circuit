
#include "EmonLib.h"
#include <LiquidCrystal.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// LCD: RS, EN, D4, D5, D6, D7
LiquidCrystal lcd(13, 12, 11, 10, 9, 8);

// Voltage measurement
EnergyMonitor emon1;

// Pin definitions
const int Relay_PIN = 7;
const int LED_PIN = 6;
const int RESET_BUTTON_PIN = 5;
const int Voltage_PIN = A0;
const int Sensor_Pin = A1;

// Adjustable protection thresholds
int UNDER_VOLTAGE = 207;          // Allowed: 180-230 V
int OVER_VOLTAGE = 270;           // Allowed: 270-300 V
float OVER_CURRENT_LIMIT = 1.5;   // Allowed: 1.5-4.0 A

// EmonLib calibration
const double VOLTAGE_CALIBRATION = 187.0;
const double PHASE_SHIFT = 1.7;

// ACS712 5A settings
const float SENSITIVITY = 200.0;  // Calibrated mV/A
const float ADC_REFERENCE_MV = 5000.0;

float measuredCurrent = 0.0;

// Latched protection faults
bool overVoltageLatched = false;
bool underVoltageLatched = false;
bool overCurrentLatched = false;

// Reset button debounce
int lastButtonReading = HIGH;
int stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Serial command buffer
const byte COMMAND_BUFFER_SIZE = 40;
char commandBuffer[COMMAND_BUFFER_SIZE];
byte commandIndex = 0;


// ------------------------------------
// Measure AC current using RMS sampling
// ------------------------------------
float measureCurrent()
{
  const int samples = 1000;

  unsigned long sum = 0;
  unsigned long sumSquares = 0;

  for (int i = 0; i < samples; i++)
  {
    int reading = analogRead(Sensor_Pin);

    sum += reading;
    sumSquares += (unsigned long)reading * reading;
  }

  double mean = (double)sum / samples;
  double meanSquare = (double)sumSquares / samples;

  // Remove DC offset
  double variance = meanSquare - (mean * mean);

  if (variance < 0)
    variance = 0;

  double rmsADC = sqrt(variance);

  // Convert ADC counts to millivolts
  double rmsMillivolts =
      rmsADC * ADC_REFERENCE_MV / 1023.0;

  // Convert to RMS current
  return rmsMillivolts / SENSITIVITY;
}


// ------------------------------------
// Reset button with debounce
// ------------------------------------
bool resetButtonPressed()
{
  int reading = digitalRead(RESET_BUTTON_PIN);

  if (reading != lastButtonReading)
  {
    lastDebounceTime = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastDebounceTime >= DEBOUNCE_DELAY)
  {
    if (reading != stableButtonState)
    {
      stableButtonState = reading;

      // Detect one press
      if (stableButtonState == LOW)
        return true;
    }
  }

  return false;
}


// ------------------------------------
// Display command instructions
// ------------------------------------
void printHelp()
{
  Serial.println(F("\r\n--- Voltage Protector Commands ---"));
  Serial.println(F("OV=280 : Overvoltage (270-300 V)"));
  Serial.println(F("UV=200 : Undervoltage (180-230 V)"));
  Serial.println(F("OC=2.5 : Overcurrent (1.5-4.0 A)"));
  Serial.println(F("STATUS : Show current settings"));
  Serial.println(F("HELP   : Show commands"));
  Serial.println(F("RESET  : Reset protection faults"));
  Serial.println(F("Enter one command per line."));
}


// ------------------------------------
// Display current settings
// ------------------------------------
void printStatus()
{
  Serial.println(F("\r\n--- Current Protection Settings ---"));

  Serial.print(F("Overvoltage: "));
  Serial.print(OVER_VOLTAGE);
  Serial.println(F(" V"));

  Serial.print(F("Undervoltage: "));
  Serial.print(UNDER_VOLTAGE);
  Serial.println(F(" V"));

  Serial.print(F("Overcurrent: "));
  Serial.print(OVER_CURRENT_LIMIT, 1);
  Serial.println(F(" A"));
}


// ------------------------------------
// Clear latched faults
// ------------------------------------
void resetProtectionFaults()
{
  overVoltageLatched = false;
  underVoltageLatched = false;
  overCurrentLatched = false;

  Serial.println(
    F("Protection faults cleared. Rechecking conditions.")
  );
}


// ------------------------------------
// Process a complete terminal command
// ------------------------------------
void processCommand(char *command)
{
  // Remove leading spaces
  while (*command && isspace((unsigned char)*command))
    command++;

  // Remove trailing spaces
  size_t length = strlen(command);

  while (length > 0 &&
         isspace((unsigned char)command[length - 1]))
  {
    command[--length] = '\0';
  }

  if (length == 0)
    return;

  // Make command case-insensitive
  for (size_t i = 0; command[i]; i++)
    command[i] = toupper((unsigned char)command[i]);

  if (strcmp(command, "HELP") == 0)
  {
    printHelp();
    return;
  }

  if (strcmp(command, "STATUS") == 0)
  {
    printStatus();
    return;
  }

  // Optional computer-based reset command
  if (strcmp(command, "RESET") == 0)
  {
    resetProtectionFaults();
    return;
  }

  char *equalsSign = strchr(command, '=');

  if (equalsSign == NULL)
  {
    Serial.println(F("Error: Invalid command. Type HELP."));
    return;
  }

  *equalsSign = '\0';

  char *setting = command;
  char *valueText = equalsSign + 1;

  if (*valueText == '\0')
  {
    Serial.println(F("Error: Missing threshold value."));
    return;
  }

  // Parse and validate numeric input
  char *endPointer;
  double value = strtod(valueText, &endPointer);

  while (*endPointer &&
         isspace((unsigned char)*endPointer))
  {
    endPointer++;
  }

  if (endPointer == valueText ||
      *endPointer != '\0' ||
      !isfinite(value))
  {
    Serial.println(F("Error: Enter a valid number."));
    return;
  }

  // Overvoltage threshold
  if (strcmp(setting, "OV") == 0)
  {
    if (value < 270.0 || value > 300.0 ||
        floor(value) != value)
    {
      Serial.println(
        F("Rejected: OV must be 270-300 V (whole number).")
      );
      return;
    }

    OVER_VOLTAGE = (int)value;

    Serial.print(F("Overvoltage threshold set to "));
    Serial.print(OVER_VOLTAGE);
    Serial.println(F(" V"));
  }

  // Undervoltage threshold
  else if (strcmp(setting, "UV") == 0)
  {
    if (value < 180.0 || value > 230.0 ||
        floor(value) != value)
    {
      Serial.println(
        F("Rejected: UV must be 180-230 V (whole number).")
      );
      return;
    }

    UNDER_VOLTAGE = (int)value;

    Serial.print(F("Undervoltage threshold set to "));
    Serial.print(UNDER_VOLTAGE);
    Serial.println(F(" V"));
  }

  // Overcurrent threshold
  else if (strcmp(setting, "OC") == 0)
  {
    if (value < 1.5 || value > 4.0)
    {
      Serial.println(F("Rejected: OC must be 1.5-4.0 A."));
      return;
    }

    // Store to one decimal place
    OVER_CURRENT_LIMIT = round(value * 10.0) / 10.0;

    Serial.print(F("Overcurrent threshold set to "));
    Serial.print(OVER_CURRENT_LIMIT, 1);
    Serial.println(F(" A"));
  }

  else
  {
    Serial.println(
      F("Error: Unknown setting. Use OV, UV, or OC.")
    );
  }
}


// ------------------------------------
// Read incoming serial commands
// ------------------------------------
void readSerialCommands()
{
  while (Serial.available() > 0)
  {
    char incoming = Serial.read();

    if (incoming == '\r' || incoming == '\n')
    {
      if (commandIndex > 0)
      {
        commandBuffer[commandIndex] = '\0';
        processCommand(commandBuffer);
        commandIndex = 0;
      }
    }
    else if (commandIndex < COMMAND_BUFFER_SIZE - 1)
    {
      commandBuffer[commandIndex++] = incoming;
    }
    else
    {
      commandIndex = 0;
      Serial.println(F("Error: Command too long. Try again."));
    }
  }
}


// ------------------------------------
// Setup
// ------------------------------------
void setup()
{
  pinMode(Relay_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Safe startup state
  digitalWrite(Relay_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(9600);

  emon1.voltage(
    Voltage_PIN,
    VOLTAGE_CALIBRATION,
    PHASE_SHIFT
  );

  lcd.begin(20, 4);

  lcd.setCursor(0, 0);
  lcd.print("Overload Protection");

  lcd.setCursor(0, 1);
  lcd.print("Made by Anik Das");

  delay(1000);
  lcd.clear();

  Serial.println(F("\r\nVoltage Protector Ready."));
  printHelp();
  printStatus();
}


// ------------------------------------
// Main loop
// ------------------------------------
void loop()
{
  // 1. Process terminal commands
  readSerialCommands();

  // 2. Voltage measurement
  emon1.calcVI(20, 2000);
  int Voltage = emon1.Vrms;

  // 3. Current measurement
  measuredCurrent = measureCurrent();

  // 4. Check protection conditions
  bool overVoltage = Voltage > OVER_VOLTAGE;
  bool underVoltage = Voltage < UNDER_VOLTAGE;
  bool overCurrent =
      measuredCurrent > OVER_CURRENT_LIMIT;

  // 5. Reset button
  if (resetButtonPressed())
  {
    resetProtectionFaults();
  }

  // 6. Latch faults
  if (overVoltage)
    overVoltageLatched = true;

  if (underVoltage)
    underVoltageLatched = true;

  if (overCurrent)
    overCurrentLatched = true;

  // 7. Relay and LED control
  bool faultLatched =
      overVoltageLatched ||
      underVoltageLatched ||
      overCurrentLatched;

  if (faultLatched)
  {
    digitalWrite(Relay_PIN, LOW);
    digitalWrite(LED_PIN, HIGH);
  }
  else
  {
    digitalWrite(Relay_PIN, HIGH);
    digitalWrite(LED_PIN, LOW);
  }

  // 8. LCD line 0: Voltage
  lcd.setCursor(0, 0);
  lcd.print("Voltage: ");
  lcd.print(Voltage);
  lcd.print(" V       ");

  // 9. LCD line 1: Voltage status
  lcd.setCursor(0, 1);

  if (overVoltageLatched)
    lcd.print("OVER VOLTAGE        ");
  else if (underVoltageLatched)
    lcd.print("UNDER VOLTAGE       ");
  else
    lcd.print("NORMAL VOLTAGE      ");

  // 10. LCD line 2: Current
  lcd.setCursor(0, 2);
  lcd.print("Current: ");
  lcd.print(measuredCurrent, 2);
  lcd.print(" A       ");

  // 11. LCD line 3: Current status
  lcd.setCursor(0, 3);

  if (overCurrentLatched)
    lcd.print("OVER CURRENT        ");
  else
    lcd.print("NORMAL CURRENT      ");

  delay(200);
}
