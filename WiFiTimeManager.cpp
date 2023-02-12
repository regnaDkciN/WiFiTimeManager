/////////////////////////////////////////////////////////////////////////////////
// WiFiTimeManager.cpp
//
// This file implements the WiFiTimeManager class.  It derives from the
// WiFiManager class (https://github.com/tzapu/WiFiManager).  The WiFiManager
// class enables the user to easily configure the WiFi connection of a WiFi
// capable device via any web browser.  This class (WiFiTimeManager) extends the
// behavior of the WiFiManager class by adding the the following capaabilities:
//      - Ability to to select the timezone via the Setup web page.
//      - Ability to set daylight savings time (DST) start and end information
//        via the Setup web page.
//      - Ability to select and automatically connect to a local NTP server
//        to keep accurate time.
//      - Ability to (fairly) easily add additional fields to the Setup web page.
//      - Ability to easily add real time clock (RTC) hardware to allow for
//        accurate time keeping even when not connected to an NTP server.
//
// The latest version of WiFiTimeManager code with documentation and examples
// can be found on github at: https://github.com/regnaDkciN/WiFiTimeManager .
//
// History:
// - jmcorbett 12-FEB-2023
//   Major rework to replace use of Timezone_Generic library with ESP32 SNTP
//   library.  Several interface changes were made as a result:
//   o GetUtcTime() now takes a pointer to a tm structure in which the broken-down
//     UTC time gets stored.  Also returns the tm pointer.
//   o Added GetUtcTimeT() to return broken-down UTC time.  Takes a force
//     argument which GetUtcTime() used to use.
//   o GetDateTimeString() removed timezone argument and replaced time_t
//     argument with pointer to tm structure.
//   o GetLocalTime() added a pointer to tm structure argument, and return it.
//   o PrintDateTime() removed timezone argument and replaced time_t
//     argument with pointer to tm structure.
//   o Renamed SetMinNtpRate() to SetMinNtpRateSec().
//   o Renamed GetMinNtpRate() to GetMinNtpRateSec().
//   o Removed NTP port selection since port 123 is universally used.
//   o Replaced setSaveParamsCallback() with SetSaveParamsCallback().  The
//     WiFiManager version is no longer available to the user.
//
// - jmcorbett 19-JAN-2023 Original creation.
//
// Copyright (c) 2023, Joseph M. Corbett
//
/////////////////////////////////////////////////////////////////////////////////

#include <Preferences.h>        // For NVS data storage.
#include <ArduinoJson.h>        // For JSON handling.
#include <ESPmDNS.h>            // For Mdns support.
#include "WiFiTimeManager.h"    // For WiFiTimeManager class.

// Some constants used by the WiFiTimeManager class.
const size_t  WiFiTimeManager::MAX_NVS_NAME_LEN     = 15U;
const char   *WiFiTimeManager::pPrefSavedStateLabel = "All Time Data";
char          WiFiTimeManager::WebPageBuffer[WiFiTimeManager::MAX_WEB_PAGE_SIZE];
const char   *WiFiTimeManager::m_pName = "TIME DATA";
volatile bool WiFiTimeManager::m_UsingNetworkTime = false;

// Create our WiFiManagerParameter custom_field.  This custom field contains
// all of our data fields embedded within.
static WiFiManagerParameter tzSelectField;


/////////////////////////////////////////////////////////////////////////////////
// Constructor
//
// Initialize our instance data.
//
/////////////////////////////////////////////////////////////////////////////////
WiFiTimeManager::WiFiTimeManager() : WiFiManager(), m_pApName(NULL),
                                     m_pApPassword(NULL), m_PrintLevel(PL_DFLT_MASK),
                                     m_Params(),
                                     m_MinNtpRateMs(60 * 60 * 1000),
                                     m_LastUpdateMs(0),
                                     m_pSaveParamsCallback(NULL),
                                     m_pUtcGetCallback(NULL),
                                     m_pUtcSetCallback(NULL),
                                     m_pUpdateWebPageCallback(NULL)
{
} // End constructor.



/////////////////////////////////////////////////////////////////////////////
// Instance()
//
// Return our singleton instance of the class.  The constructor is private so
// the only way to create an object is via this method.
//
// Returns:
//   Returns a pointer to the only instance of this class.
//
/////////////////////////////////////////////////////////////////////////////
WiFiTimeManager *WiFiTimeManager::Instance()
{
    // Since this method is static, the instance is created only on the first
    // call to Instance().  A pointer to it is always returned.
    static WiFiTimeManager instance;
    return &instance;
} // End Instance().


