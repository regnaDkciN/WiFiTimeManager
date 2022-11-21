/////////////////////////////////////////////////////////////////////////////////
// WiFiTimeManager.h
//
// This class implements the WiFiTimeManager class.  It handles miscellaneous 
// timezone and DST related tasks.
//
// History:
// - jmcorbett 17-SEP-2022 Original creation.
//
// Copyright (c) 2022, Joseph M. Corbett
//
/////////////////////////////////////////////////////////////////////////////////

#if !defined TIMESETTINGS_H
#define TIMESETTINGS_H

#include <WiFiManager.h>        // Manage connection. https://github.com/tzapu/WiFiManager
#include <string.h>             // For strncpy().
#include <Timezone_Generic.hpp> // For timezone handling.
#include "WebPages.h"           // For our web page character string.


//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Default timezone and DST values.  CHANGE THESE AS DESIRED.
// These are used only on the first startup of the program.  After that,
// (possibly) updated values are stored in and recovered from NVS on each
// startup.  The TP_VERSION value should be bumped any time a change is made
// to the structure.
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
static const uint32_t TP_VERSION           = 4;       // Struct version.  Bump on changes.
static const int32_t  DFLT_TZ_OFST         = -300;    // Eastern time (5 hrs behind).
static const char    *DFLT_TZ_ABBREV       = "EST";   // DST end rule abbreviation.
static const bool     DFLT_USE_DST         = true;    // true to use DST.
static const int32_t  DFLT_DST_OFST        = 60;      // 30 or 60 minute DST offset.
static const char    *DFLT_DST_START_ABBREV= "EDT";   // DST start rule abbreviation.
static const uint32_t DFLT_DST_START_WK    = Second;  // DST starts 2nd 
static const uint32_t DFLT_DST_START_DOW   = Sun;     //    Sunday
static const uint32_t DFLT_DST_START_MONTH = Mar;     //    of March
static const uint32_t DFLT_DST_START_HOUR  = 2;       //    at 2 AM.
static const uint32_t DFLT_DST_END_WK      = First;   // DST ends 1st
static const uint32_t DFLT_DST_END_DOW     = Sun;     //    Sunday
static const uint32_t DFLT_DST_END_MONTH   = Nov;     //    of November
static const uint32_t DFLT_DST_END_HOUR    = 2;       //    at 2 AM.
static const char    *DFLT_NTP_ADDR        = "time.nist.gov"; // NTP server
static const uint32_t DFLT_NTP_PORT        = 123;     // NTP port.


/////////////////////////////////////////////////////////////////////////////////
// TimeParameters structure
//
// Holds timezone offset and DST start/end times.
//
/////////////////////////////////////////////////////////////////////////////////
struct TimeParameters
{
    // Constructor.
    TimeParameters() :
        m_Version(TP_VERSION), m_TzOfst(DFLT_TZ_OFST), m_UseDst(DFLT_USE_DST),
        m_DstOfst(DFLT_DST_OFST), m_NtpPort(DFLT_NTP_PORT),
        m_DstStartRule{{'\0', '\0', '\0', '\0', '\0', '\0'},
                       DFLT_DST_START_WK,
                       DFLT_DST_START_DOW,
                       DFLT_DST_START_MONTH,
                       DFLT_DST_START_HOUR,
                       DFLT_TZ_OFST + DFLT_DST_OFST},
        m_DstEndRule{{'\0', '\0', '\0', '\0', '\0', '\0'},
                     DFLT_DST_END_WK,
                     DFLT_DST_END_DOW,
                     DFLT_DST_END_MONTH,
                     DFLT_DST_END_HOUR,
                     DFLT_TZ_OFST}
    { 
        strncpy(m_NtpAddr, DFLT_NTP_ADDR, MAX_NTP_ADDR - 1);
        strncpy(m_DstStartRule.abbrev, DFLT_DST_START_ABBREV, sizeof(m_DstStartRule.abbrev) - 1);
        strncpy(m_DstEndRule.abbrev, DFLT_TZ_ABBREV, sizeof(m_DstStartRule.abbrev) - 1);
    }
    
    static const size_t MAX_NTP_ADDR = 26;
    
