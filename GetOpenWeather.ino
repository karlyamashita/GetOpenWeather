// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
	Name:       GetOpenWeather.ino
	Created:	1/22/2020 2:58:33 PM
	Author:     Karl-PC\Karl


	Version:  1.0.6 - Removed unnecessary code.
			  1.0.5 - Made commands lower case and removed quotes. Using : and = within commands
			  1.0.4 - Changed so that user can update Open Weather Maps API key
			  1.0.3 - Renamed CMD text to either SET or GET. Return STAT if information from ESP instead of CMD.
			  1.0.2 - Added LED_BUILTIN for ESP-01S. Blinks LED during uart Tx/Rx
			  1.0.1 - Added get version number. Added set timezone. Added get raw json data

	The commands to send to ESP
	set ssid= - Send with ssid of Wi-Firouter in quotes. e.g. set ssid=myssid
	set password= - Send with password of Wi-Fi router in quotes. e.g. set password=mypassword
	set api= - Send with Open Weather Map API key. e.g. set api=xxxxxxxxxxxxxxxxxxx
	set reset - Resets the ESP module
	set clear wifi - Clears the ssid and password of router credentials from the ESP
	get json text - gets the raw data from openweather server
	get weather info - Gets the weather from openweathermap.org server
	set ntp offset= - Send with time offset in quotes. e.g. SET_SET_NTP_TIMEZONE"-25200"
	get ntp time - Gets the time from pool.ntp.org server

	The commands sent from the ESP to receiving device and example strings
	esp:ssid=xxxxxx // The SSID the ESP is connected to
	esp:ip address=192.168.1.16 // The IP address the ESP is connected to. If we get an IP address then we're connected to the internet
	esp:ntp offset=-25200 // the timezone current offset
	esp:ntp epoch=1625388728
	esp:ntp day=Sunday
	esp:ntp time=21:04:59
	esp:weather id=800
	esp:weather main=Clear
	esp:description=clear sky
	esp:icon=01d
	esp:temp=69.62
	esp:feels like=69.06
	esp:temp min=58.44
	esp:temp max=90.73
	esp:humid=59
	esp:wind=5.01
	esp:timezone=-25200
	esp:date receiving=07:52:01
	esp:sunrise=04:43:52
	esp:sunset=19:02:51
	esp:year=2021
	esp:city=Wildomar
*/



#include "NTPClient.h" // https://github.com/arduino-libraries/NTPClient
#include "ESP8266WiFi.h" // https://github.com/esp8266/Arduino
#include "WiFiUdp.h" // part of the Arduino esp8266 library

#include "TimeLib.h" // https://github.com/PaulStoffregen/Time
#include "ArduinoJson.h" // https://github.com/bblanchon/ArduinoJson
#include "EEPROM.h" // standard library


/***** User changable values *****/
const char version[] = "1.0.6";

// Serial baudrate. Default 115200
#define baudRate 115200

// Time out seconds. Default 15 
// The amount of seconds to wait before indicating ESP can not aquire a Wi-Fi connection
#define TIME_OUT 15

/***** END User changable values *****/


/***** DO NOT MODIFY section  *****/
WiFiClient client;
const char servername[] = "api.openweathermap.org";  // remote server we will connect to