/////////////////////////////////////////////////////////////////////////////
// Init()
//
// This method initializes the WiFiTimeManager class.
//
// Arguments:
//    - pApName     - This is the NULL terminated character string representing
//                    the network name to be used for the access point.
//    - pApPassword - Pointer to to a NULL terminated string representing the
//                    password to be used by the access point.  This argument
//                    is optional and if not used, the access point will not
//                    require a password.  If a password is used, it should be
//                    8 to 63 characters.
//    - setupButton - Set to true if a separate "Setup" button should be
//                    displayed on the WiFiManager home page.  When selected,
//                    this button will cause a separate setup button to be
//                    displayed.  The separate setup page will contain time
//                    related parameters that the user may adjust.  In this
//                    case, the WiFi configuration page will not display
//                    the time parameters.  If set to false, then the time
//                    parameters will be selectable from the WiFi
//                    configuration page.
//
// Returns:
//    Returns true on successful completion.  Returns false if pApName is empty.
//
/////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Init(const char *pApName, const char *pApPassword,
                           bool setupButton)
{
    // We're not using network time yet.
    m_UsingNetworkTime = false;

    // Set the WiFi mode.  ESP defaults to STA+AP.
    WiFi.mode(WIFI_STA);

    // Initialize the ESP32 SNTP system.
    sntp_init();

    // Validate our AP name.
    if ((pApName == NULL) || (*pApName == '\0'))
    {
        return false;
    }

    // Save AP connection data.
    m_pApName     = pApName;
    m_pApPassword = pApPassword;

    // Restore our saved state info.  If no state info has been saved yet, then
    // save our default values.
    if (!Restore())
    {
        WTMPrint(PL_WARN_BP, "Restore failed.\n");
        if (!Save())
        {
            WTMPrint(PL_WARN_BP, "Save failed.\n");
            return false;
        }
    }

    // Set our network name for DNS use.
    MDNS.begin(pApName);

    // Create our web page using previously saved values.
    UpdateWebPage();

    // Use a placement new to install our web page into the WiFiManager.
    new (&tzSelectField) WiFiManagerParameter(GetWebPage());

    //  Let the WiFiManager know about our web page.
    addParameter(&tzSelectField);

    // Install our "save parameter" handler.  This handler fetches any (possibly
    // changed) values of our timezone and NTP parameters after the user saves
    // the Setup page.
    WiFiManager::setSaveParamsCallback(SaveParamCallback);

    // Setup our custom menu.  Note: if 'setupButton' is true we want our setup page
    // button to appear before the WiFi config button on the web page.
    const char *menu[] = {"param", "wifi", "info", "sep", "restart", "exit"};
    if (setupButton)
    {
        setMenu(menu, sizeof(menu) / sizeof(menu[0]));
    }
    else
    {
        setMenu(&menu[1], (sizeof(menu) / sizeof(menu[0])) - 1);
    }

    // Enable captive portal redirection.
    setCaptivePortalEnable(true);
    // Always disconnect before connecting.
    setCleanConnect(true);
    // Show erase WiFi config button on info page.
    setShowInfoErase(true);

    // Block until done.  This is the default behavior.  May be changed by the
    // user later.
    setConfigPortalBlocking(true);
    // Set to dark theme.  This is the default behavior.  May be changed by the
    // user later.
    setClass("invert");

    // Set the timezone string per current values.
    const uint32_t MAX_TZ_STR = 64;
    char tzBuf[MAX_TZ_STR];
    GetTimezoneString(tzBuf, sizeof(tzBuf));
    setenv("TZ", tzBuf, 1);
    tzset();

    // Initialize the clock to the start of 2023 as default.  Will (hopefully)
    // be updated to the correct time later.
    const time_t UTC_2023_START = 1672531200;
    timeval tv = { .tv_sec = UTC_2023_START };
    settimeofday(&tv, NULL);

    // If the user has specified a UTC get callback, then get the current time
    // and force it to use the time from the user callback code.  This will
    // (hopefully) let us start with a fairly accurate date/time.
    if (m_pUtcGetCallback)
    {
        GetUtcTimeT(true);

        // We're not using network time yet so make sure that our status is
        // correct (will have been set to true falsely on first call to
        // GetUtcTimeT()).
        m_UsingNetworkTime = false;
    }

    // Return success if we got this far.
    return true;
} // End Init().


