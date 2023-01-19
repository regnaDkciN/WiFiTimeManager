/////////////////////////////////////////////////////////////////////////////////
// WiFiTimeManager.h
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
// - jmcorbett 19-JAN-2023 Original creation.
//
// Copyright (c) 2023, Joseph M. Corbett
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
static const char    *DFLT_NTP_ADDR        = "time.nist.gov";
                                                      // NTP server
static const uint32_t DFLT_NTP_PORT        = 123;     // NTP port.


/////////////////////////////////////////////////////////////////////////////////
// TimeParameters structure
//
// Holds timezone offset and DST start/end times, and NTP address and port.
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

    static const size_t MAX_NTP_ADDR = 26; // Maximumsize of the NTP address string.

    // Time related fields.
    uint32_t       m_Version;              // Struct version.  Bump on changes.
    int32_t        m_TzOfst;               // Timezone offset in minutes.
    bool           m_UseDst;               // true to use DST.
    int32_t        m_DstOfst;              // 30 or 60 minute DST offset.
    uint32_t       m_NtpPort;              // NTP port.
    char           m_NtpAddr[MAX_NTP_ADDR];// NTP server address.
    TimeChangeRule m_DstStartRule;         // Rule for starting DST.
    TimeChangeRule m_DstEndRule;           // Rule for ending DST.

}; // End struct TimeParameters.