// for NTPClient
String weekDays[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
String months[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

/* Don't set these wifi credentials. They are configurated at runtime and stored in the EEPROM */
char ssid[32] = "";
char password[32] = "";
char APIKEY[48] = "";
char ntpTimeZone[32] = "";
char zipcodeID[32] = "";

bool rawJSON_flag = false;
bool enableLED_flag = false;
/***** END DO NOT MODIFY section  *****/


// The setup() function runs once each time the micro-controller starts
void setup()
{
	uint16_t timeOutCounter = 0; // timeout counter

	pinMode(LED_BUILTIN, OUTPUT); // init GPIO for built in led
	digitalWrite(LED_BUILTIN, HIGH); // off

	Serial.begin(baudRate);

	// load wifi credentials to ssid and passwod array variables. Added timeZone 07/03/2021
	loadCredentials();

	getVersion();

	if (ssid[0] == '\0') {
		// Send "set ssid={ssid}" 
		Serial.println("esp:no ssid set");
	}
	else if (password[0] == '\0')
	{
		// Send "set password={password}"
		Serial.println("esp:no password set");
	}
	else if (APIKEY[0] == '\0')
	{
		// Send "set api={api key}"
		Serial.println("esp:no api key set");
	}
	else if (ntpTimeZone[0] == '\0')
	{
		Serial.println("esp:no ntp timezone set");
	}
	else if (zipcodeID[0] == '\0')
	{
		Serial.println("esp:no zipcode set");
	}
	else // connect to wifi
	{
		Serial.print("esp:ssid=");
		Serial.println(ssid);
		Serial.print("esp:connecting");
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
			Serial.print("esp:connected using ip address=");
			Serial.println(WiFi.localIP().toString());

			// start time client
			long ret;
			ret = strtol(ntpTimeZone, NULL, 10);

			timeClient.begin();
			timeClient.setTimeOffset(ret);

			getTimeZone();
			getTime();
			getWeather();
		}
		else
		{
			Serial.println();
			Serial.println("esp:no wifi connection");
		}
	}
}

/* Loop here until uart message is received */
void loop()
{
	String uartMsg = Serial.readStringUntil('\n');
	if (uartMsg != "") {
		if (enableLED_flag) {
			digitalWrite(LED_BUILTIN, LOW);
		}
		parseCommand(uartMsg);
		digitalWrite(LED_BUILTIN, HIGH);
	}

	ParseWeatherInfo();
}

// parse the uart messages
void parseCommand(String uartMsg)
{
	if (uartMsg.indexOf("set ssid") != std::string::npos)
	{
		uartMsg = trimQuotesSpaces(uartMsg, "set ssid");
		if (uartMsg != ssid)
		{
			memset(&ssid, 0, sizeof(ssid));
			strcpy(ssid, uartMsg.c_str());
			saveCredentials();
			Serial.println("esp:ack");
		}
		else
		{
			Serial.println("esp:ssid same");// the ssid is the same
		}
	}
	else if (uartMsg.indexOf("set password") != std::string::npos)
	{
		uartMsg = trimQuotesSpaces(uartMsg, "set password");
		if (uartMsg != password)
		{
			memset(&password, 0, sizeof(password));
			strcpy(password, uartMsg.c_str());
			saveCredentials();
			Serial.println("esp:ack");
		}
		else
		{
			Serial.println("esp:password same");// the password is the same
		}
	}
	else if (uartMsg.indexOf("set api") != std::string::npos)
	{
		uartMsg = trimQuotesSpaces(uartMsg, "set api");
		if (uartMsg != APIKEY)
		{
			memset(&APIKEY, 0, sizeof(APIKEY));
			strcpy(APIKEY, uartMsg.c_str());
			saveCredentials();
			Serial.println("esp:ack");
		}
		else
		{
			Serial.println("esp:api same");// the API is the same
		}
	}
	else if (uartMsg.indexOf("set reset") != std::string::npos)
	{
		Serial.println("esp:ack");
		delay(1000);
		ESP.reset();
	}
	else if (uartMsg.lastIndexOf("set clear settings") != std::string::npos)
	{
		EraseEEPROM();
		Serial.println("esp:ack");
	}
	else if (uartMsg.indexOf("get weather info") != std::string::npos)
	{
		Serial.println("esp:ack");
		getWeather();
	}
	else if (uartMsg.indexOf("set zipcode") != std::string::npos)
	{
		uartMsg = trimQuotesSpaces(uartMsg, "set zipcode");
		memset(&zipcodeID, 0, sizeof(zipcodeID));
		strcpy(zipcodeID, uartMsg.c_str());
		saveCredentials();
		Serial.println("esp:ack");
	}
	else if (uartMsg.indexOf("get zipcode") != std::string::npos)
	{
		Serial.println("esp:ack");
		getZipCodeID();
	}
	else if (uartMsg.indexOf("get ntp time") != std::string::npos)
	{
		Serial.println("esp:ack");
		getTimeZone();
		getTime();
	}
	else if (uartMsg.indexOf("get json text") != std::string::npos)
	{
		Serial.println("esp:ack");
		rawJSON_flag = true;
		getWeather();
	}
	else if (uartMsg.indexOf("set enable led") != std::string::npos)
	{
		enableLED_flag = 1;
		Serial.println("esp:ack");
	}
	else if (uartMsg.indexOf("set disable led") != std::string::npos)
	{
		enableLED_flag = 0;
		Serial.println("esp:ack");
	}
	else if (uartMsg.indexOf("get version") != std::string::npos)
	{
		Serial.println("esp:ack");
		getVersion();
	}
	else if (uartMsg.indexOf("set ntp offset") != std::string::npos)
	{
		uartMsg = trimQuotesSpaces(uartMsg, "set ntp offset");
		if (uartMsg != ntpTimeZone)
		{
			memset(&ntpTimeZone, 0, sizeof(ntpTimeZone));
			strcpy(ntpTimeZone, uartMsg.c_str());
			saveCredentials();
			timeClient.setTimeOffset(atoi(ntpTimeZone));
			Serial.println("esp:ack");
		}
		else
		{
			Serial.println("esp:ntp offset same");
		}
	}
	else
	{
		Serial.println("esp:command unknown");
	}
}

/* Remove the command, trims and remove equal. Used for SSID and PW */
String trimQuotesSpaces(String uartMsg, String toRemove)
{
	uartMsg.replace(toRemove, "");
	uartMsg.replace("=", "");
	uartMsg.trim();
	return uartMsg;
}

void getVersion()
{
	Serial.print("esp:version=");
	Serial.println(version);
}

void getTimeZone()
{
	String thisTimeZone(ntpTimeZone);
	Serial.println("esp:ntp offset=" + thisTimeZone);
}

void getTime()
{
	timeClient.update();

	time_t epochTime = timeClient.getEpochTime();

	Serial.print("esp:ntp epock=");
	Serial.println(epochTime);

	Serial.print("esp:ntp time=");
	Serial.println(timeClient.getFormattedTime());

	Serial.print("esp:ntp day=");
	Serial.println(weekDays[timeClient.getDay()]);

	struct tm* ptm = gmtime((time_t*)&epochTime);

	long monthDay = ptm->tm_mday;
	Serial.print("esp:ntp day=");
	Serial.println(monthDay);

	long currentMonth = ptm->tm_mon + 1;
	Serial.print("esp:ntp month=");
	Serial.println(currentMonth);

	String currentMonthName = months[currentMonth - 1];
	Serial.print("esp:ntp month name=");
	Serial.println(currentMonthName);

	long currentYear = ptm->tm_year + 1900;
	Serial.print("esp:ntp year=");
	Serial.println(currentYear);

	//Print complete date:
	String currentDate = String(currentMonth) + "-" + String(monthDay) + "-" + String(currentYear);
	Serial.print("esp:ntp date=");
	Serial.println(currentDate);
}

void getWeather()
{
	const int httpPort = 80;

	if (!client.connect(servername, httpPort)) {
		Serial.println("esp:could not connect to weather server"); // debug
		return;
	}
	// We now create a URI for the request
	String zip = String(zipcodeID);
	if (zip == "") {
		Serial.println("esp:please set the zipcode id");
		return;
	}
	String url = "/data/2.5/weather?zip=" + zip + "&units=imperial&cnt=1&APPID=" + APIKEY;

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + servername + "\r\n" +
		"Connection: close\r\n\r\n");
}

