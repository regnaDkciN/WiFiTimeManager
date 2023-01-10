/////////////////////////////////////////////////////////////////////////////////
// Blocking.ino
//
// This file contains example code demonstrating the use of the WiFiTimeManager
// in blocking mode.
//
// In blocking mode, if a network connection cannot be made at power-up, the
// system starts the config portal and blocks until a WiFi connection is made.
// The config portal creates a DNS web server with IP address of 192.168.4.1 .
// Any WiFi enabled device can use its web browser to access the web server and
// configure the WiFi credentials, timezone, DST start and end times, and
// NTP server information.  While the config portal is active, the system is
// blocked from performing any other tasks.  Once the WiFi connection succeeds,
// the accurate NTP time is set as the system time..
//
// This example includes the following:
//   - A reset button connected to GPIO 14.  This button is used to either start
//     the config portal on a short press, or reset all state information
//     including WiFi credentials, timezone, DST, and NTP information.
//   - An LED connected to GPIO 12 that lights when NTP time is being used.
//   - An LED connected to GPIO 27 that lights the config portal is active.
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

#define RESET_PIN     14        // GPIO pin for the reset button.
#define NET_CLOCK_PIN 12        // GPIO pin for the NTP clock LED.
#define BLOCKING_PIN  27        // GPIO pin for the blocking LED.

static WiFiTimeManager *gpWtm;  // Pointer to the WiFiTimeManager singleton instance.
static const bool SETUP_BUTTON = true;
                                // Use a separate Setup button on the web page.
static const bool BLOCKING_MODE = true;
                                // Use blocking mode.
static const char *AP_NAME = "WiFi Clock Setup";
                                // AP name user will see as network device.
static const char *AP_PWD  = NULL;
                                // AP password.  NULL == no password.


/////////////////////////////////////////////////////////////////////////////////
// CheckButton()
//
// This function checks the reset button.  If pressed for a long time
// (about 3.5 seconds), it will reset all of our WiFi credentials as well as
// all timezone, DST, and NTP data then resets the processor.
/////////////////////////////////////////////////////////////////////////////////
void CheckButton()
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
        }
    }
} // End CheckButton().


/////////////////////////////////////////////////////////////////////////////////
// SetLeds()
//
// Lights one of the clock LEDs based on the input value.  If v is true, then
// the NTP LED will be lit and the local LED will be off.  Otherwise, the
// blocking LED will be lit and the NTP LED will be off.
/////////////////////////////////////////////////////////////////////////////////
void SetLeds(bool v)
{
    digitalWrite(v ? BLOCKING_PIN : NET_CLOCK_PIN, LOW);
    digitalWrite(v ? NET_CLOCK_PIN : BLOCKING_PIN, HIGH);
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
    pinMode(NET_CLOCK_PIN, OUTPUT);
    digitalWrite(NET_CLOCK_PIN, LOW);
    pinMode(BLOCKING_PIN, OUTPUT);
    digitalWrite(BLOCKING_PIN, LOW);

    // Cycle the LED at power up just to show that they work.
    digitalWrite(BLOCKING_PIN, HIGH);
    delay(1000);
    digitalWrite(BLOCKING_PIN, LOW);
    digitalWrite(NET_CLOCK_PIN, HIGH);
    delay(1000);
    digitalWrite(NET_CLOCK_PIN, LOW);
    digitalWrite(BLOCKING_PIN, HIGH);

    // Init a pointer to our WiFiTimeManager instance.
    // This should be done before RTC init since the WiFiTimeManager
    // gets created on the first call to WiFiManager::Instance() and it
    // initializes a default time that the RTC may want to override.
    gpWtm = WiFiTimeManager::Instance();

    // Initialize the WiFiTimeManager class with our AP and button selections.
    gpWtm->Init(AP_NAME, AP_PWD, SETUP_BUTTON);

    // Contact the NTP server no more than once per minute.
    gpWtm->SetMinNtpRate(60);

    // Attempt to connect to the network in blocking mode.
    gpWtm->setConfigPortalBlocking(BLOCKING_MODE);
    if(!gpWtm->autoConnect())
    {
        Serial.println("Failed to connect or hit timeout");
    }
    else
    {
        // If you get here you have connected to the WiFi.
        Serial.println("connected...yeey :)");
    }
} // End setup().


/////////////////////////////////////////////////////////////////////////////////
// loop()
//
// The Arduino loop() function.  Checks the reset button, and as a
// demonstration, periodically get UTC and local time from the WiFiTimeManager
// and display the results.
/////////////////////////////////////////////////////////////////////////////////
void loop()
{
    // Check and handle the reset button.
    CheckButton();

    // Read the time every 10 seconds.
    static uint32_t lastTime = millis();
    uint32_t thisTime = millis();
    static const uint32_t updateTime = 10000;
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