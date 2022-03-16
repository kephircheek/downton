#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>

#define FIRMWARE_VERSION "0.1.5"

#define DEVICE_ID "vespa"
#define DEVICE_PUBLISH_FREQ 2 // per minute
#define DEVICE_MAC "c4:5b:be:6c:ce:57"

#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."

#define MQTT_ENDPOINT "..." // MQTT Broker ip or hostname
#define MQTT_PORT 1883 // MQTT Broker ip
#define MQTT_USER "..."
#define MQTT_PASSWORD "..."
#define MQTT_MAX_BUFFER_SIZE 512

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long timestamp = micros();

void setup(void) {

  Serial.begin(9600);
  Serial.println();
  Serial.println("Booting Sketch...");
  WiFi.mode(WIFI_AP_STA);

	wifiConnect();

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  Serial.printf(
	  "HTTPUpdateServer ready! Open http://%s/update in your browser\n",
		WiFi.localIP().toString().c_str()
	);

	mqttClient.setServer(MQTT_ENDPOINT, MQTT_PORT);
	mqttClient.setCallback(mqttCallback);
	mqttClient.setBufferSize(MQTT_MAX_BUFFER_SIZE);

  httpServer.begin();
  httpServer.on("/", httpCallbackIndex);
  httpServer.on("/ping", httpCallbackPing);

	// other setup below

}

void loop() {
  httpServer.handleClient();
	wifiConnect();
	mqttConnect();
	mqttClient.loop();

	int delay = 60 * 1e6 / 2;
	if (delay < (micros() - timestamp)) {

		// main part
		float temperature = mesuareTemperature();
		float humidity = mesuareHumidity();
		mqttPublishData(temperature, humidity);
		timestamp = micros();

	}
}

// Return current temperature
float mesuareTemperature() {
	return random(200, 250) / 10.0;
}

// Return current humidity now
float mesuareHumidity() {
	return random(300, 600) / 10.0;
}

void wifiConnect() {
	if (WiFi.status() != WL_CONNECTED) {
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		while (WiFi.waitForConnectResult() != WL_CONNECTED) {
			Serial.printf("Failed connection to '%s', retrying...", WIFI_SSID);
			WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		}
	}
}

void mqttConnect() {
	if (!mqttClient.connected()) {
		while (!mqttClient.connected()) {
			Serial.println("Connecting to MQTT server");
			if (mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD)) {
				Serial.println("Connected to MQTT server");
				mqttClient.subscribe("ping/" DEVICE_ID);
				break;
			}
			Serial.printf("Failed connection to '%s:%d' as '%s', retrying...", MQTT_ENDPOINT, MQTT_PORT, MQTT_USER);
			delay(5000);
		}
	}
}

void mqttPublish(char topic[], char payload[]) {
	mqttClient.publish(topic, payload);
	delay(100);
	Serial.printf("[mqtt] publish %s %s\n", topic, payload);
}

void mqttPublishData(float temperature, float humidity) {
	char topicT[] = "homeassistant/sensor/" DEVICE_ID "T/config";
	char payloadT[] = "{"
		"\"uniq_id\": \"" DEVICE_ID "-t\","
		"\"object_id\": \"" DEVICE_ID "-t\","
		"\"device_class\": \"temperature\","
	  "\"name\": \"Temperature\","
		"\"icon\": \"mdi:thermometer\","
		"\"state_topic\": \"homeassistant/sensor/" DEVICE_ID "/state\","
		"\"unit_of_measurement\": \"°C\","
		"\"value_template\": \"{{ value_json.temperature}}\","
		"\"device\": {"
		"  \"name\": \"" DEVICE_ID "\","
		"  \"connections\": [[\"mac\", \"" DEVICE_MAC "\"]],"
		"  \"sw_version\": \"" FIRMWARE_VERSION "\""
		"}"
	"}";

	char topicH[] = "homeassistant/sensor/" DEVICE_ID "H/config";
	char payloadH[] = "{"
		"\"uniq_id\": \"" DEVICE_ID "-h\","
		"\"object_id\": \"" DEVICE_ID "-h\","
		"\"device_class\": \"humidity\","
	  "\"name\": \"Humidity\","
		"\"icon\": \"mdi:water-percent\","
		"\"state_topic\": \"homeassistant/sensor/" DEVICE_ID "/state\","
		"\"unit_of_measurement\": \"%\","
		"\"value_template\": \"{{ value_json.humidity}}\","
		"\"device\": {"
		"  \"name\": \"" DEVICE_ID "\","
		"  \"connections\": [[\"mac\", \"" DEVICE_MAC "\"]],"
		"  \"sw_version\": \"" FIRMWARE_VERSION "\""
		"}"
	"}";

	char topic[] = "homeassistant/sensor/" DEVICE_ID "/state";
	char payload[128];
	sprintf(payload, "{ \"temperature\": %.2f, \"humidity\": %.2f }", temperature, humidity);

	mqttPublish(topicT, payloadT);
	mqttPublish(topicH, payloadH);
	delay(100);
	mqttPublish(topic, payload);
}

void callbackPing() {
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message arrived [%s] ", topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
	callbackPing();
}

void httpCallbackPing() {
	callbackPing();
	httpServer.send(200, "text/plain", "pong");
}

void httpCallbackIndex() {
  //Print Hello at opening homepage
	char content[1024];
	sprintf(
	  content,
    "<!DOCTYPE html>"
    "<html>"
    "  <head>"
    "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "  </head>"
    "  <body>"
    "    <h1> %s (ESP8266) version: %s</h1>"
		"    <p>Temperature: %.2f</p>"
		"    <p>Humidity: %.2f</p>"
    "    <p>"
		"      <a href=\"/update\">Update</a> firmware via web or upload through terminal with:"
    "      <pre>curl -F \"image=@firmware.bin\" http://%s/update</pre>"
    "    </p>"
    "  </body>"
    "</html>",
		DEVICE_ID,
		FIRMWARE_VERSION,
		mesuareTemperature(),
		mesuareHumidity(),
		WiFi.localIP().toString().c_str()
  );
  httpServer.send(200, "text/html", content);
}
