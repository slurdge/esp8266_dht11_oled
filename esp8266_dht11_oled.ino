#include <time.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <U8g2lib.h>

#include "Secrets.h"

#define FINAL 0        // put to 1 for final build
#define API_NO_HTTPS 0 // used for testing on local servers

#if FINAL
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINT(...)
#else
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#endif

// Various constants, change here accordingly with your schematics and usage

static unsigned int const DHTPIN  = D5;    // Pin which is connected to the DHT sensor.
static unsigned int const DHTTYPE = DHT11; // DHT 11

#if FINAL
static unsigned int const PAUSE_POWERSAVE_IN_MS = 1200 * 1000; // 20 minutes
static unsigned int const PAUSE_ACTIVE_IN_MS    = 1000;        // 1 sec
static unsigned int const POWERSAVE_IN_MS       = 40 * 1000;   // 40 secs
#else
static unsigned int const PAUSE_POWERSAVE_IN_MS = 600 * 1000; // 10 minutes
static unsigned int const PAUSE_ACTIVE_IN_MS    = 1000;       // 1 sec
static unsigned int const POWERSAVE_IN_MS       = 120 * 1000; // 2 minutes
#endif
static unsigned int const UPLOAD_TIME_IN_MS  = 3600 * 1000 * 2; // 2 hours - must be less than 50 days
static unsigned int       pauseInMs          = PAUSE_ACTIVE_IN_MS;
static unsigned int       lastUploadTimeInMs = 0;
static unsigned int       lastMillis         = 0;
static unsigned int const NUM_LAST_SAMPLES   = 8; // You should respect this: UPLOAD_TIME_IN_MS > NUM_LAST_SAMPLES * POWERSAVE_IN_MS

static unsigned int const CURRENT_TIME_ZONE_IN_S = 3600; // UTC+1

static unsigned int const SCREEN_WIDTH  = 128;
static unsigned int const SCREEN_HEIGHT = 32;

extern const unsigned char LECertRoot[] PROGMEM;
extern const unsigned int  LECertRootSize;

static const unsigned int NUM_WIFI_FRAMES                          = 5;
static const uint8_t      WIFI_FRAMES[NUM_WIFI_FRAMES][32] PROGMEM = {
    {
        0x00, 0x00, 0x0A, 0x60, 0x04, 0x60, 0x0A, 0x60, 0x00, 0x6C, 0x04, 0x6C, 0x04, 0x60, 0x24, 0x64,
        0x74, 0x6E, 0xE4, 0x67, 0xC4, 0x63, 0xC4, 0x63, 0xE4, 0x67, 0x74, 0x6E, 0x24, 0x64, 0x00, 0x00,
    },
    {
        0x00, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00,
        0x04, 0x00, 0x04, 0x00, 0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x00, 0x00,
    },
    {
        0x00, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x84, 0x01,
        0x84, 0x01, 0x84, 0x01, 0xB4, 0x01, 0xB4, 0x01, 0xB4, 0x01, 0xB4, 0x01, 0xB4, 0x01, 0x00, 0x00,
    },
    {
        0x00, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x00, 0x0C, 0x04, 0x0C, 0x04, 0x0C, 0x84, 0x0D,
        0x84, 0x0D, 0x84, 0x0D, 0xB4, 0x0D, 0xB4, 0x0D, 0xB4, 0x0D, 0xB4, 0x0D, 0xB4, 0x0D, 0x00, 0x00,
    },
    {
        0x00, 0x00, 0x0A, 0x60, 0x04, 0x60, 0x0A, 0x60, 0x00, 0x6C, 0x04, 0x6C, 0x04, 0x6C, 0x84, 0x6D,
        0x84, 0x6D, 0x84, 0x6D, 0xB4, 0x6D, 0xB4, 0x6D, 0xB4, 0x6D, 0xB4, 0x6D, 0xB4, 0x6D, 0x00, 0x00,
    }
};

static const uint8_t UPLOAD_FRAMES[2][32] PROGMEM = { {
	                                                      0x00, 0x00, 0xFC, 0x3F, 0x02, 0x40, 0x32, 0x52, 0x4A, 0x4A, 0x4A, 0x46, 0x4A, 0x4A, 0x32, 0x52,
	                                                      0x02, 0x40, 0xFC, 0x3F, 0x02, 0x40, 0x56, 0xD5, 0xAB, 0xAA, 0x55, 0xD5, 0xAB, 0xAA, 0xFF, 0xFF,
	                                                  },
	                                                  {
	                                                      0x00, 0x00, 0xFC, 0x3F, 0x02, 0x40, 0x4A, 0x4C, 0x5A, 0x52, 0x5A, 0x52, 0x6A, 0x52, 0x4A, 0x4C,
	                                                      0x02, 0x40, 0xFC, 0x3F, 0x02, 0x40, 0x56, 0x55, 0xAB, 0xAA, 0x55, 0xD5, 0xAB, 0xAA, 0xFF, 0xFF,
	                                                  } };