    // Time related fields.
    uint32_t       m_Version;              // Struct version.  Bump on changes.
    int32_t        m_TzOfst;               // Timezone offset in minutes.
    bool           m_UseDst;               // true to use DST.
    int32_t        m_DstOfst;              // 30 or 60 minute DST offset.
    uint32_t       m_NtpPort;              // Ntp port.
    char           m_NtpAddr[MAX_NTP_ADDR];// Ntp server address.
    TimeChangeRule m_DstStartRule;         // Rule for starting DST.
    TimeChangeRule m_DstEndRule;           // Rule for ending DST.
    
}; // End struct TimeParameters.


/////////////////////////////////////////////////////////////////////////////////
// WiFiTimeManager class
//
// Handles miscellaneous timezone and DST related tasks.
//
/////////////////////////////////////////////////////////////////////////////////
class WiFiTimeManager : public WiFiManager
{
public:
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
    static WiFiTimeManager *Instance();
    
    
    /////////////////////////////////////////////////////////////////////////////
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
    /////////////////////////////////////////////////////////////////////////////
    bool Init(const char *pApName, bool setupButton = true);


    /////////////////////////////////////////////////////////////////////////////
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
    // NOTE: If a new connection was just establiched we will not return from
    //       this method.  Instead we will reset the ESP32.  (See the commented
    //       code in WiFiTimeManager.cpp).
    //
    /////////////////////////////////////////////////////////////////////////////
    bool process();


    /////////////////////////////////////////////////////////////////////////////
    // autoConnect()
    //
    // Overrides the WiFiManager method to add m_connected status.
    // Auto connect to saved wifi, or custom, and start config portal on failure.
    // If the version with no arguments is called, an auto generated name will
    // be used for the access point.
    //
    // Arguments:
    //   pApName     - Pointer to a null terminated string representing the name
    //                 to be used for the access point.
    //   pApPassword - Pointer to to a null terminated string representing the
    //                 password to be used by the access point.  This argument
    //                 is optional and if not used, the access point will not 
    //                 require a password.  If a password is used, it should be
    //                 8 to 63 characters.
    //
    // Returns:
    //    Returns 'true' if the network was successfully connected.  Returns
    //    'false' otherwise.
    //
    /////////////////////////////////////////////////////////////////////////////
    boolean autoConnect() { m_Connected = WiFiManager::autoConnect(); return m_Connected; }
    boolean autoConnect(char const *pApName, char const *pApPassword = NULL)
            {m_Connected = WiFiManager::autoConnect(pApName, pApPassword); return m_Connected; }


    /////////////////////////////////////////////////////////////////////////////
    // setSaveParamsCallback()
    //
    // Overrides WiFiManager::setSaveParamsCallback().
    // Sets a callback that will be invoked when saving either params-in-wifi
    // or the params page.
    //
    // Arguments:
    //   func - Pointer to the function to be called upon saving the params.  This
    //          function will be called after the parameters are handled by our
    //          class's handler.  A NULL value is acceptable.
    //
    /////////////////////////////////////////////////////////////////////////////
    void setSaveParamsCallback(std::function<void()> func) { m_pSaveParamsCallback = func; }


    /////////////////////////////////////////////////////////////////////////////
    // SetUtcGetCallback()
    //
    // Sets a callback that will be invoked when non-NTP UTC time is needed.
    // Can be used to read time from a hardware real timme clock or other time
    // source.
    //
    // Arguments:
    //   func - Pointer to the callback function that will return UTC time in
    //          seconds since Januar 1, 1970 (Unix time).
    //
    /////////////////////////////////////////////////////////////////////////////
    void SetUtcGetCallback(std::function<time_t()> func);


    /////////////////////////////////////////////////////////////////////////////
    // SetUtcSetCallback()
    //
    // Sets a callback that will be invoked when NTP UTC time has been received.
    // Can be used to set the time of a hardware real time clock or other time
    // source.
    //
    // Arguments:
    //   func - Pointer to the function to be called to set UTC time.  The
    //          function takes a time_t argument which specifies the number of
    //          seconds since January 1, 1970 (Unix time).
    //
    /////////////////////////////////////////////////////////////////////////////
    void SetUtcSetCallback(std::function<void(time_t t)> func);


