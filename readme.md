# M5StickC Plus BLE Mouse Jiggler

Bluetooth mouse jiggler for the **M5StickC Plus 1.1** using an ESP32 BLE mouse library.

This project turns an M5StickC Plus into a Bluetooth mouse that can gently move the cursor left and right to keep a computer awake. It also displays connection status, enabled/disabled state, runtime, battery level, device temperature, and clock time on the built-in LCD.

The current version also includes a small on-device settings menu, similar in style to the M5-Nemo button workflow.

---

## Current Version

```text
V1.1l
```

---

## Features

- Bluetooth HID mouse using BLE
- Toggle mouse movement using the front/home button
- Built-in LCD status display
- Runtime counter shown as minutes and seconds
- Battery percentage display
- BLE battery level reporting
- Battery reporting throttled to prevent BLE disconnects
- Temperature display from the AXP192 power chip
- User-selectable Celsius/Fahrenheit temperature display in code
- Clock display with AM/PM format
- Helper functions to set the RTC clock from code
- On-device settings menu
- Set clock hour, minute, and AM/PM from the device menu
- Rotate screen orientation from the device menu
- LCD screen dimming after 10 minutes
- Press any button to wake/undim the screen
- Screen stays bright for another 10 minutes after wake
- Corrected M5StickC Plus brightness scale: 0-100
- AXP/power button restart behavior on main screen
- AXP/power button exits the settings menu

---

## Hardware

Tested with:

- M5StickC Plus 1.1

May also work with similar M5StickC models, but the code was written for the Plus 1.1.

---

## Arduino Board Setup

Add the M5Stack board manager URL in Arduino IDE:

```text
https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json
```

Then install the correct M5Stack ESP32 board package.

---

## Required Libraries

Install these Arduino libraries:

```text
M5StickCPlus
ESP32 BLE Mouse
Timer
```

Library references used by the sketch:

```text
https://github.com/m5stack/M5StickC
https://github.com/T-vK/ESP32-BLE-Mouse
https://github.com/sstaub/Timer
```

---

## Bluetooth Device Name

The device advertises as:

```text
M5 Mouse
```

Manufacturer name:

```text
M5
```

This is set in the sketch with:

```cpp
BleMouse bleMouse("M5 Mouse", "M5", 100);
```

---

## Main Screen Controls

| Button | Action |
|---|---|
| Short press front/home button | Toggle jiggler enabled/disabled |
| Hold front/home button for 1 second | Open settings menu |
| Short press AXP/power button | Restart device |
| Any button while screen is dimmed | Wake screen only |

When the screen is dimmed, the first button press only wakes the screen. It does not toggle the jiggler or restart the device.

---

## Settings Menu Controls

The menu uses an M5-Nemo-style button layout.

| Button | Action |
|---|---|
| Side button | Move to next menu item |
| Front/home button | Select/change current menu item |
| AXP/power button | Exit menu |

---

## Settings Menu Items

| Menu Item | Action |
|---|---|
| Hour | Increments hour from 1-12 |
| Minute | Increments minute from 00-59 |
| AM/PM | Toggles AM or PM |
| Save Clock | Saves selected time to RTC |
| Rotate | Toggles landscape screen rotation |
| Exit Menu | Returns to main screen |

Seconds are reset to `0` when saving time from the menu.

---

## Screen Display

The LCD shows:

| Line | Display |
|---|---|
| 0 | Project title/version |
| 1 | Enabled/disabled status |
| 2 | BLE connection/movement status |
| 3 | Runtime in minutes and seconds |
| 4 | Battery level |
| 5 | Temperature |
| 6 | Clock with AM/PM |

Example:

```text
Mouse Jig v1.1l
>> ENABLED
MOVING mouse
1 min 4 sec
Battery Level 85%
Temp: 89F
Time: 9:04:07 PM
```

---

## Battery Disconnect Fix

The original code had a battery update issue that could cause Bluetooth disconnects.

Problem line:

```cpp
oldBat == getBatCapacity();
```

That line compares values instead of assigning a new value.

Correct assignment would be:

```cpp
oldBat = getBatCapacity();
```

The updated code uses a better fix:

- Battery level is checked only every 30 seconds
- BLE battery level is only sent when the value changes
- Battery percentage is clamped to 0-100
- Repeated calls to `bleMouse.setBatteryLevel()` are avoided

This prevents the BLE battery service from being spammed many times per second.

---

## Runtime Display

The runtime display was changed from seconds only:

```text
64 sec
```

To minutes and seconds:

```text
1 min 4 sec
```

This is handled by:

```cpp
String getRuntimeDisplay()
```

---

## Temperature Display

The AXP192 temperature is read in Celsius.

Change this value near the top of the sketch:

```cpp
const bool TEMP_IN_FAHRENHEIT = false;
```

Options:

```text
false = Celsius
true  = Fahrenheit
```

Example:

