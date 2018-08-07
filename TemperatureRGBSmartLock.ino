#include <ESP8266WiFi.h>
#include <TM1637Display.h>
#include <SimpleDHT.h>
#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <ThingerESP8266.h>

#define RGB_PIN D2
#define SERVO_PIN D0
#define TEMP_PIN D3

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      1

// Hardware used
// WEMOS D1 Mini - ESP8266
// DHT22 - Temperature module
// WS2812 - Breakout module
// SG90 - Servo 

Servo myservo;
SimpleDHT22 dht22;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);
ThingerESP8266 *thing;

char ssid[] = "<WIFI SSID>";  //  your network SSID (name)
char pass[] = "<WIFI PASSWORD>";       // your network password

const char* host = "<INSTANCE_NAME>.service-now.com";
const int httpsPort = 443;

bool getTemperatureHumidity(float&, float&); // get temp

unsigned long int t_SENSE_TEMP = 0;
unsigned long int t_DISPLAY_DELAY = 0;
unsigned long int t_FLASH = 0;

unsigned int g_connectionCounter = 1;

void setup()
{
	pixels.begin(); // This initializes the NeoPixel library.
  	pixels.setBrightness(200);

  	myservo.attach(SERVO_PIN);

	Serial.begin(115200);
	// We start by connecting to a WiFi network
	Serial.print("Connecting to ");
	Serial.println(ssid);
	WiFi.begin(ssid, pass);

	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}

	if(g_connectionCounter) {
		Serial.println("");
		Serial.println("WiFi connected");
		Serial.println("IP address: ");
		Serial.println(WiFi.localIP());
	}

		// define the thing
	thing =  new ThingerESP8266("<THINGER_USERNAME>", "<THINGER_DEVICE_NAME>", "<THINGER_DEVICE_SECRET>");

	// lambda function to control the motor via thinger - define the function as motor()
	(*thing)["setGreen"] << [](pson & in) {
		Serial.print("Green :");
		Serial.println(in.is_empty());

		pixels.setPixelColor(0, pixels.Color(0, 255, 0));
		pixels.show();
	};

	(*thing)["setRed"] << [](pson & in) {
		Serial.print("Red :");
		Serial.println(in.is_empty());

		pixels.setPixelColor(0, pixels.Color(255, 0, 0));
		pixels.show();
	};

	(*thing)["lock"] << [](pson & in) {
		Serial.print("lock :");
		Serial.println(in.is_empty());

		myservo.write(0);
	};

	(*thing)["unlock"] << [](pson & in) {
		Serial.print("unlock :");
		Serial.println(in.is_empty());

		myservo.write(180);
	};

	t_SENSE_TEMP = millis();
	t_FLASH = t_DISPLAY_DELAY = millis();
}

void loop() {

	float temperature, humidity;

	// read the temperature & humidity

	(*thing).handle();

	if(millis() - t_SENSE_TEMP > 6000) {

		if(getTemperatureHumidity(temperature, humidity)) {
			if(pushValueToThinger(temperature, humidity, "myDevice1")) {
				Serial.println("Successfully posted to thinger ...");
			} else {
				Serial.println("Failed to post data to ServiceNow ! Check your internet connectivity ...");
			}
			Serial.print("Temp: ");
			Serial.println(temperature);
			Serial.print("Humidity :");
			Serial.println(humidity);

		} else {
			Serial.println("Unable to read temperature data ... ");
		}
		t_SENSE_TEMP = millis();
	}

	if(millis() - t_FLASH > 500) {
		t_FLASH = millis();
	}

}

bool getTemperatureHumidity(float &temp, float &hum) {
  // read without samples.
  float temperature = 0;
  float humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read2(TEMP_PIN, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Reading DHT22 failed - may not be connected, err=");
    Serial.println(err);
    return false;
  }

  temp = temperature;
  hum = humidity;

  Serial.print("Temperature - ");
  Serial.println(temp);

  Serial.print("Humidity - ");
  Serial.println(hum);

  Serial.println();

  return true;
}

bool pushValueToThinger(float temp, float humidity, String device_id)
{
	pson out;

  	out["temperature"] = temp;
  	out["humidity"] = humidity;

	(*thing).handle();
	if ((*thing).call_endpoint("weatherdata", out) == 1) {
		Serial.println("Posted successfully ...");
		Serial.println("**ThingerManager: a posted to ServiceNow successfully via Thinger ");
		return true;
	}
	Serial.println("Failed to post data ...");
	return false;
}

// Cannot use Thinger and HTTPS at the same time, so choose one.
// This is an IoT Hardware limitation and not a limitation with the server.
bool pushValue(float temp, float humidity, String device_id) {

  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  if (!client.connect(host, httpsPort) || (WiFi.status() != WL_CONNECTED)) {
    return false;
  }

  String url = "/api/now/v2/table/x_snc_iot_temperat_x_snc_iot_temp";
  String jsonContent = "{'temp':'"+ String(temp) +"','humidity':'"+ String(humidity) + "','device_id':'"+ device_id + "'}\r\n";

  Serial.print("Sending value to service now : ");
  Serial.println(jsonContent);

  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Content-Type: application/json\r\n" +
               "Accept: application/json\r\n" +
               "Authorization: Basic YWRtaW46QWRtaW4xMjM=\r\n" +
               "Content-Length: " + jsonContent.length() + "\r\n" +
               "Connection: close\r\n\r\n" +
               jsonContent);

  //  bypass HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    //Serial.println( "Header: " + line );
    if (line == "\r") {
      break;
    }
  }
  //  read body length
  int bodyLength = 0;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    bodyLength = line.toInt();
    break;
  }

  Serial.println("Successfully posted values to ServiceNow instance ... ");

  return true;

}