    /////////////////////////////////////////////////////////////////////////////
    // ResetData()
    //
    // This method resets/clears any saved network connection credentials
    // (network SSID and password) that may have been previously saved.
    // The next reboot will cause the wifi manager access point to execute and
    // a new set of credentials will be needed.
    //
    /////////////////////////////////////////////////////////////////////////////
    void ResetData();


    /////////////////////////////////////////////////////////////////////////////
    // Save()
    //
    // Saves our current state to NVS.
    //
    // Returns:
    //    Returns 'true' if successful, or 'false' otherwise.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool Save() const;


    /////////////////////////////////////////////////////////////////////////////
    // Restore()
    //
    // Restores our state from NVS.
    //
    // Returns:
    //    Returns 'true' if successful, or 'false' otherwise.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool Restore();


    /////////////////////////////////////////////////////////////////////////////
    // Reset()
    //
    // Reset our state info in NVS.
    //
    // Returns:
    //    Returns 'true' if successful, or 'false' otherwise.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool Reset();

    
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
    time_t GetUtcTime();
    
    
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
    time_t GetLocalTime();
    
    
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
    void PrintDateTime(time_t t, const char *pTz);
    
    
    /////////////////////////////////////////////////////////////////////////////
    // GetLocalTimezoneString()
    //
    // Returns a pointer to the currently active timezone string, or NULL on error.
    //
    /////////////////////////////////////////////////////////////////////////////
    const char *GetLocalTimezoneString();
    
    
    /////////////////////////////////////////////////////////////////////////////
    // Getters and setters.
    /////////////////////////////////////////////////////////////////////////////
    bool     IsConnected()      const { return m_Connected; }
    int32_t  GetTzOfst()        const { return m_Params.m_TzOfst; }
    char    *GetTzAbbrev()            { return m_Params.m_DstEndRule.abbrev; }
    bool     GetUseDst()        const { return m_Params.m_UseDst; }
    int32_t  GetDstOfst()       const { return m_Params.m_DstOfst; }
    char    *GetDstAbbrev()           { return m_Params.m_DstStartRule.abbrev; }
    uint32_t GetDstStartWk()    const { return m_Params.m_DstStartRule.week; }
    uint32_t GetDstStartDow()   const { return m_Params.m_DstStartRule.dow; }
    uint32_t GetDstStartMonth() const { return m_Params.m_DstStartRule.month; }
    uint32_t GetDstStartHour()  const { return m_Params.m_DstStartRule.hour; }
    int32_t  GetDstStartOfst()  const { return m_Params.m_DstStartRule.offset; }
    uint32_t GetDstEndWk()      const { return m_Params.m_DstEndRule.week; }
    uint32_t GetDstEndDow()     const { return m_Params.m_DstEndRule.dow; }
    uint32_t GetDstEndMonth()   const { return m_Params.m_DstEndRule.month; }
    uint32_t GetDstEndHour()    const { return m_Params.m_DstEndRule.hour; }
    int32_t  GetDstEndOfst()    const { return m_Params.m_DstEndRule.offset; }
    char    *GetWebPage()             { return WebPageBuffer; }
    char    *GetNtpAddr()             { return m_Params.m_NtpAddr; }
    uint32_t GetNtpPort()       const { return m_Params.m_NtpPort; }
    bool     UsingNetworkTime() const { return m_UsingNetworkTime; }
    
    // Min and max constants for selectable fields.
    static const uint32_t WK_MIN     = Last;    // Last
    static const uint32_t WK_MAX     = Fourth;  // Fourth
    static const uint32_t DOW_MIN    = Sun;     // Sunday
    static const uint32_t DOW_MAX    = Sat;     // Saturday
    static const uint32_t MONTH_MIN  = Jan;     // January
    static const uint32_t MONTH_MAX  = Dec;     // December
    static const uint32_t HOUR_MIN   = 0;       // Midnight
    static const uint32_t HOUR_MAX   = 23;      // 11:00 PM
    static const uint32_t OFFSET_MIN = 30;      // 30 minutes
    static const uint32_t OFFSET_MAX = 60;      // 60 minutes
    static const uint32_t OFFSET_MID = (OFFSET_MAX + OFFSET_MIN) / 2; // Middle of offset range.
        