/////////////////////////////////////////////////////////////////////////////////
// WiFiTimeManager class
//
// Handles miscellaneous timezone, DST, and NTP related tasks.
//
/////////////////////////////////////////////////////////////////////////////////
class WiFiTimeManager : public WiFiManager
{
public:
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
    static WiFiTimeManager *Instance();


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
    //                    related parameters that the user may adjust.  In this
    //                    case, the WiFi configuratioin page will not display
    //                    the time parameters.  If set to false, then the time
    //                    parameters will be selectable from the WiFi
    //                    configuration page.
    //
    // Returns:
    //    Returns true on successful completion.  Returns false if pApName is empty.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool Init(const char *pApName, const char *pApPassword = NULL,
              bool setupButton = true);


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
    // NOTE: If a new connection was just establiched this method update the time.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool process();


    /////////////////////////////////////////////////////////////////////////////
    // autoConnect()
    //
    // Overrides the WiFiManager autoConnect() method in order to return the
    // status of the WiFi connection.  Automatically connects to the saved WiFi
    // network, or starts the config portal on failure.
    // This method is overloaded.  The version with no arguments uses the values
    // passed to Init().  The other version uses the specified access point
    // name and password, but should not normally be used..
    //
    // Arguments:
    //   pApName     - Pointer to a null terminated string representing the name
    //                 to be used for the access point.  This argument is optional.
    //                 If not present, then the value passed to Init() is used.
    //   pApPassword - Pointer to to a null terminated string representing the
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
    boolean autoConnect()
            { WiFiManager::autoConnect(m_pApName, m_pApPassword); return IsConnected(); }
    boolean autoConnect(char const *pApName, char const *pApPassword = NULL)
            { WiFiManager::autoConnect(pApName, pApPassword); return IsConnected(); }


    /////////////////////////////////////////////////////////////////////////////
    // setSaveParamsCallback()
    //
    // Overrides WiFiManager::setSaveParamsCallback().
    // Sets a callback that will be invoked when saving the Setup page.
    //
    // Arguments:
    //   func - Pointer to the function to be called upon saving the params.  This
    //          function will be called after the parameters are handled by our
    //          class's handler.  A NULL value is acceptable.
    //
    /////////////////////////////////////////////////////////////////////////////
    void setSaveParamsCallback(std::function<void()> func)
            { m_pSaveParamsCallback = func; }


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
    // SetUpdateWebPageCallback()
    //
    // Sets a callback that will be invoked when when the Setup web page is
    // updated.  The user can use this callback to monitor or modify the
    // contents of the Setup web page.
    //
    // Arguments:
    //   func - Pointer to the function to be called any time the Setup web
    //          page gets updated.  A NULL value is acceptable.
    //
    // Note: The rWebPage argument of the callback is a reference to a String
    //       that contains the web page that will be sent to the client.  It may
    //       be modified as needed.  The maxSize argument of the callback is the
    //       maximum size of the String in rWebPage.
    //
    //       Upon return from the callback, the contents of the rWebPage will be
    //       safely copied into a buffer that will be sent to the client.  If
    //       the size of the (modified) rWebPage exceeds  the value of maxSize,
    //       the buffer will be truncated, so be careful.
    //
    //       Several comments appear within the string that
    //       may be employed to locate specific places within the standard
    //       web page.  The user can use these comments to insert relevant
    //       HTML or javascript code.  These comments are:
    //          "<!-- HTML START -->"
    //              This marks the start of HTML, which is also the start of
    //              the web page string.
    //          "<!-- HTML END -->"
    //              This marks the end of HTML and is just before the end of the
    //              web page body.
    //          "// JS START"
    //              This marks the start of the java script, just after the
    //              <script> declaration.
    //          "// JS ONLOAD"
    //              This marks a spot within the onload() function where
    //              java script initialization code can be added.
    //          "// JS SAVE"
    //              This marks a spot within the function that is executed when
    //              the submit (save) button is pressed.  It can be used to
    //              perform java script save actions.
    //          "// JS END"
    //              This marks the end of of the java script, just before the
    //              </script> declaration.
    //
    /////////////////////////////////////////////////////////////////////////////
    void SetUpdateWebPageCallback(
        std::function<void(String &rWebPage, uint32_t maxSize)> func)
        { m_pUpdateWebPageCallback = func; }


    /////////////////////////////////////////////////////////////////////////////
    // ResetData()
    //
    // This method resets/clears any saved network connection credentials
    // (network SSID and password) and all timezone and NTP data that may have
    // been previously saved.  The next reboot will cause the WiFi Manager access
    // point to execute and new set of network credentials will be needed.
    //
    /////////////////////////////////////////////////////////////////////////////
    void ResetData();


    /////////////////////////////////////////////////////////////////////////////
    // Save()
    //
    // Saves our current state to NVS.  This includes all timezone and NTP data.
    //
    // Returns:
    //    Returns 'true' if successful, or 'false' otherwise.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool Save() const;


    /////////////////////////////////////////////////////////////////////////////
    // Restore()
    //
    // Restores our state from NVS.  This includes all timezone and NTP data.
    //
    // Returns:
    //    Returns 'true' if successful, or 'false' otherwise.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool Restore();


    /////////////////////////////////////////////////////////////////////////////
    // Reset()
    //
    // Reset our state info in NVS.  This includes all timezone and NTP data.
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
    // returned data is used to update the local clock.  If it is too soon since
    // the server was contacted, then the user supplied UtcGetCallback (if any)
    // is called to allow the user to update clock time (i.e. an RTC).  If no
    // user supplied UtcGetCallback is present, then the Arduino time library
    // version of current UTC time is returned.
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
    // GetDateTimeString
    //
    // Format and print a unix time_t value, with a time zone label appended.
    //
    // Arguments:
    //   pBuf - Pointer to buffer that will receive the time string.  Must be at
    //          least 32 characters long to receive the full string.  If the buffer
    //          is shorter than 32 characters, the time string will be truncated.
    //   size - The size, in bytes, of buf;
    //   t    - time_t value containing the time to be displayed.
    //   pTz  - Pointer to the timezone string associated with 't'.  NULL is OK.
    //
    // Returns:
    //   Always returns the number of characters in the time string minus the
    //   terminating NULL.
    //
    /////////////////////////////////////////////////////////////////////////////
    int GetDateTimeString(char *pBuf, int size, time_t t, const char *pTz);


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
    void PrintDateTime(time_t t, const char *pTz = NULL);


    /////////////////////////////////////////////////////////////////////////////
    // GetLocalTimezoneString()
    //
    // Returns a pointer to the currently active timezone string, or NULL on error.
    //
    /////////////////////////////////////////////////////////////////////////////
    const char *GetLocalTimezoneString();


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
    String GetParamString(String name);


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
    //    Returns the value in pBuf.  The returned buffer will contain the
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
    //   name - String containing the name of the argument whose value is to be
    //          returned.
    //
    // Returns:
    //    Returns the int value of the specified Setup parameter.
    //
    /////////////////////////////////////////////////////////////////////////////
    int    GetParamInt(String name);


    /////////////////////////////////////////////////////////////////////////////
    // UpdateTimezoneRules()
    //
    // Update our timezone rules.  Should be called any time timezone or DST
    // data changes.
    //
    /////////////////////////////////////////////////////////////////////////////
    void UpdateTimezoneRules();


    /////////////////////////////////////////////////////////////////////////////
    // Getters and setters.
    /////////////////////////////////////////////////////////////////////////////
    bool     IsConnected()            { return getLastConxResult() == WL_CONNECTED; }
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
    char    *GetNtpAddr()             { return m_Params.m_NtpAddr; }
    uint32_t GetNtpPort()       const { return m_Params.m_NtpPort; }
    bool     UsingNetworkTime() const { return m_UsingNetworkTime; }
    uint32_t GetMinNtpRate()    const { return m_MinNtpRateSec; }

    // Min and max constants for selectable fields.
    // Constants (Last, Fourth, ...) defined in Timezone_Generic.hpp.
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

    // Print level values.  Use bit patterns to define levels.  The BP(v) macro
    // below creates a bit pattern based on the given value.  Note that the
    // BP() macro is only valid for values greater than zero.  The MASK(v) macro
    // below creates a bit mask that includes its level and all lower levels.
    // Note that the MASK() macro is valid for values greater than or equal to
    // zero.
    #define BP(v)   (1 << ((v) - 1))
    #define MASK(v) ((1 << (v)) - 1)
    static const uint32_t PL_NONE  = 0;                   // No status will be printed.
    static const uint32_t PL_WARN  = 1;                   // Display warnings.
    static const uint32_t PL_INFO  = 2;                   // Display informational info.
    static const uint32_t PL_DEBUG = 3;                   // Display debug info.
    static const uint32_t PL_DFLT  = PL_WARN;             // Default display level = warn.

    static const uint32_t PL_NONE_BP  = 0;                // No status will be printed.
    static const uint32_t PL_WARN_BP  = BP(PL_WARN);      // Display warnings.
    static const uint32_t PL_INFO_BP  = BP(PL_INFO);      // Display informational info.
    static const uint32_t PL_DEBUG_BP = BP(PL_DEBUG);     // Display debug info.
    static const uint32_t PL_DFLT_BP  = PL_WARN_BP;       // Default display level = warn.

    static const uint32_t PL_NONE_MASK  = 0;              // No status will be printed.
    static const uint32_t PL_WARN_MASK  = MASK(PL_WARN);  // Display warnings.
    static const uint32_t PL_INFO_MASK  = MASK(PL_INFO);  // Display informational info.
    static const uint32_t PL_DEBUG_MASK = MASK(PL_DEBUG); // Display debug info.
    static const uint32_t PL_DFLT_MASK  = PL_WARN_MASK;   // Default display level = warn.


    void SetTzOfst(int32_t v)         { m_Params.m_TzOfst = v; }
    void SetTzAbbrev(char *v)         { strncpy(m_Params.m_DstEndRule.abbrev, v, sizeof(m_Params.m_DstStartRule.abbrev) - 1); }
    void SetUseDst(bool v)            { m_Params.m_UseDst = v; }
    void SetDstOfst(int32_t v)        { m_Params.m_DstOfst = (v <= OFFSET_MID ? OFFSET_MIN : OFFSET_MAX); }
    void SetDstAbbrev(char *v)        { strncpy(m_Params.m_DstStartRule.abbrev, v, sizeof(m_Params.m_DstStartRule.abbrev) - 1); }
    void SetDstStartWk(uint32_t v)    { m_Params.m_DstStartRule.week = constrain(v, WK_MIN, WK_MAX); }
    void SetDstStartDow(uint32_t v)   { m_Params.m_DstStartRule.dow = constrain(v, DOW_MIN, DOW_MAX); }
    void SetDstStartMonth(uint32_t v) { m_Params.m_DstStartRule.month = constrain(v, MONTH_MIN, MONTH_MAX); }
    void SetDstStartHour(uint32_t v)  { m_Params.m_DstStartRule.hour = constrain(v, HOUR_MIN, HOUR_MAX); }
    void SetDstStartOfst(int32_t v)   { m_Params.m_DstStartRule.offset = v; }
    void SetDstEndWk(uint32_t v)      { m_Params.m_DstEndRule.week = constrain(v, WK_MIN, WK_MAX); }
    void SetDstEndDow(uint32_t v)     { m_Params.m_DstEndRule.dow = constrain(v, DOW_MIN, DOW_MAX); }
    void SetDstEndMonth(uint32_t v)   { m_Params.m_DstEndRule.month = constrain(v, MONTH_MIN, MONTH_MAX); }
    void SetDstEndHour(uint32_t v)    { m_Params.m_DstEndRule.hour = constrain(v, HOUR_MIN, HOUR_MAX); }
    void SetDstEndOfst(int32_t v)     { m_Params.m_DstEndRule.offset = v; }
    static const uint32_t MIN_NTP_UPDATE_SEC = 4;
    void SetMinNtpRate(uint32_t r)    { m_MinNtpRateSec = r >= MIN_NTP_UPDATE_SEC ? r : MIN_NTP_UPDATE_SEC; }
    void SetNtpAddr(char *v)          { strncpy(m_Params.m_NtpAddr, v, sizeof(m_Params.m_NtpAddr) - 1); }
    void SetNtpPort(uint32_t p)       { m_Params.m_NtpPort = constrain(p, 1, 65535); }
    void SetPrintLevel(uint32_t lvl)  { m_PrintLevel = MASK(lvl); }

