/////////////////////////////////////////////////////////////////////////////////
// WiFiTimeManager.cpp
//
// Contains the WiFiTimeManager class that manages timezone and DST settings.
//
// History:
// - jmcorbett 17-SEP-2022 Original creation.
//
// Copyright (c) 2022, Joseph M. Corbett
//
/////////////////////////////////////////////////////////////////////////////////

#include <Preferences.h>        // For NVS data storage.
#include <ArduinoJson.h>        // For JSON handling.
#include <ESPmDNS.h>            // For Mdns support.
#include <Timezone_Generic.h>   // For Timezone.  Must be in only 1 file.
#include "WiFiTimeManager.h"    // For WiFiTimeManager class.


// Some constants used by the class.
const size_t WiFiTimeManager::MAX_NVS_NAME_LEN     = 15U;
const char  *WiFiTimeManager::pPrefSavedStateLabel = "All Time Data";
char         WiFiTimeManager::WebPageBuffer[WiFiTimeManager::MAX_WEB_PAGE_SIZE];
const char  *WiFiTimeManager::m_pName = "TIME DATA";
// Unix time starts on Jan 1 1970. In seconds, that's 2208988800
static const uint32_t SEVENTY_YEARS = 2208988800UL;


// WiFiManagerParameter custom_field; // global param ( for non blocking w params )
static WiFiManagerParameter tzSelectField;
// Create parameters with NULL customHTML in order to not need to do placement new
// later since all parameters are defined in one long string (tzSelectField) above.
static WiFiManagerParameter useDstField(NULL);
static WiFiManagerParameter dstWeek1Field(NULL);
static WiFiManagerParameter dstDay1Field(NULL);
static WiFiManagerParameter dstMonth1Field(NULL);
static WiFiManagerParameter dstHour1Field(NULL);
static WiFiManagerParameter dstOffsetField(NULL);
static WiFiManagerParameter dstWeek2Field(NULL);
static WiFiManagerParameter dstDay2Field(NULL);
static WiFiManagerParameter dstMonth2Field(NULL);
static WiFiManagerParameter dstHour2Field(NULL);
static WiFiManagerParameter tzAbbreviationField(NULL);
static WiFiManagerParameter dstAbbreviationField(NULL);
static WiFiManagerParameter ntpAddressField(NULL);
static WiFiManagerParameter ntpPortField(NULL);




/////////////////////////////////////////////////////////////////////////////////
// Constructor
//
// Initialize our instance data.
//
/////////////////////////////////////////////////////////////////////////////////
WiFiTimeManager::WiFiTimeManager() : WiFiManager(), m_LastTime(), m_Udp(),
                                     m_UsingNetworkTime(false),
                                     m_Params(), m_Timezone(m_Params.m_DstStartRule,
                                     m_Params.m_DstEndRule),
                                     m_MinNtpRateSec(60 * 60),
                                     m_pLclTimeChangeRule(NULL),
                                     m_pSaveParamsCallback(NULL),
                                     m_pUtcGetCallback(DefaultUtcGetCallback),
                                     m_pUtcSetCallback(DefaultUtcSetCallback),
                                     m_pUpdateWebPageCallback(NULL)
{
    // Clear the UDP packet buffer.
    memset(m_PacketBuf, 0, NTP_PACKET_SIZE);

    // If DST is not used, then fix the timezone rules.
    UpdateTimezoneRules();

    // Initialize clock to on Jan 1 1970. In seconds, that's 2208988800.
    setTime(SEVENTY_YEARS);
} // End constructor.



/////////////////////////////////////////////////////////////////////////////
// Instance()
//
// Return our singleton instance of the class.  Constructor is private so
// the only way to create an object is via this method.
//
// Returns:
//   Returns a pointer to the only instance of this class.
//
/////////////////////////////////////////////////////////////////////////////
WiFiTimeManager *WiFiTimeManager::Instance()
{
    static WiFiTimeManager instance;
    return &instance;
} // End Instance().