    void SetTzOfst(int32_t v)         { m_Params.m_TzOfst = v; }
    void SetTzAbbrev(char *v)         { strncpy(m_Params.m_DstEndRule.abbrev, v, sizeof(m_Params.m_DstStartRule.abbrev) - 1); }
    void SetUseDst(bool v)            { m_Params.m_UseDst = v; }
    void SetDstOfst(int32_t v)        { m_Params.m_DstOfst = v; }
    void SetDstAbbrev(char *v)        { strncpy(m_Params.m_DstStartRule.abbrev, v, sizeof(m_Params.m_DstStartRule.abbrev) - 1); }
    void SetDstStartWk(uint32_t v)    { m_Params.m_DstStartRule.week = constrain(v, WK_MIN, WK_MAX); }
    void SetDstStartDow(uint32_t v)   { m_Params.m_DstStartRule.dow = constrain(v, DOW_MIN, DOW_MAX); }
    void SetDstStartMonth(uint32_t v) { m_Params.m_DstStartRule.month = constrain(v, MONTH_MIN, MONTH_MAX); }
    void SetDstStartHour(uint32_t v)  { m_Params.m_DstStartRule.hour = constrain(v, HOUR_MIN, HOUR_MAX); }
    void SetDstStartOfst(int32_t v)   { m_Params.m_DstStartRule.offset = (v <= OFFSET_MID ? OFFSET_MIN : OFFSET_MAX); }
    void SetDstEndWk(uint32_t v)      { m_Params.m_DstEndRule.week = constrain(v, WK_MIN, WK_MAX); }
    void SetDstEndDow(uint32_t v)     { m_Params.m_DstEndRule.dow = constrain(v, DOW_MIN, DOW_MAX); }
    void SetDstEndMonth(uint32_t v)   { m_Params.m_DstEndRule.month = constrain(v, MONTH_MIN, MONTH_MAX); }
    void SetDstEndHour(uint32_t v)    { m_Params.m_DstEndRule.hour = constrain(v, HOUR_MIN, HOUR_MAX); }
    void SetDstEndOfst(int32_t v)     { m_Params.m_DstEndRule.offset = (v <= OFFSET_MID ? OFFSET_MIN : OFFSET_MAX); }
    static const uint32_t MIN_NTP_UPDATE_SEC = 4;
    void SetMinNtpRate(uint32_t r)    { m_MinNtpRateSec = r >= MIN_NTP_UPDATE_SEC ? r : MIN_NTP_UPDATE_SEC; }
    void SetNtpAddr(char *v)          { strncpy(m_Params.m_NtpAddr, v, sizeof(m_Params.m_NtpAddr) - 1); }
    void SetNtpPort(uint32_t p)       { m_Params.m_NtpPort = constrain(p, 1, 65535); }
    
protected:


private:
    /////////////////////////////////////////////////////////////////////////////
    // Constructor and destructor.
    //
    // Private to enforce singleton behavior.
    /////////////////////////////////////////////////////////////////////////////
    WiFiTimeManager();
    ~WiFiTimeManager() {}


    /////////////////////////////////////////////////////////////////////////////
    // Override some WiFiManager methods that we don't want our end user to be
    // able to access since we use them exclusively.
    /////////////////////////////////////////////////////////////////////////////
    bool addParameter(WiFiManagerParameter *p) { return WiFiManager::addParameter(p); }
    void resetSettings() {WiFiManager::resetSettings(); }
    void setMenu(std::vector<const char*>& menu) {WiFiManager::setMenu(menu); }
    void setMenu(const char* menu[], uint8_t size) {WiFiManager::setMenu(menu, size); }


    /////////////////////////////////////////////////////////////////////////////
    // UpdateTimezoneRules()
    //
    // Update our timezone rules.  Should be called any time timezone or DST
    // data changes.
    //
    /////////////////////////////////////////////////////////////////////////////
    void UpdateTimezoneRules();
    

    /////////////////////////////////////////////////////////////////////////////
    // UpdateWebPage()
    //
    // Update our custom Setup page based on current timezone and DST settings.
    //
    /////////////////////////////////////////////////////////////////////////////
    void UpdateWebPage();


    /////////////////////////////////////////////////////////////////////////////
    // Unimplemented methods.  We don't want users to try to use these.
    /////////////////////////////////////////////////////////////////////////////
    WiFiTimeManager(WiFiTimeManager &rCs);
    WiFiTimeManager &operator=(WiFiTimeManager &rCs);