/////////////////////////////////////////////////////////////////////////////
// process()
//
// Overrides the WiFiManager process() method.
// This method handles the non-blocking WiFi manager completion.
//
// Returns:
//    Returns a bool indicating whether or not the network is now connected.
//    A 'true' value indicates connected, while a 'false' value
//    indicates not connected.
//
// NOTE: The time will be updated immediately after a new connection is
//       established.
//
/////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::process()
{
    // If we're not yet connected, then call the WiFi manager to see if a
    // connection was recently made.
    if (!IsConnected())
    {
        if (WiFiManager::process())
        {
            // We must have just connected.  Delay briefly to let the network
            // settle, then attempt to get the time from the network.
            StartNewConnection();

            // Make sure the WiFi manager stops.
            stopWebPortal();
        }
    }

    // Return an indication of the network connection status.
    return IsConnected();
} // End process().


/////////////////////////////////////////////////////////////////////////////
// ResetData()
//
// This method resets/clears any saved network connection credentials
// (network SSID and password) and all timezone and NTP data that may have
// been previously saved.  The next reboot will cause the WiFi Manager access
// point to execute and new set of network credentials will be needed.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::ResetData()
{
    // Clear the WiFiManager network data.
    resetSettings();

    // Clear our timezone/DST state data.
    Preferences prefs;
    prefs.begin(m_pName);
    prefs.clear();
    prefs.end();
} // End ResetData().


/////////////////////////////////////////////////////////////////////////////////
// Save()
//
// Saves our current state to NVS.  This includes all timezone and NTP data.
//
// Returns:
//    Returns 'true' if successful, or 'false' otherwise.
//
/////////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Save() const
{
    // Assume that the save will fail.
    bool saved = false;
    WTMPrint(PL_INFO_BP, "Saving Data.\n");

    // Read our currently saved state.  If it hasn't changed, then don't
    // bother to do the save in order to conserve writes to NVS.
    TimeParameters nvsState;
    Preferences prefs;
    prefs.begin(m_pName);

    // Get the previously saved data.
    size_t nvsSize =
        prefs.getBytes(pPrefSavedStateLabel, &nvsState, sizeof(TimeParameters));

    // See if our working data has changed since our last save.
    if ((nvsSize != sizeof(TimeParameters)) ||
        memcmp(&nvsState, &m_Params, sizeof(TimeParameters)))
    {
        // Data has changed so go ahead and save it.
        WTMPrint(PL_INFO_BP, "\nTimeSettings - saving to NVS.\n");
        saved =
          (prefs.putBytes(pPrefSavedStateLabel, &m_Params, sizeof(TimeParameters)) ==
              sizeof(TimeParameters));
    }
    else
    {
        // Data has not changed.  Do nothing.
        saved = true;
        WTMPrint(PL_INFO_BP, "\nTimeSettings - not saving to NVS.\n");
    }
    prefs.end();

    // Let the caller know if we succeeded or failed.
    return saved;
 } // End Save().


/////////////////////////////////////////////////////////////////////////////////
// Restore()
//
// Restores our state from NVS.  This includes all timezone and NTP data.
//
// Returns:
//    Returns 'true' if successful, or 'false' otherwise.
//
/////////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Restore()
{
    // Assume we're gonna fail.
    bool succeeded = false;
    WTMPrint(PL_INFO_BP, "Restoring Saved Data.\n");

    // Restore our state data to a temporary structure.
    TimeParameters cachedState;
    Preferences prefs;
    prefs.begin(m_pName);
    size_t restored =
        prefs.getBytes(pPrefSavedStateLabel, &cachedState, sizeof(TimeParameters));

    // Save the restored values only if the get was successful.
    if ((restored == sizeof(TimeParameters)) &&
        (cachedState.m_Version == TP_VERSION))
    {
        memcpy(&m_Params, &cachedState, sizeof(TimeParameters));
        succeeded = true;
    }
    prefs.end();

    // Let the caller know if we succeeded or failed.
    return succeeded;
 } // End Restore().


/////////////////////////////////////////////////////////////////////////////
// Reset()
//
// Reset our state info in NVS.  This includes all timezone and NTP data.
// WiFi credential information is not affected.
//
// Returns:
//    Returns 'true' if successful, or 'false' otherwise.
//
/////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Reset()
{
    bool status = false;

    // Erase all the stored information in the WiFiManager.
    ResetData();

    // Remove our state data from NVS.
    Preferences prefs;
    prefs.begin(m_pName);
    status = prefs.remove(pPrefSavedStateLabel);
    prefs.end();

    // Let the user know if we succeeded or failed.
    return status;
} // End Reset().


