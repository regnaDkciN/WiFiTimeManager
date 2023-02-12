/////////////////////////////////////////////////////////////////////////////////
// EditWebPage.ino
//
// This file contains example code demonstrating the use of the WiFiTimeManager
// library in the polled (non-blocking) mode.  It also demonstrates the ease
// of adding a user defined input field to the Setup page.
//
// A simple dummy data field is added to the Setup web page.
//
// In non-blocking mode, if a network connection cannot be made at power-up, the
// system starts the config portal and continues execution so that the user code
// can continue to execute while the config portal runs.
//
// The config portal creates a DNS web server with IP address of 192.168.4.1 .
// Any WiFi enabled device can use its web browser to access the web server and
// configure the WiFi credentials, timezone, DST start and end times, and
// NTP server information.  While the config portal is active, the system
// continues to execute any other user code as usual.  The WiFiTimeManager
// must be polled within the main loop() function in order to keep the config
// portal up to date.
//
// This example includes the following:
//   - A reset button connected to GPIO 14.  This button is used to either start
//     the config portal on a short press, or reset all state information
//     including WiFi credentials, timezone, DST, and NTP information.
//   - An LED connected to GPIO 12 that lights when NTP time is being used.
//   - An LED connected to GPIO 27 that lights when the local clock is supplying
//     time data (i.e. when NTP time cannot be retrieved from the net).
//   - A simple customer defined numeric input field is added to the Setup page.
//
// The latest version of WiFiTimeManager code with documentation and examples
// can be found on github at: https://github.com/regnaDkciN/WiFiTimeManager .
//
// History:
// - jmcorbett 12-FEB-2023
//   Updated per changes in WiFiTimeManager interface.
//
// - jmcorbett 019-JAN-2023 Original creation.
//
// Copyright (c) 2023, Joseph M. Corbett
//
/////////////////////////////////////////////////////////////////////////////////

#include <String>               // For String class.
#include <WiFiTimeManager.h>    // Manages timezone, DST, and NTP.

#define RESET_PIN       14      // GPIO pin for the reset button.
#define NTP_CLOCK_PIN   12      // GPIO pin for the NTP clock LED.
#define LOCAL_CLOCK_PIN 27      // GPIO pin for the local clock LED.

static WiFiTimeManager *gpWtm;  // Pointer to the WiFiTimeManager singleton instance.
static const bool SETUP_BUTTON  = true;
                                // Use a separate Setup button on the web page.
static const bool BLOCKING_MODE = false;
                                // Use non-blocking mode.
static const char *AP_NAME = "WiFi Clock Setup";
                                // AP name user will see as network device.
static const char *AP_PWD  = NULL;
                                // AP password.  NULL == no password.

// Example HTML code to demonstrate insertion of data fields on the Setup web page.
static const char EDIT_TEST_STR[] = R"(
    <br>
    <h3 style="display:inline">AN INTEGER VALUE:</h3>
    <input type="number" id="editTestNumber" name="editTestNumber" min="0" max="1000" value="0">
    <br>
)";


/////////////////////////////////////////////////////////////////////////////////
// The following functions are for demonstration and simply report entry.
// These are not normally needed, and are included only as an example of how to
// use the many callbacks.
/////////////////////////////////////////////////////////////////////////////////
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


/////////////////////////////////////////////////////////////////////////////////
// UpdateWebPageCallback()
//
// This callback is invoked when the web page needs to be updated.  This is
// demonstration code to show how one might add an entry to the Setup web page.
/////////////////////////////////////////////////////////////////////////////////
void UpdateWebPageCallback(String &rWebPage, uint32_t maxSize)
{
    Serial.println("UpdateWebPageCallback");
    // Replace the HTML END marker with our demonstration code.
    rWebPage.replace("<!-- HTML END -->", EDIT_TEST_STR);
} // End UpdateWebPageCallback().


/////////////////////////////////////////////////////////////////////////////////
// SaveParamsCallback()
//
// This callback is invoked when the user saves the Setup web page.  This is
// demonstration code to show how one might retrieve the value of an entry
// that was added via the UpdateWebPageCallback().
/////////////////////////////////////////////////////////////////////////////////
void SaveParamsCallback()
{
    Serial.println("UpdateWebPageCallback");
    // Get our field value from the WiFiTimeManager and display its value.
    Serial.printf("Integer Value = %d\n", gpWtm->GetParamInt("editTestNumber"));
} // End SaveParamsCallback().


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

    // The web page is updated in Init(), so setup our callbacks before
    // WiFiTimeManager::Init() is called.  In this case, we have added some
    // demonstration code above to illustrate how to add fields to the web page.
    gpWtm->SetUpdateWebPageCallback(UpdateWebPageCallback);
    gpWtm->SetSaveParamsCallback(SaveParamsCallback);

    // Initialize the WiFiTimeManager class with our AP and button selections.
    gpWtm->Init(AP_NAME, AP_PWD, SETUP_BUTTON);

    // Contact the NTP server no more than once per minute.
    gpWtm->SetMinNtpRateSec(60);

    // Setup some demo callbacks that simply report entry (not normally needed).
    gpWtm->setAPCallback(APCallback);
    gpWtm->setWebServerCallback(WebServerCallback);
    gpWtm->setConfigResetCallback(ConfigResetCallback);
    gpWtm->setSaveConfigCallback(SaveConfigCallback);
    gpWtm->setPreSaveConfigCallback(PreSaveConfigCallback);
    gpWtm->setPreSaveParamsCallback(PreSaveParamsCallback);
    gpWtm->setPreOtaUpdateCallback(PreOtaUpdateCallback);


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
    }
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
    if(!gpWtm->IsConnected())
    {
        // Avoid delays() in loop when non-blocking and other long running code.
        if (gpWtm->process())
        {
            // This is the place to do something when we transition from
            // unconnected to connected.  As an example, here we get the time.
            gpWtm->GetUtcTimeT();
        }
    }

    // Check and handle the reset button.
    CheckButton();

    // Read the time every 10 seconds.
    static uint32_t lastTime = millis();
    uint32_t thisTime = millis();
    static const uint32_t updateTime = 10000;   // 10 seconds between reading time
    if (thisTime - lastTime >= updateTime)
    {
        // Read the time and display the results.
        lastTime = thisTime;
        tm localTime;
        gpWtm->GetUtcTime(&localTime);
        gpWtm->PrintDateTime(&localTime);
        gpWtm->GetLocalTime(&localTime);
        gpWtm->PrintDateTime(&localTime);
        Serial.println();
    }

    // Update the LEDs.
    SetLeds(gpWtm->UsingNetworkTime());
    delay(1);

} // End loop().