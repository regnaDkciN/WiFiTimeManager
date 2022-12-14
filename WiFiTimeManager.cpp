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
// - jmcorbett 17-SEP-2022 Original creation.
//
// Copyright (c) 2023, Joseph M. Corbett
//
/////////////////////////////////////////////////////////////////////////////////

#include <Preferences.h>        // For NVS data storage.
#include <ArduinoJson.h>        // For JSON handling.
#include <ESPmDNS.h>            // For Mdns support.
#include <Timezone_Generic.h>   // For Timezone.  Must be in only 1 file.
#include "WiFiTimeManager.h"    // For WiFiTimeManager class.


// Some constants used by the WiFiTimeManager class.
const size_t WiFiTimeManager::MAX_NVS_NAME_LEN     = 15U;
const char  *WiFiTimeManager::pPrefSavedStateLabel = "All Time Data";
char         WiFiTimeManager::WebPageBuffer[WiFiTimeManager::MAX_WEB_PAGE_SIZE];
const char  *WiFiTimeManager::m_pName = "TIME DATA";
// Unix time starts on Jan 1 1970. In seconds, that's 2208988800.
static const uint32_t SEVENTY_YEARS = 2208988800UL;

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
                                     m_LastTime(), m_Udp(),
                                     m_UsingNetworkTime(false), m_Params(),
                                     m_Timezone(m_Params.m_DstStartRule, m_Params.m_DstEndRule),
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

    // Initialize the clock to the start of 2023.
    const time_t UTC_2023_START = 1672531200;
    setTime(UTC_2023_START);
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
    // Since this is static, the instance is created only on the first call to
    // Instance().  A pointer to it is always returned.
    static WiFiTimeManager instance;
    return &instance;
} // End Instance().


/////////////////////////////////////////////////////////////////////////////
// Init()
//
// This method initializes the WiFiTimeManager class.
//
// Arguments:
//    - pApName     - This is the network name to be used for the access point.
//    - pApPassword - Pointer to to a null terminated string representing the
//                    password to be used by the access point.  This argument
//                    is optional and if not used, the access point will not
//                    require a password.  If a password is used, it should be
//                    8 to 63 characters.
//    - setupButton - Set to true if a separate "Setup" button should be
//                    displayed on the WiFiManager home page.  When selected,
//                    this button will cause a separate setup button to be
//                    displayed.  The separate setup page will contain time
//                    related parameters that the user may adjust.  In this case,
//                    the WiFi configuratioin page will not display the time
//                    parameters.  If set to false, then the time parameters
//                    will be selectable from the WiFi configuration page.
//
// Returns:
//    Returns true on successful completion.  Returns false if pApName is empty.
//
/////////////////////////////////////////////////////////////////////////////
bool WiFiTimeManager::Init(const char *pApName, const char *pApPassword,
                           bool setupButton)
{
    // Set the WiFi mode.  ESP defaults to STA+AP.
    WiFi.mode(WIFI_STA);

    // Setup our AP name if one was given.
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
        WTMPrint(PL_WARN_BP, "Restore failed\n");
        if (!Save())
        {
            WTMPrint(PL_WARN_BP, "Save failed\n");
            return false;
        }
    }

    // Set our network name.
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

    // Setup our custom menu.  Note: if 'separate' is true we want our setup page
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

    // Return success if we got this far.
    return true;
} // End Init().


/////////////////////////////////////////////////////////////////////////////////
// process()
//
// Overrides the WiFiManager process() method.
// This method handles the non-blocking WiFiManager completion.
//
// Returns:
//    Returns a bool indicating whether or not the network is now connected.
//    A 'true' value indicates connected, while a 'false' value
//    indicates not connected.
//
// NOTE: If a new connection was just established the network time gets updated.
//
/////////////////////////////////////////////////////////////////////////////////
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
            UpdateTimezoneRules();
            delay(500);
            GetUtcTime();

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

    // Clear our timezone/dst state data.
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
    WTMPrint(PL_INFO_BP, "Saving Data\n");

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
    WTMPrint(PL_INFO_BP, "Restoring Saved Data\n");

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
// UpdateTimezone()
//
// Update our timezone rules.  Should be called any time timezone or DST
// data changes.
//
/////////////////////////////////////////////////////////////////////////////
void WiFiTimeManager::UpdateTimezoneRules()
{
    // Handle use of DST differently.  If DST is being used, then use both
    // timezone rules.  Otherwise, use the DST end rule for both arguments
    // to setRules().
    if (m_Params.m_UseDst)
    {
        m_Timezone.setRules(m_Params.m_DstStartRule, m_Params.m_DstEndRule);
    }
    else
    {
        m_Timezone.setRules(m_Params.m_DstEndRule, m_Params.m_DstEndRule);
    }

    // Display debug info if debug display is enabled.
    if (PL_DEBUG_BP && m_PrintLevel)
    {
        m_Timezone.display_DST_Rule();
        m_Timezone.display_STD_Rule();
    }
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
    doc["NTP_PORT"] = GetNtpPort();

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

    // All NTP fields have been given values, now we send a packet
    // requesting a timestamp.  NTP requests are to the NTP port.
    m_Udp.beginPacket(GetNtpAddr(), GetNtpPort());
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
        // We're either not connected or it is too soon to contact the NTP
        // server, so handle the time fetching locally.
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
            // Read packet into our packet buffer.
            m_Udp.read(m_PacketBuf, NTP_PACKET_SIZE);

            // Convert four bytes starting at location 40 to a long integer.
            uint32_t secsSince1900;
            secsSince1900 =  static_cast<uint32_t>(m_PacketBuf[40] << 24);
            secsSince1900 |= static_cast<uint32_t>(m_PacketBuf[41] << 16);
            secsSince1900 |= static_cast<uint32_t>(m_PacketBuf[42] << 8);
            secsSince1900 |= static_cast<uint32_t>(m_PacketBuf[43]);

            // Subtract seventy years and round the seconds up.
            uint32_t nowMs = millis();
            uint32_t epochSec = (secsSince1900 - SEVENTY_YEARS) +
                                (nowMs - beginWaitMs + MS_PER_SEC / 2) / MS_PER_SEC;

            // Set the system time to UTC and call the user's set callback if any,
            setTime(static_cast<time_t>(epochSec));
            m_pUtcSetCallback(static_cast<time_t>(epochSec));

            // Remember when we last accessed the NTP server.
            lastMs = nowMs;

            // Remember that we got NTP time.
            m_UsingNetworkTime = true;
            WTMPrint(PL_INFO_BP, "Got NTP time.\n");
            return static_cast<time_t>(epochSec);
        }
    }

    // If we got here we were unable to connect to the NTP server.  It's possible
    // that we previously received good NTP data, but this time we didn't.
    // So handle the time fetching locally.
    m_UsingNetworkTime = false;
    WTMPrint(PL_WARN_BP, "No NTP Response!\n");

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
// Format and print a unix time_t value, with a time zone appended.
//
// Arguments:
//   t   - time_t value containing the time to be displayed.
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


/////////////////////////////////////////////////////////////////////////////
// WTMPrint()
//
// Print the specified data if the specified level matches the m_PrintLevel
// that was set via SetPrintLevel().  Note that this method is overloaded
// to work with null terminated strings, and String objects.
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
// Print the specified data if the specified level matches the m_PrintLevel
// that was set via SetPrintLevel().  Note that this method is overloaded
// to work with null terminated strings, and String objects.
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

