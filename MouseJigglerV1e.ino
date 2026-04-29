// ------------------------
// bluetooth mouse Jiggler
// V1.1f  fixed battery transmit disconnect issue
//        added clock set function
//        added AM/PM display
//        added C/F temperature option
// for M5 STICK C PLUS 1.1
// ------------------------

// additional board mgr  = https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json

//#include <Arduino.h>     // For PlatformIO
#include <M5StickCPlus.h>  // https://github.com/m5stack/M5StickC
#include <BleMouse.h>      // https://github.com/T-vK/ESP32-BLE-Mouse
#include "Timer.h"         // https://github.com/sstaub/Timer

BleMouse bleMouse("M5 Mouse", "M5", 100);  // setup bluetooth mouse name/mfg
Timer timer;                                // setup timer

int     mve             = 0;
int     moveme          = 0;
boolean isActivated     = false;
String  lcdText[7]      = { " ", " ", " ", " ", " ", " ", " " };
String  tstr            = " ";
String  bstr            = " ";
String  tmstr           = " ";
String  clockstr        = " ";
byte    counter         = 0;

RTC_TimeTypeDef TimeStruct;

// -----------------------------------------------------------------------------
// Temperature display option
//
// false = Celsius
// true  = Fahrenheit
// -----------------------------------------------------------------------------
const bool TEMP_IN_FAHRENHEIT = false;

// -----------------------------------------------------------------------------
// Battery update control
// -----------------------------------------------------------------------------
int oldBat = -999;
int currentBat = -999;

unsigned long lastBatteryCheck = 0;
const unsigned long BATTERY_CHECK_INTERVAL = 30000; // 30 seconds

// -----------------------------------------------------------------------------
// Convert battery voltage to percentage.
// BLE battery level should be 0-100.
// -----------------------------------------------------------------------------
int getBatCapacity() {
  float volt = M5.Axp.GetBatVoltage();

  if (volt < 3.20) return 0;
  if (volt < 3.27) return 0;
  if (volt < 3.61) return 5;
  if (volt < 3.69) return 10;
  if (volt < 3.71) return 15;
  if (volt < 3.73) return 20;
  if (volt < 3.75) return 25;
  if (volt < 3.77) return 30;
  if (volt < 3.79) return 35;
  if (volt < 3.80) return 40;
  if (volt < 3.82) return 45;
  if (volt < 3.84) return 50;
  if (volt < 3.85) return 55;
  if (volt < 3.87) return 60;
  if (volt < 3.91) return 65;
  if (volt < 3.95) return 70;
  if (volt < 3.98) return 75;
  if (volt < 4.02) return 80;
  if (volt < 4.08) return 85;
  if (volt < 4.11) return 90;
  if (volt < 4.15) return 95;

  return 100;
}

// -----------------------------------------------------------------------------
// Return a number as two digits.
// Example:
//   4  -> "04"
//   12 -> "12"
// Used for clock minutes and seconds.
// -----------------------------------------------------------------------------
String twoDigits(byte value) {
  if (value < 10) {
    return String("0") + String(value);
  }

  return String(value);
}

// -----------------------------------------------------------------------------
// Round a float to the nearest whole number.
// Avoids needing extra math includes.
// -----------------------------------------------------------------------------
int roundToInt(float value) {
  if (value >= 0) {
    return int(value + 0.5);
  } else {
    return int(value - 0.5);
  }
}

// -----------------------------------------------------------------------------
// Get formatted temperature text for LCD line 5.
//
// M5.Axp.GetTempInAXP192() returns Celsius.
// If TEMP_IN_FAHRENHEIT is true, convert C to F for display.
// -----------------------------------------------------------------------------
String getTemperatureDisplay() {
  float tempC = M5.Axp.GetTempInAXP192();

  if (TEMP_IN_FAHRENHEIT) {
    float tempF = (tempC * 9.0 / 5.0) + 32.0;
    return String("Temp: ") + String(roundToInt(tempF)) + String("F");
  }

  return String("Temp: ") + String(roundToInt(tempC)) + String("C");
}