static char const POST_HEADERS[] = "POST " API_ENDPOINT " HTTP/1.1\r\n"
                                   "Host: " API_HOSTNAME "\r\n"
                                   "Content-Type: application/json\r\n"
                                   "User-Agent: ESP8266\r\n"
                                   "Accept-Encoding: identity;q=1\r\n"
                                   "X-Auth-Token:" API_AUTH_TOKEN "\r\n";

class WiFiConnection
{
  public:
	WiFiConnection()
	  : lastWiFiBegin(0)
	{}

	void checkWiFi()
	{
		if (WiFi.status() != WL_CONNECTED)
		{
			DEBUG_PRINTLN("Connecting...");
			if (lastWiFiBegin == 0 || (millis() - lastWiFiBegin > INTERVAL_BETWEEN_CONNECT_MS))
			{
				WiFi.begin(WIFI_NAME, WIFI_PASS);
				lastWiFiBegin = millis();
			}
			localIP = "No connection.";
			Serial.println(WiFi.status());
		} else
		{
			localIP = WiFi.localIP().toString();
		}
	}

	const bool isConnected() { return WiFi.status() == WL_CONNECTED; }

	const String& getPublicIP() { return localIP; }

	const unsigned int getSignalStrength()
	{
		if (isConnected())
		{
			if (WiFi.RSSI() > -100)
				return (100 + WiFi.RSSI()) * 4 / 100 + 1;
			else
				return 0;
		} else
		{
			return 0;
		}
	}

  private:
	unsigned long              lastWiFiBegin;
	String                     localIP;
	static unsigned long const INTERVAL_BETWEEN_CONNECT_MS = 5000;
};

WiFiConnection wifiConnection;
#if !(API_NO_HTTPS)
WiFiClientSecure wifiClient;
#else
WiFiClient                wifiClient;
#endif
DHT_Unified                            dht(DHTPIN, DHTTYPE);
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);

unsigned long lastButtonPressed                   = 0;
bool          displayOn                           = true;
float         sensorTemperature[NUM_LAST_SAMPLES] = { 0 };
float         sensorHumidity[NUM_LAST_SAMPLES]    = { 0 };
unsigned int  sampleIndex                         = 0;
bool          lastCollectionOk                    = false;
bool          lastPostOk                          = false;

void synchronizeTime()
{
	// Synchronize time using SNTP. This is necessary to verify that
	// the TLS certificates offered by the server are currently valid.
	DEBUG_PRINTLN("Setting time using SNTP");
	configTime(CURRENT_TIME_ZONE_IN_S, 0, "pool.ntp.org", "time.nist.gov");
	time_t now = time(nullptr);
	while (now < 1000)
	{
		delay(500);
		DEBUG_PRINT(".");
		now = time(nullptr);
	}
	DEBUG_PRINTLN("");
	DEBUG_PRINT("Current time: ");
	DEBUG_PRINTLN(ctime(&now));
}

void setup()
{
#if !FINAL
	Serial.begin(115200);
	Serial.println();
	Serial.setDebugOutput(true);
#endif

	pinMode(D3, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(D3), buttonPressed, FALLING);

	dht.begin();
	u8g2.begin();
	wifiConnection.checkWiFi();
	drawScreen();

	synchronizeTime();
#if !(API_NO_HTTPS)
	wifiClient.setCACert_P(LECertRoot, LECertRootSize);
#endif

	lastMillis = millis();
}

extern "C" void esp_schedule();

void buttonPressed()
{
	DEBUG_PRINTLN("Button pressed!");
	lastButtonPressed = 0;
	displayOn         = true;
	pauseInMs         = PAUSE_ACTIVE_IN_MS;
	u8g2.display();
	esp_schedule(); // we use a trick here to wakeup the main loop
}

