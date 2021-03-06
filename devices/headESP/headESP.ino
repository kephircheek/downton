// Пример умного устройства для HomeAssistant на основе протокола MQTT.
//
// Необходимые параметры для работы скрипта
// FIRMWARE_VERSION - версия скрипта. например ("1.2.3"). При установке версии
//     рекумедуется руководствоваться правилами "Семантическое Версионирование 2.0.0"
//     https://semver.org/lang/ru/
//
// DEVICE_ID - уникальное имя устройства. Желательно в названии указать
//     расположение устройства, например "InDoorWestESP", то есть
//     внутри дома, западная сторона, устройство ESP8266.
//
// DEVICE_MAC - MAC адрес устройства, например "c4:5b:be:6c:ce:57".
//
// DEVICE_PUBLISH_INTERVAL - интервал времени между публикациями данных в секундах, например 30.
//
// DEVICE_RECONNECTING_INTERVAL - интервал времени между попытками подключения к
//     MQTT брокеру при падении соединения в секундах, например 60.
//
// DEVICE_SENSOR_DHT_<id>_PIN - контакт для датчика  (D5, D6, ...)
//
// DEVICE_SENSOR_DHT_<id>_TYPE - тип датчика (DHT_11, DHT_22, ...)
//
// DEVICE_SENSOR_DHT_<id>_T_ID - уникальное название для сенсора температуры, например "TemperatureHomeMain"
//
// DEVICE_SENSOR_DHT_<id>_H_ID - уникальное название для сенсора влажности, например "HumidityHomeMain"
//
// WIFI_SSID - имя wifi сети.
//
// WIFI_PASSWORD - пароль от wifi сети.
//
// MQTT_ENDPOINT - IP адрес или имя хоста, где расположен MQTT брокер.
//
// MQTT_PORT - номер порта брокера, например 1883
//
// MQTT_USER - имя пользователя брокера MQTT
//
// MQTT_PASSWORD - пароль пользователя брокера MQTT
//
// MQTT_MAX_BUFFER_SIZE 512


// стандартные библиотеки ESP8266 core https://github.com/esp8266/Arduino
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

// дополнительные библиотеки
#include <PubSubClient.h> // MQTT https://github.com/knolleary/pubsubclient/
#include <ArduinoJson.h>
#include <DHT.h> // DHT sensor library https://github.com/adafruit/DHT-sensor-library

// самодеятельность
#if __has_include("Secrets.h")
#include "Secrets.h":
#endif

#define FIRMWARE_VERSION "0.5.6"

// #define DEVICE_ID "..."
// #define DEVICE_MAC "c4:5b:be:6c:ce:57"
#define DEVICE_PUBLISH_INVERVAL 30
#define DEVICE_RECONNECTING_INTERVAL 60

// Указываем параметры сенсоров
#define DEVICE_SENSOR_DHT_HOMEMAIN_PIN D5
#define DEVICE_SENSOR_DHT_HOMEMAIN_TYPE DHT11
#define DEVICE_SENSOR_DHT_HOMEMAIN_T_ID "TemperatureHomeMain"
#define DEVICE_SENSOR_DHT_HOMEMAIN_H_ID "HumidityHomeMain"

#define DEVICE_SENSOR_DHT_OLDHOME_PIN D6
#define DEVICE_SENSOR_DHT_OLDHOME_TYPE DHT22
#define DEVICE_SENSOR_DHT_OLDHOME_T_ID "TemperatureOldHome"
#define DEVICE_SENSOR_DHT_OLDHOME_H_ID "HumidityOldHome"

#define DEVICE_SENSOR_DHT_BATHROOM_PIN D7
#define DEVICE_SENSOR_DHT_BATHROOM_TYPE DHT22
#define DEVICE_SENSOR_DHT_BATHROOM_T_ID "TemperatureBathroom"
#define DEVICE_SENSOR_DHT_BATHROOM_H_ID "HumidityBathroom"


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

