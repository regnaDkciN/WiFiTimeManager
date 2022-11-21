/**
 * WiFiManager advanced demo, contains advanced configurartion options
 * Implements TRIGGEN_PIN button press, press for ondemand configportal, hold for 3 seconds for reset settings.
 */
#include <String>               // For String class.
#include "WiFiTimeManager.h"    // Manages timezone and dst.

#define TRIGGER_PIN 14
#define NET_CLOCK_PIN 12
#define LOCAL_CLOCK_PIN 27

static WiFiTimeManager *pWtm;
static bool WmSeparateButton = true;
static bool WmBlocking = true;
static const char *AP_NAME = "WiFi Clock Setup";


void SetAPCallback(WiFiManager *)
{
    Serial.println("SetAPCallback");
}

void SetWebServerCallback()
{
    Serial.println("SetWebServerCallback");
}

void SetConfigResetCallback()
{
    Serial.println("SetConfigResetCallback");
}

void SetSaveConfigCallback()
{
    Serial.println("SetSaveConfigCallback");
}

void SetPreSaveConfigCallback()
{
    Serial.println("SetPreSaveConfigCallback");
}

void SetPreSaveParamsCallback()
{
    Serial.println("SetPreSaveParamsCallback");
}

void SetPreOtaUpdateCallback()
{
    Serial.println("SetPreOtaUpdateCallback");
}


void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);  
    delay(1000);
    Serial.println("\n Starting");
    
    pinMode(TRIGGER_PIN, INPUT_PULLUP);
    pinMode(NET_CLOCK_PIN, OUTPUT);
    digitalWrite(NET_CLOCK_PIN, LOW);
    pinMode(LOCAL_CLOCK_PIN, OUTPUT);
    digitalWrite(LOCAL_CLOCK_PIN, LOW);
    
    // Test LEDs
    digitalWrite(LOCAL_CLOCK_PIN, HIGH);
    delay(2000);
    digitalWrite(LOCAL_CLOCK_PIN, LOW);
    digitalWrite(NET_CLOCK_PIN, HIGH);
    delay(2000);
    digitalWrite(NET_CLOCK_PIN, LOW);
    
    
    pWtm = WiFiTimeManager::Instance();    
    pWtm->Init(AP_NAME, WmSeparateButton);
    pWtm->SetMinNtpRate(60); // Contact NTP server no more than once per minute.
    pWtm->setConfigPortalBlocking(WmBlocking);

    
    
    
pWtm->setAPCallback(SetAPCallback);
pWtm->setWebServerCallback(SetWebServerCallback);
pWtm->setConfigResetCallback(SetConfigResetCallback);
pWtm->setSaveConfigCallback(SetSaveConfigCallback);
pWtm->setPreSaveConfigCallback(SetPreSaveConfigCallback);
pWtm->setPreSaveParamsCallback(SetPreSaveParamsCallback);
pWtm->setPreOtaUpdateCallback(SetPreOtaUpdateCallback);

    
    

    bool res;
    res = pWtm->autoConnect(AP_NAME); 

    if(!res)
    {
        Serial.println("Failed to connect or hit timeout");
        // ESP.restart();
    } 
    else
    {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }
}

void checkButton()
{
    // check for button press
    if ( digitalRead(TRIGGER_PIN) == LOW )
    {
        // poor mans debounce/press-hold, code not ideal for production
        delay(50);
        if( digitalRead(TRIGGER_PIN) == LOW )
        {
            Serial.println("Button Pressed");
            // still holding button for 3000 ms, reset settings, code not ideaa for production
            delay(3000); // reset delay hold
            if( digitalRead(TRIGGER_PIN) == LOW )
            {
                Serial.println("Button Held");
                Serial.println("Erasing Config, restarting");
                pWtm->ResetData();
                ESP.restart();
            }
          
            // start portal w delay
            Serial.println("Starting config portal");
            pWtm->setConfigPortalTimeout(120);
          
            if (!pWtm->startConfigPortal(AP_NAME))
            {
                Serial.println("failed to connect or hit timeout");
                delay(3000);
                ESP.restart();
            } 
            else
            {
                //if you get here you have connected to the WiFi
                Serial.println("connected...yeey :)");
            }
        }
    }
}

void setLeds(bool v)
{
    digitalWrite(v ? LOCAL_CLOCK_PIN : NET_CLOCK_PIN, LOW);
    digitalWrite(v ? NET_CLOCK_PIN : LOCAL_CLOCK_PIN, HIGH);
}


void loop()
{
    if(!WmBlocking)
    {
        pWtm->process(); // avoid delays() in loop when non-blocking and other long running code  
    }
   
    checkButton();
    
    static uint32_t lastTime = millis();
    uint32_t thisTime = millis();
    static const uint32_t updateTime = 10000;   // 10 seconds between reading time
    if (thisTime - lastTime >= updateTime)
    {
        lastTime = thisTime;
        time_t utcTime = pWtm->GetUtcTime();
        time_t lclTime = pWtm->GetLocalTime();
        pWtm->PrintDateTime(utcTime, "UTC");
        pWtm->PrintDateTime(lclTime, pWtm->GetLocalTimezoneString());
        setLeds(pWtm->UsingNetworkTime());
        Serial.println();
    }
    delay(1);
    
}