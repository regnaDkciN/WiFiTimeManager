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
- Select the timezone via the Setup web page.
- Select use of daylight savings time (DST) or not.
- Set DST start and end information via the Setup web page.
- Select and automatically connect to a network time protocol (NTP) server to keep accurate time.
- Easily add additional fields to the Setup web page.
- Easily add real time clock (RTC) hardware to allow for accurate time keeping even when not connected to an NTP server.
- Easily fetch accurate UTC or local time.

---

## Installing WiFiTimeManager

WiFiTimeManager is not currently known by the Arduino Library Manager, so it must be installed manually.  This process is well documented [HERE](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries).  Other libraries that are needed to support  WiFiTimeManager are:
- WiFiManager at [https://github.com/tzapu/WiFiManager](https://github.com/tzapu/WiFiManager).
- ArduinoJson - at [https://arduinojson.org/](https://arduinojson.org/).
- Preferences - this should be built-in.
- ESPmDNS - this should be built-in.
- esp_sntp - this should be built-in.

---

## WiFiTimeManager Basics

At startup, an attempt is made to connect to a previously saved WiFi network.  If unsuccessful, the system enters Access Point mode and creates a web server with a default IP adddress of 192.168.4.1.  Any WiFi enabled device (e.g. phone or computer) can connect this Access Point.  Upon connection to the Access Point, a page similar to the following will appear:

   ![WiFiTimeManagerHomePage][HomePage]:  

From this page, the user may choose from the following:
- **Setup**:  Displays the timezone/DST setup page.  This page allows the user to select the local timezone, use of Daylight Savings Time (DST), the start and end times for DST in the local region (if used), and the address of the best Network Time Protocol (NTP) server for the local area.
- **Configure WiFi**: Displays a list of local WiFi networks.  Allows the user to set and save a WiFi network credentials (network SSID and password).
- **Info**: Displays useful information about the ESP32, and the WiFi network.
- **Restart**: Resets (reboots) the system.
- **Exit**: Exits the web page.  This is generally not very useful.

In general it is best to setup the timezone and DST information via the 'Setup' page before configuring the WiFi via the 'Configure WiFi' page.  This is because once the WiFi configuration is saved, the Access Point mode will be exited and the 'Setup' page is no longer available.

Once the WiFi configuration is saved, the ESP32 will try to connect to the specified WiFi network.  If successful, it immediately returns control back to the application.  If not successful, the behavior depends on the selected mode of  operation:
- **Blocking Mode**: In this mode, the WiFiTimeManager will not return to the user application until a WiFi connection is successfully made.  In other words, it will block until connected.  Blocking mode is the default.
- **Non-Blocking Mode**: In this mode, the WiFiTimeManager will return to the user application after a user specified timeout, even if no WiFi connection has been made.  In this case, the user application must poll for connection to the WiFi via a WiFiTimeManager service call.

Options are provided to select blocking mode or non-blocking mode, as well as for setting the non-blocking mode timeout.

The appearance of the 'Setup' page depends on whether or not DST is selected.  If DST is not used, then a page similar to the one below will be displayed (note that the Use DST field is unchecked here).  From this page, setting of the timezone, timezone abbreviation, and NTP server address may be saved.

![WiFiTimeManager No DST Page][SetupNoDST]



If DST is to be used, a page similar to the one below will be displayed (note that the Use DST field is checked).  From this page, setting of the timezone, timezone abbreviation, DST abbreviation, DST start  and stop times, and NTP server address may be saved.

![WiFiTimeManager DST Page][SetupDST]

---

## WiFiTimeManager Basic Use
This section describes the minimum code required to use WiFiTimeManager.  It uses the default blocking mode.  See the Examples for code using the non-blocking mode and other advanced features.

- The sketch must include the WiFiTimeManager class header file.  Note that it is not necessary to include the WiFiManager.h file since WiFiTimeManager already includes it:  
```cpp
#include "WiFiTimeManager.h"    // Manages timezone, DST, and NTP.
```

- The WiFiTimeManager class is implemented as a singleton.  Its static **Instance()** method must be called at least once to create the singleton and fetch a pointer to it.  The **Instance()** method may be called in as many places as needed.
```cpp
// Point to the WiFiTimeManager singleton instance.
WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
```

- The WiFiTimeManager must be initialized via a call to **Init()**.  By default, **Init()** will set the WiFiManager configuration portal to use blocking mode, and will default to the dark theme.  **Init()** takes three arguments as described below.
  - The first argument points to a *NULL* terminated character string representing the name  for the network Access Point (AP) that will be created.
  - The second argument is optional, and points to a *NULL* terminated character string representing the password to be used in accessing the AP.  If not supplied, the AP will not require a password.  If used, the password should be 8 to 63 characters.
  - The third argument is optional, and specifies whether a separate 'Setup' button will appear on the WiFiTimeManager home page.  If set to *true*, then a separate 'Setup' button will be displayed.  If set to *false*, the timezone, DST, and NTP fields will reside below the WiFi credentials on the 'Configure WiFi' page.  This argument defaults to a value of *true*.
```cpp
// Initialize with our AP name and button selections.
pWtm->Init("AP_NAME", "AP_PASSWORD", true);
```

- Finally, the WiFiTimeManager must connect to the WiFi via the **autoConnect()** method.  The AP name and password that were supplied in the call to **Init()** are used.
```cpp
// Attempt to connect with our AP name, and password (supplied via Init()).
pWtm->autoConnect();
```

With the minimal code as specified above, when the ESP32 starts it will try to connect to the WiFi.  If it fails, it will start in Access Point (AP) mode.  While in AP mode, its web page may be accessed via an IP address of 192.146.4.1.  Selecting the 'Setup' button of the web page will allow the user to set timezone, DST, and NTP parameters for the local area.  Selecting the 'Configure WiFi' button allows for setting credentials for the local WiFi network.  The 'Setup' page should be updated and saved before the 'Configure WiFi' page since once the WiFi credentials are configured, AP mode is disabled.

---
## Advanced Use (Non-blocking mode)
WiFiTimeManager defaults to using the blocking mode.  In this mode, the call to **autoConnect()** never returns until a WiFi connection is established.  This may be fine for many applications.  However, it is often necessary to continue normal operation, even if a WiFi connection cannot be established.  This is where non-blocking mode comes in handy.  While using non-blocking mode, **autoConnect()** will return after a specified timeout even if not yet connected to WiFi.  In this case, the Access Point is still active providing web access to the 'Configure WiFi' and 'Setup' web pages.  A skeleton of code that is used to implement non-blocking mode is as follows:

```cpp
// Arduino setup().
void setup()
{
    . . .
    // Point to the WiFiTimeManager instance.
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
    . . .
    // Initialize the WiFiTimeManager.
    pWtm->Init(AP_NAME, AP_PWD, SETUP_BUTTON);
    . . .
    // Set to non-blocking mode with a 10 second timeout.
    pWtm->setConfigPortalBlocking(false);
    pWtm->setConfigPortalTimeout(10);
    . . .
    // Try to connect to WiFi.  Return after timeout if not connected.
    pWtm->autoConnect();
    . . .
}

// Arduino loop().
void loop()
{
    . . .
    // Point to the WiFiTimeManager instance.
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
    . . .
    pWtm->process();
    . . .
    delay(1);
}
 
```

Note that the major code differences between blocking mode and non-blocking mode are:
- **setConfigPortalBlocking(*false*)** must be called before **autoConnect()** to set non-blocking mode.
- **setConfigPortalTimeout()** may be called to set a timeout, in seconds.  It specifies how long to wait for a WiFi connection.
- **process()** must be called periodically, usually from the Arduino **loop()** function.  This allows for handling of the config portal.  See the NonBlocking, EditWebPage, or RTCExample for examples of using the non-blocking mode.

---

## Advanced Use (User Setup Parameters)
As shown above, WiFiTimeManager provides built-in parameters to specify the timezone, DST start and end, and NTP server address.  This will be sufficient for most applications.  However, WiFiTimeManager supplies an easy method for users to add additional parameters as needed.  The WebPages.h file contains the long *NULL* terminated string that is used to create the Setup page of the config portal.  This string contains several comments at strategic points that may be used to insert HTML or JavaScript code.  These points are:
 -  `"<!-- HTML START -->"`  
             This marks the start of HTML, which is also the start of the web page string.
 - `"<!-- HTML END -->"`  
             This marks the end of HTML and is just before the end of the web page body.
 - `"// JS START"`  
             This marks the start of the JavaScript, just after the `<script>` declaration.
- `"// JS ONLOAD"`  
             This marks a spot within the onload() JavaScript function where JavaScript initialization code can be added.
 - `"// JS SAVE"`  
             This marks a spot within the function that is executed when the submit (Save) button is pressed.  It can be used to perform JavaScript save actions.
 - `"// JS END"`  
             This marks the end of of the JavaScript, just before the `</script>` declaration.
 
 Two methods are used to support user parameters:
 - **SetUpdateWebPageCallback()** allows the user to set a callback that gets called when the Setup page is being updated.  The called back function may insert code at any or all of the strategic comments listed above to support the new user parameter.  See **SetUpdateWebPageCallback()** below and the EditWebPage example for more information.
 - **SetSaveParamsCallback()** allows the user to read the value(s) of any added parameter(s).  The called back function is invoked after the user clicks the Save button of the Setup page.  See **SetSaveParamsCallback()** below and the EditWebPage example for more information.
 
 ---

## API
WiFiTimeManager extends the Arduino WiFiManager class.  It adds the ability to easily set timezone, DST, and NTP server data via extending the WiFiManager web page.  It uses the ESP32 SNTP library to manage timezone and DST.  The ESP32 Preferences library is used to save and restore all setup data across power cycles.  Once connected to WiFi, WiFiTimeManager automatically connects to the specified NTP server and can supply accurate UTC and local time to the user.

The WiFiTimeManager code is well commented, and should be referred to along with the examples when more detailed information is needed.  This section describes the methods that are available to the user.


### Overridden Methods
WiFiManager methods all begin with lower case letters.  WiFiTimeManager methods all begin with capital letters.  Therefore, any overridden methods may be recognized by the first letter of their method being lower case.  All others are new to WiFiTimeManager.

Most WiFiManager methods are still available with WiFiTimeManager.  However, several WiFiManager methods have been overridden or disabled by WiFiTimeManager.  These methods include:

- WiFiManager::**addParameter()** - this method is used inside WiFiTimeManager::Init() and is hidden from user code.  Any attempt to invoke it will result in a compile error.  WiFiTimeManager uses a different method of adding parameters, and is described later.
- WiFiManager::**resetSettings()** - this method has been replaced by WiFiTimeManager::**ResetData()**.
- WiFiManager::**setMenu()** - this method is used inside WiFiTimeManager::**Init()** and is hidden from user code.  Any attempt to invoke it will result in a compile error.
- WiFiManager::**process()** - this method has been overridden by WiFiTimeManager to extend the WiFiManager behavior.  The WiFiTimeManager version is available for use by user code.
- WiFiManager::**autoConnect()** - this method has been overridden by WiFiTimeManager to extend the WiFiManager behavior.  The WiFiTimeManager version is available for use by user code.
- WiFiManager::**setSaveParamsCallback()** - this method has been replaced by **SetSaveParamsCallback()** The WiFiManager version is no longer available for use by user code.

### Special Callbacks
Most calls to WiFiManager methods should be done after the call to WiFiTimeManager::**Init()** with a few exceptions.  A few methods initialize callbacks that, if used, are invoked within **Init()**, so should be called before **Init()** is called.  They may also be called later if needed.  These callbacks are (see below for full descriptions):
- **SetUpdateWebPageCallback()**
- **SetSaveParamsCallback()**
- **SetUtcGetCallback()**
- **SetUtcSetCallback()**


### TimeChangeInfo Structure
The TimeChangeInfo structure holds DST start or stop information.  It includes:
- **abbrev[]** - This is a NULL terminated string, of 5 chars maximum, representing the DST abbreviation.  For example for Cleveland, Ohio, when DST is active, the abbreviation is "EDT".  When DST is not active the abbreviation is "EST".
- **week** - The week of the month in which the time changes.  This may take a value of 1 through 5, with 5 denoting the last week of the month.  The WeekOfMonth_t enum contains values that may be used for this member.
- **dow** - The day of week in which the time changes, with Sunday = 0, through Saturday = 6.  The DayOfWeek_t enum contains values that may be used for this member.
- **month** - The month in which the time changes, with January = 1, through December = 12.  The Month_t enum contains values that may be used for this member.
- **hour** - The hour in which the time changes.  Values of 0 through 23 are accepted.
- **offset** - The offset from UTC, in minutes, for the new time.


### TimeParameters Structure
The TimeParameters structure is defined at the top of WiFiTimeManager.h.  A copy of this structure is what gets saved/restored to/from non-volatile storage (NVS) to remember settings made by the user from the Setup page.  This data includes:
- **m_Version** - The version of the data being held within the structure.  It is used to keep insure that the data stored within NVS matches the current data.  This value should be bumped any time a change is made to the structure.
- **m_TzOfst** - The timezone offset, in minutes, from UTC as specified by the user.
- **m_UseDst** - If *true*, then DST is used, otherwise DST is not used.
- **m_DstOfst** - The offset, in minutes, from TzOfst that is in effect when DST is active. This is either 30 or 60 minutes.
- **m_NtpAddr** - A *NULL* terminated string representing the address of the NTP server, as set by the user.
- **m_DstStartRule** - The TimeChangeRule for DST start info as defined in the TimeChangeInfo structure at the top of WiFiTimeManager.h.
- **m_DstEndRule** - The TimeChangeRule for DST end info as defined in the TimeChangeInfo structure at the top of WiFiTimeManager.h.

### WiFiTimeManager::Instance()
The WiFiTimeManager class is implemented as a singleton.  Therefore there can only be one instance in the system.  The **Instance()** method returns a pointer to the singleton instance.  Since the class is created in the first call to **Instance()**, this method must be called at least once from user code before the class may be used.

### WiFiTimeManager::Init()
This method must be called after **Instance()**, and before **autoConnect()**.  As its name implies, it sets up (initializes) the WiFiTimeManager instance for use. Its arguments are:
- **pApName** - The name that will be seen on the network for the WiFiManager Access Point.  This name must be at least one character long.
- **pApPassword** - The password used to access the AP.  If omitted or *NULL*, then no password is used.
- **setupButton** - Set to *true* if a separate 'Setup' button should be displayed on the WiFiManager home page.  When selected, this button will cause a separate 'Setup' button to be displayed.  The separate Setup page will contain time parameters that the user may adjust.  In this case, the WiFi configuratioin page will not display the time parameters.  If set to *false*, then the time parameters will be selectable from the WiFi configuration page.

Within **Init()**, the saved **TimeParameters** get restored if present.  If not, then the default values for **TimeParameters** get used and saved.  The web page gets updated in order to initialize the time values, and the web page and parameters are passed to WiFiManager.  Other parameters are set to safe values.  The config portal is set to blocking mode, and the dark theme is selected by default.  These default values may be changed by user code later if desired.

### WiFiTimeManager::autoConnect()
This method overrides WiFiManager::**autoConnect()** in order to return the status of the WiFi connection, and to set the AP name and password that were passed to **Init()**.  It automatically connects to the saved WiFi network, or starts the config portal on failure.  In blocking mode, this method will not return until a WiFi connection is made.  In non-blocking mode, the method will return after a user-settable timeout even if no WiFi connection was made.  In this case, the **process()** method must be periodically called from user code until the connection is made.  The method returns *true* if a WiFi connection has been made, or *false* otherwise.

### WiFiTimeManager::process()
This method overrides WiFiManager::**process()**.  It handles the non-blocking mode of operation.  It takes no arguments.  It should be called periodically from user code, usually from within the Arduino **loop()** function, when the config portal is active (i.e. no WiFi connection has been made).  It returns immediately, and returns *true* if a network connection has been made, or *false* otherwise.

### WiFiTimeManager::UpdateTimezoneRules()
This method updates the timezone rules.  WiFiTimeManager provides methods to get and set each of the values for the start and end DST times as well as the capability of enabling or disabling DST.  **UpdateTimezoneRules()** should be called after any changes are made to the current timezone rules.  The user should not normally need to use this method since the timezone rules usually get initialized via the Setup web page.

### Callbacks
This section describes the new callbacks supported by WiFiTimeManager.  A number of callbacks have been added at strategic places in the code to allow easy customization of WiFiTimeManager.  WiFiManager already provided several useful callbacks, most of which may still be used by the application. 

If used, all of these new callbacks should be installed in the Arduino **setup()** function between the WiFiTimeManager **Instance()** and **Init()** calls.  For example, here is an outline of where to do it:

```cpp
void setup()
{
    . . .
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
    . . .
    pWtm->SetSaveParamsCallback(SaveParamsCallback);
    . . .
    pWtm->Init(AP_NAME, AP_PWD, SETUP_BUTTON);
    . . .
    pWtm->autoConnect();
}
```


#### WiFiTimeManager::SetSaveParamsCallback()
This callback replaces WiFiManager::**setSaveParamsCallback()**.  It saves a pointer to a user function that is called when saving the Setup web page after the standard parameters have been  handled by WiFiTimeManager.  It is normally used to fetch the values of parameter that were added to the Setup page by the user.  For example, if an integer parameter named '*'editTestNumber*" was added to the Setup page, a simple callback function that prints the integer to the serial port when the parameters are saved might be:

```cpp
void SaveParamsCallback()
{
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
    Serial.printf("Integer Value = %d\n", pWtm->GetParamInt("editTestNumber"));
} // End SaveParamsCallback().
```


##### WiFiTimeManager::SetUpdateWebPageCallback()
Sets a callback that will be invoked when when the Setup web page is being updated.  This callback can be used to monitor or modify the contents of the Setup web page.  It is normally used to add user parameters to the Setup web page.

The callback function takes two arguments.  The  first argument is a reference to a very long String that contains the entire contents of the Setup web page that will be sent to the web client.   It may be modified as needed (see the EditWebPage example).  The second argument specifies the maximum size that is permissible for the String.  Upon return from the callback, the contents of the String will be safely copied into a buffer that will be sent to the client.  If the size of the (modified) String exceeds  the value specified maximum buffer size, the buffer will be truncated, so be careful.

Several comments appear within the string that may be employed to locate specific places within the standard
web page.  The user can use these comments to insert relevant HTML or JavaScript code.  These comments are:
-  `"<!-- HTML START -->"`  
             This marks the start of HTML, which is also the start of the web page string.
 - `"<!-- HTML END -->"`  
             This marks the end of HTML and is just before the end of the web page body.
 - `"// JS START"`  
             This marks the start of the JavaScript, just after the `<script>` declaration.
- `"// JS ONLOAD"`  
             This marks a spot within the onload() function where JavaScript initialization code can be added.
 - `"// JS SAVE"`  
             This marks a spot within the function that is executed when the submit (save) button is pressed.  It can be used to perform JavaScript save actions.
 - `"// JS END"`  
             This marks the end of of the JavaScript, just before the `</script>` declaration.

As an example, if the user wants to add a simple integer parameter named "*editTestNumber*" to the standard web page, the following code outline could be used:

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
    pWtm->SetSaveParamsCallback(SaveParamsCallback);
    . . .
    pWtm->Init(AP_NAME, AP_PWD, SETUP_BUTTON);
    . . .
    pWtm->autoConnect();
}
``` 
In this case, the callback replaces the `"<!-- HTML END -->"` substring with HTML code to add an integer parameter.  **Take  special note that SetUpdateWebPageCallback() should be called before Init()  due to Init() performing a web page update within its body.**

#### WiFiTimeManager::SetUtcGetCallback()
Sets a callback that will be invoked when non-NTP time is needed.  This callback can be used to read time from a hardware real time clock, or other time source when NTP time is not present or desired.  In general, the NTP server should not be polled very frequently.  WiFiTimeManager defaults to accessing the NTP server no more than once every 60 minutes.  This can be overridden via a call to **SetMinNtpRateSec()**.  If the user attempts to get time more frequently than the minimum NTP rate, the time is normally read from the Arduino time library which keeps pretty good track of time.  The user can override this by setting this callback to code that returns its idea of the current time in Unix time units (time_t in seconds since January 1, 1970).  For example, if a hardware RTC is present, it can be read and its value returned by this callback.  See RTCExample for example code.

#### WiFiTimeManager::SetUtcSetCallback()
Sets a callback that will be invoked when NTP UTC time has been received.  This callback can be used to set the time of a hardware real time clock or other alternate time source.  When called, the callback receives the Unix (time_t  in seconds since January 1, 1970) time that was just received from the NTP server as an argument.  See RTCExample for example code.


### WiFiTimeManager::ResetData()
This method resets/clears any saved network connection credentials (network SSID and password) and all timezone and NTP data that may have been previously saved.  The next reboot will cause the WiFi Manager Access Point to execute and new set of network credentials, timezone, DST, and NTP data will be needed.

### WiFiTimeManager::Reset()
This method is similar to **ResetData()** with the exception that **Reset()** also deletes all the WiFiTimeManager data from NVS.  It removes all traces of the timezone, DST, and NTP data from NVS.  This method is not normally needed, but in cases where a different system will be loaded onto the ESP32, this method will cleanup so that no trace of WiFiTimeManager data will remain.  Note that WiFiManager WiFi credentials are cleared, but not removed.

### WiFiTimeManager::Save()
Saves the current state data including timezone, DST, and NTP data to NVS.  As an optimization, if the state data has not changed since the last save, the data will not be saved.  Returns *true* if successful and *false* otherwise.

### WiFiTimeManager::Restore()
Restores the timezone, DST, and NTP data from NVS.  The data read from NVS is validated before being used.  If the saved data is found to be invalid, then the data read from NVS is ignored, and the current timezone, DST and NTP data is unaffected.  Returns *true* on success, and *false* otherwise.

### WiFiTimeManager::GetUtcTimeT()
Returns the best known value for UTC time (time_t  in seconds since January 1, 1970).  If it has been a while since the NTP server has been contacted, then a new NTP request is sent, and its returned data is used to update the local clock.  If it is too soon since the server was contacted, then the user supplied UtcGetCallback (if any) is called to allow the user to update clock time (i.e. an RTC).  If no user supplied UtcGetCallback is present, then the Arduino time library version of current UTC time is returned.  This method has one optional argument that is used to force an update from user code if present.  The argument defaults to `false`, and should normally use the default.

### WiFiTimeManager::GetUtcTime()
Returns the best known broken-down value for UTC time. It uses GetUtcTimeT() to fetch the time, and converts it to broken down time which is placed in the tm structure that is passed as its argument.

### WiFiTimeManager::GetLocalTime()
Returns the best known value for local time.  Converts the best known UTC time to broken-down local time and returns its value.  See GetUtcTimeT().


### WiFiTimeManager::GetDateTimeString()
Formats and prints a Unix tm structure value.  Takes a pointer to a buffer that will hold the returned time string, the size of the buffer, and a pointer to the Unix time value (tm struct) to be displayed.  The buffer must be at least 64 characters long to receive the full string.  If the buffer is shorter than 64 characters, the time string will be truncated.  The optional timezone string will be appended to the end of the time string.  This method returns the length in bytes of the returned string.  For example, the following code:
```cpp
    . . .
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();
    tm localTime;
    gpWtm->GetLocalTime(&localTime);
    gpWtm->PrintDateTime(&localTime);
    . . .
``` 
may produce a string similar to the following:  
```
   Sunday, February 12 2023 03:40:54 PM EST Day of Year: 043
```

The format of the string will probably not suit most applications, however, the built-in strftime() function can be used to format the string as needed.  For example, see the [Linux man page](https://linux.die.net/man/3/strftime) for more detail.

### WiFiTimeManager::PrintDateTime()
This method simply calls **GetDateTimeString()** and prints it to the Serial port with a terminating line feed.

### WiFiTimeManager::GetLocalTimezoneString()
This method returns a pointer to the currently active timezone string.  For example, assume that the timezone is Eastern Time ("EST") and DST starts on the second Sunday of March at 2 AM ("EDT"), and ends on the first Sunday of November at 2 AM (EST).  Then on the 17th of January 2023, this method will return a pointer to "EST".  On the first of April 2023, this method will return a pointer to "EDT".

### WiFiTimeManager::GetParamString()
This method returns the last value read for a specified Setup parameter as a String.  Its only argument is the String name of the parameter whose value is to be returned. If successful, the String value of the specified parameter is returned.  An empty String is returned on failure.

### WiFiTimeManager::GetParamChars()
Returns the last value read for a specified Setup parameter as a *NULL* terminated character string.  It takes three arguments: 
- **name** - String containing the name of the argument whose value is to be returned.
- **pBuf** - Pointer to the buffer in which the parameter's value is to be returned.
- **size** - Size of the buffer pointed to by **pBuf**.  
It returns the value pass as pBuf.  The returned buffer will contain the value of the specified parameter if successful, or an empty buffer on failure.

### WiFiTimeManager::GetParamInt()
Returns the last value read for a specified Setup parameter as an integer value.  As an argument it takes the String specifying the parameter to be read.  It always the integer (int) value of the specified parameter, or 0 on failure.

### Miscellaneous Timezone/DST/NTP Getters and Setters
All of the TimeParameters data items may be individually read or set via inline methods in WiFiTimeManager.h.  Note that after setting any of these parameters to new values, a call should be made to **UpdateTimezoneRules()**.  These getters and setters include:
- **GetTzOfst()**, **SetTzOfst()** - Timezone offset in minutes.
- **GetTzAbbrev()**, **SetTzAbbrev()** - Timezone abbreviation (i.e. "EST").
- **GetUseDst()**, **SetUseDst()** - Use DST (*true* or *false*).
- **GetDstOfst()**, **SetDstOfst()** - DST offset in minutes.
- **GetDstAbbrev()**, **SetDstAbbrev()** - DST abbreviation (i.e. "EDT").
- **GetDstStartWk()**, **SetDstStartWk()** - DST start week number (*Last = 0, First, Second, Third, Fourth*).
- **GetDstStartDow()**, **SetDstStartDow()** - DST start day of week (*Sun = 1, Mon, Tue, Wed, Thu, Fri, Sat*).
- **GetDstStartMonth()**, **SetDstStartMonth()** - DST start month (1 - 12).
- **GetDstStartHour()**, **SetDstStartHour()** - DST start hour (0 - 23).
- **GetDstStartOfst()**, **SetDstStartOfst()** - DST start offset in minutes.
- **GetDstEndWk()**, **SetDstEndWk()** - DST end week number (*Last = 0, First, Second, Third, Fourth*).
- **GetDstEndDow()**, **SetDstEndDow()** - DST end day of week (*Sun = 1, Mon, Tue, Wed, Thu, Fri, Sat*).
- **GetDstEndMonth()**, **SetDstEndMonth()** - DST end month (1 - 12).
- **GetDstEndHour()**, **SetDstEndHour()** - DST end hour (0 - 23).
- **GetDstEndOfst()**, **SetDstEndOfst()** - DST end offset in minutes.
- **GetNtpAddr()**, **SetNtpAddr()** - NTP server address (i.e. "time.nist.gov").


### Miscellaneous Methods
#### WiFiTimeManager::GetMinNtpRateSec(), WiFiTimeManager::SetMinNtpRateSec()
These methods get and set the minimum NTP rate in seconds.  In general, NTP servers should not be polled very frequently.  The default value for WiFiTimeManager is no more than once every 60 minutes.  This value may be changed via **SetMinNtpRateSec()**.  However, the absolute minimum rate allowed is once every 15 seconds.  Values smaller than this will be limited by **SetMinNtpRateSec()** to 15 seconds.

#### WiFiTimeManager::IsConnected()
This method returns *true* if a WiFi connection is currently active, or *false* otherwise.

#### WiFiTimeManager::UsingNetworkTime()
This method returns *true* if NTP time was successfully received the last time it was attempted.  It returns *false* otherwise.

#### WiFiTimeManager::SetPrintLevel()
WiFiTimeManager code contains several status and debug print statements that are meant to help verify its operation.  These prints are divided into three classes:
- Warnings - These messages occur when an unexpected operation fails.  These failures include:
   - When a Save() to NVS fails.
   - When a Restore() from NVS fails.
   - When an NTP response is not received in response to an NTP request.
- Info - These are informational messages that indicate the progress of things, and can be generally useful.
- Debug - These are verbose messages that give information that is useful for debugging.

The **SetPrintLevel()** method may be used to enable/disable specific levels of messages as desired.  Several constants have been defined to specify the desired level as follows:
- WiFiTimeManager::*PL_NONE_MASK* - Use this to disable all prints.
- WiFiTimeManager::*PL_WARN_MASK*  - Use this to enable warning prints.
- WiFiTimeManager::*PL_INFO_MASK*  - Use this to enable info and warning prints.
- WiFiTimeManager::*PL_DEBUG_MASK* - Use this to enable all prints.

The default print level is *PL_WARN_MASK*.  In general, final code should use either *PL_NONE_MASK* or *PL_WARN_MASK*.

---

## General Notes
### Use of "Partition Scheme" in Arduino IDE
A sketch that uses the WiFiTimeManager library is fairly memory hungry.  For example, building the simplest sketch (the Blocking example) uses the following memory:

```
Sketch uses 901801 bytes (68%) of program storage space. Maximum is 1310720 bytes.
Global variables use 73420 bytes (22%) of dynamic memory, leaving 254260 bytes for
local variables. Maximum is 327680 bytes.
```
The Arduino IDE has provided a way to re-distribute flash memory in order to provide more program space at the expense of NVS data space.  Under the Arduino IDE "Tools" tab exists a selection for "Partition Scheme":
![Arduino IDE Partition Schemes](https://i.imgur.com/mWNS8eR.png)
The default Partition Scheme allocates 1.2MB for the application, and 1.5MB for NVS.  This allocation may be changed to set aside more space for the application if needed.  For a better explanation see [Partition Schemes in the Arduino IDE](https://robotzero.one/arduino-ide-partitions/).

### Possible Memory Reduction
The web page string contained in WebPages.h is formatted for readability, and contains many comments that may be removed.  A test was performed in which all of the unneeded comments were removed.  The end result was that only about 1KB of memory was saved.  It was decided that it was better to keep the comments.  This is a possible area to look at in dire situations, but should not normally be needed.





[HomePage]:https://imgur.com/7WKNXam.png "WifiTimeManager Home Page"
[SetupNoDst]:https://i.imgur.com/q3Koulm.png "Setup Page With No DST"
[SetupDst]:https://i.imgur.com/Gc2CaZZ.png "Setup Page With DST"