/////////////////////////////////////////////////////////////////////////////
// GetTimezoneString()
//
// Returns a string that represents the user selected timezone information
// in a form that is compliant with the POSIX TZ environment variable.
// See https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html .
//
// Arguments:
//   pBuffer - A pointer to the buffer in which the TZ string will be returned.
//   bufSize - The size, in chars, of the buffer pointed to by pBuffer.
//             Should be at least 64 chars.
//
// Returns:
//   Returns the TZ string in pBuffer, and a pointer to pBuffer.
//
/////////////////////////////////////////////////////////////////////////////
char *WiFiTimeManager::GetTimezoneString(char *pBuffer, size_t bufSize)
{
    char *pBuf = pBuffer;
    size_t size = bufSize;

    // Start by forming the standard timezone information (e.g. "EST+5:0").
    // Abbreviation - OffsetHour : OffsetMinute
    size_t charsSoFar = snprintf(pBuf, size, "%s%+01d:%01d", GetTzAbbrev(),
        0 - GetTzOfst() / 60, abs(GetTzOfst()) % 60);

    // If DST is observed, then we need more data...
    if (GetUseDst())
    {
        // Form the timzone DST abbreviation and its offset (e.g. "EDT+4:0").
        // Abbreviation - OffsetHour : OffsetMinute
        size -= charsSoFar;
        int32_t dstOfst = GetTzOfst() + GetDstOfst();
        charsSoFar += snprintf(pBuf + charsSoFar, size, "%s%+01d:%01d",
            GetDstAbbrev(), 0 - dstOfst / 60, abs(dstOfst) % 60);
        size = bufSize - charsSoFar;

        // Form the DST start data (e.g. "M3.2.0/2").
        // M - Month . WeekNumber . DayOfWddk / Hour
        // Month = [1 - 12]
        // WeekNumber = [1 - 5] (5 == last week of month)
        // DayOfWeek = [0 - 6] (0 == Sunday)
        // Hour = [0 - 23]
        charsSoFar += snprintf(pBuf + charsSoFar, size, ",M%d.%d.%d/%d",
            GetDstStartMonth(), GetDstStartWk(), GetDstStartDow(), GetDstStartHour());
        size = bufSize - charsSoFar;

        // Form the DST end data (e.g. "M11.1.0/2").
        snprintf(pBuf + charsSoFar, size, ",M%d.%d.%d/%d",
            GetDstEndMonth(), GetDstEndWk(), GetDstEndDow(), GetDstEndHour());
    }

    return pBuffer;
} // End GetTimezoneString().


/////////////////////////////////////////////////////////////////////////////
// UpdateTimezoneRules()
//
// Update our DST timezone rules.  Should be called any time timezone or DST
// data changes.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::UpdateTimezoneRules()
{
    const uint32_t MAX_TZ_STR_LEN = 64;
    char tzBuf[MAX_TZ_STR_LEN];

    // Get the timezone string.
    GetTimezoneString(tzBuf, sizeof(tzBuf));

    // Actually update the system timezone.
    configTime(0, 0, GetNtpAddr());
    setenv("TZ", tzBuf, 1);
    tzset();

    // Display debug info if debug display is enabled.
    WTMPrint(PL_DEBUG_BP, tzBuf);
} // End UpdateTimezoneRules().



