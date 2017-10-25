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

static const uint8_t WIFI_FRAMES[5][8] = {
	0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x1,
	0x0,0x0,0x0,0x0,0x4,0x4,0x5,0x5,
	0x0,0x0,0x10,0x10,0x14,0x14,0x15,0x15,
	0x40,0x40,0x50,0x50,0x54,0x54,0x55,0x55,
	0x40,0x40,0x40,0x51,0x4a,0x44,0x4a,0x51,
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
			long rssi = WiFi.RSSI();
			Serial.print("RSSI:");
			Serial.println(rssi);
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
				return (100 + WiFi.RSSI()) * 5 / 100;
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

void button_pressed() {
	Serial.println("Button pressed!");
}

void loop() {
	wifiConnection.checkWiFi();
	
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
	displayData += " ";
	// Get humidity event and print its value.
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
	u8g2.drawUTF8(0, SCREEN_HEIGHT/2, displayData.c_str());
	static int frame = 0;
	u8g2.drawXBM(SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8, 8, 8, WIFI_FRAMES[frame]);
	frame = (frame + 1) % 5;
	u8g2.sendBuffer();

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
	delay(1000 * 1);
}


