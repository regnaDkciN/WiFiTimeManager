/////////////////////////////////////////////////////////////////////////////////
// DstTest.ino
//
// This file tests the DST related time change properties of the WiFiTimeManager
// library ( https://github.com/regnaDkciN/WiFiTimeManager ).  It exercises the
// DST related services as follows:
//   1. Initialize the WiFiTimeManager to the Cleveland, Ohio timezone.
//   2. Display a few local time values to verify that the time is correct.
//   3. Change the system time to just before DST start and verify that time
//      changes from 1:59:59 EST to 3:00:00 EDT at the correct time.
//   4. Change the system time to just before DST end and verify that time
//      changes from 1:59:59 EDT to 1:00:00 EST at the correct time.
//   5. Change the system time to the start of 2023 and display each second
//      tick in order to be able to observe how long it takes to perform an
//      NTP update and return to the correct local time.
//
// This code was based on code by Hardy Maxa with details at:
//   https://RandomNerdTutorials.com/esp32-ntp-timezones-daylight-saving/ .
//
// This example also includes the following:
//   - A reset button connected to GPIO 14.  This button is used to either start
//     the config portal on a short press, or reset all state information
//     including WiFi credentials, timezone, DST, and NTP information.
//   - An LED connected to GPIO 12 that lights when NTP time is being used.
//   - An LED connected to GPIO 27 that lights when the local clock is supplying
//     time data.
//   - A DS3231 RTC connected to the ESP32 I2C SCL and SDA pins.
//
// The latest version of WiFiTimeManager code with documentation and examples
// can be found on github at: https://github.com/regnaDkciN/WiFiTimeManager .
//
// History:
// - jmcorbett 12-FEB-2023 Original creation.
//
// Copyright (c) 2023, Joseph M. Corbett
/////////////////////////////////////////////////////////////////////////////////


#include <String>               // For String class.
#include <WiFiTimeManager.h>    // Manages timezone, DST, and NTP.

#define RESET_PIN       14      // GPIO pin for the reset button.
#define NTP_CLOCK_PIN   12      // GPIO pin for the NTP clock LED.
#define LOCAL_CLOCK_PIN 27      // GPIO pin for the local clock LED.

static WiFiTimeManager *gpWtm;  // Pointer to the WiFiTimeManager singleton instance.
static const bool SETUP_BUTTON  = true;
                                // Use a separate Setup button on the web page.
static const bool BLOCKING_MODE = true; // Use blocking mode.
static const char *AP_NAME = "WiFi Clock Setup";
                                // AP name user will see as network device.
static const char *AP_PWD  = NULL;
                                // AP password.  NULL == no password.


/////////////////////////////////////////////////////////////////////////////////
// SetTime()
//
// This function sets the current sysstem time based on the input arguments.
// The arguments should be familiar, representing year, month, day of month,
// hour, minute, and second of new time.  isDst is true if DST is currently
// in effect.
/////////////////////////////////////////////////////////////////////////////////
void SetTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst)
{
    struct tm tm;

    tm.tm_year = yr - 1900;   // Set date
    tm.tm_mon = month - 1;
    tm.tm_mday = mday;
    tm.tm_hour = hr;      // Set time
    tm.tm_min = minute;
    tm.tm_sec = sec;
    tm.tm_isdst = isDst;  // 1 or 0
    time_t t = mktime(&tm);
    Serial.printf("Setting time: %s", asctime(&tm));
    struct timeval tNow = { .tv_sec = t };
    settimeofday(&tNow, NULL);
} // End SetTime().


/////////////////////////////////////////////////////////////////////////////////
// CompareTimes()
//
// Compares two dates/times.
//
// Arguments:
//   - yr, month, mday, hr, minute, sec, isDst : Data for first time to compare.
//   - pTm : Pointer to tm struct containing second time to compare.
//
// Returns:
// Returns true if times match, false otherwise.
//
/////////////////////////////////////////////////////////////////////////////////
bool CompareTimes(int yr, int month, int mday, int hr, int minute, int sec,
                  int isDst, tm *pTm)
{
    if ((pTm->tm_year != yr - 1900) ||
        (pTm->tm_mon != month-1) ||
        (pTm->tm_mday != mday) ||
        (pTm->tm_hour != hr) ||
        (pTm->tm_min != minute) ||
        (pTm->tm_sec != sec) ||
        (pTm->tm_isdst != isDst))
    {
        Serial.println("*** TIME MISMATCH ***");
        return false;
    }
    return true;
} // End CompareTimes().


