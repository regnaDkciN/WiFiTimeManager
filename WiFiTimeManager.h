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
// - jmcorbett 10-FEB-2023
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
//
// - jmcorbett 19-JAN-2023 Original creation.
//
// Copyright (c) 2023, Joseph M. Corbett
//
/////////////////////////////////////////////////////////////////////////////////

#if !defined WIFITIMEMANAGER_H
#define WIFITIMEMANAGER_H

#include <WiFiManager.h>        // Manage connection. https://github.com/tzapu/WiFiManager
#include <string.h>             // For strncpy().
#include <esp_sntp.h>           // For ESP32 SNTP library.
#include "WebPages.h"          // To define MAX_WEB_PAGE_SIZE which depends onTZ_SELECT_STR.


/////////////////////////////////////////////////////////////////////////////////
// Define some enums that help with DST configuration:
// - DayOfWeek_t   enumerates the days of the week.
// - WeekOfMonth_t enumerates the weeks of the month.
// - Month_t       enumerates the months of the year.
/////////////////////////////////////////////////////////////////////////////////

enum DayOfWeek_t
{
    dowSun = 0, dowMon, dowTue, dowWed, dowThu, dowFri, dowSat
};

enum WeekOfMonth_t
{
    wkFirst = 1, wkSecond, wkThird, wkFour, wkLast
};

enum Month_t
{
    mJan = 1, mFeb, mMar, mApr, mMay, mJun, mJul, mAug, mSep, mOct, mNov, mDec
};


/////////////////////////////////////////////////////////////////////////////////
// TimeChangeInfo
//
// Structure that is used to specify start and end dates for DST.
/////////////////////////////////////////////////////////////////////////////////
struct TimeChangeInfo
{
    char    abbrev[6];    // Five chars max.
    uint8_t week;         // wkFirst, wkSecond, wkThird, wkFour, or wkLast.
    uint8_t dow;          // day of week, 0 = Sun, 1 = Mon, ... 6 = Sat.
    uint8_t month;        // 1=Jan, 2=Feb, ... 12=Dec
    uint8_t hour;         // 0-23
    int     offset;       // offset from UTC in minutes
};


//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Default timezone and DST values.  CHANGE THESE AS DESIRED.
// These are used only on the first startup of the program.  After that,
// (possibly) updated values are stored in and recovered from NVS on each
// startup.  The TP_VERSION value should be bumped any time a change is made
// to the structure.
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
static const uint32_t TP_VERSION           = 7;       // Struct version.  Bump on changes.
static const int32_t  DFLT_TZ_OFST         = -300;    // Eastern time (5 hrs behind).
static const char    *DFLT_TZ_ABBREV       = "EST";   // DST end rule abbreviation.
static const bool     DFLT_USE_DST         = true;    // true to use DST.
static const int32_t  DFLT_DST_OFST        = 60;      // 30 or 60 minute DST offset.
static const char    *DFLT_DST_START_ABBREV= "EDT";   // DST start rule abbreviation.
static const uint32_t DFLT_DST_START_WK    = wkSecond;// DST starts 2nd
static const uint32_t DFLT_DST_START_DOW   = dowSun;  //    Sunday
static const uint32_t DFLT_DST_START_MONTH = mMar;    //    of March
static const uint32_t DFLT_DST_START_HOUR  = 2;       //    at 2 AM.
static const uint32_t DFLT_DST_END_WK      = wkFirst; // DST ends 1st
static const uint32_t DFLT_DST_END_DOW     = dowSun;  //    Sunday
static const uint32_t DFLT_DST_END_MONTH   = mNov;    //    of November
static const uint32_t DFLT_DST_END_HOUR    = 2;       //    at 2 AM.
static const char    *DFLT_NTP_ADDR        = "time.nist.gov";
                                                      // NTP server


