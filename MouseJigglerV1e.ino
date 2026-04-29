// ------------------------
// bluetooth mouse Jiggler
// V1.1l  fixed battery transmit disconnect issue
//        added clock set function
//        added AM/PM display
//        added C/F temperature option
//        changed runtime display to minutes + seconds
//        added screen dimming after 10 minutes
//        fixed M5StickC Plus brightness scale 0-100
//        fixed wake/undim so screen stays bright for 10 more minutes
//        added M5-Nemo-style settings menu
//        added menu clock set and screen rotation
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
// Screen rotation option
//
// M5StickC Plus landscape orientations:
//   3 = original orientation used by your sketch
//   1 = flipped landscape orientation
// -----------------------------------------------------------------------------
const byte SCREEN_ROTATION_DEFAULT = 3;
byte lcdRotation = SCREEN_ROTATION_DEFAULT;

// -----------------------------------------------------------------------------
// Screen dimming option
//
// M5StickC Plus ScreenBreath brightness uses 0-100.
//
// 100 = full bright
// 15  = dim
//
// If 15 is too dim, try 20 or 25.
// If 100 is too bright, try 80 or 90.
// -----------------------------------------------------------------------------
const bool ENABLE_SCREEN_DIMMING = true;

const unsigned long SCREEN_DIM_AFTER_MS = 10UL * 60UL * 1000UL; // 10 minutes

const byte SCREEN_BRIGHTNESS_NORMAL = 100;
const byte SCREEN_BRIGHTNESS_DIM    = 15;

unsigned long lastScreenActivityMillis = 0;
bool screenIsDimmed = false;

// Prevent AXP restart immediately after using the AXP button to wake the screen.
unsigned long suppressAxpRestartUntilMillis = 0;

// -----------------------------------------------------------------------------
// Menu control
//
// Main screen:
//   Short press front/M5 button = toggle jiggler
//   Long press front/M5 button  = open settings menu
//
// Settings menu:
//   Side button       = next item
//   Front/M5 button   = select/change item
//   AXP/power button  = exit menu
// -----------------------------------------------------------------------------
const unsigned long MENU_LONG_PRESS_MS = 1000;

enum AppMode {
  MODE_MAIN,
  MODE_MENU
};

AppMode appMode = MODE_MAIN;

enum MenuItem {
  MENU_SET_HOUR,
  MENU_SET_MINUTE,
  MENU_TOGGLE_AMPM,
  MENU_SAVE_TIME,
  MENU_ROTATE_SCREEN,
  MENU_EXIT
};

const byte MENU_ITEM_COUNT = 6;

byte menuIndex = 0;
bool menuNeedsRedraw = false;

