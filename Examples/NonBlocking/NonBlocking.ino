/////////////////////////////////////////////////////////////////////////////////
// NonBlocking.ino
//
// This file contains example code demonstrating the use of the WiFiTimeManager
// library in the polled (non-blocking) mode.
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

#include <WiFiTimeManager.h>    // Manages timezone and dst.

#define RESET_PIN       14      // GPIO pin for the reset button.
#define NTP_CLOCK_PIN   12      // GPIO pin for the NTP clock LED.
#define LOCAL_CLOCK_PIN 27      // GPIO pin for the blocking LED.

static WiFiTimeManager *gpWtm;  // Pointer to the WiFiTimeManager singleton instance.
static const bool SETUP_BUTTON = true;
                                // Use a separate Setup button on the web page.
static const bool BLOCKING_MODE = false;
                                // Use non-blocking mode.
static const char *AP_NAME = "WiFi Clock Setup";
                                // AP name user will see as network device.


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
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(1000);
    Serial.println("\n Starting");

    pinMode(RESET_PIN, INPUT_PULLUP);
    pinMode(NTP_CLOCK_PIN, OUTPUT);
    digitalWrite(NTP_CLOCK_PIN, LOW);
    pinMode(LOCAL_CLOCK_PIN, OUTPUT);
    digitalWrite(LOCAL_CLOCK_PIN, LOW);

    // Test LEDs
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

    // Initialize the WiFiTimeManager class with our AP name and button selections.
    gpWtm->Init(AP_NAME, SETUP_BUTTON);

    // Contact the NTP server no more than once per minute.
    gpWtm->SetMinNtpRate(60);

    // Attempt to connect to the network.
    gpWtm->setConfigPortalBlocking(BLOCKING_MODE);
    gpWtm->setConfigPortalTimeout(0);
    if(!gpWtm->autoConnect(AP_NAME))
    {
        Serial.println("Failed to connect or hit timeout");
    }
    else
    {
        //if you get here you have connected to the WiFi
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
            gpWtm->GetUtcTime();
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
        time_t utcTime = gpWtm->GetUtcTime();
        time_t lclTime = gpWtm->GetLocalTime();
        gpWtm->PrintDateTime(utcTime, "UTC");
        gpWtm->PrintDateTime(lclTime, gpWtm->GetLocalTimezoneString());
        Serial.println();
    }

    // Update the LEDs.
    SetLeds(gpWtm->UsingNetworkTime());
    delay(1);

} // End loop().
