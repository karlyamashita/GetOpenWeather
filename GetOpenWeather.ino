// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       GetOpenWeather.ino
    Created:	1/22/2020 2:58:33 PM
    Author:     Karl-PC\Karl

	The commands to send to ESP
	CMD_SSID - Send with ssid of Wi-Firouter in quotes. Ex. CMD_SSID"myssid"
	CMD_PASSWORD - Send with password of Wi-Fi router in quotes. Ex. CMD_PASSWORD"mypassword"
	CMD_RESET - Resets the ESP module
	CMD_CLEAR_WIFI - Clears the ssid and password of router credentials from the ESP
	CMD_WEATHER_ID - Gets the weather from openweathermap.org server
	CMD_NTP_TIME - Gets the time from pool.ntp.org server

	The commands sent from the ESP to receiving device and example strings
	CMD_NTP_TIME"Saturday, 21:04:59"
	CMD_CITY"Wildomar"
	CMD_TEMP"49.91"
	CMD_DESCRIPTION"clear sky"
	CMD_WEATHER_ID"800"
*/

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "ArduinoJson.h"


// User changable values
// Serial baudrate. Default 115200
#define baudRate 115200

// Time out seconds. Default 15 
// The amount of seconds to wait before indicating ESP can not aquire a Wi-Fi connection
#define TIME_OUT 15

// NTPClient
const long utcOffsetInSeconds = -28800;

// openweather api
const String APIKEY = "90a33af8a1ef8cfdd725739ebcdc6b0d"; // Register on openweathermap.org and enter you App ID. 
const String zipcodeID = "92595,us"; // add your location here
// end user changable


/* None of the values below should be changed  */
WiFiClient client;
char* servername = "api.openweathermap.org";  // remote server we will connect to

// for NTPClient
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

/* Don't set this wifi credentials. They are configurated at runtime and stored on EEPROM */
char ssid[32] = "";
char password[32] = "";

// The setup() function runs once each time the micro-controller starts
void setup()
{
	uint16_t timeOutCounter = 0; // timeout counter

	Serial.begin(baudRate);

	// start time client
	timeClient.begin();

	// load wifi credentials to ssid and passwod array variables
	loadCredentials();

	if (ssid[0] == '\0' || password[0] == '\0') { // No credentials saved yet
		Serial.println("CMD_NO_SSID_PW");
		// We should enter loop() now. 
		// Send CMD_SSID"<ssid>" and CMD_PASSWORD"<password>" to add your wi-fi credentials and reset
	}
	else // connect to wifi
	{
		Serial.println();
		Serial.print("CMD_SSID\"");
		Serial.print(ssid);
		Serial.println("\"");
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssid, password);
		while (WiFi.status() != WL_CONNECTED) {
			delay(1000);
			Serial.print(".");
			if (++timeOutCounter == TIME_OUT) {
				break;
			}
		}

		if (WiFi.status() == WL_CONNECTED)
		{
			Serial.println();
			Serial.print("CMD_IP_ADDRESS\""); // we know we're connected if we receive an IP address
			Serial.print(WiFi.localIP().toString());
			Serial.println(+"\"");
			getTime();
			getWeather();
		}
		else
		{
			Serial.println();
			Serial.println("CMD_NO_CONNECTION");
		}
	}
}

/* Loop here until uart message is received */
void loop()
{
	String uartMsg = Serial.readStringUntil('\r');
	if (uartMsg != "") {
		parseCommand(uartMsg);
	}
}