/////////////////////////////////////////////////////////////////////////////////
// SetTimezone()
//
// Sets the timezone based on the input TZ string.  pTimezone is a pointer to
// the NULL terminated timezone string.  See
// https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// for details of forming the timezone string.
/////////////////////////////////////////////////////////////////////////////////
void SetTimezone(const char *pTimezone)
{
    Serial.printf("  Setting Timezone to %s\n", pTimezone);
    // Adjust the TZ.  Clock settings are adjusted to show the new local time.
    setenv("TZ", pTimezone, 1);
    tzset();
} // End SetTimezone().


/////////////////////////////////////////////////////////////////////////////////
// CheckButton()
//
// This function checks the reset button.  If pressed for a long time
// (about 3.5 seconds), it will reset all of our WiFi credentials as well as
// all timezone, DST, and NTP data then resets the processor.
// If pressed for a short time and the network is not connected, it will start
// the config portal.
/////////////////////////////////////////////////////////////////////////////////
void CheckButton()
{
    // Check for a button press.
    if ( digitalRead(RESET_PIN) == LOW )
    {
        // Poor mans debounce/press-hold, code not ideal for production.
        delay(50);
        if( digitalRead(RESET_PIN) == LOW )
        {
            Serial.println("Button Pressed");
            // Still holding button for 3s, reset settings and restart.
            delay(3000); // Reset delay hold.
            if( digitalRead(RESET_PIN) == LOW )
            {
                Serial.println("Button Held");
                Serial.println("Erasing Config, restarting");
                gpWtm->ResetData();
                ESP.restart();
            }

ESP.restart(); // For development only.
            // Short press, start the config portal with a delay.
            if (!gpWtm->IsConnected())
            {
                Serial.println("Starting config portal");
                gpWtm->setConfigPortalBlocking(false);
                gpWtm->setConfigPortalTimeout(0);
                gpWtm->startConfigPortal(AP_NAME);
            }
        }
    }
} // End CheckButton().


/////////////////////////////////////////////////////////////////////////////////
// SetLeds()
//
// Lights one of the clock LEDs based on the input value.  If v is true, then
// the NTP LED will be lit and the local LED will be off.  Otherwise, the
// local LED will be lit and the NTP LED will be off.
/////////////////////////////////////////////////////////////////////////////////
void SetLeds(bool v)
{
    digitalWrite(v ? LOCAL_CLOCK_PIN : NTP_CLOCK_PIN, LOW);
    digitalWrite(v ? NTP_CLOCK_PIN : LOCAL_CLOCK_PIN, HIGH);
} // End SetLeds().


/////////////////////////////////////////////////////////////////////////////////
// setup()
//
// The Arduino setup() function.  This function initializes all the hardware
// and WiFiTimeManager class.
/////////////////////////////////////////////////////////////////////////////////
void setup()
{
    // Get the Serial class ready for use.
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(1000);
    Serial.println("\n Starting");

    // Set up our GPIO devices.
    pinMode(RESET_PIN, INPUT_PULLUP);
    pinMode(NTP_CLOCK_PIN, OUTPUT);
    digitalWrite(NTP_CLOCK_PIN, LOW);
    pinMode(LOCAL_CLOCK_PIN, OUTPUT);
    digitalWrite(LOCAL_CLOCK_PIN, LOW);

    // Cycle the LED at power up just to show that they work.
    digitalWrite(LOCAL_CLOCK_PIN, HIGH);
    delay(1000);
    digitalWrite(LOCAL_CLOCK_PIN, LOW);
    digitalWrite(NTP_CLOCK_PIN, HIGH);
    delay(1000);
    digitalWrite(NTP_CLOCK_PIN, LOW);

    // Init a pointer to our WiFiTimeManager instance.
    // This should be done before RTC init since the WiFiTimeManager
    // gets created on the first call to WiFiManager::Instance() and it
    // initializes a default time that the RTC may want to override.
    gpWtm = WiFiTimeManager::Instance();

    // Initialize the WiFiTimeManager class with our AP and button selections.
    gpWtm->Init(AP_NAME, AP_PWD, SETUP_BUTTON);

    // Contact the NTP server no more than once per minute.
    gpWtm->SetMinNtpRateSec(90);

    // Attempt to connect to the network in non-blocking mode.
    gpWtm->setConfigPortalBlocking(BLOCKING_MODE);
    gpWtm->setConfigPortalTimeout(0);
    if(!gpWtm->autoConnect())
    {
        Serial.println("Failed to connect or hit timeout");
    }
    else
    {
        // If we get here you have connected to the WiFi.
        Serial.println("connected...yeey :)");
        gpWtm->GetUtcTimeT();
    }

    // Set the timezone to Cleveland, Ohio time.
    SetTimezone("EST+5:0EDT+4:0,M3.2.0/2,M11.1.0/2");
} // End setup().


