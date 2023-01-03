/////////////////////////////////////////////////////////////////////////////////
// RTCExample.ino
//
// This file contains example code demonstrating how to use a DS3231 hardware
// real time clock (RTC) with the WiFiTimeManager library.  This example also
// demosnstrates use of the polled (non-blocking) use of WiFiTimeManager.
// All RTC specific example code is bracketed by:
//      #if defined USE_RTC
//         ... RTC specific code
//      #endif
//
// This setup includes the following:
//   - A button connected to GPIO 14.  This button is used to either start the
//     config portal on a short press, or reset all state information including
//     WiFi credentials, timezone, DST, and NTP information.
//   - An LED connected to GPIO 12 that lights when NTP time is being used.
//   - An LED connected to GPIO 27 that lights when the local clock is supplying
//     time data.
//   - A DS3231 RTC connected to the ESP32 I2C SCL and SDA pins.
//
// The latest version of WiFiTimeManager code with documentation and examples
// can be found on github at: https://github.com/regnaDkciN/WiFiTimeManager .
//
// History:
// - jmcorbett 03-JAN-2023 Original creation.
//
// Copyright (c) 2023, Joseph M. Corbett
//
/////////////////////////////////////////////////////////////////////////////////


// Comment out the following line if no RTC is connected.
#define USE_RTC 1

#include <String>               // For String class.

#if defined USE_RTC
    #include <DS323x_Generic.h> // https://github.com/khoih-prog/DS323x_Generic
#endif

#include "WiFiTimeManager.h"    // Manages timezone and dst.

#define RESET_PIN       14      // GPIO pin for the reset button.
#define NTP_CLOCK_PIN   12      // GPIO pin for the NTP clock LED.
#define LOCAL_CLOCK_PIN 27      // GPIO pin for the local clock LED.

#if defined USE_RTC
    static DS323x gRtc;         // The DS3231 instance.
#endif
static WiFiTimeManager *gpWtm;  // Pointer to the WiFiTimeManager singleton instance.
static bool WmSeparateButton = true;
                                // Use a separate Setup button on the web page.
static bool WmBlocking = false; // Use non-blocking mode.
static const char *AP_NAME = "WiFi Clock Setup";
                                // AP name user will see as network device.
// Example HTML code to demonstrate insertion of data fields on the Setup web page.
static const char EDIT_TEST_STR[] = R"(
    <br>
    <h3 style="display:inline">AN INTEGER VALUE:</h3>
    <input type="number" id="editTestNumber" name="editTestNumber" min="1" max="1000" value="0">
    <br>
)";

// The following functions are simply for demonstration and simply report entry.
// These are not needed, and are included only as an example of how to use
// the many callbacks.
void APCallback(WiFiManager *)
{
    Serial.println("APCallback");
}

void WebServerCallback()
{
    Serial.println("WebServerCallback");
}

void ConfigResetCallback()
{
    Serial.println("ConfigResetCallback");
}

void SaveConfigCallback()
{
    Serial.println("SaveConfigCallback");
}

void PreSaveConfigCallback()
{
    Serial.println("PreSaveConfigCallback");
}

void PreSaveParamsCallback()
{
    Serial.println("PreSaveParamsCallback");
}

void PreOtaUpdateCallback()
{
    Serial.println("PreOtaUpdateCallback");
}

// Special RTC code.
#if defined USE_RTC
    // This callback is invoked when NTP time is not called, and is used to
    // supply current time_t time.  In this case we read the current time from
    // the RTC, convert it to time_t, and return it.
    time_t UtcGetCallback()
    {
        Serial.println("UtcGetCallback");
        DateTime t = gRtc.now();
        return t.get_time_t();
    }

    // This callback is invoked after NTP time is received from the NTP server.
    // It converts the time_t value to a DateTime value and sets the RTC.
    void UtcSetCallback(time_t t)
    {
        Serial.println("UtcSetCallback");

        // Push the new time to the RTC.
        gRtc.now(DateTime(t));

        // Reset the RTC stop flag to inidcate that the RTC time is valid.
        gRtc.oscillatorStopFlag(false);
    }
#endif

// This callback is invoked when the web page needs to be updated.  This is
// demonstration code to show how one might add an entry to the Setup web page.
void UpdateWebPageCallback(String &rWebPage, uint32_t maxSize)
{
    Serial.println("UpdateWebPageCallback");
    rWebPage.replace("<!-- HTML END -->", EDIT_TEST_STR);
}

// This callback is invoked when the user saves the Setup web page.  This is
// demonstration code to show how one might retrieve the value of an entry
// that was added via the UpdateWebPageCallback().
void SaveParamsCallback()
{
    Serial.println("UpdateWebPageCallback");
    Serial.printf("Integer Value = %d\n", gpWtm->GetParamInt("editTestNumber"));
}

