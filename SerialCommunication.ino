
#include "SerialCommand.h"
#include "analog.c"

#define SerialPort Serial
#define Baudrate 115200

int numberOfPings;
SerialCommand sCmd(SerialPort);

// Alarm FSM states
enum AlarmState {STATE_OK = 0, STATE_ALARM = 1, STATE_CONFIRMED = 2};
AlarmState alarmState = STATE_OK;

// pins (per exercise)
const uint8_t PIN_LED = 2;    // LED
const uint8_t PIN_BUZZER = 3; // Buzzer
const uint8_t PIN_BUTTON = 5; // Button (use INPUT_PULLUP)

// timing
unsigned long lastAlarmMillis = 0;
const unsigned long ALARM_INTERVAL_MS = 1000;

// button debounce
int lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

void setup() 
{
  digitalWrite(13, LOW);
  SerialPort.begin(Baudrate);

  sCmd.addCommand("set", onSet);
  sCmd.addCommand("toggle", onToggle);
  sCmd.addCommand("get", onGet);
  sCmd.addCommand("ping", onPing);
  sCmd.addCommand("help", onHelp);
  sCmd.addCommand("debug", onDebug);
  sCmd.setDefaultHandler(onUnknownCommand);

  for (int i = 2; i < 5; i++) pinMode(i, OUTPUT);
  for (int i = 9; i < 12; i++) pinMode(i, OUTPUT);
  for (int i = 5; i < 8; i++) pinMode(i, INPUT_PULLUP);
  for (int i = A0; i <= A5; i++) pinMode(i, INPUT);

  analogReference(DEFAULT);
  
  //SerialPort.println(F("ready"));
}

void loop() 
{
  sCmd.readSerial();

  // run alarm FSM periodically
  unsigned long now = millis();
  if (now - lastAlarmMillis >= ALARM_INTERVAL_MS)
  {
    lastAlarmMillis = now;
    runAlarmCycle();
  }
}

