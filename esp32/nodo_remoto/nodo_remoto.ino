/*
  Nodo Remoto - DCSH01 Evaluación Sumativa 3
  Grupo: SHJPC

  Se suscribe al estado publicado por el nodo sensor/actuador
  y refleja alertas en el LED integrado de la placa.
*/

#include <WiFi.h>
#include <PubSubClient.h>

// ---------- WiFi ----------
const char* ssid = "WIFI_DE_TU_COMPANERO";
const char* password = "PASSWORD_DE_TU_COMPANERO";

// ---------- MQTT ----------
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_client_id = "SHJPC_nodo2";

const char* TOPIC_ESTADO = "dcsh01/SHJPC/nodo1/estado";

// LED integrado - GPIO2 es lo más común en ESP32 DevKit,
// si no prende, prueba GPIO4 o GPIO5
#define PIN_LED_INTEGRADO 2

WiFiClient espClient;
PubSubClient client(espClient);

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje;
  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  Serial.print("Mensaje recibido en [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(mensaje);

  if (mensaje == "ALERTA") {
    digitalWrite(PIN_LED_INTEGRADO, HIGH);
  } else if (mensaje == "NORMAL") {
    digitalWrite(PIN_LED_INTEGRADO, LOW);
  }
}

void reconectarMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando a MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println(" conectado");
      client.subscribe(TOPIC_ESTADO);
    } else {
      Serial.print(" fallo, rc=");
      Serial.print(client.state());
      Serial.println(" reintentando en 2s");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_INTEGRADO, OUTPUT);
  digitalWrite(PIN_LED_INTEGRADO, LOW);

  conectarWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconectarMQTT();
  }
  client.loop();
}