/////////////////////////////////////////////////////////////////////////////////
// TimeParameters structure
//
// Holds timezone offset and DST start/end times, and NTP address.
//
/////////////////////////////////////////////////////////////////////////////////
struct TimeParameters
{
    // Constructor.
    TimeParameters() :
        m_Version(TP_VERSION), m_TzOfst(DFLT_TZ_OFST), m_UseDst(DFLT_USE_DST),
        m_DstOfst(DFLT_DST_OFST),
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

    static const size_t MAX_NTP_ADDR = 26; // Maximum size of the NTP address string.

    // Time related fields.
    uint32_t       m_Version;              // Struct version.  Bump on changes.
    int32_t        m_TzOfst;               // Timezone offset in minutes.
    bool           m_UseDst;               // true to use DST.
    int32_t        m_DstOfst;              // 30 or 60 minute DST offset.
    char           m_NtpAddr[MAX_NTP_ADDR];// NTP server address.
    TimeChangeInfo m_DstStartRule;         // Rule for starting DST.
    TimeChangeInfo m_DstEndRule;           // Rule for ending DST.

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
    // NOTE: The time will be updated immediately after a new connection is
    //       established.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool process();


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
    boolean autoConnect();
    boolean autoConnect(char const *pApName, char const *pApPassword = NULL);


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
    void SetUtcGetCallback(std::function<time_t()> func)
        { m_pUtcGetCallback = func; }


    /////////////////////////////////////////////////////////////////////////////
    // SetUtcSetCallback()
    //
    // Sets a callback that will be invoked when a NTP UTC time update has been
    // received.  Can be used to set the time of a hardware real time clock or
    // other time source.
    //
    // Arguments:
    //   func - Pointer to the function to be called to set UTC time.  The
    //          function takes a time_t argument which specifies the number of
    //          seconds since January 1, 1970 (Unix time).
    //
    /////////////////////////////////////////////////////////////////////////////
    void SetUtcSetCallback(std::function<void(time_t t)> func)
        { m_pUtcSetCallback = func; }


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
    //       maximum size of the String contained in rWebPage.
    //
    //       Upon return from the callback, the contents of the rWebPage will be
    //       safely copied into a buffer that will be sent to the client.  If
    //       the size of the (modified) rWebPage exceeds the value of maxSize,
    //       the buffer will be truncated, so be careful.
    //
    //       Several comments appear within the string that
    //       may be employed to locate specific places within the standard
    //       web page.  The user can use these comments to insert relevant
    //       HTML or javascript code as needed.  These comments are:
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
    //              the submit (Save) button is pressed.  It can be used to
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
    // WiFi credential information is not affected.
    //
    // Returns:
    //    Returns 'true' if successful, or 'false' otherwise.
    //
    /////////////////////////////////////////////////////////////////////////////
    bool Reset();


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
    //           is optional, and defaults to false.
    //
    // Returns:
    //   Always returns the current UTC time as kept by the internal Unix time
    //   library or the user specified device if present.
    //
    /////////////////////////////////////////////////////////////////////////////
    time_t GetUtcTimeT(bool force = false);


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
    tm *GetUtcTime(tm *pTm);


    /////////////////////////////////////////////////////////////////////////////
    // GetLocalTime()
    //
    // Returns the best known value for local time.  Converts the best known
    // UTC time to local time and returns its value.  See GetUtcTime().
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
    tm *GetLocalTime(tm *pTm);


    /////////////////////////////////////////////////////////////////////////////
    // GetDateTimeString
    //
    // Format and print a UNIX time_t value.  Uses strftime() to do the conversion.
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
    int GetDateTimeString(char *pBuf, int size, tm *pTime);


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
    void PrintDateTime(tm *pTime);