// Объявляем сенсоры
DHT dhtHome(DEVICE_SENSOR_DHT_HOMEMAIN_PIN, DEVICE_SENSOR_DHT_HOMEMAIN_TYPE);
DHT dhtOldHome(DEVICE_SENSOR_DHT_OLDHOME_PIN, DEVICE_SENSOR_DHT_OLDHOME_TYPE);
DHT dhtBathroom(DEVICE_SENSOR_DHT_BATHROOM_PIN, DEVICE_SENSOR_DHT_BATHROOM_TYPE);

void setup(void) {
    Serial.begin(9600);
    Serial.println();
    Serial.println("Booting Sketch...");

    wifiClientSetup();
    httpServerSetup();
    mqttClientSetup();
    ledSetup();

    // Отправляем в MQTT брокер описание наших сенсоров
    // Описание создается автоматически по названию сенсора
    // Сейчас поддерживаются только датчики температуры и влажности
    haTemperatureSensorSetup(DEVICE_SENSOR_DHT_HOMEMAIN_T_ID);
    haHumiditySensorSetup(DEVICE_SENSOR_DHT_HOMEMAIN_H_ID);

    haTemperatureSensorSetup(DEVICE_SENSOR_DHT_OLDHOME_T_ID);
    haHumiditySensorSetup(DEVICE_SENSOR_DHT_OLDHOME_H_ID);

    haTemperatureSensorSetup(DEVICE_SENSOR_DHT_BATHROOM_T_ID);
    haHumiditySensorSetup(DEVICE_SENSOR_DHT_BATHROOM_H_ID);

    // Начинаем считывание данных с сенсоров DHT
    dhtHome.begin();
    dhtOldHome.begin();
    dhtBathroom.begin();

    // other setup below
}

void loop() {
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

            // Собираем данные с сенсоров
            data[DEVICE_SENSOR_DHT_HOMEMAIN_T_ID] = dhtHome.readTemperature();
            data[DEVICE_SENSOR_DHT_HOMEMAIN_H_ID] = dhtHome.readHumidity();

            data[DEVICE_SENSOR_DHT_OLDHOME_T_ID] = dhtOldHome.readTemperature();
            data[DEVICE_SENSOR_DHT_OLDHOME_H_ID] = dhtOldHome.readHumidity();

            data[DEVICE_SENSOR_DHT_BATHROOM_T_ID] = dhtBathroom.readTemperature();
            data[DEVICE_SENSOR_DHT_BATHROOM_H_ID] = dhtBathroom.readHumidity();

            // Отправляет собранные данные в MQTT брокер
            mqttClientPublish("homeassistant/sensor/" DEVICE_ID "/state", data);

        } else {
            Serial.println("[mqtt] data publishing failed because server is not connected.");
        }

    }
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

DynamicJsonDocument haTemperatureSensorConfig(char id[]) {
    DynamicJsonDocument doc = haSensorConfig(id);
    doc["device_class"] = "temperature";
    doc["name"] = "Temperature";
    doc["icon"] = "mdi:thermometer";
    doc["unit_of_measurement"] = "°C";
    return doc;
}

DynamicJsonDocument haHumiditySensorConfig(char id[]) {
    DynamicJsonDocument doc = haSensorConfig(id);
    doc["device_class"] = "humidity";
    doc["name"] = "Humidity";
    doc["icon"] = "mdi:water-percent";
    doc["unit_of_measurement"] = "%";
    return doc;
}

void haTemperatureSensorSetup(char* uniqueSensorID) {
    char sensorConfigTopic[64];
    sprintf(sensorConfigTopic, "homeassistant/sensor/" DEVICE_ID "-%s/config", uniqueSensorID);
    mqttClientPublish(
        sensorConfigTopic,
        haTemperatureSensorConfig(uniqueSensorID)
    );
}

void haHumiditySensorSetup(char* uniqueSensorID) {
    char sensorConfigTopic[64];
    sprintf(sensorConfigTopic, "homeassistant/sensor/" DEVICE_ID "-%s/config", uniqueSensorID);
    mqttClientPublish(
        sensorConfigTopic,
        haHumiditySensorConfig(uniqueSensorID)
    );
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