protected:


private:
    /////////////////////////////////////////////////////////////////////////////
    // Private methods.
    /////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////////////////
    // Unimplemented methods.  We don't want users to try to use these.
    /////////////////////////////////////////////////////////////////////////////
    WiFiTimeManager(WiFiTimeManager &rCs);
    WiFiTimeManager &operator=(WiFiTimeManager &rCs);


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
    // UpdateWebPage()
    //
    // Update our custom Setup page based on current timezone, DST, and NTP settings.
    //
    /////////////////////////////////////////////////////////////////////////////
    void UpdateWebPage();


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
    // SendNtpPacket()
    //
    // Sends a time request packet via UDP to the NTP server.
    //
    /////////////////////////////////////////////////////////////////////////////
    void SendNtpPacket();


    /////////////////////////////////////////////////////////////////////////////
    // GetWebPage()
    //
    // Returns a pointer to the web page buffer.
    //
    /////////////////////////////////////////////////////////////////////////////
    char *GetWebPage() { return WebPageBuffer; }


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
    //   str   - String instance to print.
    /////////////////////////////////////////////////////////////////////////////
    void WTMPrint(uint32_t level, const char *fmt...) const;
    void WTMPrint(uint32_t level, String &str) const;


    /////////////////////////////////////////////////////////////////////////////
    // Private static constants.
    /////////////////////////////////////////////////////////////////////////////
    static const char    *pPrefSavedStateLabel;       // Labes for saving preferences.
    static const size_t   MAX_NVS_NAME_LEN;           // Maximum NVS preferences name length.
    static const size_t   MAX_JSON_SIZE     = 350;    // Max web pag JSON size.
    static const char    *m_pName;                    // Preferences name.
    static const int      NTP_PACKET_SIZE   = 48;     // NTP timestamp is in the
                                                      //    first 48 bytes of the message.
    static const int      UDP_TIMEOUT_MS    = 2000;   // Timeout in miliseconds to
                                                      //    wait for NTP packet to arrive.

    // Allocate enough space to buffer twice the size of our original web page.
    // This allows for the user to add HTML and/or java script if needed.
    // This is wasteful, but easy.  %%%jmc Possibly fix this in the future.
    static const size_t   MAX_WEB_PAGE_SIZE = 2 * sizeof(TZ_SELECT_STR) + MAX_JSON_SIZE;
    static char           WebPageBuffer[MAX_WEB_PAGE_SIZE];


    /////////////////////////////////////////////////////////////////////////////
    // Private instance data.
    /////////////////////////////////////////////////////////////////////////////
    const char    *m_pApName;             // Network name for AP.
    const char    *m_pApPassword;         // AP password.
    uint32_t       m_PrintLevel;          // Status print level selection.
    time_t         m_LastTime;            // Timestamp from the last NTP packet.
    WiFiUDP        m_Udp;                 // UDP instance for tx & rx of UDP packets.
    bool           m_UsingNetworkTime;    // True if using time from NTP server.
    TimeParameters m_Params;              // Timezone and DST data.
    Timezone       m_Timezone;            // Timezone class for TZ and DST change.
    uint32_t       m_MinNtpRateSec;       // Minimum seconds between NTP updates.
    TimeChangeRule *m_pLclTimeChangeRule; // Pointer to time change rule in use.
    std::function<void()> m_pSaveParamsCallback;
                                          // Pointer to save params callback.
    std::function<time_t()> m_pUtcGetCallback;
                                          // Pointer to get UTC callback.
    std::function<void(time_t t)> m_pUtcSetCallback; // Pointer to set UTC callback.
    std::function<void(String &rWebPage, uint32_t maxSize)> m_pUpdateWebPageCallback;
                                          // Pointer to update web page callback.
    uint8_t        m_PacketBuf[NTP_PACKET_SIZE];
                                          // Buffer to hold in & out packets.

}; // End class WiFiTimeManager.



#endif // TIMESETTINGS_H