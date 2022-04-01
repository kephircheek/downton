//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

// определение режима соединения и подключение библиотеки RemoteXY
#define REMOTEXY_MODE__ESP8266WIFI_LIB
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <RemoteXY.h>

// дополнительные библиотеки
#include <PubSubClient.h> // MQTT https://github.com/knolleary/pubsubclient/
#include <ArduinoJson.h>

// самодеятельность
#if __has_include("Secrets.h")
#include "Secrets.h":
#endif

#define FIRMWARE_VERSION "0.1.0"

// #define DEVICE_ID "..."
// #define DEVICE_MAC "..."
#define DEVICE_PUBLISH_INVERVAL 30
#define DEVICE_RECONNECTING_INTERVAL 60

// настройки соединения
#define REMOTEXY_WIFI_SSID "DowntonAbbey"
#define REMOTEXY_WIFI_PASSWORD "Ipad64gb"
#define REMOTEXY_SERVER_PORT 6377


// конфигурация интерфейса
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] =   // 51 bytes
  { 255,1,0,0,0,44,0,16,31,1,2,0,20,47,22,11,2,26,31,31,
  79,78,0,79,70,70,0,129,0,16,26,29,6,24,208,147,208,184,209,128,
  208,187,209,143,208,189,208,180,208,176,0 };

// структура определяет все переменные и события вашего интерфейса управления
struct {

