# WiFiTimeManager

ESP32 WiFi Time manager with fallback web configuration portal and timezone, DST, and NTP setup.

This library is derived from tzapu's WiFiManager library:  
[https://github.com/tzapu/WiFiManager](https://github.com/tzapu/WiFiManager)

This library works with the ESP32 Arduino platform:  
[https://github.com/esp8266/Arduino](https://github.com/esp8266/Arduino)

It has been tested on the Adafruit Feather Huzzah 32 - ESP32 Feather Board:  
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

### WiFiManager Setup Page With No DST
![WiFiManager Setup Page With No DST](https://imgur.com/CHDmdGf.png)

### WiFiTimeManager Setup Page With DST
 ![WiFiTimeManager Setup Page With DST](https://imgur.com/3gHHUuj.png)
 
 

---

## Installing WiFiTimeManager

WiFiTimeManager is not currently known by the Arduino Library Manager, so it must be installed manually.  This process is well documented [HERE](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries).  Other libraries that are needed to support  WiFiTimeManager are:
- WiFiManager at [https://github.com/tzapu/WiFiManager](https://github.com/tzapu/WiFiManager).
- Timezone_Generic at [https://github.com/khoih-prog/Timezone_Generic](https://github.com/khoih-prog/Timezone_Generic).
- ArduinoJson - at [https://arduinojson.org/](https://arduinojson.org/).
- Preferences - this should be built-in.
- ESPmDNS - this should be built-in.

---

## Using WiFiTimeManager
This section describes the minimum code required to use WiFiTimeManager.  It uses the default blocking mode.  See the Examples for code using the non-blocking mode and other advanced features.

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
  - The second argument is optional, and points to a NULL terminated character string representing the password to be used in accessing the AP.  If not supplied, the AP will not require a password.  If used, the password should be 8 to 63 characters.
  - The third argument is optional, and specifies whether a separate 'Setup' button will appear on the WiFiTimeManager home page.  If set to 'true', then a separate 'Setup' will be displayed.  If set to 'false', the timezone, DST, and NTP fields will reside below the WiFi credentials on the 'Configure WiFi' page.  This argument defaults to a value of 'true'.
```cpp
// Initialize with our AP name and button selections.
pWtm->Init("AP_NAME", "AP_PASSWORD", true);
```

- Finally, the WiFiTimeManager must connect to the WiFi via the autoConnect() method.  The PA name and password that were supplied in the call to Init() are used.
```cpp
// Attempt to connect with our AP name, and password (supplied via Init()).
pWtm->autoConnect();
```

With the minimal code as specified above, when the ESP32 starts it will try to connect to the WiFi.  If it fails, it will start in Access Point (AP) mode.  While in AP mode, its web page may be accessed via an IP address of 192.146.4.1.  Selecting the 'Setup' button of the web page will allow the user to set timezone, DST, and NTP parameters for the local area.  Selecting the 'Configure WiFi' button allows for setting credentials for the local WiFi network.  The 'Setup' page should be updated and saved before the 'Configure WiFi' page since once the WiFi credentials are configured, AP mode is disabled.

---
## Documentation

### General
WiFiTimeManager extends the Arduino WiFiManager class.  It adds the ability to easily set timezone, DST, and NTP server data via extending the WiFiManager web page.  It uses the Arduino Timezone_Generic library to manage timezone and DST.  The ESP32 Preferences library is used to save and restore all setup data across power cycles.  Once connected to WiFi, WiFiTimeManager automatically connects to the specified NTP server and can supply accurate UTC and local time to the user. 

### Overridden Methods
WiFiManager methods all begin with lower case letters.  WiFiTimeManager methods all begin with capital letters.  Therefore, any overridden methods may be recognized by the first letter of their method being lower case.  All others are new to WiFiTimeManager.

Most WiFiManager methods are still available with WiFiTimeManager.  However, several WiFiManager methods have been overridden or disabled by WiFiTimeManager.  These methods include:

- WiFiManager::addParameter() - this method is used inside WiFiTimeManager::Init() and is hidden from user code.  Any attempt to invoke it will result in a compile error.  WiFiTimeManager uses a different method of adding parameters, and is described later.
- WiFiManager::resetSettings() - this method is extended by WiFiTimeManager.  This functionality has been replaced by WiFiTimeManager::ResetData().
- WiFiManager::setMenu() - this method is used inside WiFiTimeManager::Init() and is hidden from user code.  Any attempt to invoke it will result in a compile error.
- WiFiManager::process() - this method has been overridden by WiFiTimeManager to extend the WiFiManager behavior.  It is still available for use by user code.
- WiFiManager::autoConnect() - this method has been overridden by WiFiTimeManager to extend the WiFiManager behavior.  It is still available for use by user code.
- WiFiManager::setSaveParamsCallback() - this method has been overridden by WiFiTimeManager to extend the WiFiManager behavior.  It is still available for use by user code.

Any calls to WiFiManager methods should be done after the call to WiFiTimeManager::Init().

### WiFiTimeManager Internals
The WiFiTimeManager code is well documented, and should be referred to along with the examples when any detailed information is needed.  This section gives an overview of the internals of the library.

#### TimeParameters
The TimeParameters structure is defined at the top of WiFiTimeManager.h.  A copy of this structure is what gets saved/restored to/from non-volatile storage (NVS) to remember settings made by the user from the Setup page.  This data includes:
- m_Version - The version of the data being held within the structure.  It is used to keep insure that the data stored within NVS matches the current data.  This value should be bumped any time a change is made to the structure.
- m_TzOfst - The timezone offset, in minutes, from UTC as specified by the user.
- m_UseDst - If 'true', then DST is used, otherwise DST is not used.
- m_DstOfst - The offset, in minutes, from TzOfst that is in effect when DST is active. This is either 30 or 60 minutes.
- m_NtpPort - The port number to use for NTP messages.
- m_NtpAddr - A NULL terminated string representing the address of the NTP server, as set by the user.
- m_DstStartRule - The TimeChangeRule as defined in Timezone_Generic that contains DST start info.
- m_DstEndRule - The TimeChangeRule as defined in Timezone_Generic that contains DST end info.

#### WiFiTimeManager::Instance()
The WiFiTimeManager class is implemented as a singleton.  Therefore there can only be one instance in the system.  The Instance() method returns a pointer to the singleton instance.  Since the class is created in the first call to Instance(), this method must be called at least once from user code before the class may be used.

#### WiFiTimeManager::Init()
This method must be called after Instance(), and before autoConnect().  As its name implies, it sets up (initializes) the WiFiTimeManager instance for use. Its arguments are:
- pApName - The name that will be seen on the network for the WiFiManager access point.  This name must be at least one character long.
- pApPassword - The password used to access the AP.  If omitted or NULL, then no password is used.
- setupButton - Set to true if a separate "Setup" button should be displayed on the WiFiManager home page.  When selected, this button will cause a separate setup button to be displayed.  The separate setup page will contain time parameters that the user may adjust.  In this case, the WiFi configuratioin page will not display the time parameters.  If set to false, then the time parameters will be selectable from the WiFi configuration page.

Within Init(), the saved TimeParameters get restored if present.  If not, then the default values for TimeParameters get saved.  The web page gets updated in order to initialize the time values, and the web page and parameters are passed to WiFiManager.  Other parameters are set to safe values.  The config portal is set to blocking mode, and the dark theme is selected by default.  These default values may be changed by user code later if desired.

#### WiFiTimeManager::autoConnect()
This method overrides WiFiManager::autoConnect() in order to return the status of the WiFi connection, and to set the AP name and password that were passed to Init().  It automatically connects to the saved WiFi network, or starts the config portal on failure.  In blocking mode, this method will not return until a WiFi connection is made.  In non-blocking mode, the method will return after a user-settable timeout even if no WiFi connection was made.  In this case, the process() method must be periodically called from user code until the connection is made.  The method returns 'true' if a WiFi connection has been made, or false otherwise.

#### WiFiTimeManager::process()
This method overrides WiFiManager::process().  It handles the non-blocking mode of operation.  It takes no arguments.  It should be called periodically from user code, usually from within the Arduino loop() function, when the config portal is active (i.e. no WiFi connection has been made).  It returns immediately, and returns 'true' if a network connection has been made, or false otherwise.

#### WiFiTimeManager::UpdateTimezoneRules()
This method updates the timezone rules.  WiFiTimeManager provides methods to get and set each of the values for the start and end DST times as well as the capability of enabling or disabling DST.  UpdateTimezoneRules() should be called after any changes are made to the current timezone rules.  The user should not normally need to use this method since the timezone rules usually get initialized via the Setup web page.

#### Callbacks
This section describes the new callbacks supported by WiFiTimeManager.  A number of callbacks have been added at strategic places in the code to allow easy customization of WiFiTimeManager.  WiFiManager already provided several useful callbacks, most of which may still be used by the application. 

In general, pointers to callback functions are installed in the Arduino setup() function between the WiFiTimeManager Init() and autoConnect() calls.  For example, here is an outline of where to do it:

```cpp
void setup()
{
    . . .
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
    . . .
    pWtm->Init(AP_NAME, AP_PWD, SETUP_BUTTON);
    . . .
    pWtm->setSaveParamsCallback(SaveParamsCallback);
    . . .
    pWtm->autoConnect();
}
```


##### WiFiTimeManager::setSaveParamsCallback()
This callback overrides WiFiManager::setSaveParamsCallback().  It saves a pointer to a user function that is called when saving the Setup web page after the standard parameters have been  handled by WiFiTimeManager.  It is normally used to fetch the values of parameter that were added to the Setup page by the user.  For example, if an integer parameter named ''editTestNumber" was added to the Setup page, a simple callback function that prints the integer to the serial port when the parameters are saved might be:

```cpp
void SaveParamsCallback()
{
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
    Serial.printf("Integer Value = %d\n", pWtm->GetParamInt("editTestNumber"));
} // End SaveParamsCallback().
```


See SetUpdateWebPageCallback() for more information.  

##### WiFiTimeManager::SetUpdateWebPageCallback()
Sets a callback that will be invoked when when the Setup web page is being updated.  This callback can be used to monitor or modify the contents of the Setup web page.  It is normally used to add user parameters to the Setup web page.

The callback function takes two arguments.  The  first argument is a reference to a very long String that contains the entire contents of the Setup web page that will be sent to the web client.   It may be modified as needed (see the EditWebPage example).  The second argument specifies the maximum size that is permissible for the String.  Upon return from the callback, the contents of the String will be safely copied into a buffer that will be sent to the client.  If the size of the (modified) String exceeds  the value specified maximum buffer size, the buffer will be truncated, so be careful.

Several comments appear within the string that may be employed to locate specific places within the standard
web page.  The user can use these comments to insert relevant HTML or javascript code.  These comments are:
-  `"<!-- HTML START -->"`  
             This marks the start of HTML, which is also the start of the web page string.
 - `"<!-- HTML END -->"`  
             This marks the end of HTML and is just before the end of the web page body.
 - `"// JS START"`  
             This marks the start of the java script, just after the `<script>` declaration.
- `"// JS ONLOAD"`  
             This marks a spot within the onload() function where java script initialization code can be added.
 - `"// JS SAVE"`  
             This marks a spot within the function that is executed when the submit (save) button is pressed.  It can be used to perform java script save actions.
 - `"// JS END"`  
             This marks the end of of the java script, just before the `</script>` declaration.

As an example, if the user wants to add a simple integer parameter named "editTestNumber" to the standard web page, the following code outline could be used:

```cpp
// Example HTML code to demonstrate insertion of data fields on the Setup web page.
static const char EDIT_TEST_STR[] = R"(
    <br>
    <h3 style="display:inline">AN INTEGER VALUE:</h3>
    <input type="number" id="editTestNumber" name="editTestNumber" min="0" max="1000" value="0">
    <br>
)";

void UpdateWebPageCallback(String &rWebPage, uint32_t maxSize)
{
    rWebPage.replace("<!-- HTML END -->", EDIT_TEST_STR);
} // End UpdateWebPageCallback().

void setup()
{
    . . .
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
    . . .
    // The web page is updated in Init(), so setup our callback before
    // WiFiTimeManager::Init() is called.
    pWtm->SetUpdateWebPageCallback(UpdateWebPageCallback);
    . . .
    pWtm->Init(AP_NAME, AP_PWD, SETUP_BUTTON);
    . . .
    pWtm->autoConnect();
}
``` 
In this case, the callback replaces the `"<!-- HTML END -->"` substring with HTML code to add an integer parameter.  **Take  special note that SetUpdateWebPageCallback() should be called before Init()  due to Init() performing a web page update within its body.**

##### WiFiTimeManager::SetUtcGetCallback()
Sets a callback that will be invoked when non-NTP time is needed.  This callback can be used to read time from a hardware real time clock, or other time source when NTP time is not present or desired.  In general, the NTP server should not be polled very frequently.  In general, NTP servers should not be polled very frequently.  WiFiTimeManager defaults to accessing the NTP server no more than once every 60 minutes.  This can be overridden via a call to SetMinNtpRate().  If the user attempts to get time more frequently than the minimum NTP rate, the time is normally read from the Arduino time library which keeps pretty good track of time.  The user can override this by setting this callback to code that returns its idea of the current time in Unix time units (time_t in seconds since January 1, 1970).  For example, if a hardware RTC is present, it can be read and its value returned by this callback.  See RTCExample for example code.

##### WiFiTimeManager::SetUtcSetCallback()
Sets a callback that will be invoked when NTP UTC time has been received.  This callback can be used to set the time of a hardware real time clock or other alternate time source.  When called, the callback receives the Unix (time_t  in seconds since January 1, 1970) time that was just received from the NTP server as an argument.  See RTCExample for example code.


#### WiFiTimeManager::Reset()

#### WiFiTimeManager::ResetData()

#### WiFiTimeManager::Save()

#### WiFiTimeManager::Restore()

#### WiFiTimeManager::GetUtcTime()

#### WiFiTimeManager::GetLocalTime()

#### WiFiTimeManager::GetDateTimeString()

#### WiFiTimeManager::PrintDateTime()

#### WiFiTimeManager::GetLocalTimezoneString()

#### WiFiTimeManager::GetParamString()

#### WiFiTimeManager::GetParamChars()

#### WiFiTimeManager::GetParamInt()

#### Miscellaneous Getters

#### Miscellaneous Setters

