#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <U8g2lib.h>

#include "wifi_creds.h"

#define FINAL 0

#if FINAL
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINT(...)
#else
#define DEBUG_PRINTLN(...)	Serial.println(__VA_ARGS__)
#define DEBUG_PRINT(...)	Serial.print(__VA_ARGS__)
#endif

static unsigned int const DHTPIN = D5;      // Pin which is connected to the DHT sensor.
static unsigned int const DHTTYPE = DHT11;      // DHT 11 

DHT_Unified dht(DHTPIN, DHTTYPE);
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

String postData = String("temperature=");

HTTPClient http;

unsigned long lastButtonPressed = 0;
bool displayOn = true;
#if FINAL
static unsigned int const PAUSE_POWERSAVE_IN_MS = 1000 * 1200; //20 minutes
static unsigned int const PAUSE_ACTIVE_IN_MS = 500; // half a sec
static unsigned int const POWERSAVE_IN_MS = 20 * 1000; // 20 secs
#else
static unsigned int const PAUSE_POWERSAVE_IN_MS = 1000 * 360; //10 minutes
static unsigned int const PAUSE_ACTIVE_IN_MS = 500; // half a sec
static unsigned int const POWERSAVE_IN_MS = 120 * 1000; // 2 minutes
#endif
static unsigned int pause_in_ms = PAUSE_ACTIVE_IN_MS;

static const unsigned int NUM_WIFI_FRAMES = 5;
static const uint8_t WIFI_FRAMES[NUM_WIFI_FRAMES][32] PROGMEM= {
	{
	0x00, 0x00, 0x0A, 0x60, 0x04, 0x60, 0x0A, 0x60, 0x00, 0x6C, 0x04, 0x6C,
	0x04, 0x60, 0x24, 0x64, 0x74, 0x6E, 0xE4, 0x67, 0xC4, 0x63, 0xC4, 0x63,
	0xE4, 0x67, 0x74, 0x6E, 0x24, 0x64, 0x00, 0x00,
	},
	{
	0x00, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x34, 0x00, 0x34, 0x00,
	0x34, 0x00, 0x34, 0x00, 0x34, 0x00, 0x00, 0x00,
	},
	{
	0x00, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x04, 0x00, 0x84, 0x01, 0x84, 0x01, 0x84, 0x01, 0xB4, 0x01, 0xB4, 0x01,
	0xB4, 0x01, 0xB4, 0x01, 0xB4, 0x01, 0x00, 0x00,
	},
	{
	0x00, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x00, 0x0C, 0x04, 0x0C,
	0x04, 0x0C, 0x84, 0x0D, 0x84, 0x0D, 0x84, 0x0D, 0xB4, 0x0D, 0xB4, 0x0D,
	0xB4, 0x0D, 0xB4, 0x0D, 0xB4, 0x0D, 0x00, 0x00,
	},
	{
	0x00, 0x00, 0x0A, 0x60, 0x04, 0x60, 0x0A, 0x60, 0x00, 0x6C, 0x04, 0x6C,
	0x04, 0x6C, 0x84, 0x6D, 0x84, 0x6D, 0x84, 0x6D, 0xB4, 0x6D, 0xB4, 0x6D,
	0xB4, 0x6D, 0xB4, 0x6D, 0xB4, 0x6D, 0x00, 0x00, }
};

class WiFiConnection
{
public:

	WiFiConnection() : lastWiFiBegin(0) {}

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
		}
		else
		{
			localIP = WiFi.localIP().toString();
		}
	}

	const bool isConnected()
	{
		return WiFi.status() == WL_CONNECTED;
	}

	const String& getPublicIP()
	{
		return localIP;
	}

	const unsigned int getSignalStrength()
	{
		if (isConnected())
		{
			if (WiFi.RSSI() > -100)
				return (100 + WiFi.RSSI()) * 4 / 100 + 1;
			else
				return 0;
		}
		else
		{
			return 0;
		}
	}

private:
	unsigned long lastWiFiBegin;
	String localIP;
	static unsigned long const INTERVAL_BETWEEN_CONNECT_MS = 5000;
};

WiFiConnection wifiConnection;

void setup()
{
#if !FINAL
	Serial.begin(115200);
	Serial.println();
#endif

	pinMode(D3, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(D3), button_pressed, FALLING);

	dht.begin();
	u8g2.begin();
	wifiConnection.checkWiFi();
}

extern "C" void esp_schedule();

extern void delay_end(void* arg);

void button_pressed() {
	DEBUG_PRINTLN("Button pressed!");
	lastButtonPressed = 0;
	displayOn = true;
	pause_in_ms = PAUSE_ACTIVE_IN_MS;
	u8g2.display();
	esp_schedule(); //we use a trick here to wakeup the main loop
}

void display() {
	String displayData;
	sensors_event_t event;
	dht.temperature().getEvent(&event);
	if (isnan(event.temperature)) {
		DEBUG_PRINTLN("Error reading temperature!");
		displayData += "NaN";
	}
	else {
		DEBUG_PRINT("Temperature: ");
		DEBUG_PRINT(event.temperature);
		DEBUG_PRINTLN(" *C");
		displayData += (int)event.temperature;
		displayData += "°C";
	}
	displayData += " - ";
	dht.humidity().getEvent(&event);
	if (isnan(event.relative_humidity)) {
		DEBUG_PRINTLN("Error reading humidity!");
		displayData += "NaN";
	}
	else {
		DEBUG_PRINT("Humidity: ");
		DEBUG_PRINT(event.relative_humidity);
		DEBUG_PRINTLN("%");
		displayData += (int)event.relative_humidity;
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

void loop() {
	wifiConnection.checkWiFi();

	if (displayOn) {
		display();
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
	lastButtonPressed += pause_in_ms; // a little false since we are doing other, stuff, but, meh
	if (lastButtonPressed >= POWERSAVE_IN_MS)
	{
		pause_in_ms = PAUSE_POWERSAVE_IN_MS;
		displayOn = false;
		u8g2.noDisplay();
	}
		
	delay(pause_in_ms);
}