// -----------------------------------------------------------------------------
// Set the RTC clock using 24-hour time.
// Example:
//   setClock24Hour(21, 5, 0);  // 9:05:00 PM
// -----------------------------------------------------------------------------
void setClock24Hour(byte hour24, byte minuteValue, byte secondValue) {
  // Basic safety checks so invalid values do not get written to the RTC.
  if (hour24 > 23) hour24 = 0;
  if (minuteValue > 59) minuteValue = 0;
  if (secondValue > 59) secondValue = 0;

  TimeStruct.Hours   = hour24;
  TimeStruct.Minutes = minuteValue;
  TimeStruct.Seconds = secondValue;

  M5.Rtc.SetTime(&TimeStruct);
}

// -----------------------------------------------------------------------------
// Set the RTC clock using 12-hour time with AM/PM.
// Examples:
//   setClock12Hour(9, 5, 0, false);  // 9:05:00 AM
//   setClock12Hour(9, 5, 0, true);   // 9:05:00 PM
//   setClock12Hour(12, 0, 0, false); // 12:00:00 AM / midnight
//   setClock12Hour(12, 0, 0, true);  // 12:00:00 PM / noon
// -----------------------------------------------------------------------------
void setClock12Hour(byte hour12, byte minuteValue, byte secondValue, bool isPM) {
  byte hour24;

  // Keep bad input from breaking the clock.
  if (hour12 < 1 || hour12 > 12) hour12 = 12;
  if (minuteValue > 59) minuteValue = 0;
  if (secondValue > 59) secondValue = 0;

  if (isPM) {
    // PM:
    // 12 PM = 12
    // 1 PM  = 13
    // 9 PM  = 21
    if (hour12 == 12) {
      hour24 = 12;
    } else {
      hour24 = hour12 + 12;
    }
  } else {
    // AM:
    // 12 AM = 0
    // 1 AM  = 1
    // 9 AM  = 9
    if (hour12 == 12) {
      hour24 = 0;
    } else {
      hour24 = hour12;
    }
  }

  setClock24Hour(hour24, minuteValue, secondValue);
}

// -----------------------------------------------------------------------------
// Convert 24-hour RTC time to 12-hour display.
// Examples:
//   0  -> 12 AM
//   9  -> 9 AM
//   12 -> 12 PM
//   21 -> 9 PM
// -----------------------------------------------------------------------------
String getClock12HourDisplay(byte hour24, byte minuteValue, byte secondValue) {
  byte displayHour;
  String ampm;

  if (hour24 == 0) {
    displayHour = 12;
    ampm = "AM";
  } else if (hour24 < 12) {
    displayHour = hour24;
    ampm = "AM";
  } else if (hour24 == 12) {
    displayHour = 12;
    ampm = "PM";
  } else {
    displayHour = hour24 - 12;
    ampm = "PM";
  }

  return String("Time: ") +
         String(displayHour) +
         String(":") +
         twoDigits(minuteValue) +
         String(":") +
         twoDigits(secondValue) +
         String(" ") +
         ampm;
}

// -----------------------------------------------------------------------------
// Function to paint the lines of text.
// Only rewrites a line when the text changes.
// -----------------------------------------------------------------------------
void writeText(String _Text, int _ColorText, int _Line) {
  if (lcdText[_Line] != _Text) {
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setCursor(5, _Line * 20);
    M5.Lcd.println(lcdText[_Line]);

    M5.Lcd.setTextColor(_ColorText);
    M5.Lcd.setCursor(5, _Line * 20);
    M5.Lcd.println(_Text);

    lcdText[_Line] = _Text;
  }
}

void setup() {
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH);

  // Initialize M5 hardware first.
  M5.begin();

  // Start BLE mouse after M5 hardware is ready.
  bleMouse.begin();

  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);

  writeText(" Mouse Jig v1.1f", ORANGE, 0);

  // ---------------------------------------------------------------------------
  // OPTIONAL: Set the clock here.
  //
  // Uncomment ONE of these lines, upload the sketch, then comment it back out
  // and upload again if you do not want the time reset on every boot.
  // ---------------------------------------------------------------------------

  // setClock12Hour(9, 5, 0, false);  // 9:05:00 AM
  // setClock12Hour(9, 5, 0, true);   // 9:05:00 PM

  // setClock24Hour(9, 5, 0);         // 9:05:00 AM
  // setClock24Hour(21, 5, 0);        // 9:05:00 PM
}