/////////////////////////////////////////////////////////////////////////////////
// loop()
//
// The Arduino loop() function.  Simply polls the WiFiTimeManager if we are not
// already connected to the WiFi.  On a transition of the WiFi being connected,
// we simply get the UTC time.  We also check the reset button, and as a
// demonstration, periodically get UTC and local time from the WiFiTimeManager
// and display the results.
/////////////////////////////////////////////////////////////////////////////////
void loop()
{
    // Check and handle the reset button.
    CheckButton();

    // Update the LEDs.
    SetLeds(gpWtm->UsingNetworkTime());

    static bool initializing = true;
    tm localTime;
    if (initializing)
    {
        uint32_t i = 0;
        Serial.println("Lets show the local time for a bit.  Starting with TZ set for Cleveland, Ohio");
        for (i = 0; i < 10; i++)
        {
            delay(1000);
            gpWtm->GetLocalTime(&localTime);
            gpWtm->PrintDateTime(&localTime);
        }

        // Set it to 5 seconds before daylight savings comes in.
        // Note: isDst = 0 to indicate that the time we set is not in DST.
        Serial.println("Now change the time.  5 sec before DST should start. (2nd Sunday of March)");
        SetTime(2023, 3, 12, 1, 59, 55, 0);
        for (i = 0; i < 10; i++)
        {
            delay(1000);
            gpWtm->GetLocalTime(&localTime);
            gpWtm->PrintDateTime(&localTime);
            // Check to make sure that time jumped forward by 1 hour.
            if (i == 5)
            {
                CompareTimes(2023, 3, 12, 3, 0, 0, 1, &localTime);
            }
        }


        // Set it to 5 seconds before daylight savings ends.
        // Not: isDst = 1 to indicate that the time we set is in DST.
        Serial.println("Now change the time.  10 sec before DST should finish. (1st Sunday of November");
        SetTime(2023, 11, 5, 1, 59, 55, 1);
        for (i = 0; i < 10; i++)
        {
            delay(1000);
            gpWtm->GetLocalTime(&localTime);
            gpWtm->PrintDateTime(&localTime);
            // Check to make sure that time jumped back by 1 hour.
            if (i == 5)
            {
                CompareTimes(2023, 11, 5, 1, 0, 0, 0, &localTime);
            }
        }

        // Set UTC to known value - Jan 1, 2023 00:00:00 (1672531200)
        // https://iotespresso.com/esp32-arduino-time-operations/
        // Set for Cleveland, Ohio EST/EDT
        Serial.println("Now change the time to Jan 1, 2023 00:00:00 (UTC)");
        struct timeval tv_ts = {.tv_sec = 1672531200};
        settimeofday(&tv_ts, NULL);

        // Now lets watch the time and see how long it takes for NTP to fix the clock
        Serial.println("Waiting for NTP update (expect in about 90 seconds)");
        initializing = false;
    }

    gpWtm->GetLocalTime(&localTime);
    gpWtm->PrintDateTime(&localTime);

    delay(1000);

} // End loop().