/////////////////////////////////////////////////////////////////////////////
// UpdateWebPage()
//
// Update our custom Setup page based on current timezone, DST, and NTP settings.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::UpdateWebPage()
{
    // Start with our basic web page.
    String WebPageString(TZ_SELECT_STR);

    // Create our JSON document containing all of our user settable parameters.
    DynamicJsonDocument doc(MAX_JSON_SIZE);
    doc["TIMEZONE"] = GetTzOfst();
    doc["USE_DST"] = GetUseDst();
    doc["DST_START_WEEK"] = GetDstStartWk();
    doc["DST_START_DOW"] = GetDstStartDow();
    doc["DST_START_MONTH"] = GetDstStartMonth();
    doc["DST_START_HOUR"] = GetDstStartHour();
    doc["DST_START_OFFSET"] = GetDstOfst();
    doc["DST_END_WEEK"] = GetDstEndWk();
    doc["DST_END_DOW"] = GetDstEndDow();
    doc["DST_END_MONTH"] = GetDstEndMonth();
    doc["DST_END_HOUR"] = GetDstEndHour();
    doc["TZ_ABBREVIATION"] = GetTzAbbrev();
    doc["DST_ABBREVIATION"] = GetDstAbbrev();
    doc["NTP_ADDRESS"] = GetNtpAddr();

    // Convert the JSON document to a String.
    String JsonStr;
    serializeJson(doc, JsonStr);
    WTMPrint(PL_DEBUG_BP, JsonStr);

    // Update our web page with our user settable parameters in JSON form.
    WebPageString.replace("*PUT_TZ_JSON_DATA_HERE*", JsonStr);

    // If selected, call back to let user code add HTML and/or java script.
    if (m_pUpdateWebPageCallback != NULL)
    {
        m_pUpdateWebPageCallback(WebPageString, MAX_WEB_PAGE_SIZE - 1);
    }

    // Save our (possibly modified) web page for later use.
    strncpy(WebPageBuffer, WebPageString.c_str(), MAX_WEB_PAGE_SIZE - 1);
} // End UpdateWebPage().


/////////////////////////////////////////////////////////////////////////////
// SaveParamCallback()
//
// Static method that is called when the WiFiManager user saves the time
// configuration.  Updates all time parameters, then updates the timezone
// rules based on the (possibly) new timezone values.  Finally, it updates
// the web page so that its values will be up to date the next time it is
// displayed.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::SaveParamCallback()
{
    // Since this is a static method, we need to point to the singleton instance.
    WiFiTimeManager *pWtm = Instance();

    pWtm->WTMPrint(PL_INFO_BP, "SaveParamCallback\n");
    // Stuff the (possibly) new values into our local data.
    char buf[TimeParameters::MAX_NTP_ADDR];
    int tzOfst = pWtm->GetParamInt("timezoneOffset");
    int dstOfst = pWtm->GetParamInt("dstOffset");
    pWtm->SetTzOfst(tzOfst);
    pWtm->SetTzAbbrev(pWtm->GetParamChars("dstEndString", buf, sizeof(buf)));
    pWtm->SetUseDst(!strcmp(pWtm->GetParamChars("useDstField", buf, sizeof(buf)), "true"));
    pWtm->SetDstOfst(dstOfst);
    pWtm->SetDstAbbrev(pWtm->GetParamChars("dstStartString", buf, sizeof(buf)));
    pWtm->SetDstStartWk(pWtm->GetParamInt("weekNumber1"));
    pWtm->SetDstStartDow(pWtm->GetParamInt("dayOfWeek1"));
    pWtm->SetDstStartMonth(pWtm->GetParamInt("month1"));
    pWtm->SetDstStartHour(pWtm->GetParamInt("hour1"));
    pWtm->SetDstStartOfst(tzOfst + dstOfst);
    pWtm->SetDstEndWk(pWtm->GetParamInt("weekNumber2"));
    pWtm->SetDstEndDow(pWtm->GetParamInt("dayOfWeek2"));
    pWtm->SetDstEndMonth(pWtm->GetParamInt("month2"));
    pWtm->SetDstEndHour(pWtm->GetParamInt("hour2"));
    pWtm->SetDstEndOfst(tzOfst);
    pWtm->SetNtpAddr(pWtm->GetParamChars("ntpServerAddr", buf, sizeof(buf)));

    // Save the (possibly) new values in NVS for later use.
    pWtm->Save();

    // Update the timezone rules and web page values.
    pWtm->UpdateTimezoneRules();
    pWtm->UpdateWebPage();

    // Call back the user's save parameter handler if any was specified.
    if (pWtm->m_pSaveParamsCallback != NULL)
    {
        pWtm->m_pSaveParamsCallback();
    }
} // End SaveParamCallback(),


/////////////////////////////////////////////////////////////////////////////
// GetParamString()
//
// Returns the last value read of the specified Setup parameter.
//
// Arguments:
//   name - String containing the name of the argument whose value is to be
//          returned.
//
// Returns:
//    Always returns the String value of the specified argument if successful,
//    or an empty String on failure.
//
/////////////////////////////////////////////////////////////////////////////
String WiFiTimeManager::GetParamString(String name)
{
    // Read the parameter that was returned from server.
    String value;
    if(server->hasArg(name))
    {
        value = server->arg(name);
    }
    return value;
} // End GetParamString().