byte editHour12 = 12;
byte editMinute = 0;
bool editIsPM = false;

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
// Get formatted runtime text for LCD line 3.
//
// Format example:
//   1 min 4 sec
// -----------------------------------------------------------------------------
String getRuntimeDisplay() {
  unsigned long totalSeconds = timer.read() / 1000;
  unsigned long minutesValue = totalSeconds / 60;
  unsigned long secondsValue = totalSeconds % 60;

  return String(" ") +
         String(minutesValue) +
         String(" min ") +
         String(secondsValue) +
         String(" sec");
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
// Reset cached LCD line text.
// Call this after fillScreen() or rotation changes.
// -----------------------------------------------------------------------------
void resetLcdLineCache() {
  for (byte i = 0; i < 7; i++) {
    lcdText[i] = " ";
  }
}

// -----------------------------------------------------------------------------
// Function to paint the lines of text.
// Only rewrites a line when the text changes.
// -----------------------------------------------------------------------------
void writeText(String _Text, int _ColorText, int _Line) {
  if (_Line < 0 || _Line > 6) {
    return;
  }

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

// -----------------------------------------------------------------------------
// Set LCD backlight brightness.
// Uses the M5StickC Plus AXP power chip.
//
// Brightness range:
//   0   = off / very dark
//   100 = full bright
// -----------------------------------------------------------------------------
void setScreenBrightness(byte brightnessValue) {
  if (brightnessValue > 100) {
    brightnessValue = 100;
  }

  // Make sure the LCD backlight is enabled.
  M5.Axp.ScreenSwitch(true);

  // Set brightness level.
  M5.Axp.ScreenBreath(brightnessValue);
}

// -----------------------------------------------------------------------------
// Set screen to normal brightness and restart the 10-minute dim timer.
//
// This is the important part:
// every wake/button event resets lastScreenActivityMillis.
// -----------------------------------------------------------------------------
void wakeScreenAndResetDimTimer() {
  setScreenBrightness(SCREEN_BRIGHTNESS_NORMAL);
  lastScreenActivityMillis = millis();
  screenIsDimmed = false;
}

// -----------------------------------------------------------------------------
// Dim screen after 10 minutes since the last wake/button activity.
// This does not stop the jiggler or BLE mouse.
// -----------------------------------------------------------------------------
void updateScreenDimmer() {
  if (!ENABLE_SCREEN_DIMMING) {
    return;
  }

  if (!screenIsDimmed && millis() - lastScreenActivityMillis >= SCREEN_DIM_AFTER_MS) {
    setScreenBrightness(SCREEN_BRIGHTNESS_DIM);
    screenIsDimmed = true;
  }
}

// -----------------------------------------------------------------------------
// Front/M5 button is active LOW in your original sketch.
// -----------------------------------------------------------------------------
bool isFrontButtonDown() {
  return digitalRead(M5_BUTTON_HOME) == LOW;
}

// -----------------------------------------------------------------------------
// Wait until the front/home and side/right buttons are released.
// Used when waking from dim mode so the same press does not also toggle state.
// -----------------------------------------------------------------------------
void waitForFrontAndSideButtonsReleased() {
  while (isFrontButtonDown() || M5.BtnB.isPressed()) {
    M5.update();
    delay(1);
  }

  delay(50); // small debounce delay
}

// -----------------------------------------------------------------------------
// Check whether any physical button was pressed.
//
// Front/Home button:
//   digitalRead(M5_BUTTON_HOME) == LOW
//
// Side/right button:
//   M5.BtnB.isPressed()
//
// AXP power button:
//   axpButtonPress == 0x01 or 0x02
// -----------------------------------------------------------------------------
bool anyButtonPressed(byte axpButtonPress) {
  if (isFrontButtonDown()) {
    return true;
  }

  if (M5.BtnB.isPressed()) {
    return true;
  }

  if (axpButtonPress == 0x01 || axpButtonPress == 0x02) {
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
// Wake screen from any button.
//
// If screen is dimmed:
//   - Wake screen to full brightness
//   - Reset 10-minute timer
//   - Return true so loop() skips the rest of this pass
//
// If screen is already awake:
//   - Reset the 10-minute timer when a button is pressed
//   - Return false so normal button behavior continues
//
// Important:
//   bool wasDimmed = screenIsDimmed;
// captures the old dim state BEFORE wakeScreenAndResetDimTimer() changes it.
// -----------------------------------------------------------------------------
bool handleScreenWakeFromAnyButton(byte axpButtonPress) {
  if (!ENABLE_SCREEN_DIMMING) {
    return false;
  }

  bool wasDimmed = screenIsDimmed;

  if (anyButtonPressed(axpButtonPress)) {
    wakeScreenAndResetDimTimer();

    if (wasDimmed) {
      // If the AXP button was used to wake the screen, suppress restart briefly.
      if (axpButtonPress == 0x01 || axpButtonPress == 0x02) {
        suppressAxpRestartUntilMillis = millis() + 1500;
      }

      waitForFrontAndSideButtonsReleased();

      // true means:
      // "screen was dimmed, this button press was only used to wake it"
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
// Set the RTC clock using 24-hour time.
// Example:
//   setClock24Hour(21, 5, 0);  // 9:05:00 PM
// -----------------------------------------------------------------------------
void setClock24Hour(byte hour24, byte minuteValue, byte secondValue) {
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

  if (hour12 < 1 || hour12 > 12) hour12 = 12;
  if (minuteValue > 59) minuteValue = 0;
  if (secondValue > 59) secondValue = 0;

  if (isPM) {
    if (hour12 == 12) {
      hour24 = 12;
    } else {
      hour24 = hour12 + 12;
    }
  } else {
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
// Convert 24-hour RTC value into the editable 12-hour menu values.
// -----------------------------------------------------------------------------
void loadCurrentTimeIntoMenuEditor() {
  M5.Rtc.GetTime(&TimeStruct);

  byte hour24 = TimeStruct.Hours;

  if (hour24 == 0) {
    editHour12 = 12;
    editIsPM = false;
  } else if (hour24 < 12) {
    editHour12 = hour24;
    editIsPM = false;
  } else if (hour24 == 12) {
    editHour12 = 12;
    editIsPM = true;
  } else {
    editHour12 = hour24 - 12;
    editIsPM = true;
  }

  editMinute = TimeStruct.Minutes;
}

// -----------------------------------------------------------------------------
// Draw the normal main screen title.
// -----------------------------------------------------------------------------
void drawMainHeader() {
  writeText(" Mouse Jig v1.1l", ORANGE, 0);
}

// -----------------------------------------------------------------------------
// Apply the current LCD rotation and redraw the active screen.
// -----------------------------------------------------------------------------
void applyScreenRotation() {
  M5.Lcd.setRotation(lcdRotation);
  M5.Lcd.fillScreen(BLACK);
  resetLcdLineCache();

  if (appMode == MODE_MENU) {
    menuNeedsRedraw = true;
  } else {
    drawMainHeader();
  }
}

// -----------------------------------------------------------------------------
// Toggle between the two landscape orientations.
// -----------------------------------------------------------------------------
void rotateScreen() {
  if (lcdRotation == 3) {
    lcdRotation = 1;
  } else {
    lcdRotation = 3;
  }

  applyScreenRotation();
}

// -----------------------------------------------------------------------------
// Open settings menu.
// -----------------------------------------------------------------------------
void openSettingsMenu() {
  wakeScreenAndResetDimTimer();

  appMode = MODE_MENU;
  menuIndex = 0;

  loadCurrentTimeIntoMenuEditor();

  M5.Lcd.fillScreen(BLACK);
  resetLcdLineCache();

  menuNeedsRedraw = true;
}

// -----------------------------------------------------------------------------
// Close settings menu and return to normal status display.
// -----------------------------------------------------------------------------
void closeSettingsMenu() {
  wakeScreenAndResetDimTimer();

  appMode = MODE_MAIN;

  M5.Lcd.fillScreen(BLACK);
  resetLcdLineCache();

  drawMainHeader();
}

// -----------------------------------------------------------------------------
// Build one menu line with selected marker.
// -----------------------------------------------------------------------------
String menuLine(byte itemIndex, String textValue) {
  if (menuIndex == itemIndex) {
    return String("> ") + textValue;
  }

  return String("  ") + textValue;
}

// -----------------------------------------------------------------------------
// Draw settings menu.
//
// Menu uses 7 lines:
//   0 = title
//   1-6 = menu items
// -----------------------------------------------------------------------------
void drawSettingsMenu() {
  if (!menuNeedsRedraw) {
    return;
  }

  M5.Lcd.fillScreen(BLACK);
  resetLcdLineCache();

  writeText(" SETTINGS MENU", ORANGE, 0);

  writeText(menuLine(MENU_SET_HOUR, String("Hour: ") + String(editHour12)), WHITE, 1);
  writeText(menuLine(MENU_SET_MINUTE, String("Minute: ") + twoDigits(editMinute)), WHITE, 2);
  writeText(menuLine(MENU_TOGGLE_AMPM, String("AM/PM: ") + String(editIsPM ? "PM" : "AM")), WHITE, 3);
  writeText(menuLine(MENU_SAVE_TIME, "Save Clock"), GREEN, 4);
  writeText(menuLine(MENU_ROTATE_SCREEN, String("Rotate: ") + String(lcdRotation)), BLUE, 5);
  writeText(menuLine(MENU_EXIT, "Exit Menu"), RED, 6);

  menuNeedsRedraw = false;
}

// -----------------------------------------------------------------------------
// Activate selected menu item.
// -----------------------------------------------------------------------------
void selectCurrentMenuItem() {
  wakeScreenAndResetDimTimer();

  switch (menuIndex) {
    case MENU_SET_HOUR:
      editHour12++;
      if (editHour12 > 12) {
        editHour12 = 1;
      }
      menuNeedsRedraw = true;
      break;

    case MENU_SET_MINUTE:
      editMinute++;
      if (editMinute > 59) {
        editMinute = 0;
      }
      menuNeedsRedraw = true;
      break;

    case MENU_TOGGLE_AMPM:
      editIsPM = !editIsPM;
      menuNeedsRedraw = true;
      break;

    case MENU_SAVE_TIME:
      // Seconds are reset to 0 when saving from the menu.
      setClock12Hour(editHour12, editMinute, 0, editIsPM);
      closeSettingsMenu();
      break;

    case MENU_ROTATE_SCREEN:
      rotateScreen();
      menuNeedsRedraw = true;
      break;

    case MENU_EXIT:
      closeSettingsMenu();
      break;
  }
}

// -----------------------------------------------------------------------------
// Handle settings menu controls.
//
// Side button:
//   Move to next menu item.
//
// Front/M5 button:
//   Select/change current menu item.
//
// AXP/power button:
//   Exit menu.
// -----------------------------------------------------------------------------
void handleSettingsMenu(byte axpButtonPress) {
  drawSettingsMenu();

  // AXP/power button acts as Home/Back while inside the menu.
  if (axpButtonPress == 0x01 || axpButtonPress == 0x02) {
    closeSettingsMenu();
    return;
  }

  // Side button moves cursor to next item.
  if (M5.BtnB.wasPressed()) {
    wakeScreenAndResetDimTimer();

    menuIndex++;
    if (menuIndex >= MENU_ITEM_COUNT) {
      menuIndex = 0;
    }

    menuNeedsRedraw = true;
    return;
  }

  // Front/M5 button selects current item.
  if (isFrontButtonDown()) {
    selectCurrentMenuItem();

    while (isFrontButtonDown()) {
      M5.update();
      delay(1);
    }

    delay(50);
    return;
  }
}

// -----------------------------------------------------------------------------
// Toggle mouse jiggler enabled/disabled.
// -----------------------------------------------------------------------------
void toggleJigglerState() {
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
}

// -----------------------------------------------------------------------------
// Handle front/M5 button on the main screen.
//
// Short press:
//   Toggle jiggler.
//
// Long press:
//   Open settings menu.
// -----------------------------------------------------------------------------
void handleMainFrontButton() {
  if (!isFrontButtonDown()) {
    return;
  }

  wakeScreenAndResetDimTimer();

  unsigned long pressStartMillis = millis();
  bool menuOpened = false;

  while (isFrontButtonDown()) {
    M5.update();

    if (!menuOpened && millis() - pressStartMillis >= MENU_LONG_PRESS_MS) {
      openSettingsMenu();
      menuOpened = true;
    }

    delay(1);
  }

  delay(50);

  if (menuOpened) {
    return;
  }

  // Short press keeps original behavior.
  if (bleMouse.isConnected()) {
    toggleJigglerState();
  }
}

void setup() {
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH);

  // Initialize M5 hardware first.
  M5.begin();

  // Force screen on and full brightness immediately at boot.
  setScreenBrightness(SCREEN_BRIGHTNESS_NORMAL);

  // Start BLE mouse after M5 hardware is ready.
  bleMouse.begin();

  M5.Lcd.setRotation(lcdRotation);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);

  // Start the 10-minute dim timer.
  wakeScreenAndResetDimTimer();

  drawMainHeader();

  // ---------------------------------------------------------------------------
  // OPTIONAL: Set the clock here.
  //
  // Uncomment ONE of these lines, upload the sketch, then comment it back out
  // and upload again if you do not want the time reset on every boot.
  //
  // The new settings menu can also set hour/minute/AM/PM directly on the device.
  // ---------------------------------------------------------------------------

  // setClock12Hour(9, 5, 0, false);  // 9:05:00 AM
  // setClock12Hour(9, 5, 0, true);   // 9:05:00 PM

  // setClock24Hour(9, 5, 0);         // 9:05:00 AM
  // setClock24Hour(21, 5, 0);        // 9:05:00 PM
}

void loop() {
  // Required for M5.BtnB button state updates.
  M5.update();

  // Read AXP button once per loop.
  byte axpButtonPress = M5.Axp.GetBtnPress();

  // ---------------------------------------------------------------------------
  // Wake screen from any button.
  //
  // If the screen was dimmed, the button press only wakes the screen and resets
  // the 10-minute timer. The same press will not toggle, select, or restart.
  // ---------------------------------------------------------------------------
  if (handleScreenWakeFromAnyButton(axpButtonPress)) {
    updateScreenDimmer();
    delay(5);
    return;
  }

  // ---------------------------------------------------------------------------
  // Settings menu mode.
  // ---------------------------------------------------------------------------
  if (appMode == MODE_MENU) {
    handleSettingsMenu(axpButtonPress);
    updateScreenDimmer();
    delay(5);
    return;
  }

  // ---------------------------------------------------------------------------
  // Calculate mouse movement direction.
  // ---------------------------------------------------------------------------
  if (mve > 900) {
    mve = -900;
  } else {
    mve = mve + 1;
  }

  if (mve > 0) {
    moveme = 3;
  } else {
    moveme = -3;
  }

  // ---------------------------------------------------------------------------
  // M5 front/home button behavior on main screen.
  //
  // Short press:
  //   Toggle jiggler.
  //
  // Long press:
  //   Open settings menu.
  // ---------------------------------------------------------------------------
  handleMainFrontButton();

  // If the menu opened, stop processing the main screen this pass.
  if (appMode == MODE_MENU) {
    updateScreenDimmer();
    delay(5);
    return;
  }

  // ---------------------------------------------------------------------------
  // If BLE disconnected, force disabled state.
  // ---------------------------------------------------------------------------
  if (!bleMouse.isConnected()) {
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
  //
  // Format example:
  //   1 min 4 sec
  // ---------------------------------------------------------------------------
  tstr = getRuntimeDisplay();
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
  // AXP button handling on main screen.
  //
  // If screen was dimmed:
  //   That press was already handled as wake-only at the top of loop().
  //
  // If screen is awake:
  //   Short AXP press restarts the device.
  //
  // In settings menu:
  //   AXP button exits menu instead.
  //
  // 0x01 = long press
  // 0x02 = short press
  // ---------------------------------------------------------------------------
  if (axpButtonPress == 0x02 && millis() > suppressAxpRestartUntilMillis) {
    esp_restart();
  }

  // ---------------------------------------------------------------------------
  // Dim screen after 10 minutes from the last button/wake activity.
  //
  // This does not stop the mouse jiggler.
  // ---------------------------------------------------------------------------
  updateScreenDimmer();

  delay(5);
}  if (volt < 3.91) return 65;
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
// Get formatted runtime text for LCD line 3.
//
// Old format:
//   64 sec
//
// New format:
//   1 min 4 sec
// -----------------------------------------------------------------------------
String getRuntimeDisplay() {
  unsigned long totalSeconds = timer.read() / 1000;
  unsigned long minutesValue = totalSeconds / 60;
  unsigned long secondsValue = totalSeconds % 60;

  return String(" ") +
         String(minutesValue) +
         String(" min ") +
         String(secondsValue) +
         String(" sec");
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

  if (hour12 < 1 || hour12 > 12) hour12 = 12;
  if (minuteValue > 59) minuteValue = 0;
  if (secondValue > 59) secondValue = 0;

  if (isPM) {
    if (hour12 == 12) {
      hour24 = 12;
    } else {
      hour24 = hour12 + 12;
    }
  } else {
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

  M5.begin();
  bleMouse.begin();

  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);

  writeText(" Mouse Jig v1.1g", ORANGE, 0);

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
    moveme = 3;
  } else {
    moveme = -3;
  }

  // ---------------------------------------------------------------------------
  // M5 button toggle on/off.
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
  //
  // New format example:
  //   1 min 4 sec
  // ---------------------------------------------------------------------------
  tstr = getRuntimeDisplay();
  writeText(tstr, ORANGE, 3);

  // ---------------------------------------------------------------------------
  // Show and transmit battery level.
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
  // ---------------------------------------------------------------------------
  tmstr = getTemperatureDisplay();
  writeText(tmstr, BLUE, 5);

  // ---------------------------------------------------------------------------
  // Show clock on line 6.
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
}  if (volt < 3.77) return 30;
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
