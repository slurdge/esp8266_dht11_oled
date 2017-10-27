#include <time.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
//#include <ESP8266HTTPClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <U8g2lib.h>

#include "Secrets.h"

#define FINAL 0

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
static unsigned int const PAUSE_POWERSAVE_IN_MS = 1000 * 1200; // 20 minutes
static unsigned int const PAUSE_ACTIVE_IN_MS    = 500;         // half a sec
static unsigned int const POWERSAVE_IN_MS       = 20 * 1000;   // 20 secs
#else
static unsigned int const PAUSE_POWERSAVE_IN_MS = 1000 * 360; // 10 minutes
static unsigned int const PAUSE_ACTIVE_IN_MS    = 500;        // half a sec
static unsigned int const POWERSAVE_IN_MS       = 120 * 1000; // 2 minutes
#endif
static unsigned int pauseInMs = PAUSE_ACTIVE_IN_MS;

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
// HTTPClient http;
WiFiClientSecure                       wifiClientSecure;
DHT_Unified                            dht(DHTPIN, DHTTYPE);
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);

unsigned long   lastButtonPressed = 0;
bool            displayOn         = true;
sensors_event_t sensorTemperature = { 0 };
sensors_event_t sensorHumidity = { 0 };
String          postData          = String("temperature=");

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
	wifiClientSecure.setCACert_P(LECertRoot, LECertRootSize);

	if (!wifiClientSecure.connect(API_HOSTNAME, 443))
	{
		DEBUG_PRINTLN("connection failed");
		while (true)
		{
			delay(500);
		}
	}
	// Verify validity of server's certificate
	if (wifiClientSecure.verifyCertChain(API_HOSTNAME))
	{
		DEBUG_PRINTLN("Server certificate verified");
	} else
	{
		DEBUG_PRINTLN("ERROR: certificate verification failed!");
		while (true)
		{
			delay(500);
		}
	}

	String url = "/";
	Serial.print("requesting URL: ");
	Serial.println(url);

	wifiClientSecure.print(String("GET ") + url +
	                       " HTTP/1.1\r\n"
	                       "Host: " API_HOSTNAME "\r\n"
	                       "User-Agent: ESP8266\r\n"
	                       "Connection: close\r\n\r\n");

	Serial.println("request sent");
	while (wifiClientSecure.connected())
	{
		String line = wifiClientSecure.readStringUntil('\n');
		Serial.println(line);
		if (line == "\r")
		{
			Serial.println("headers received");
			break;
		}
	}
	Serial.println("reply was:");
	Serial.println("==========");
	while (wifiClientSecure.connected())
	{
		String line = wifiClientSecure.readStringUntil('\n');
		Serial.println(line);
	}
	Serial.println("==========");
	Serial.println();
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
	dht.temperature().getEvent(&sensorTemperature);
	dht.humidity().getEvent(&sensorHumidity);
}

void drawScreen()
{
	String displayData;
	if (isnan(sensorTemperature.temperature))
	{
		DEBUG_PRINTLN("Error reading temperature!");
		displayData += "NaN";
	} else
	{
		DEBUG_PRINT("Temperature: ");
		DEBUG_PRINT(sensorTemperature.temperature);
		DEBUG_PRINTLN("°C");
		displayData += (int)sensorTemperature.temperature;
		displayData += "°C";
	}
	displayData += " - ";
	if (isnan(sensorHumidity.relative_humidity))
	{
		DEBUG_PRINTLN("Error reading humidity!");
		displayData += "NaN";
	} else
	{
		DEBUG_PRINT("Humidity: ");
		DEBUG_PRINT(sensorHumidity.relative_humidity);
		DEBUG_PRINTLN("%");
		displayData += (int)sensorHumidity.relative_humidity;
		displayData += "%";
	}

	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_5x7_tf);
	unsigned int size = u8g2.getStrWidth(wifiConnection.getPublicIP().c_str());
	u8g2.drawStr(0, SCREEN_HEIGHT, wifiConnection.getPublicIP().c_str());
	u8g2.setFont(u8g2_font_9x15B_tf);
	u8g2.drawUTF8(0, SCREEN_HEIGHT / 2, displayData.c_str());
	static unsigned int wifiFrame = 0;
	if (wifiConnection.isConnected())
		wifiFrame = wifiConnection.getSignalStrength();
	else
		wifiFrame = wifiFrame + 1 % NUM_WIFI_FRAMES;
	u8g2.drawXBMP(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16, 16, 16, WIFI_FRAMES[wifiFrame]);
	u8g2.sendBuffer();
}

void loop()
{
	wifiConnection.checkWiFi();

	getSensorData();
	if (displayOn)
	{
		drawScreen();
	}

	/*
	  http.begin("http:///");

	  String fullpostData=postData+sensor_buffer;
	  Serial.println(fullpostData);
	  http.addHeader("Content-Type","application/x-www-form-urlencoded");
	  int httpCode = http.POST(fullpostData);
	  if(httpCode == HTTP_CODE_OK)
	  {
	    Serial.print("HTTP response code ");
	    Serial.println(httpCode);
	    String response = http.getString();
	    Serial.println(response);
	  }
	  else
	  {
	     Serial.println("Error in HTTP request");
	  }

	  http.end();
	*/
	lastButtonPressed += pauseInMs; // a little false since we are doing other, stuff, but, meh
	if (lastButtonPressed >= POWERSAVE_IN_MS)
	{
		pauseInMs = PAUSE_POWERSAVE_IN_MS;
		displayOn = false;
		u8g2.noDisplay();
	}

	delay(pauseInMs);
}