void loop() {

  // ---------------------------------------------------------------------------
  // Calculate mouse movement direction.
  // ---------------------------------------------------------------------------
  if (mve > 900) {
    mve = -900;
  } else {
    mve = mve + 1;
  }

  if (mve > 0) {
    moveme = 3;    // move mouse right
  } else {
    moveme = -3;   // move mouse left
  }

  // ---------------------------------------------------------------------------
  // M5 button toggle on/off.
  // Show button status on line 1.
  // ---------------------------------------------------------------------------
  if (bleMouse.isConnected()) {
    if (digitalRead(M5_BUTTON_HOME) == LOW) {
      if (!isActivated) {
        isActivated = true;
        writeText(">> ENABLED ", WHITE, 1);
        digitalWrite(M5_LED, LOW);
      } else {
        isActivated = false;
        writeText(">> DISABLED", RED, 1);
        digitalWrite(M5_LED, HIGH);
        bleMouse.release(MOUSE_MIDDLE);
      }

      while (digitalRead(M5_BUTTON_HOME) == LOW) {
        delay(1);
      }
    }
  } else {
    writeText(" ", WHITE, 1);
    isActivated = false;
    digitalWrite(M5_LED, HIGH);
  }

  // ---------------------------------------------------------------------------
  // Print line two - status line.
  // ---------------------------------------------------------------------------
  if (bleMouse.isConnected() && isActivated) {
    if (timer.state() == STOPPED) {
      timer.start();
    }

    writeText("MOVING mouse ", YELLOW, 2);
    bleMouse.move(moveme, 0);

  } else if (bleMouse.isConnected()) {
    if (timer.state() == RUNNING) {
      timer.stop();
    }

    writeText("CONNECTED", GREEN, 2);

  } else {
    writeText("*NOT CONNECTED*", WHITE, 2);

    if (timer.state() == RUNNING) {
      timer.stop();
    }
  }

  // ---------------------------------------------------------------------------
  // Show run time on line 3.
  // ---------------------------------------------------------------------------
  tstr = String(timer.read() / 1000);
  tstr = String(" ") + tstr + String(" sec");
  writeText(tstr, ORANGE, 3);

  // ---------------------------------------------------------------------------
  // Show and transmit battery level.
  //
  // Only checks every 30 seconds.
  // Only transmits to BLE if the battery value changed.
  // ---------------------------------------------------------------------------
  if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL || currentBat == -999) {
    lastBatteryCheck = millis();

    currentBat = getBatCapacity();

    bstr = String("Battery Level ") + String(currentBat) + String("%");
    writeText(bstr, BLUE, 4);

    if (bleMouse.isConnected() && currentBat != oldBat) {
      bleMouse.setBatteryLevel(currentBat);
      oldBat = currentBat;
    }
  }

  // ---------------------------------------------------------------------------
  // Show temp on line 5.
  //
  // Controlled by:
  //   const bool TEMP_IN_FAHRENHEIT = false;
  //
  // false = Celsius
  // true  = Fahrenheit
  // ---------------------------------------------------------------------------
  tmstr = getTemperatureDisplay();
  writeText(tmstr, BLUE, 5);

  // ---------------------------------------------------------------------------
  // Show clock on line 6.
  //
  // Clock displays AM/PM.
  // Example:
  //   Time: 9:04:07 PM
  // ---------------------------------------------------------------------------
  M5.Rtc.GetTime(&TimeStruct);

  clockstr = getClock12HourDisplay(
    TimeStruct.Hours,
    TimeStruct.Minutes,
    TimeStruct.Seconds
  );

  writeText(clockstr, ORANGE, 6);

  // ---------------------------------------------------------------------------
  // AXP button short press reset.
  // 0x01 = long press
  // 0x02 = short press
  // ---------------------------------------------------------------------------
  if (M5.Axp.GetBtnPress() == 0x02) {
    esp_restart();
  }

  delay(5);
}
