#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <U8g2lib.h>

#include "wifi_creds.h"

#define DHTPIN            D5         // Pin which is connected to the DHT sensor.
#define DHTTYPE           DHT11      // DHT 11 

DHT_Unified dht(DHTPIN, DHTTYPE);
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

static char sensor_buffer[] = "TT,YY";
String postData = String("temperature=");

HTTPClient http;
String localIP;

#define FINAL 0

#if FINAL
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINT(...)
#else
#define DEBUG_PRINTLN(...)	Serial.println(__VA_ARGS__)
#define DEBUG_PRINT(...)	Serial.print(__VA_ARGS__)
#endif

unsigned long lastWiFIBegin = 0;
static const unsigned long INTERVAL_BETWEEN_CONNECT_MS = 5000;

void checkWiFi()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		DEBUG_PRINTLN("Connecting...");
		if (lastWiFIBegin == 0 || (millis() - lastWiFIBegin > INTERVAL_BETWEEN_CONNECT_MS))
		{
			WiFi.begin(WIFI_NAME, WIFI_PASS);
			lastWiFIBegin = millis();
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
	checkWiFi();
	/*
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(500);
	}
	*/
}

void button_pressed() {
	Serial.println("Button pressed!");
}

void loop() {
	checkWiFi();
	/*
	int button = digitalRead(D3);
	Serial.print("Button: ");
	Serial.println(button);
	// Get temperature event and print its value.
	sensors_event_t event;
	dht.temperature().getEvent(&event);
	if (isnan(event.temperature)) {
		Serial.println("Error reading temperature!");
	}
	else {
		Serial.print("Temperature: ");
		Serial.print(event.temperature);
		Serial.println(" *C");
		sensor_buffer[0] = '0' + (int)event.temperature / 10;
		sensor_buffer[1] = '0' + (int)event.temperature % 10;
	}
	// Get humidity event and print its value.
	dht.humidity().getEvent(&event);
	if (isnan(event.relative_humidity)) {
		Serial.println("Error reading humidity!");
	}
	else {
		Serial.print("Humidity: ");
		Serial.print(event.relative_humidity);
		Serial.println("%");
		sensor_buffer[3] = '0' + (int)event.relative_humidity / 10;
		sensor_buffer[4] = '0' + (int)event.relative_humidity % 10;
	}
	*/
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_5x7_tr);
	unsigned int size = u8g2.getStrWidth(localIP.c_str());
	u8g2.drawStr(SCREEN_WIDTH - size, SCREEN_HEIGHT - 1, localIP.c_str());
	u8g2.sendBuffer();

	//u8g2.drawBitmap(0,10, 2, 16, )
	   //u8g2.drawStr(64,24,humidity_buffer);

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