/////////////////////////////////////////////////////////////////////////////
// GetParamChars()
//
// Returns the last value read of the specified Setup parameter.
//
// Arguments:
//   name - String containing the name of the argument whose value is to be
//          returned.
//   pBuf - Pointer to the buffer in which the parameter's value is to be
//          returned.
//   size - Size of the buffer pointed to by pBuf.
//
// Returns:
//    Returns the value passed as pBuf.  The returned buffer will contain the
//    value of the specified parameter if successful, or an empty buffer on failure.
//
/////////////////////////////////////////////////////////////////////////////
char *WiFiTimeManager::GetParamChars(String name, char *pBuf, size_t size)
{
    GetParamString(name).toCharArray(pBuf, size);
    return pBuf;
} // End GetParamChars().


/////////////////////////////////////////////////////////////////////////////
// GetParamInt()
//
// Returns the last value read of the specified timezone parameter as an
// integer.
//
// Arguments:
//   name - String containing the name of the argument whose value is to be
//          returned.
//
// Returns:
//    Returns the int value of the specified Setup parameter.
//
/////////////////////////////////////////////////////////////////////////////
int WiFiTimeManager::GetParamInt(String name)
{
    return GetParamString(name).toInt();
} // End GetParamInt().


/////////////////////////////////////////////////////////////////////////////
// GetUtcTimeT()
//
// This method returns UTC time in UNIX time (time_t).  If a user
// UTC get callback has been specified, and it has been a while since an
// NTP or user update has occurred, then the user code (if any) is called
// to update the clock time.  Otherwise, the ESP32 time is returned.
//
// Arguments:
//   force - Forces update from user callback if present.  This argument
//           is ptional, and defaults to false.
//
// Returns:
//   Always returns the current UTC time as kept by the internal Unix time
//   library or the user specified device if present.
//
/////////////////////////////////////////////////////////////////////////////
time_t WiFiTimeManager::GetUtcTimeT(bool force)
{
    time_t timeNow = 0;
    uint32_t millisNow = millis();
    bool updateTimedOut = (millisNow - m_LastUpdateMs) >= 4 * m_MinNtpRateMs;

    // If a user callback exists and we're forced, or it has been a long time
    // since an NTP update, get the current time from the user device.
    if (m_pUtcGetCallback && (force || updateTimedOut))
    {
        // Get the time from user code.
        timeNow = m_pUtcGetCallback();
        // Set the ESP32 time based on returned callback value.
        timeval tv = { .tv_sec = timeNow };
        settimeofday(&tv, NULL);

        // Reset our last update time.
        m_LastUpdateMs = millisNow;
    }
    else
    {
        timeNow = time(&timeNow);
    }

    // Remember if we are not getting NTP time.
    if (updateTimedOut)
    {
        m_UsingNetworkTime = false;
    }

    return timeNow;
} // End GetUtcTimeT().


/////////////////////////////////////////////////////////////////////////////
// GetUtcTime()
//
// This method returns UTC broken-down time.  It uses GetUtcTimeT() to
// fetch the UTC time.
//
// Arguments:
//   pTm - A pointer to the tm structure into which the UTC time will be
//         saved.
//
// Returns:
//   Always returns the value passed in as pTm.
//
/////////////////////////////////////////////////////////////////////////////
tm *WiFiTimeManager::GetUtcTime(tm *pTm)
{
    time_t utc = GetUtcTimeT();
    return gmtime_r(&utc, pTm);
} // End GetUtcTime().


/////////////////////////////////////////////////////////////////////////////
// GetLocalTime()
//
// Returns the best known value for local time.  Converts the best known
// UTC time to the broken-down local time and returns its pointer.
// See GetUtcTimeT().
//
// Arguments:
//   pTm - A pointer to the tm structure into which the local time data will
//         be returned.
//
// Returns:
//   Returns the best known value for the current local time in the
//   structure pointed to by pTm.  Also returns pTm.
//
/////////////////////////////////////////////////////////////////////////////
tm *WiFiTimeManager::GetLocalTime(tm *pTm)
{
    time_t utc = GetUtcTimeT();
    return localtime_r(&utc, pTm);
} // End GetLocalTime();