void ParseWeatherInfo()
{
	String result = "";

	if (client.available() == 0) return;

	/* Read all the lines of the reply from server */
	while (client.available()) {
		result = client.readStringUntil('\r');
		// TODO - add time out
	}

	if (rawJSON_flag == true) {
		rawJSON_flag = false;
		//  Serial.println(url); // Debugging purpose only. This shows your API!
		Serial.println(result); // JSON string. Debugging purpose only.
		return;
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

	String idString = doc["weather"]["id"];
	String wMain = doc["weather"]["main"];
	String description = doc["weather"]["description"];
	String icon = doc["weather"]["icon"];

	String temperature = doc["main"]["temp"];
	String feelsLike = doc["main"]["feels_like"];
	String tempMin = doc["main"]["temp_min"];
	String tempMax = doc["main"]["temp_max"];
	String humidity = doc["main"]["humidity"];

	String wind = doc["wind"]["speed"];
	String time_epoch = doc["dt"]; // time receiving

	String sunrise_epoch = doc["sys"]["sunrise"];
	String sunset_epoch = doc["sys"]["sunset"];

	String location_name = doc["name"];
	String timezone = doc["timezone"];

	Serial.println("esp:weather id=" + idString);
	Serial.println("esp:weather main=" + wMain);
	Serial.println("esp:description=" + description);
	Serial.println("esp:icon=" + icon);

	Serial.println("esp:temp=" + temperature);
	Serial.println("esp:feels like=" + feelsLike);
	Serial.println("esp:temp min=" + tempMin);
	Serial.println("esp:temp max=" + tempMax);
	Serial.println("esp:humid=" + humidity);

	Serial.println("esp:wind=" + wind);
	Serial.println("esp:dt=" + time_epoch);

	Serial.println("esp:timezone=" + timezone);

	Serial.println(GetTimeFromEpoch("esp:time receiving=", time_epoch));
	Serial.println(GetTimeFromEpoch("esp:sunrise=", sunrise_epoch));
	Serial.println(GetTimeFromEpoch("esp:sunset=", sunset_epoch));

	Serial.println("esp:city=" + location_name);
}

void getZipCodeID()
{
	String strZipcode(zipcodeID);
	Serial.println("esp:zipcode=" + strZipcode);
}

String GetTimeFromEpoch(String str_IN, String epoch_IN)
{
	char str[32];
	char str2[16];

	strcpy(str, epoch_IN.c_str()); // convert to array
	long ret = strtol(str, NULL, 10); // get long number
	long offSet = strtol(ntpTimeZone, NULL, 10);

	strcpy(str, str_IN.c_str());
	sprintf(str2, "%02d:%02d:%02d", hour(ret + offSet), minute(ret + offSet), second(ret + offSet));
	strcat(str, str2);

	return str;
}

/* Load WLAN credentials from EEPROM */
void loadCredentials() {
	EEPROM.begin(512);
	EEPROM.get(0, ssid);
	EEPROM.get(sizeof(ssid), password);
	EEPROM.get(sizeof(ssid) + sizeof(password), ntpTimeZone); // added timeZone 07/3/2021
	EEPROM.get(sizeof(ssid) + sizeof(password) + sizeof(ntpTimeZone), zipcodeID); // added zipcodeID 03/06/2022
	EEPROM.get(sizeof(ssid) + sizeof(password) + sizeof(ntpTimeZone) + sizeof(zipcodeID), APIKEY); // added API key 03/22/2022
	char ok[2 + 1];
	EEPROM.get(sizeof(ssid) + sizeof(password) + sizeof(ntpTimeZone) + sizeof(zipcodeID) + sizeof(APIKEY), ok);
	EEPROM.end();
	if (String(ok) != String("OK")) {
		ssid[0] = 0;
		password[0] = 0;
		ntpTimeZone[0] = 0;
		zipcodeID[0] = 0;
		APIKEY[0] = 0;
	}
}

/* Store WLAN credentials to EEPROM */
void saveCredentials() {
	EEPROM.begin(512);
	EEPROM.put(0, ssid);
	EEPROM.put(sizeof(ssid), password);
	EEPROM.put(sizeof(ssid) + sizeof(password), ntpTimeZone); // added timeZone 07/3/2021
	EEPROM.put(sizeof(ssid) + sizeof(password) + sizeof(ntpTimeZone), zipcodeID); // added zipcodeID 03/06/2022
	EEPROM.put(sizeof(ssid) + sizeof(password) + sizeof(ntpTimeZone) + sizeof(zipcodeID), APIKEY); // added API key 03/22/2022
	char ok[2 + 1] = "OK";
	EEPROM.put(sizeof(ssid) + sizeof(password) + sizeof(ntpTimeZone) + sizeof(zipcodeID) + sizeof(APIKEY), ok);
	EEPROM.commit();
	EEPROM.end();
}

void EraseEEPROM() {
	int i;
	EEPROM.begin(512);
	for (i = 0; i < 512; i++) {
		EEPROM.put(i, 0);
	}
	EEPROM.commit();
	EEPROM.end();
}