void getSensorData()
{
	sensors_event_t localTemperature;
	sensors_event_t localHumidity;
	dht.temperature().getEvent(&localTemperature);
	dht.humidity().getEvent(&localHumidity);
	if (isnan(localTemperature.temperature) || isnan(localHumidity.relative_humidity))
	{
		DEBUG_PRINTLN("Error reading temperature or humidity!");
		lastCollectionOk = false;
	} else
	{
		DEBUG_PRINT("Temperature: ");
		DEBUG_PRINT(localTemperature.temperature);
		DEBUG_PRINT("°C , Humidity: ");
		DEBUG_PRINT(localHumidity.relative_humidity);
		DEBUG_PRINTLN("%");
		sensorTemperature[sampleIndex] = localTemperature.temperature;
		sensorHumidity[sampleIndex]    = localHumidity.relative_humidity;
		sampleIndex                    = (sampleIndex + 1) % NUM_LAST_SAMPLES;
		lastCollectionOk               = true;
	}
}

void postData()
{
	lastPostOk = false;

	if (!wifiClient.connect(API_HOSTNAME, API_PORT))
	{
		DEBUG_PRINTLN("connection failed");
		return;
	}
	// Verify validity of server's certificate
#if !(API_NO_HTTPS)
	bool verified = wifiClient.verifyCertChain(API_HOSTNAME);
	if (verified)
	{
		DEBUG_PRINTLN("Server certificate verified");
	} else
	{
		DEBUG_PRINTLN("ERROR: certificate verification failed!");
		return;
	}
#endif

	float temperature_average = 0;
	float humidity_average    = 0;
	for (unsigned int i = 0; i < NUM_LAST_SAMPLES; ++i)
	{
		temperature_average += sensorTemperature[i];
		humidity_average += sensorHumidity[i];
	}
	temperature_average /= (float)NUM_LAST_SAMPLES;
	humidity_average /= (float)NUM_LAST_SAMPLES;

	String request = POST_HEADERS;
	String body    = String("{\"temperature\":") + temperature_average + ",\"humidity\":" + humidity_average + "}\r\n";
	request.concat(String("Content-Length: ") + body.length() + "\r\n");
	request.concat("Connection: close\r\n\r\n");
	request.concat(body);

	DEBUG_PRINTLN(request);
	wifiClient.write((const unsigned char*)(request.c_str()), request.length());

	DEBUG_PRINTLN("POST done");
	while (wifiClient.connected())
	{
		String line = wifiClient.readStringUntil('\n');
		DEBUG_PRINTLN(line);
		if (line == "\r") break;
	}
	DEBUG_PRINTLN("Receiving");
	if (wifiClient.connected())
	{
		String line = wifiClient.readStringUntil('\n');
		DEBUG_PRINTLN(line);
		if (line.startsWith("{\"success\":true")) lastPostOk = true;
	}
	lastUploadTimeInMs = 0;
}

void drawScreen()
{
	String       displayData;
	unsigned int lastSampleIndex = (sampleIndex + NUM_LAST_SAMPLES - 1) % NUM_LAST_SAMPLES;
	if (!lastCollectionOk)
	{
		displayData = "Err. - Err.";
	} else
	{
		displayData += (int)sensorTemperature[lastSampleIndex];
		displayData += "°C - ";
		displayData += (int)sensorHumidity[lastSampleIndex];
		displayData += "%";
	}

	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_5x7_tf);
	u8g2.drawStr(0, SCREEN_HEIGHT, wifiConnection.getPublicIP().c_str());
	u8g2.setFont(u8g2_font_9x15B_tf);
	u8g2.drawUTF8(0, SCREEN_HEIGHT / 2 - 2, displayData.c_str());
	static unsigned int wifiFrame = 0;
	if (wifiConnection.isConnected())
		wifiFrame = wifiConnection.getSignalStrength();
	else
		wifiFrame = wifiFrame + 1 % NUM_WIFI_FRAMES;
	u8g2.drawXBMP(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16, 16, 16, WIFI_FRAMES[wifiFrame]);
	u8g2.drawXBMP(SCREEN_WIDTH - 16, 0, 16, 16, UPLOAD_FRAMES[lastPostOk ? 0 : 1]);


	u8g2.sendBuffer();
}

void loop()
{
	lastUploadTimeInMs += millis() - lastMillis;
	lastMillis = millis();
	wifiConnection.checkWiFi();

	getSensorData();
	if (displayOn)
	{
		drawScreen();
	}
	if (lastUploadTimeInMs > UPLOAD_TIME_IN_MS)
	{
		postData();
	}

	lastButtonPressed += pauseInMs; // a little false since we are doing other, stuff, but, meh
	if (lastButtonPressed >= POWERSAVE_IN_MS)
	{
		pauseInMs = PAUSE_POWERSAVE_IN_MS;
		displayOn = false;
		u8g2.noDisplay();
	}

	delay(pauseInMs);
}