    /////////////////////////////////////////////////////////////////////////////
    // Private methods.
    /////////////////////////////////////////////////////////////////////////////

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
    static void SaveParamCallback();
    
    
    /////////////////////////////////////////////////////////////////////////////
    // DefaultUtcGetCallback()
    //
    // This static method returns UTC time as kept by the Unix time library.
    // It is called by default when UTC time is needed, but network time should
    // not be called due to invocation frequency limitations, or when no network
    // connection exists.
    //
    // Returns:
    //   Always returns the current UTC time as kept by the internal Unix time
    //   library.
    //
    // Note that this method may be overridden by user code by supplying a  user
    // implemented callback via SetUtcGetCallback().
    //
    /////////////////////////////////////////////////////////////////////////////
    static time_t DefaultUtcGetCallback() { return now(); }
    
    
    /////////////////////////////////////////////////////////////////////////////
    // DefaultUtcSetCallback()
    //
    // This static method sets UTC time as kept by the Unix time library.
    // It is called by default when UTC time is initialized or updated from
    // network time.
    //
    // Arguments:
    //   t - The UTC time in seconds since January 1, 1970 as read from a NTP
    //       server.
    //
    // Note that this method may be overridden by user code by supplying a  user
    // implemented callback via SetUtcSetCallback().
    //
    /////////////////////////////////////////////////////////////////////////////
    static void DefaultUtcSetCallback(time_t t) { setTime(t); }
    

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
    String GetParamString(String name);
    
    
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
    //    value of the specified parameter if successful, or an empty buffer on
    //    failure.
    //
    /////////////////////////////////////////////////////////////////////////////
    char  *GetParamChars(String name, char *buf, size_t size);
    
    
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
    int    GetParamInt(String name);
    
    
    /////////////////////////////////////////////////////////////////////////////
    // SendNtpPacket()
    //
    // Sends a time request packet via UDP to the NTP server.
    //
    /////////////////////////////////////////////////////////////////////////////
    void SendNtpPacket();
    
    
    /////////////////////////////////////////////////////////////////////////////
    // Private static constants.
    /////////////////////////////////////////////////////////////////////////////
    static const char    *pPrefSavedStateLabel;
    static const size_t   MAX_NVS_NAME_LEN;
    static const int      DFLT_SERVER_PORT = 80;
    static const size_t   MAX_JSON_SIZE = 350;
    static const size_t   MAX_WEB_PAGE_SIZE = sizeof(TZ_SELECT_STR) + MAX_JSON_SIZE;
    static char           WebPageBuffer[MAX_WEB_PAGE_SIZE];
    static const char    *m_pName;
    static const int      NTP_PACKET_SIZE  = 48;     // NTP timestamp is in the 
                                                     //    first 48 bytes of the message.
    static const int      UDP_TIMEOUT_MS   = 2000;   // Timeout in miliseconds to
                                                     //    wait for n UDP packet to arrive.


    /////////////////////////////////////////////////////////////////////////////
    // Private instance data.
    /////////////////////////////////////////////////////////////////////////////
    time_t         m_LastTime;          // Timestamp from the last NTP packet.
    WiFiUDP        m_Udp;               // UDP instance for tx & rx of UDP packets.
    bool           m_Connected;         // Set if server is connected to net.
    bool           m_UsingNetworkTime;  // True if using time from NTP server.
    TimeParameters m_Params;            // Timezone and DST data.
    Timezone       m_Timezone;          // Timezone class for TZ and DST change.
    const char    *m_pNtpServer;        // Ntp server address string pointer.
    unsigned       m_NtpPort;           // Port used for NTP requests.
    uint32_t       m_MinNtpRateSec;     // Minimum seconds between NTP updates.
    TimeChangeRule *m_pLclTimeChangeRule; // Pointer to time change rule in use.
    std::function<void()> m_pSaveParamsCallback;     // Pointer to save params callback.
    std::function<time_t()> m_pUtcGetCallback;       // Pointer to get UTC callback.
    std::function<void(time_t t)> m_pUtcSetCallback; // Pointer to set UTC callback.
    uint8_t        m_PacketBuf[NTP_PACKET_SIZE]; // Buffer to hold in & out packets.
    

}; // End class WiFiTimeManager.



#endif // TIMESETTINGS_H