# WiFiTimeManager

ESP32 WiFi Time manager with fallback web configuration portal and timezone, DST, and NTP setup.

This library is derived from tzapu's WiFiManager library  
[https://github.com/tzapu/WiFiManager](https://github.com/tzapu/WiFiManager)

This library works with the ESP32 Arduino platform  
[https://github.com/esp8266/Arduino](https://github.com/esp8266/Arduino)

It has been tested on the Adafruit Feather Huzzah 32 - ESP32 Feather Board  
[https://www.adafruit.com/product/3405](https://www.adafruit.com/product/3405)

---

## What It Does

WiFiTimeManager builds upon tzapu's WiFiManager library.  For basic operation, see the WiFiManager github at  
[https://github.com/tzapu/WiFiManager](https://github.com/tzapu/WiFiManager).

WiFiTimeManager adds the following features:
- Ability to to select the timezone via the Setup web page.
- Ability to select use of daylight savings time (DST) or not.
- Ability to set DST start and end information via the Setup web page.
- Ability to select and automatically connect to a local network time protocol (NTP) server to keep accurate time.
- Ability to (fairly) easily add additional fields to the Setup web page.
- Ability to easily add real time clock (RTC) hardware to allow for accurate time keeping even when not connected to an NTP server.

---

## How It Looks

### WiFiTimeManager Home Page
![WiFiManager Home Page](https://imgur.com/7WKNXam.png)  
#### WiFiTimeManager Setup Page
 ![WiFiTimeManager Setup Page](https://imgur.com/3gHHUuj.png)

---

## Installing WiFiTimeManager

WiFiTimeManager is not currently known by the Arduino Library Manager, so it must be installed manually.  This process is well documented [HERE](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries).   WiFiTimeManager is also dependent on WiFiManager, so WiFiManager will also need to be installed as described on its github page.

---

## Using WiFiTimeManager

- The sketch must include the WiFiTimeManager class header file.  Note that it is not necessary to include the WiFiManager.h file since WiFiTimeManager already includes it:  
```cpp
#include "WiFiTimeManager.h"    // Manages timezone and dst.
```

- The WiFiTimeManager class is implemented as a singleton.  Its static Instance() method must be called at least once to create the singleton and fetch a pointer to it.  The Instance() method may be called in as many places as needed.
```cpp
// Point to the WiFiTimeManager singleton instance.
WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
```

- The WiFiTimeManager must be initialized via a call to Init().  By default, Init() will set the WiFiManager configuration portal to use blocking mode, and will default to the dark theme.  Init() takes two arguments as described below.
  - The first argument points to a NULL terminated character string representing the name  for the network access point (AP) that will be generated.
  - The second argument is optional, and specifies whether a separate 'Setup' button will appear on the WiFiTimeManager home page.  If set to 'true', then a separate 'Setup' will be displayed.  If set to 'false', the timezone, DST, and NTP fields will reside below the WiFi credentials on the 'Configure WiFi' page.  This argument defaults to a value of 'true'.
```cpp
// Initialize with our AP name and button selections.
pWtm->Init(AP_NAME, true);
```

- Finally, the WiFiTimeManager must connect to the WiFi via the autoConnect() method.
 - The first (optional) argument points to a NULL terminated character string representing the name  for the network access point (AP) that will be generated.  This should point to the same string that was passed to the Init() method.
 - The second (optional) argument specifies the AP password, if desired.  If not specified, or NULL, no password will be used.
```cpp
// Attempt to connect with our AP name, and password.
pWtm->autoConnect(AP_NAME, AP_PASSWORD);
```

With the minimal code as specified above, when the ESP32 starts it will try to connect to the WiFi.  If it fails, it will start in Access Point (AP) mode.  While in AP mode, its web page may be accessed via an IP address of 192.146.4.1.  Selecting the 'Setup' button of the web page will allow the user to set timezone, DST, and NTP parameters for the local area.  Selecting the 'Configure WiFi' button allows for setting credentials for the local WiFi network.  The 'Setup' page should be completed before the 'Configure WiFi' page since once the WiFi credentials are configured, AP mode is disabled.

---
## Documentation