/////////////////////////////////////////////////////////////////////////////
// GetDateTimeString
//
// Format and print a unix time_t value.  Uses strftime() to do the conversion.
// See https://linux.die.net/man/3/strftime for information on formatting the
// date time string using strftime().
//
// Arguments:
//   pBuf  - Pointer to a buffer that will receive the time string.  Must be at
//           least 64 characters long to receive the full string.  If the buffer
//           is shorter than 64 characters, the time string will be truncated.
//   size  - The size, in bytes, of the buffer pointed to by pBuf.
//   pTime - Pointer to the tm time structure containing the time to be
//           converted.
//
// Returns:
//   Always returns the number of characters in the time string minus the
//   terminating NULL.
//
/////////////////////////////////////////////////////////////////////////////
int WiFiTimeManager::GetDateTimeString(char *pBuf, int size, tm *pTime)
{
    return strftime(pBuf, size, "%A, %B %d %Y %r %Z Day of Year: %j", pTime);
} // End GetDateTimeString().


/////////////////////////////////////////////////////////////////////////////
// PrintDateTime
//
// Format and print a UNIX broken-down tm time value.
//
// Arguments:
//   pTime - Pointer to the tm time structure containing the time to be
//           displayed.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::PrintDateTime(tm *pTime)
{
    const int MAX_TIME_STRING = 64;
    char buf[MAX_TIME_STRING];

    // Get the time string and print it.
    GetDateTimeString(buf, MAX_TIME_STRING, pTime);
    Serial.println(buf);
} // End PrintDateTime().


/////////////////////////////////////////////////////////////////////////////
// GetLocalTimezoneString()
//
// Returns a pointer to the currently active timezone string, or NULL on error.
// (e.g. "EST").
//
/////////////////////////////////////////////////////////////////////////////
const char *WiFiTimeManager::GetLocalTimezoneString()
{
    tm localTime;
    GetLocalTime(&localTime);
    const char *pStr = NULL;
    if (localTime.tm_isdst)
    {
        pStr = GetDstAbbrev();
    }
    else
    {
        pStr = GetTzAbbrev();
    }
    return pStr;
} // End GetLocalTimezoneString().


/////////////////////////////////////////////////////////////////////////////
// WTMPrint()
//
// Print the specified data if the selected level matches the m_PrintLevel
// that was set via SetPrintLevel().  Note that this method is overloaded
// to work with NULL terminated strings, and String objects.
//
// Arguments:
//   level - The bit pattern of the types of data that may be printed.
//   fmt   - The printf() style format string.
//   ...   - Arguments to the fmt string.
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::WTMPrint(uint32_t level, const char *fmt...) const
{
    if (m_PrintLevel & level)
    {
        // Set up our variable argument list.
        va_list args;
        va_start(args, fmt);

        // Print the message, prepended with our ID.
        Serial.print("[WTM] ");
        Serial.printf(fmt, args);
    }
} // End WTMPrint().


/////////////////////////////////////////////////////////////////////////////
// WTMPrint()
//
// Print the specified data if the selected level matches the m_PrintLevel
// that was set via SetPrintLevel().  Note that this method is overloaded
// to work with NULL terminated strings, and String objects.
//
// Arguments:
//   level - The bit pattern of the types of data that may be printed.
//   str   - String instance to print.
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::WTMPrint(uint32_t level, String &str) const
{
    if (m_PrintLevel & level)
    {
        // Print the message, prepended with our ID.
        Serial.print("[WTM] ");
        Serial.print(str);
    }
} // End WTMPrint().


/////////////////////////////////////////////////////////////////////////////
// UtcSetCallback()
//
// This static method sets UTC time as kept by the Unix time library.
// It is called when UTC time updated from NTP network time.  If the user
// has specified a callback, then it is called.
//
// Arguments:
//   pTvt -  Pointer to the timeval struct containing the UTC time in seconds
//           since January 1, 1970 in its tv_sec member.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::UtcSetCallback(timeval *pTv)
{
    // Since we're a static method, point to our singleton instance.
    WiFiTimeManager *pWtm = WiFiTimeManager::Instance();

    // Remember that we've successfully received NTP time.
    pWtm->m_UsingNetworkTime = true;

    // If a callback was specified, then call it to update user hardware.
    if (pWtm->m_pUtcSetCallback)
    {
        pWtm->m_pUtcSetCallback(pTv->tv_sec);
    }

    // Remember when we got updated for later use.
    pWtm->m_LastUpdateMs = millis();
} // End UtcSetCallback().


