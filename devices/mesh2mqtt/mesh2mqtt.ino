#include "Secrets.h"
#include <painlessMesh.h>
#include <PubSubClient.h>

#define MQTT_BROKCKER_IS_NOT_CONNECTED !mqttClient.connected()
#define WIFI_IS_NOT_CONNECTED          WiFi.status() != WL_CONNECTED
#define GATEWAY_IS_READY               WiFi.status() == WL_CONNECTED && mqttClient.connected()

const char* firmwareVersion = "1.0.0";

const char* hardwareID = "GATEWAY_MESH2MQTT";
const uint8_t hardwareMAC[] = MESH_ROOT_MAC;
const int hardwareBaudrate = 9600;
const enum WiFiMode hardwareWiFiMode = WIFI_AP_STA;

const char* meshSSID = MESH_SSID;
const char* meshPassword = MESH_PASSWORD;
const int meshPort = MESH_PORT;
const size_t meshPayloadMaxSize = MESH_PAYLOAD_MAX_SIZE;
const char* meshMsgKeyTopic = "topic";
const char* meshMsgKeyPayload = "payload";

const char* stationSSID = STATION_SSID;
const char* stationPassword = STATION_PASSWORD;

const char* mqttEndpoint = MQTT_ENDPOINT;
const int mqttPort = MQTT_PORT;
const size_t mqttPayloadMaxSize = MQTT_PAYLOAD_MAX_SIZE;
const char* mqttUser = MQTT_USER;
const char* mqttPassword = MQTT_PASSWORD;
const char* mqttErrorTopicPrefix = "_error/";

unsigned int n_forwarded = 0;
unsigned int n_received = 0;

void sendStatistic();
void ledBlink();
void ledOff();
void reportWifiClientStatus();
void connectToMQTTBrocker();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void meshReceivedCallback(const uint32_t &id, const String &payload);


painlessMesh mesh;
WiFiClient wifiClient;
PubSubClient mqttClient(mqttEndpoint, mqttPort, mqttCallback, wifiClient);

Scheduler scheduler;
Task taskReportWifiClientStatus(
    5 * TASK_SECOND,
    TASK_FOREVER,
    &reportWifiClientStatus,
    &scheduler,
    false
);
Task taskConnectToMQTTBrocker(
    5 * TASK_SECOND,
    TASK_FOREVER,
    &connectToMQTTBrocker,
    &scheduler,
    false
);
Task taskBlinkLed(
    500,
    TASK_FOREVER,
    &ledBlink,
    &scheduler,
    true,
    NULL,
    &ledOff
);
Task taskSendStatistic(
    5 * TASK_SECOND,
    TASK_FOREVER,
    &sendStatistic,
    &scheduler,
    true
);

uint32_t scanStationChannel(const char* ssid) {
    uint8_t n = WiFi.scanNetworks();
    for (uint8_t i = 0; i < n; i++) {
        if (strcmp(WiFi.SSID(i).c_str(), ssid) == 0) {
            uint32_t channel = WiFi.channel(i);
            Serial.printf("#DEBUG Channel of WiFi network '%s' is %u.\n", ssid, channel);
            return channel;
        }
    }
    Serial.printf("#CRITICAL WiFi network '%s' not found.\n.", ssid);
    return 0;
}

void setup() {
    Serial.begin(hardwareBaudrate);
    Serial.printf(
        "\nBooting '%s' firmware (version=%s) ...\n",
        hardwareID, firmwareVersion
    );

    pinMode(LED_BUILTIN, OUTPUT);

    uint8_t mac[6];
    memcpy(mac, hardwareMAC, 6);
    wifi_set_macaddr(SOFTAP_IF, mac);

    uint32_t stationChannel = 0;
    while(!stationChannel) {
        stationChannel = scanStationChannel(stationSSID);
    }

    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
    mesh.init(meshSSID, meshPassword, &scheduler, meshPort, hardwareWiFiMode, stationChannel);
    mesh.onReceive(&meshReceivedCallback);

    mesh.stationManual(stationSSID, stationPassword);
    mesh.setHostname(hardwareID);

    mesh.setRoot(true);
    mesh.setContainsRoot(true);

    mqttClient.setBufferSize(mqttPayloadMaxSize);
}


void loop() {
    mesh.update();
    if (GATEWAY_IS_READY) {
        taskBlinkLed.disable();
        mqttClient.loop();
    }
    else {
        taskBlinkLed.enableIfNot();
        if (WIFI_IS_NOT_CONNECTED) {
            taskReportWifiClientStatus.enableIfNot();
        }
        else if (MQTT_BROKCKER_IS_NOT_CONNECTED)  {
            taskConnectToMQTTBrocker.enableIfNot();
        }
    }
}