    // input variables
  uint8_t switch_1; // =1 если переключатель включен и =0 если отключен

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;
#pragma pack(pop)

/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////

#define PIN_SWITCH_1 D3

// #define WIFI_SSID "..."
// #define WIFI_PASSWORD "..."

// #define MQTT_ENDPOINT "..."
// #define MQTT_PORT 1883
// #define MQTT_USER "..."
// #define MQTT_PASSWORD "..."
#define MQTT_MAX_BUFFER_SIZE 512

// глобальные переменные
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long publishTimestamp = 0;
unsigned long wifiClientConnectingTimestamp = 0;
unsigned long mqttClientConnectingTimestamp = 0;


void setup()
{
  RemoteXY_Init ();

  pinMode (PIN_SWITCH_1, OUTPUT);

   Serial.begin(115200);

   wifiClientSetup();
    httpServerSetup();
    mqttClientSetup();
    ledSetup();


  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  WiFi.mode(WIFI_STA);

WiFi.disconnect();

   // подключаемся к WiFi-сети:
  Serial.println();
  Serial.print("Connecting to ");  //  "Подключаемся к "
  Serial.println(REMOTEXY_WIFI_SSID);

  WiFi.begin(REMOTEXY_WIFI_SSID, REMOTEXY_WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
             //  "Подключение к WiFi выполнено"
  Serial.print("IP address: ");
   // печатаем IP-адрес ESP:
  Serial.println(WiFi.localIP());
  // TODO you setup code

}

void loop()
{
  RemoteXY_Handler ();

  digitalWrite(PIN_SWITCH_1, (RemoteXY.switch_1==0)?HIGH:LOW);

   if (WiFi.status() == WL_CONNECTED) {
        httpServer.handleClient();
        if (mqttClient.connected()) {
            mqttClient.loop();
        } else {
            ledBlink(5);
        }

    } else {
        ledBlink(5);
    }

    // unblocking MQTT broker reconnection
    unsigned long mqttClientConnectingInverval = DEVICE_RECONNECTING_INTERVAL * 1e6;
    unsigned long mqttClientConnectingDelay = micros() - mqttClientConnectingTimestamp;
    if (mqttClientConnectingDelay > mqttClientConnectingInverval && !mqttClient.connected()) {
        Serial.println("[mqtt] connection lost, retrying...");
        mqttClientConnectingTimestamp = micros();
        mqttClientConnect();
        mqttClientSubscribe();
    }

    // unblocking data publishing
    unsigned long publishInterval = DEVICE_PUBLISH_INVERVAL * 1e6;
    unsigned long publishDelay = micros() - publishTimestamp;
    if (publishDelay > publishInterval) {
        publishTimestamp = micros();
        if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
            DynamicJsonDocument data(MQTT_MAX_BUFFER_SIZE);
     }
    }
  // TODO you loop code
  // используйте структуру RemoteXY для передачи данных
  // не используйте функцию delay()
}

// WIFI Client
void wifiClientSetup() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("[wifi] failed connection to '" WIFI_SSID "', retrying...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    Serial.println("[wifi] connected to '" WIFI_SSID "'.");
}


// HTTP server
void httpServerSetup() {
    httpServer.begin();
    httpServer.on("/", httpCallbackIndex);
    httpServer.on("/ping", httpCallbackPing);

    httpUpdater.setup(&httpServer);
    httpServer.begin();
    Serial.printf(
        "[http] HTTPUpdateServer ready! Open http://%s/update in your browser\n",
        WiFi.localIP().toString().c_str()
    );
}

void httpCallbackPing() {
    ping();
    httpServer.send(200, "text/plain", "pong");
}

void httpCallbackIndex() {
    //Print Hello at opening homepage
    char content[1024];
    sprintf(
        content,
        "<!DOCTYPE html>"
        "<html>"
        "    <head>"
        "        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "    </head>"
        "    <body>"
        "        <h1> %s (ESP8266) version: %s</h1>"
        "        <p>"
        "            <a href=\"/update\">Update</a> firmware via web or upload through terminal with:"
        "            <pre>curl -F \"image=@firmware.bin\" http://%s/update</pre>"
        "        </p>"
        "    </body>"
        "</html>",
        DEVICE_ID,
        FIRMWARE_VERSION,
        WiFi.localIP().toString().c_str()
    );
    httpServer.send(200, "text/html", content);
}


// MQTT Client
bool mqttClientConnect() {
    Serial.println("[mqtt] connecting to server...");
    if (mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("[mqtt] connected to server.");
        return true;
    }
    Serial.printf(
        "[mqtt] failed connection to '%s:%d' as '%s', retrying...\n",
        MQTT_ENDPOINT, MQTT_PORT, MQTT_USER
    );
    return false;
}

void mqttClientSetup() {
    mqttClient.setServer(MQTT_ENDPOINT, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(MQTT_MAX_BUFFER_SIZE);

    mqttClientConnect();
    while (!mqttClient.connected()) {
        if (mqttClientConnect()) {
            break;
        } else {
            Serial.println("[mqtt] wait 5 seconds...");
            delay(5000);
        }
    }
    mqttClientSubscribe();

}

void mqttClientSubscribe() {
    mqttClient.subscribe("homeassistant/" DEVICE_ID "/#");
    Serial.println("[mqtt] subscribe on homeassistant/" DEVICE_ID "/#");
}

void mqttClientPublish(char topic[], DynamicJsonDocument payload) {
    char payloadJson[MQTT_MAX_BUFFER_SIZE];
    serializeJson(payload, payloadJson);
    mqttClient.publish(topic, payloadJson);
    delay(100);
    Serial.printf("[mqtt] publish %s %s\n", topic, payloadJson);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("[mqtt] message arrived on '%s':\n", topic);
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    if (!strcmp(topic, "homeassistant/" DEVICE_ID "/ping")) {
        ping();
    } else {
        Serial.printf("[mqtt] unsupported topic: %s\n", topic);
    }
}


// HomeAssistant
DynamicJsonDocument haSensorConfig(char id[]) {
    char uniqID[128];
    sprintf(uniqID, DEVICE_ID "-%s", id);
    DynamicJsonDocument doc(MQTT_MAX_BUFFER_SIZE);
    doc["uniq_id"] = uniqID;
    doc["object_id"] = uniqID;
    doc["state_topic"] = "homeassistant/sensor/" DEVICE_ID "/state";
    char valueTemplate[128];
    sprintf(valueTemplate, "{{ value_json.%s | round(1) }}", id);
    doc["value_template"] =  valueTemplate;
    doc["device"]["name"] = DEVICE_ID;
    doc["device"]["connections"][0][0] = "mac";
    doc["device"]["connections"][0][1] = DEVICE_MAC;
    doc["device"]["sw_version"] = FIRMWARE_VERSION;
    return doc;
}


// Builtin Led
void ledSetup() {
    pinMode(LED_BUILTIN, OUTPUT);
}

void ledBlink(int n) {
    for (size_t i=0; i<n; i++) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
    }
}

// Other
void ping() {
    ledBlink(10);
}