/////////////////////////////////////////////////////////////////////////////
// SetMinNtpRateSec()
//
// Sets the minimum time, in seconds, between NTP updates.  Enforces a minimum
// value of MIN_NTP_UPDATE_MS.
//
// Arguments:
//    rate - The minimum time, in seconds, between NTP updates.  Will be
//           limited to a value no less than MIN_NTP_UPDATE_MS.
//
// Returns:
//   Returns an indication of true if successful, or false on failure.
//
/////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::SetMinNtpRateSec(uint32_t rate)
{
    // Limit the frequency of NTP updates.
    m_MinNtpRateMs = max(rate * 1000, MIN_NTP_UPDATE_MS);

    // Set the new NTP rate.
    sntp_set_sync_interval(m_MinNtpRateMs);

    // Return an indication of success of failure.
    return sntp_restart();
} // End SetMinNtpRateSec().


/////////////////////////////////////////////////////////////////////////////
// autoConnect()
//
// Overrides the WiFiManager autoConnect() method in order to return the
// status of the WiFi connection.  Automatically connects to the saved WiFi
// network, or starts the config portal on failure.
//
// This method is overloaded.  The version with no arguments uses the
// AP name and password values that were passed to Init().  The other version
// uses the specified access point name and password, but should not normally
// be used.
//
// Arguments:
//   pApName     - Pointer to a NULL terminated string representing the name
//                 to be used for the access point.  This argument is optional.
//                 If not present, then the value passed to Init() is used.
//   pApPassword - Pointer to to a NULL terminated string representing the
//                 password to be used by the access point.  This argument
//                 is optional.  If not present, then the value passed to
//                 Init() is used.  If a password is used, it should be
//                 8 to 63 characters.
//
// Returns:
//    Returns 'true' if the network was successfully connected.  Returns
//    'false' otherwise.
//
/////////////////////////////////////////////////////////////////////////////
boolean WiFiTimeManager::autoConnect()
{
    WiFiManager::autoConnect(m_pApName, m_pApPassword);
    bool connected = IsConnected();
    if (connected)
    {
        // Init SNTP and current time.
        StartNewConnection();
    }
    return connected;
} // End autoConnect().

boolean WiFiTimeManager::autoConnect(char const *pApName, char const *pApPassword)
{
    WiFiManager::autoConnect(pApName, pApPassword);
    bool connected = IsConnected();
    if (connected)
    {
        // Init SNTP and current time.
        StartNewConnection();
    }
    return connected;
} // End autoConnect().


/////////////////////////////////////////////////////////////////////////////
// InitSntpTime()
//
// Initializes the SNTP code with NTP address, update rate, timezone info,
// and force an NTP sync.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::InitSntpTime()
{
    // Init our NTP address.
    configTime(0, 0, GetNtpAddr());

    tm timeInfo;
    getLocalTime(&timeInfo);

    // Setup our callback for NTP updates.  We call any user specified handler
    // from within our callback.
    sntp_set_time_sync_notification_cb(UtcSetCallback);

    // Init our update rate.
    sntp_set_sync_interval(m_MinNtpRateMs);
    sntp_restart();

    // Init our timezone information.
    const uint32_t MAX_TZ_STR_LEN = 64;
    char tzBuf[MAX_TZ_STR_LEN];
    GetTimezoneString(tzBuf, sizeof(tzBuf));
    setenv("TZ", tzBuf, 1);
    tzset();

    // Force an NTP sync only if we are currently conntcted.
    if (IsConnected())
    {
        // This is necessary since without it, the nextNTP  sync is often far into
        // the future.  We do this by getting the current time, adding half a
        // second to account for latency, then pass the time to sntp_sync_time()
        // which will force an NTP sync message to be sent.
        const suseconds_t HALF_SECOND_IN_MICROS = 500000;
        time_t timeNow = time(&timeNow);
        timeval tv = { .tv_sec = timeNow, .tv_usec = HALF_SECOND_IN_MICROS };
        sntp_sync_time(&tv);
    }
} // End InitSntpTime().


/////////////////////////////////////////////////////////////////////////////
// StartNewConnection()
//
// This method is called on a transition from WiFi not connected to WiFi
// connected.  It initializes SNTP, updates timezone rules, and reads UTC
// time to prime the internal clock.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::StartNewConnection()
{
    // Init SNTP and current time.
    delay(100);
    InitSntpTime();

    // Update timezone rules to be sure that they are current.
    UpdateTimezoneRules();
    delay(100);

    // Prime the clock.
    GetUtcTimeT();
} // End StartNewConnection().
