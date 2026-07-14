/*
  Nodo Sensor/Actuador - DCSH01 Evaluación Sumativa 3
  Grupo: SHJPC

  Publica temperatura, nivel y estado por MQTT.
  Se suscribe a comando de servo (activación desde Flask).
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ---------- WiFi ----------
const char* ssid = "Pablo";
const char* password = "&369852147&";

// ---------- MQTT ----------
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_client_id = "SHJPC_nodo1";

// Topics
const char* TOPIC_TEMPERATURA = "dcsh01/SHJPC/nodo1/temperatura";
const char* TOPIC_NIVEL       = "dcsh01/SHJPC/nodo1/nivel";
const char* TOPIC_ESTADO      = "dcsh01/SHJPC/nodo1/estado";
const char* TOPIC_SERVO_CMD   = "dcsh01/SHJPC/servo/comando";

// ---------- Pines ----------
#define PIN_DS18B20   4
#define PIN_FLOTANTE  5
#define PIN_SDA       21
#define PIN_SCL       22
#define PIN_SERVO     13
#define PIN_LED_VERDE 14
#define PIN_LED_ROJO  27

// ---------- Objetos ----------
WiFiClient espClient;
PubSubClient client(espClient);

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensores(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo;

unsigned long ultimaPublicacion = 0;
const unsigned long INTERVALO_PUBLICACION = 5000; // 5 segundos

// ---------- WiFi ----------
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

// ---------- Callback MQTT (comandos entrantes) ----------
void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje;
  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  Serial.print("Mensaje recibido en [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(mensaje);

  if (String(topic) == TOPIC_SERVO_CMD) {
    if (mensaje == "ABRIR") {
      servo.write(90);
    } else if (mensaje == "CERRAR") {
      servo.write(0);
    }
  }
}

// ---------- Reconexión MQTT ----------
void reconectarMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando a MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println(" conectado");
      client.subscribe(TOPIC_SERVO_CMD);
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

  sensores.begin();
  pinMode(PIN_FLOTANTE, INPUT_PULLUP);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  servo.attach(PIN_SERVO);
  servo.write(0);

  conectarWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconectarMQTT();
  }
  client.loop();

  unsigned long ahora = millis();
  if (ahora - ultimaPublicacion >= INTERVALO_PUBLICACION) {
    ultimaPublicacion = ahora;

    // ---- Leer sensores ----
    sensores.requestTemperatures();
    float temperatura = sensores.getTempCByIndex(0);
    bool flotanteActivado = (digitalRead(PIN_FLOTANTE) == LOW);
    bool alerta = flotanteActivado || temperatura > 30.0 || temperatura < 0.0;

    // ---- Actuar sobre LEDs ----
    digitalWrite(PIN_LED_VERDE, alerta ? LOW : HIGH);
    digitalWrite(PIN_LED_ROJO, alerta ? HIGH : LOW);

    // ---- Publicar por MQTT ----
    char bufferTemp[10];
    dtostrf(temperatura, 4, 1, bufferTemp);
    client.publish(TOPIC_TEMPERATURA, bufferTemp);
    client.publish(TOPIC_NIVEL, flotanteActivado ? "CRITICO" : "OK");
    client.publish(TOPIC_ESTADO, alerta ? "ALERTA" : "NORMAL");

    // ---- Mostrar en LCD ----
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperatura, 1);
    lcd.print((char)223);
    lcd.print("C     ");
    lcd.setCursor(0, 1);
    lcd.print("Nivel:");
    lcd.print(flotanteActivado ? "ALERTA" : "OK    ");

    Serial.print("Publicado -> Temp: ");
    Serial.print(temperatura);
    Serial.print(" | Nivel: ");
    Serial.print(flotanteActivado ? "CRITICO" : "OK");
    Serial.print(" | Estado: ");
    Serial.println(alerta ? "ALERTA" : "NORMAL");
  }
}
