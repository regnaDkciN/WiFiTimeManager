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
static bool WmBlocking = false;
static const char *AP_NAME = "WiFi Clock Setup";
static const char EDIT_TEST_STR[] = R"(
    <br>
    <h3 style="display:inline">AN INTEGER VALUE:</h3>
    <input type="number" id="editTestNumber" name="editTestNumber" min="1" max="1000" value="0">
    <br>
)";


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

time_t UtcGetCallback()
{
    Serial.println("UtcGetCallback");
    return 0;
}

void UtcSetCallback(time_t t)
{
    Serial.println("UtcSetCallback");    
}

void UpdateWebPageCallback(String &rWebPage, uint32_t maxSize)
{
    Serial.println("UpdateWebPageCallback"); 
    rWebPage.replace("<!-- HTML END -->", EDIT_TEST_STR);
//    Serial.println(rWebPage);
}

void SaveParamsCallback()
{
    Serial.println("UpdateWebPageCallback"); 
    Serial.printf("Integer Value = %d\n", pWtm->GetParamInt("editTestNumber"));
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
    delay(1000);
    digitalWrite(LOCAL_CLOCK_PIN, LOW);
    digitalWrite(NET_CLOCK_PIN, HIGH);
    delay(1000);
    digitalWrite(NET_CLOCK_PIN, LOW);


    pWtm = WiFiTimeManager::Instance();
    
    // Web page is updated in init, so setup callback before Init() is called.
    pWtm->SetUpdateWebPageCallback(UpdateWebPageCallback);
    
    pWtm->Init(AP_NAME, WmSeparateButton);
    pWtm->SetMinNtpRate(60); // Contact NTP server no more than once per minute.

    // Setup some demo callbacks that simply report entry.
    pWtm->setAPCallback(APCallback);
    pWtm->setWebServerCallback(WebServerCallback);
    pWtm->setConfigResetCallback(ConfigResetCallback);
    pWtm->setSaveConfigCallback(SaveConfigCallback);
    pWtm->setPreSaveConfigCallback(PreSaveConfigCallback);
    pWtm->setPreSaveParamsCallback(PreSaveParamsCallback);
    pWtm->setPreOtaUpdateCallback(PreOtaUpdateCallback);
    
    pWtm->setSaveParamsCallback(SaveParamsCallback);
    
//    pWtm->SetUtcGetCallback(UtcGetCallback);
//    pWtm->SetUtcSetCallback(UtcSetCallback);
    

    // Attempt to connect to the network.
    pWtm->setConfigPortalBlocking(WmBlocking);
    pWtm->setConfigPortalTimeout(0);
    if(!pWtm->autoConnect(AP_NAME))
    {
        Serial.println("Failed to connect or hit timeout");
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
            if (!pWtm->IsConnected())
            {
                Serial.println("Starting config portal");
                pWtm->setConfigPortalBlocking(false);
                pWtm->setConfigPortalTimeout(0);
                pWtm->startConfigPortal(AP_NAME);
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
    if(!pWtm->IsConnected())
    {
        // Avoid delays() in loop when non-blocking and other long running code
        if (pWtm->process())
        {
            // This is the place to do something when we transition from
            // unconnected to connected.  As an example, here we get the time.
            pWtm->GetUtcTime();
        }
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