/////////////////////////////////////////////////////////////////////////////////
// Init()
//
// This method initializes the spool collection.
//
// Arguments:
//    - pApName     - This is the network name to be used for the access point.
//    - setupButton - Set to true if a separate "Setup" button should be
//                    displayed on the WiFiManager home page.  When selected,
//                    this button will cause a separate setup to be displayed.
//                    The separate setup page will contain timezone parameters
//                    that the user may adjust.  In this case, the WiFi
//                    configuratioin page will not display the timezone
//                    parameters.  If set to false, then the timezone
//                    parameters will be selectable from the WiFi
//                    configuration page.
//
// Returns:
//    Returns true on successful completion.  Returns false if pApName is empty.
//
/////////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Init(const char *pApName, bool setupButton)
{
    // Set mode.  ESP defaults to STA+AP.
    WiFi.mode(WIFI_STA);

    // Setup our AP name if one was given.
    if ((pApName == NULL) || (*pApName == '\0'))
    {
        return false;
    }

    // Restore saved state info.  If no state info has been saved yet, then
    // save our default values.
    if (!Restore())
    {
        Serial.println("Restore failed");
        if (!Save())
        {
            Serial.println("Save failed");
            return false;
        }
    }

    MDNS.begin(pApName);

    UpdateWebPage();
    new (&tzSelectField) WiFiManagerParameter(GetWebPage()); // custom html input

    addParameter(&tzSelectField);

    addParameter(&useDstField);

    addParameter(&dstWeek1Field);
    addParameter(&dstDay1Field);
    addParameter(&dstMonth1Field);
    addParameter(&dstHour1Field);
    addParameter(&dstOffsetField);

    addParameter(&dstWeek2Field);
    addParameter(&dstDay2Field);
    addParameter(&dstMonth2Field);
    addParameter(&dstHour2Field);
    addParameter(&tzAbbreviationField);
    addParameter(&dstAbbreviationField);

    addParameter(&ntpAddressField);
    addParameter(&ntpPortField);

    WiFiManager::setSaveParamsCallback(SaveParamCallback);

    // Setup our custom menu.  Note: if 'separate' is true we want our setup page
    // button to be before the wifi config button.
    const char *menu[] = {"param", "wifi", "info", "sep", "restart", "exit"};
    if (setupButton)
    {
        setMenu(menu, sizeof(menu) / sizeof(menu[0]));
    }
    else
    {
        setMenu(&menu[1], (sizeof(menu) / sizeof(menu[0])) - 1);
    }

    // Setup the wifi manager.
    // Enable captive portal redirection.
    setCaptivePortalEnable(true);
    // Always disconnect before connecting.
    setCleanConnect(true);
    // Show erase wifi config button on info page.
    setShowInfoErase(true);
    // Block until done.
    setConfigPortalBlocking(true);
    // Set to dark theme.
    setClass("invert");

    // Set our default time to the start of 2022.
    const time_t START_2022_UNIX_TIME = 1640995200;
    setTime(START_2022_UNIX_TIME);

    return true;
} // End Init().


/////////////////////////////////////////////////////////////////////////////////
// process()
//
// Overrides the WiFiManager process() method.
// This method handles the non-blocking wifi manager completion.
//
// Returns:
//    Returns a bool indicating whether or not the network is now connected.
//    A 'true' value indicates connected, while a 'false' value
//    indicates not connected.
//
// NOTE: If a new connection was just established we update the network time.
//
/////////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::process()
{
    // If we're not yet connected, then call the wifi manager to see if a
    // connection was recently made.
    if (!IsConnected())
    {
        if (WiFiManager::process())
        {
            // Delay briefly to let the network settle, then attempt to get
            // the time from the network.
            delay(500);
            GetUtcTime();
        }
    }

    return IsConnected();
} // End process().


/////////////////////////////////////////////////////////////////////////////
// ResetData()
//
// This method resets/clears any saved network connection credentials
// (network SSID and password) that may have been previously saved.
// It also clears all timezone and DST data that may have been previously save.
// The next reboot will cause the wifi manager access point to execute and
// a new set of credentials and time data will be needed.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::ResetData()
{
    // Clear the WiFiManager network data.
    resetSettings();

    // Clear our timezone/dst state data.
    Preferences prefs;
    prefs.begin(m_pName);
    prefs.clear();
    prefs.end();
} // End ResetData().