// parse the uart messages
void parseCommand(String uartMsg)
{
	if (uartMsg.indexOf("CMD_SSID") != std::string::npos)
	{
		uartMsg = parseUartValue(uartMsg, "CMD_SSID");
		if (uartMsg != ssid)
		{
			memset(&ssid, 0, sizeof(ssid));
			strcpy(ssid, uartMsg.c_str());
			saveCredentials();
			Serial.println("CMD_ACK");
		}
		else
		{
			Serial.println("CMD_SAME");// the ssid is the same
		}
	}
	else if (uartMsg.indexOf("CMD_PASSWORD") != std::string::npos)
	{
		uartMsg = parseUartValue(uartMsg, "CMD_PASSWORD");
		if (uartMsg != password)
		{
			memset(&password, 0, sizeof(password));
			strcpy(password, uartMsg.c_str());
			saveCredentials();
			Serial.println("CMD_ACK");
		}
		else
		{
			Serial.println("CMD_SAME");// the password is the same
		}
	}
	else if (uartMsg.indexOf("CMD_RESET") != std::string::npos)
	{
		Serial.println("CMD_ACK");
		ESP.reset();
	}
	else if (uartMsg.lastIndexOf("CMD_CLEAR_WIFI") != std::string::npos)
	{
		memset(&ssid, 0, sizeof(ssid));
		memset(&password, 0, sizeof(password));
		Serial.println("CMD_ACK");
	}
	else if (uartMsg.indexOf("CMD_WEATHER_ID") != std::string::npos)
	{
		Serial.println("CMD_ACK");
		getWeather();
	}
	else if (uartMsg.indexOf("CMD_NTP_TIME") != std::string::npos)
	{
		Serial.println("CMD_ACK");
		getTime();
	}
}

/* Remove the command, trims and remove quotes. Used for SSID and PW */
String parseUartValue(String uartMsg, String removeCommand)
{
	uartMsg.replace(removeCommand, "");
	uartMsg.replace("\"", "");
	uartMsg.trim();
	return uartMsg;
}

void getTime()
{
	timeClient.update();
	Serial.print("CMD_NTP_DAY\"");
	Serial.print(daysOfTheWeek[timeClient.getDay()]);
	Serial.println("\"");

	Serial.print("CMD_NTP_TIME\"");
	Serial.println(timeClient.getFormattedTime() + "\"");
}

void getWeather() 
{
	String result = "";
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(servername, httpPort)) {
		return;
	}
	// We now create a URI for the request
	String url = "/data/2.5/weather?zip=" + zipcodeID + "&units=imperial&cnt=1&APPID=" + APIKEY;

	Serial.println(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + servername + "\r\n" +
		"Connection: close\r\n\r\n");
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 5000) {
			client.stop();
			return;
		}
	}

	/* Read all the lines of the reply from server */
	while (client.available()) {
		result = client.readStringUntil('\r');
	}

	result.replace('[', ' ');
	result.replace(']', ' ');
	char jsonArray[1024];
	result.toCharArray(jsonArray, sizeof(jsonArray));
	jsonArray[result.length() + 1] = '\0';

	StaticJsonDocument<1024> doc;
	DeserializationError error = deserializeJson(doc, jsonArray);
	if (error)
	{
		Serial.print("deserializeJson() failed: ");
		Serial.println(error.c_str());
		return;
	}

	String location = doc["name"];
	String temperature = doc["main"]["temp"];
	String description = doc["weather"]["description"];
	String idString = doc["weather"]["id"];

	Serial.println("CMD_CITY\"" + location + "\"");
	Serial.println("CMD_TEMP\"" + temperature + "\"");
	Serial.println("CMD_DESCRIPTION\"" + description + "\"");
	Serial.println("CMD_WEATHER_ID\"" + idString + "\"");
}

/* Load WLAN credentials from EEPROM */
void loadCredentials() {
	EEPROM.begin(512);
	EEPROM.get(0, ssid);
	EEPROM.get(0 + sizeof(ssid), password);
	char ok[2 + 1];
	EEPROM.get(0 + sizeof(ssid) + sizeof(password), ok);
	EEPROM.end();
	if (String(ok) != String("OK")) {
		ssid[0] = 0;
		password[0] = 0;
	}
}

/* Store WLAN credentials to EEPROM */
void saveCredentials() {
	EEPROM.begin(512);
	EEPROM.put(0, ssid);
	EEPROM.put(0 + sizeof(ssid), password);
	char ok[2 + 1] = "OK";
	EEPROM.put(0 + sizeof(ssid) + sizeof(password), ok);
	EEPROM.commit();
	EEPROM.end();
}