void sendStatistic() {
    std::list nodeList = mesh.getNodeList();
    Serial.printf(
        "#INFO Connected: %u; Received: %u; Forwarded: %u;\n",
        nodeList.size(), n_received, n_forwarded
    );
}


void ledBlink() {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}


void ledOff() {
    digitalWrite(LED_BUILTIN, HIGH); // its mean turn off
}


void reportWifiClientStatus() {
    if (WIFI_IS_NOT_CONNECTED) {
        Serial.printf("#WARNING Gateway wait connection to station '%s'\n", stationSSID);
    } else {
        Serial.printf(
            "#INFO Gateway have connected to station! IP is %s.\n",
            WiFi.localIP().toString().c_str()
        );
        taskReportWifiClientStatus.disable();
    }
}


void connectToMQTTBrocker() {
    bool ok = mqttClient.connect(hardwareID, mqttUser, mqttPassword);
    if (!ok) {
        Serial.printf(
            "#WARNING Gateway is not connected to MQTT broker on %s:%d as '%s'.\n",
            mqttEndpoint, mqttPort, mqttUser
        );
    } else {
        Serial.printf(
            "#INFO Gateway have connected to MQTT broker on %s:%d as '%s'.\n",
            mqttEndpoint, mqttPort, mqttUser
        );
        mqttClient.subscribe("homeassistant/#");
        taskConnectToMQTTBrocker.disable();
    }
}


void meshReceivedCallback(const uint32_t &id, const String &payload) {
    n_received++;
    Serial.printf(
        "#DEBUG Gateway received message via mesh from %u with payload: %s.\n",
        id, payload.c_str()
    );
    if (!(GATEWAY_IS_READY)) {
        Serial.println("#WARNING Gateway is not ready.");
        return;
    }

    DynamicJsonDocument data(meshPayloadMaxSize);
    DeserializationError deserializationError = deserializeJson(data, payload.c_str());
    const char* error;
    if (deserializationError) {
        error = deserializationError.c_str();
    }
    else if (!data.is<JsonObject>()) {
        error = "Message payload is not JSON object";
    }
    else if (!data.containsKey(meshMsgKeyTopic)) {
        error = "Required field 'topic' is missed";
    }
    else if (!data.containsKey(meshMsgKeyPayload)) {
        error = "Required field 'payload' is missed";
    }
    else if (!data[meshMsgKeyPayload].is<const char*>()) {
        error = "MQTT Payload is not a 'const char*'.";
    } else {
        error = 0;
    }

    if (error != 0) {
        Serial.printf("#ERROR %s.\n", error);
        data.clear();
        data["id"] = id;
        data["payload"] = payload;
        data["comment"] = error;
        String mqttPayload;
        serializeJson(data, mqttPayload);
        String topic = mqttErrorTopicPrefix + String(hardwareID);
        bool ok = mqttClient.publish(topic.c_str(), mqttPayload.c_str());
        if (!ok) {
            Serial.println("#CRITICAL Gateway can not dump error to MQTT Broker.");
        }
    } else {
        Serial.println("#DEBUG Message is valid.");
        const char* topic = data[meshMsgKeyTopic];
        const char* mqttPayload = data[meshMsgKeyPayload];
        Serial.print("#DEBUG Publishing... ");
        bool ok = mqttClient.publish(topic, mqttPayload);
        if (ok) {
            n_forwarded++;
            Serial.println("OK.");
            Serial.println("#DEBUG Gateway forward message to MQTT Broker successfully.");
        } else {
            Serial.println("FAIL.");
            Serial.println("#CRITICAL Gateway can not forward message to MQTT Broker.");
        }
    }
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
    n_received++;
    char* payload_ = (char*)malloc(length+1);
    memcpy(payload_, payload, length);
    payload_[length] = '\0';
    Serial.printf(
        "#DEBUG Gateway receive message from MQTT broker with topic '%s': %s.\n",
        topic, payload_
    );

    DynamicJsonDocument payloadMeshDoc(meshPayloadMaxSize);
    payloadMeshDoc[meshMsgKeyTopic] = topic;
    payloadMeshDoc[meshMsgKeyPayload] = payload_;

    String payloadMeshStr;
    serializeJson(payloadMeshDoc, payloadMeshStr);
    bool ok = mesh.sendBroadcast(payloadMeshStr);
    if (ok) {
        n_forwarded++;
        Serial.println("#DEBUG Gateway broadcast MQTT message via mesh successfully.");
    } else {
        Serial.println("#CRITICAL Gateway can not broadcast MQTT message via mesh.");
    }
    free(payload_);
}