/////////////////////////////////////////////////////////////////////////////////
// Save()
//
// Saves our current state to NVS.
//
// Returns:
//    Returns 'true' if successful, or 'false' otherwise.
//
/////////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Save() const
{
    bool saved = false;
Serial.println("Saving Data");

    // Read our currently saved state.  If it hasn't changed, then don't
    // bother to do the save in order to conserve writes to NVS.
    TimeParameters nvsState;

    // Save our state data.
    Preferences prefs;
    prefs.begin(m_pName);

    // Get the saved data.
    size_t nvsSize =
        prefs.getBytes(pPrefSavedStateLabel, &nvsState, sizeof(TimeParameters));

    // See if our working data has changed since our last save.
    if ((nvsSize != sizeof(TimeParameters)) ||
        memcmp(&nvsState, &m_Params, sizeof(TimeParameters)))
    {
        // Data has changed so go ahead and save it.
        Serial.println("\nTimeSettings - saving to NVS.");
        saved =
          (prefs.putBytes(pPrefSavedStateLabel, &m_Params, sizeof(TimeParameters)) ==
              sizeof(TimeParameters));
    }
    else
    {
        // Data has not changed.  Do nothing.
        saved = true;
        Serial.println("\nTimeSettings - not saving to NVS.");
    }
    prefs.end();

    // Let the caller know if we succeeded or failed.
    return saved;
 } // End Save().