```cpp
const bool TEMP_IN_FAHRENHEIT = true;
```

---

## Screen Dimming

The screen dims after 10 minutes.

Important brightness values:

```cpp
const byte SCREEN_BRIGHTNESS_NORMAL = 100;
const byte SCREEN_BRIGHTNESS_DIM    = 15;
```

M5StickC Plus brightness uses a 0-100 scale.

```text
100 = full bright
15  = dim
```

To change the dim delay, edit:

```cpp
const unsigned long SCREEN_DIM_AFTER_MS = 10UL * 60UL * 1000UL;
```

Examples:

```cpp
const unsigned long SCREEN_DIM_AFTER_MS = 5UL * 60UL * 1000UL;   // 5 minutes
const unsigned long SCREEN_DIM_AFTER_MS = 15UL * 60UL * 1000UL;  // 15 minutes
```

Behavior:

```text
Boots bright
After 10 minutes, dims
Press any button
Wakes to full brightness
Stays bright for another 10 minutes
```

---

## Clock Setting From Code

The sketch includes two helper functions for setting the clock manually from code.

### Set Clock Using 12-Hour Time

```cpp
setClock12Hour(9, 5, 0, false);  // 9:05:00 AM
setClock12Hour(9, 5, 0, true);   // 9:05:00 PM
```

### Set Clock Using 24-Hour Time

```cpp
setClock24Hour(9, 5, 0);         // 9:05:00 AM
setClock24Hour(21, 5, 0);        // 9:05:00 PM
```

To set the time from code:

1. Uncomment one clock-setting line in `setup()`.
2. Upload the sketch.
3. Comment the line back out.
4. Upload again.

This prevents the clock from resetting every time the M5StickC Plus boots.

---

## Clock Setting From Menu

To set the clock directly on the M5StickC Plus:

1. Hold the front/home button for 1 second.
2. Use the side button to move through the menu.
3. Select `Hour` and press the front/home button until the correct hour is shown.
4. Select `Minute` and press the front/home button until the correct minute is shown.
5. Select `AM/PM` and press the front/home button to toggle AM/PM.
6. Select `Save Clock`.
7. Press the front/home button to save.

The screen returns to the main display after saving.

---

## Screen Rotation From Menu

To rotate the screen:

1. Hold the front/home button for 1 second.
2. Use the side button to move to `Rotate`.
3. Press the front/home button.
4. The screen toggles between landscape rotations.

The sketch uses these two rotation values:

```cpp
3 = default landscape orientation
1 = flipped landscape orientation
```

---

## Upload Steps

1. Open the sketch in Arduino IDE.
2. Select the correct M5Stack/ESP32 board.
3. Select the correct serial port.
4. Upload the sketch.
5. Pair the device with your computer using Bluetooth.
6. Look for the Bluetooth device named:

```text
M5 Mouse
```

---

## Usage

1. Power on the M5StickC Plus.
2. Pair it with the computer over Bluetooth.
3. Wait for the screen to show:

```text
CONNECTED
```

4. Short press the front/home button.
5. The screen should show:

```text
>> ENABLED
MOVING mouse
```

6. Short press the front/home button again to stop mouse movement.

---

## Version History

### v1.1l

- Added M5-Nemo-style settings menu
- Added long-press front/home button to open settings menu
- Added menu-driven clock setting
- Added menu-driven screen rotation
- Side button moves through menu items
- Front/home button selects menu items
- AXP/power button exits menu

### v1.1k

- Fixed M5StickC Plus brightness scale
- Changed screen brightness values to 0-100 scale
- Forced screen on at boot using `M5.Axp.ScreenSwitch(true)`
- Fixed startup dim screen issue

### v1.1j

- Fixed screen wake logic
- Screen now stays bright for 10 more minutes after waking
- Button press from dim mode is treated as wake-only

### v1.1i

- Added wake/undim screen when any button is pressed

### v1.1h

- Added screen dimming after 10 minutes

### v1.1g

- Changed runtime display from seconds only to minutes and seconds

### v1.1f

- Added Celsius/Fahrenheit temperature display option

### v1.1e

- Added AM/PM clock display
- Added `setClock12Hour()`
- Added `setClock24Hour()`
- Added clock formatting helper functions
- Added leading zeroes for minutes and seconds

### v1.1d

- Added leading zeroes to clock minutes and seconds

### v1.1c

- Fixed battery transmit disconnect issue
- Changed battery updates to every 30 seconds
- Only transmits BLE battery level when changed
- Clamped BLE battery level to 0-100
- Changed setup order so `M5.begin()` runs before `bleMouse.begin()`

### v1.1b

- Original working Bluetooth mouse jiggler sketch
- LCD status display
- Runtime display
- Battery display
- Temperature display
- RTC clock display

---

## Notes

This project is intended for personal use, testing, and learning with the M5StickC Plus and ESP32 BLE HID functionality.

Use responsibly and follow your workplace policies.
