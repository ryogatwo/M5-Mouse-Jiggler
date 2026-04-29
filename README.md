# M5StickC Plus BLE Mouse Jiggler

Bluetooth mouse jiggler for the **M5StickC Plus 1.1** using an ESP32 BLE mouse library.

This project turns an M5StickC Plus into a Bluetooth mouse that can gently move the cursor left and right to keep a computer awake. It also displays connection status, enabled/disabled state, runtime, battery level, device temperature, and clock time on the built-in LCD.

---

## Features

- Bluetooth HID mouse using BLE
- Toggle mouse movement using the front/home button
- Built-in LCD status display
- Runtime counter
- Battery percentage display
- BLE battery level reporting
- Battery reporting throttled to prevent BLE disconnects
- Temperature display from the AXP192 power chip
- Clock display with AM/PM format
- Helper functions to set the RTC clock
- Short AXP button press restarts the device

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

## Button Controls

### Front/Home Button

Press the front/home button to toggle mouse movement:

| State | Action |
|---|---|
| Disabled | Press button to enable mouse movement |
| Enabled | Press button to disable mouse movement |

When enabled, the cursor moves left and right slightly.

### AXP Power Button

A short press of the AXP button restarts the device.

---

## Screen Display

The LCD shows:

| Line | Display |
|---|---|
| 0 | Project title/version |
| 1 | Enabled/disabled status |
| 2 | BLE connection/movement status |
| 3 | Runtime in seconds |
| 4 | Battery level |
| 5 | Temperature |
| 6 | Clock with AM/PM |

Example:

```text
Mouse Jig v1.1e
>> ENABLED
MOVING mouse
123 sec
Battery Level 85%
Temp: 32C
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

## Clock Setting

The sketch includes two helper functions for setting the clock.

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

To set the time:

1. Uncomment one clock-setting line in `setup()`.
2. Upload the sketch.
3. Comment the line back out.
4. Upload again.

This prevents the clock from being reset every time the M5StickC Plus boots.

---

## Example Clock Setup Section

```cpp
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

4. Press the front/home button.
5. The screen should show:

```text
>> ENABLED
MOVING mouse
```

6. Press the front/home button again to stop mouse movement.

---

## Version History

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