/////////////////////////////////////////////////////////////////////////////////
// Restore()
//
// Restores our state from NVS.
//
// Returns:
//    Returns 'true' if successful, or 'false' otherwise.
//
/////////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Restore()
{
    // Assume we're gonna fail.
    bool succeeded = false;
Serial.println("Restoring Saved Data");
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
// Reset our state info in NVS.
//
// Returns:
//    Returns 'true' if successful, or 'false' otherwise.
//
/////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Reset()
{
    bool status = false;

    // Erase all stored information in the wifi manager.
    ResetData();

    // Remove our state data from NVS.
    Preferences prefs;
    prefs.begin(m_pName);
    status = prefs.remove(pPrefSavedStateLabel);
    prefs.end();

    return status;
} // End Reset().


/////////////////////////////////////////////////////////////////////////////
// UpdateTimezone()
//
// Update our timezone rules.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::UpdateTimezoneRules()
{
    if (m_Params.m_UseDst)
    {
        m_Timezone.setRules(m_Params.m_DstStartRule, m_Params.m_DstEndRule);
    }
    else
    {
        m_Timezone.setRules(m_Params.m_DstEndRule, m_Params.m_DstEndRule);
    }
    m_Timezone.display_DST_Rule();
    m_Timezone.display_STD_Rule();
} // End UpdateTimezoneRules().



/////////////////////////////////////////////////////////////////////////////
// UpdateWebPage()
//
// Update our custom Setup page based on current timezone and DST settings.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::UpdateWebPage()
{
    // Start with our basic web page.
    String WebPageString(TZ_SELECT_STR);

    // Create our JSON document containing all of our user settable parameters.
    DynamicJsonDocument doc(MAX_JSON_SIZE);
    doc["TIMEZONE"] = GetTzOfst();;
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
    doc["NTP_PORT"] = GetNtpPort();

    // Convert the JSON document to a String.
    String JsonStr;
    serializeJson(doc, JsonStr);
    Serial.println(JsonStr);

    // Update our web page with our user settable parameters in JSON form.
    WebPageString.replace("*PUT_TZ_JSON_DATA_HERE*", JsonStr);

    // Save our web page for later use.
    strncpy(WebPageBuffer, WebPageString.c_str(), MAX_WEB_PAGE_SIZE - 1);

    // If selected, call back to let user code add HTML and/or java script.
    if (m_pUpdateWebPageCallback != NULL)
    {
        m_pUpdateWebPageCallback();
    }

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
    WiFiTimeManager *pWtm = Instance();

    Serial.println("SaveParamCallback");
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
    pWtm->SetNtpPort(pWtm->GetParamInt("ntpServerPort"));

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
// Returns the last value read of the specified timezone parameter.
//
// Arguments:
//   name - String containing the name of the argument whose value will be
//          returned.
//
// Returns:
//    Always returns the String value of the specified argument if successful,
//    or an empty String on failure.
//
/////////////////////////////////////////////////////////////////////////////
String WiFiTimeManager::GetParamString(String name)
{
    // Read the parameter from server.
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
// Returns the last value read of the specified timezone parameter.
//
// Arguments:
//   name - String containing the name of the argument whose value will be
//          returned.
//   pBuf - Pointer to the buffer in which the parameter's value will be
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
//   name - String containing the name of the argument whose value will be
//          returned.
//
// Returns:
//    Returns the int value of the specified timezone parameter.
//
/////////////////////////////////////////////////////////////////////////////
int WiFiTimeManager::GetParamInt(String name)
{
    return GetParamString(name).toInt();
} // End GetParamInt().


/////////////////////////////////////////////////////////////////////////////
// SendNtpPacket()
//
// Sends a time request packet via UDP to the NTP server.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::SendNtpPacket()
{
    // Set all bytes in the buffer to 0.
    memset(m_PacketBuf, 0, NTP_PACKET_SIZE);

    // Initialize values needed to form NTP request
    m_PacketBuf[0]  = 0b11100011;     // LI, Version, Mode.
    m_PacketBuf[1]  = 0;              // Stratum, or type of clock.
    m_PacketBuf[2]  = 6;              // Polling Interval.
    m_PacketBuf[3]  = 0xEC;           // Peer Clock Precision.
    // 8 bytes of zero for Root Delay & Root Dispersion.
    m_PacketBuf[12] = 49;
    m_PacketBuf[13] = 0x4E;
    m_PacketBuf[14] = 49;
    m_PacketBuf[15] = 52;

    // All NTP fields have been given values, now we can send a packet
    // requesting a timestamp:
    m_Udp.beginPacket(GetNtpAddr(), GetNtpPort()); // NTP requests are to the NTP port.
    m_Udp.write(m_PacketBuf, NTP_PACKET_SIZE);
    m_Udp.endPacket();
} // End SendNtpPacket().


/////////////////////////////////////////////////////////////////////////////
// GetUtcTime()
//
// Returns the best known value for UTC time.  If it has been a while since
// the NTP server has been contacted, then a new NTP request is sent, and its
// returned data is used to update the local clock.  If it has been too soon
// since the server was contacted, then the RTC clock (if any) time is used.
// If no too soon, but no RTC, then the time is estimated as the last known good
// time value plus the amount of time that has elapsed since it was established.
//
// Returns:
//   Returns the best known value for the current UTC time.
//
/////////////////////////////////////////////////////////////////////////////
time_t WiFiTimeManager::GetUtcTime()
{
    // Limit the frequency of packets sent to the NTP server.
    const uint32_t  MS_PER_SEC = 1000;
    static uint32_t lastMs     = millis() - (2 * m_MinNtpRateSec * MS_PER_SEC);
    uint32_t        deltaMs    = millis() - lastMs;
    if (!IsConnected() || (deltaMs < (m_MinNtpRateSec * MS_PER_SEC)))
    {
        return m_pUtcGetCallback();
    }

    // Discard any previously received packets.
    while (m_Udp.parsePacket() > 0)
        ;

    // Request a time packet.
    SendNtpPacket();

    // We won't wait forever for an NTP reply.
    uint32_t beginWaitMs = millis();
    while ((millis() - beginWaitMs) < UDP_TIMEOUT_MS)
    {
        uint32_t size = m_Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            // Read packet into the buffer.
            m_Udp.read(m_PacketBuf, NTP_PACKET_SIZE);

            // Convert four bytes starting at location 40 to a long integer.
            uint32_t secsSince1900;
            secsSince1900 =  static_cast<uint32_t>(m_PacketBuf[40] << 24);
            secsSince1900 |= static_cast<uint32_t>(m_PacketBuf[41] << 16);
            secsSince1900 |= static_cast<uint32_t>(m_PacketBuf[42] << 8);
            secsSince1900 |= static_cast<uint32_t>(m_PacketBuf[43]);

            // Subtract seventy years:
            uint32_t nowMs = millis();
            uint32_t epochSec = (secsSince1900 - SEVENTY_YEARS) +
                                (nowMs - beginWaitMs + MS_PER_SEC / 2) / MS_PER_SEC;

            // Set the system time to UTC.
            setTime(static_cast<time_t>(epochSec));
            m_pUtcSetCallback(static_cast<time_t>(epochSec));
            lastMs = nowMs;
            m_UsingNetworkTime = true;
            Serial.println("Got NTP time.");
            return static_cast<time_t>(epochSec);
        }
    }

    // If we got here we were unable to connect to the NTP server.  It's possible
    // that we previously received good NTP data, but this time we didn't.
    m_UsingNetworkTime = false;
    Serial.println("No NTP Response!");

    return m_pUtcGetCallback();
} // End GetUtcTime();


/////////////////////////////////////////////////////////////////////////////
// GetLocalTime()
//
// Returns the best known value for local time.  Converts the best known
// UTC time to local time and returns its value.  See GetUtcTime().
//
// Returns:
//   Returns the best known value for the current local time.
//
/////////////////////////////////////////////////////////////////////////////
time_t WiFiTimeManager::GetLocalTime()
{
    time_t utc = GetUtcTime();
    return m_Timezone.toLocal(utc, &m_pLclTimeChangeRule);
} // End GetUtcTime();


/////////////////////////////////////////////////////////////////////////////
// PrintDateTime
//
// Format and print a time_t value, with a time zone appended.
//
// Arguments:
//   t   - Time_t structure containing the time to be displayed.
//   pTz - Pointer to the timezone string associated with 't'.  NULL is OK.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::PrintDateTime(time_t t, const char *pTz)
{
    char buf[32];
    sprintf(buf, "%.2d:%.2d:%.2d %s %.2d %s %d %s",
          hour(t), minute(t), second(t), dayShortStr(weekday(t)), day(t),
          monthShortStr(month(t)), year(t), pTz ? pTz : "");
    Serial.println(buf);
} // End PrintDateTime().


/////////////////////////////////////////////////////////////////////////////
// GetLocalTimezoneString()
//
// Returns a pointer to the currently active timezone string, or NULL on error.
//
/////////////////////////////////////////////////////////////////////////////
const char *WiFiTimeManager::GetLocalTimezoneString()
{
    // If local time has never been read, then read it now.  This will update
    // m_pLclTimeChangeRule.
    if (m_pLclTimeChangeRule == NULL)
    {
        GetLocalTime();
    }

    // If m_pLclTimeChangeRule is still NULL, then return NULL as an error.
    return m_pLclTimeChangeRule == NULL ? NULL : m_pLclTimeChangeRule->abbrev;
} // End GetLocalTimezoneString().


/////////////////////////////////////////////////////////////////////////////
// SetUtcGetCallback()
//
// Sets a callback that will be invoked when non-NTP UTC time is needed.
// Can be used to read time from a hardware real timme clock or other time source.
//
// Arguments:
//   func - Pointer to the callback function that will return UTC time in
//          seconds since Januar 1, 1970 (Unix time).
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::SetUtcGetCallback(std::function<time_t()> func)
{
    m_pUtcGetCallback = (func == NULL) ? DefaultUtcGetCallback : func;
} // End SetUtcGetCallback().


/////////////////////////////////////////////////////////////////////////////
// SetUtcSetCallback()
//
// Sets a callback that will be invoked when NTP UTC time has been received.
// Can be used to set the time of a hardware real time clock or other time source.
//
// Arguments:
//   func - Pointer to the function to be called to set UTC time.  The function
//          takes a time_t argument which specifies the number of seconds since
//          January 1, 1970 (Unix time).
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::SetUtcSetCallback(std::function<void(time_t t)> func)
{
    m_pUtcSetCallback = (func == NULL) ? DefaultUtcSetCallback : func;
} // End SetUtcSetCallback().