// This function checks the reset button.  If pressed for a long time
// (about 3.5 seconds), it will reset all of our WiFi credentials as well as
// all timezone, DST, and NTP data then resets the processof.
// If pressed for a short time and the network is not connected, it will start
// the config portal.
void checkButton()
{
    // check for button press
    if ( digitalRead(RESET_PIN) == LOW )
    {
        // Poor mans debounce/press-hold, code not ideal for production.
        delay(50);
        if( digitalRead(RESET_PIN) == LOW )
        {
            Serial.println("Button Pressed");
            // Still holding button for 3s, reset settings and restart.
            delay(3000); // reset delay hold
            if( digitalRead(RESET_PIN) == LOW )
            {
                Serial.println("Button Held");
                Serial.println("Erasing Config, restarting");
                gpWtm->ResetData();
                ESP.restart();
            }

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
}


// Lights one of the clock LEDs based on the input value.  If v is true, then
// the NTP LED will be lit and the local LED will be off.  Otherwise, the
// local LED will be lit and the NTP LED will be off.
void setLeds(bool v)
{
    digitalWrite(v ? LOCAL_CLOCK_PIN : NTP_CLOCK_PIN, LOW);
    digitalWrite(v ? NTP_CLOCK_PIN : LOCAL_CLOCK_PIN, HIGH);
}


// The Arduino setup() function.  This function initializes all the hardware
// and WiFiTimeManager class.
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

#if defined USE_RTC
    // Initialize I2C for RTC.
    Wire.begin();
    gRtc.attach(Wire);

    // If the RTC is uninitialized, then set a default time (the start of 2023).
    if (gRtc.oscillatorStopFlag())
    {
        Serial.println("RTC uninitialized.");
        gRtc.now(DateTime(2023, 1, 1, 0, 0, 0));
        gRtc.oscillatorStopFlag(false);
    }
#endif

    // The web page is updated in init, so setup our callback before
    // WiFiTimeManager::Init() is called.  In this case, we have added some
    // demonstration code above to illustrate how to add fields to the web page.
    gpWtm->SetUpdateWebPageCallback(UpdateWebPageCallback);

    // Initialize the WiFiTimeManager class with our AP name and button selections.
    gpWtm->Init(AP_NAME, WmSeparateButton);

    // Contact the NTP server no more than once per minute.
    gpWtm->SetMinNtpRate(60);

    // Setup some demo callbacks that simply report entry.
    gpWtm->setAPCallback(APCallback);
    gpWtm->setWebServerCallback(WebServerCallback);
    gpWtm->setConfigResetCallback(ConfigResetCallback);
    gpWtm->setSaveConfigCallback(SaveConfigCallback);
    gpWtm->setPreSaveConfigCallback(PreSaveConfigCallback);
    gpWtm->setPreSaveParamsCallback(PreSaveParamsCallback);
    gpWtm->setPreOtaUpdateCallback(PreOtaUpdateCallback);

    // For demo purposes, use a save parameters callback to fetch our added
    // web page data.
    gpWtm->setSaveParamsCallback(SaveParamsCallback);

#if defined USE_RTC
    gpWtm->SetUtcGetCallback(UtcGetCallback);
    gpWtm->SetUtcSetCallback(UtcSetCallback);
#endif

    // Attempt to connect to the network in non-blocking mode.
    gpWtm->setConfigPortalBlocking(WmBlocking);
    gpWtm->setConfigPortalTimeout(0);
    if(!gpWtm->autoConnect(AP_NAME))
    {
        Serial.println("Failed to connect or hit timeout");
    }
    else
    {
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
        gpWtm->GetUtcTime();
    }
} // End setup().


// The Arduino loop() function.  Simply polls the WiFiTimeManager if we are not
// already connected to the WiFi.  On a transition of the WiFi being connected,
// we simply get the UTC time.  We also check the reset button, and as a
// demonstration, get UTC and local time from the WiFiTimeManager and display
// the results.
void loop()
{
    if(!gpWtm->IsConnected())
    {
        // Avoid delays() in loop when non-blocking and other long running code
        if (gpWtm->process())
        {
            // This is the place to do something when we transition from
            // unconnected to connected.  As an example, here we get the time.
            gpWtm->GetUtcTime();
        }
    }

    // Check the reset button.
    checkButton();

    // Read teh time every 10 seconds.
    static uint32_t lastTime = millis();
    uint32_t thisTime = millis();
    static const uint32_t updateTime = 10000;   // 10 seconds between reading time.
    if (thisTime - lastTime >= updateTime)
    {
        // Read the time and display the resulats.
        lastTime = thisTime;
        time_t utcTime = gpWtm->GetUtcTime();
        time_t lclTime = gpWtm->GetLocalTime();
        gpWtm->PrintDateTime(utcTime, "UTC");
        gpWtm->PrintDateTime(lclTime, gpWtm->GetLocalTimezoneString());
        Serial.println();
    }

    // Update the LEDs.
    setLeds(gpWtm->UsingNetworkTime());
    delay(1);

}