    /////////////////////////////////////////////////////////////////////////////
    // GetLocalTimezoneString()
    //
    // Returns a pointer to the currently active timezone string, or NULL on error
    // (e.g. "EST").
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
    // Update our DST timezone rules.  Should be called any time timezone or DST
    // data changes.
    //
    /////////////////////////////////////////////////////////////////////////////
    void UpdateTimezoneRules();


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
    bool SetMinNtpRateSec(uint32_t rate);


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
    bool     UsingNetworkTime() const { return m_UsingNetworkTime; }
    uint32_t GetMinNtpRateSec() const { return m_MinNtpRateMs; }

    // Min and max constants for selectable fields.
    static const uint32_t WK_MIN     = wkFirst; // First
    static const uint32_t WK_MAX     = wkLast;  // Last
    static const uint32_t DOW_MIN    = dowSun;  // Sunday
    static const uint32_t DOW_MAX    = dowSat;  // Saturday
    static const uint32_t MONTH_MIN  = mJan;    // January
    static const uint32_t MONTH_MAX  = mDec;    // December
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
    void SetNtpAddr(char *v)          { strncpy(m_Params.m_NtpAddr, v, sizeof(m_Params.m_NtpAddr) - 1); }
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
    // InitSntpTime()
    //
    // Initializes the SNTP code with NTP address, update rate, timezone info,
    // and force an NTP sync.
    //
    /////////////////////////////////////////////////////////////////////////////
    void InitSntpTime();


    /////////////////////////////////////////////////////////////////////////////
    // StartNewConnection()
    //
    // This method is called on a transition from WiFi not connected to WiFi
    // connected.  It initializes SNTP, updates timezone rules, and reads UTC
    // time to prime the internal clock.
    //
    /////////////////////////////////////////////////////////////////////////////
    void StartNewConnection();


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
    char *GetTimezoneString(char *pBuffer, size_t bufSize);


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
    static void UtcSetCallback(timeval *pTv);


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
    // Print the specified data if the selected level matches the m_PrintLevel
    // that was set via SetPrintLevel().  Note that this method is overloaded
    // to work with NULL terminated strings, and String objects.
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
    static const char    *pPrefSavedStateLabel;       // Label for saving preferences.
    static const size_t   MAX_NVS_NAME_LEN;           // Maximum NVS preferences name length.
    static const size_t   MAX_JSON_SIZE     = 350;    // Max web page JSON size.
    static const char    *m_pName;                    // Preferences name.
    static const uint32_t MIN_NTP_UPDATE_MS = 15000;  // Minimum NTP update rate (milliseconds).

    // Allocate enough space to buffer twice the size of our original web page.
    // This allows for the user to add HTML and/or java script if needed.
    // This is wasteful, but easy.  %%%jmc Possibly fix this in the future.
    static const size_t   MAX_WEB_PAGE_SIZE = 2 * sizeof(TZ_SELECT_STR) + MAX_JSON_SIZE;
    static char           WebPageBuffer[MAX_WEB_PAGE_SIZE];
    static volatile bool  m_UsingNetworkTime;


    /////////////////////////////////////////////////////////////////////////////
    // Private instance data.
    /////////////////////////////////////////////////////////////////////////////
    const char    *m_pApName;             // Network name for AP.
    const char    *m_pApPassword;         // AP password.
    uint32_t       m_PrintLevel;          // Status print level selection.
    TimeParameters m_Params;              // Timezone and DST data.
    uint32_t       m_MinNtpRateMs;        // Minimum milliseconds between NTP updates.
    uint32_t       m_LastUpdateMs;        // Last ms time of NTP update.
    std::function<void()> m_pSaveParamsCallback;
                                          // Pointer to save params callback.
    std::function<time_t()> m_pUtcGetCallback;
                                          // Pointer to get UTC callback.
    std::function<void(time_t t)> m_pUtcSetCallback; // Pointer to set UTC callback.
    std::function<void(String &rWebPage, uint32_t maxSize)> m_pUpdateWebPageCallback;
                                          // Pointer to update web page callback.

}; // End class WiFiTimeManager.


#endif // WIFITIMEMANAGER_H