// Read sensors, update FSM and outputs, and print status over serial
void runAlarmCycle()
{
  // Read LM35 at A0 (current temperature)
  int rawCurrent = analogReadDelay(A0, 50000);
  double currentC = rawCurrent * (500.0 / 1023.0); // 0..500°C per earlier scaling (LM35)

  // Read potentiometer at A1 (alarm threshold) -> scale 0..1023 -> -10..60
  int rawPot = analogReadDelay(A1, 50000);
  double alarmThreshold = rawPot * (70.0 / 1023.0) - 10.0; // -10..60°C

  // Read button (active LOW because INPUT_PULLUP)
  int reading = digitalRead(PIN_BUTTON);
  if (reading != lastButtonReading)
  {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
  {
    // if the button state has changed:
    if (reading == LOW && lastButtonReading == HIGH)
    {
      // button pressed event (falling edge)
      handleButtonPress(currentC, alarmThreshold);
    }
  }
  lastButtonReading = reading;

  // FSM transitions (when not button-driven)
  if (alarmState == STATE_OK)
  {
    if (currentC > alarmThreshold)
    {
      alarmState = STATE_ALARM; // go into alarm when threshold exceeded
    }
  }
  else if (alarmState == STATE_ALARM)
  {
    // remain in ALARM until user confirms or button forces OK when below threshold
    // nothing automatic here
  }
  else if (alarmState == STATE_CONFIRMED)
  {
    // go back to OK if temperature drops below threshold
    if (currentC < alarmThreshold)
    {
      alarmState = STATE_OK;
    }
  }

  // Outputs according to state
  switch (alarmState)
  {
    case STATE_OK:
      digitalWrite(PIN_LED, LOW);
      digitalWrite(PIN_BUZZER, LOW);
      break;
    case STATE_ALARM:
      digitalWrite(PIN_LED, HIGH);
      digitalWrite(PIN_BUZZER, HIGH);
      break;
    case STATE_CONFIRMED:
      digitalWrite(PIN_LED, HIGH);
      digitalWrite(PIN_BUZZER, LOW);
      break;
  }

  // Print status to serial (one-line, easy to parse)
  SerialPort.print(F("state: "));
  if (alarmState == STATE_OK) SerialPort.print(F("OK"));
  else if (alarmState == STATE_ALARM) SerialPort.print(F("ALARM"));
  else SerialPort.print(F("BEVESTIGD"));
  SerialPort.print(F(" | threshold: "));
  SerialPort.print(alarmThreshold, 1);
  SerialPort.print(F(" C | current: "));
  SerialPort.print(currentC, 1);
  SerialPort.println(F(" C"));
}

void handleButtonPress(double currentC, double alarmThreshold)
{
  if (alarmState == STATE_ALARM)
  {
    // if temperature already dropped below threshold, go straight to OK
    if (currentC < alarmThreshold)
    {
      alarmState = STATE_OK;
    }
    else
    {
      // acknowledge and go to confirmed
      alarmState = STATE_CONFIRMED;
    }
  }
  else if (alarmState == STATE_CONFIRMED)
  {
    // allow user to cancel confirmation (toggle back to OK)
    if (currentC < alarmThreshold)
    {
      alarmState = STATE_OK;
    }
  }
  // if OK and button pressed, no action
}

void onUnknownCommand(char* cmd)
{
  SerialPort.print(F("unknown command \""));
  SerialPort.print(cmd);
  SerialPort.println(F("\""));
  sCmd.clearBuffer();
}

void onSet()
{
  char* arg1 = sCmd.next();
  char* arg2 = sCmd.next();

  if ((arg1 == NULL) || (arg2 == NULL)) SerialPort.println(F("Error: incorrect number of arguments"));
  else if (startsWith(arg1, "d"))
  {
    arg1 = &arg1[1];
    if (isValidNumber(arg1))
    {
      int pin = atoi(arg1);
      if ((pin >= 2) && (pin <= 4))
      {
        if ((strcmp(arg2, "high") == 0) || (strcmp(arg2, "on") == 0) || (strcmp(arg2, "1") == 0)) 
        {
          digitalWrite(pin, HIGH);
          SerialPort.println(F("set done"));
        }
        else if ((strcmp(arg2, "low") == 0) || (strcmp(arg2, "off") == 0) || (strcmp(arg2, "0") == 0))
        {
          digitalWrite(pin, LOW);
          SerialPort.println(F("set done"));       
        }
        else SerialPort.println(F("Error: invalid argument 2"));
      }
      else SerialPort.println(F("Error: invalid argument 1"));
    }
    else SerialPort.println(F("Error: invalid argument 1"));
    
    
  }
  else if (startsWith(arg1, "pwm"))
  {
    arg1 = &arg1[3];
    if (isValidNumber(arg1))
    {
      int pin = atoi(arg1);
      if ((pin >= 9) && (pin <= 11))
      {
        if (isValidNumber(arg2))
        {
          int value = atoi(arg2);
          if ((value >= 0) && (value <= 255))
          {
            analogWrite(pin, value);
            SerialPort.println(F("set done"));
          }
          else SerialPort.println(F("Error: invalid argument 2"));
          
        }
        else SerialPort.println(F("Error: invalid argument 2"));
      }
      else SerialPort.println(F("Error: invalid argument 1"));
    }
    else SerialPort.println(F("Error: invalid argument 1"));
  }
  else SerialPort.println(F("Error: invalid argument 1"));
}

void onToggle()
{
  char* arg1 = sCmd.next();

  if (arg1 == NULL) SerialPort.println(F("Error: incorrect number of arguments"));
  else if (startsWith(arg1, "d"))
  {
    arg1 = &arg1[1];
    if (isValidNumber(arg1))
    {
      int pin = atoi(arg1);
      if ((pin >= 2) && (pin <= 4))
      {
          digitalWrite(pin, not digitalRead(pin));
          SerialPort.println(F("toggle done"));
      }
      else SerialPort.println(F("Error: invalid argument 1"));
    }
    else SerialPort.println(F("Error: invalid argument 1"));
  }
  else SerialPort.println(F("Error: invalid argument 1"));
}

void onGet()
{
  char* arg1 = sCmd.next();

  if (arg1 == NULL) SerialPort.println(F("Error: incorrect number of arguments"));
  else if (startsWith(arg1, "d"))
  {
    arg1 = &arg1[1];
    if (isValidNumber(arg1))
    {
      int pin = atoi(arg1);
      if ((pin >= 2) && (pin <= 7))
      {
        SerialPort.print("d");
        SerialPort.print(pin);
        SerialPort.print(": ");
        SerialPort.println(digitalRead(pin));
      }
      else SerialPort.println(F("Error: invalid argument 1"));
    }
    else SerialPort.println(F("Error: invalid argument 1"));
  }
  else if (startsWith(arg1, "a"))
  {
    arg1 = &arg1[1];
    if (isValidNumber(arg1))
    {
      int pin = atoi(arg1);
      if ((pin >= 0) && (pin <= 5))
      {
        SerialPort.print("a");
        SerialPort.print(pin);
        SerialPort.print(": ");
        SerialPort.println(analogReadDelay(A0 + pin, 50000));        
      }
      else SerialPort.println(F("Error: invalid argument 1"));
    }
    else SerialPort.println(F("Error: invalid argument 1"));    
  }
  else SerialPort.println(F("Error: invalid argument 1"));
}

void onPing()
{
  SerialPort.println(F("pong"));
  numberOfPings++;  
}

void onHelp()
{
  SerialPort.println(F("BZL opdracht seriële communicatie"));
  SerialPort.println();
  SerialPort.println(F("Verbind 3 leds met 220Ω voorschakelweerstand: kathode -> weerstand -> GND."));
  SerialPort.println(F("LET OP: korte pin/platte kant van LED = kathode (naar GND)."));
  SerialPort.println();
  SerialPort.println(F("Schema digitale uitgangen: d2, d3, d4 als outputs."));
  SerialPort.println();
  SerialPort.println(F("Commando's:"));
  SerialPort.println(F("\tset d[2..4] [0|1|on|off|high|low]"));
  SerialPort.println(F("\tset pwm[9..11] [0..255]"));
  SerialPort.println(F("\ttoggle d[2..4]"));
  SerialPort.println(F("\tget d[2..7]"));
  SerialPort.println(F("\tget a[0..5]"));
  SerialPort.println(F("\tping"));
  SerialPort.println(F("\tdebug"));
  SerialPort.println(F("\thelp"));
  SerialPort.println();
  SerialPort.println(F("Probeer in de seriële monitor:"));
  SerialPort.println(F("\tset d2 1"));
  SerialPort.println(F("\ttoggle d3"));
  SerialPort.println(F("\tset d4 high"));
  SerialPort.println(F("\ttoggle d2"));
  SerialPort.println(F("\tset d3 off"));
  SerialPort.println(F("\tset d4 0"));
  SerialPort.println();
  SerialPort.println(F("Experimenten / vragen:"));
  SerialPort.println(F("\tWat gebeurt er bij: SET d2 low / set d1 high / set d2 OFF ?"));
  SerialPort.println(F("\tWat gebeurt er als je 'Geen regeleinde' kiest?"));
  SerialPort.println(F("\tWat als je de polariteit van één LED omdraait?"));
  SerialPort.println(F("\tWat als je één voorschakelweerstand 10x groter maakt?"));
  SerialPort.println(F("\tWat als je de verbinding tussen Arduino en PC verbreekt?"));
}

void onDebug()
{
  SerialPort.print(F("Je hebt "));
  SerialPort.print(numberOfPings);
  SerialPort.println(F(" keer ping pong gespeeld sinds de laatste reset"));
  SerialPort.println(F("De ping pong teller wordt nu gereset"));
  numberOfPings = 0;
}

boolean isValidNumber(char* str)
{
  int len = strlen(str);
  if ((len < 1) || (len > 3)) return false;
  else for (int i = 0; i < len; i++) if (!isDigit(str[i])) return false;
  return true;
} 

boolean startsWith(char* arg, char* str)
{
  int len = strlen(str);
  if (len >= strlen(arg)) return false;
  else for (int i = 0; i < len; i++) if (arg[i] != str[i]) return false;
  return true;
}
