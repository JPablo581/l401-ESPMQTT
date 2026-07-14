"""
Monitor Flask + MQTT + Supabase
DCSH01 - Evaluación Sumativa 3 - Grupo SHJPC
"""

import os
import threading
from collections import deque
from datetime import datetime

import requests
import paho.mqtt.client as mqtt
from flask import Flask, render_template, request, redirect, url_for
from dotenv import load_dotenv

load_dotenv()

# ---------- Configuración ----------
MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.hivemq.com")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC_PREFIX = os.getenv("TOPIC_PREFIX", "dcsh01/SHJPC")

TOPIC_TEMPERATURA = f"{TOPIC_PREFIX}/nodo1/temperatura"
TOPIC_NIVEL = f"{TOPIC_PREFIX}/nodo1/nivel"
TOPIC_ESTADO = f"{TOPIC_PREFIX}/nodo1/estado"
TOPIC_SERVO_CMD = f"{TOPIC_PREFIX}/servo/comando"

SUPABASE_URL = os.getenv("SUPABASE_URL")
SUPABASE_KEY = os.getenv("SUPABASE_KEY")

app = Flask(__name__)

# ---------- Estado en memoria ----------
estado_sistema = {
    "temperatura": "—",
    "nivel": "—",
    "estado": "—",
    "ultima_actualizacion": "—",
}

log_terminal = deque(maxlen=40)  # últimas 40 líneas del monitor
ultimo_estado_registrado = None  # para no duplicar registros en Supabase


def agregar_log(linea: str):
    hora = datetime.now().strftime("%H:%M:%S")
    log_terminal.appendleft(f"[{hora}] {linea}")


def registrar_evento_supabase(tipo: str, valor: str):
    if not SUPABASE_URL or not SUPABASE_KEY:
        agregar_log("AVISO: Supabase no configurado, evento no registrado")
        return
    try:
        resp = requests.post(
            f"{SUPABASE_URL}/rest/v1/eventos",
            headers={
                "apikey": SUPABASE_KEY,
                "Authorization": f"Bearer {SUPABASE_KEY}",
                "Content-Type": "application/json",
                "Prefer": "return=minimal",
            },
            json={
                "tipo": tipo,
                "valor": valor,
                "nodo": "nodo1",
                "fecha_hora": datetime.now().isoformat(),
            },
            timeout=5,
        )
        if resp.status_code >= 300:
            agregar_log(f"ERROR Supabase ({resp.status_code}): {resp.text[:80]}")
        else:
            agregar_log(f"Evento registrado en Supabase: {tipo}={valor}")
    except requests.RequestException as e:
        agregar_log(f"ERROR de conexión a Supabase: {e}")


# ---------- Callbacks MQTT ----------
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        agregar_log("Conectado al broker MQTT")
        client.subscribe(TOPIC_TEMPERATURA)
        client.subscribe(TOPIC_NIVEL)
        client.subscribe(TOPIC_ESTADO)
    else:
        agregar_log(f"Fallo al conectar a MQTT, código: {reason_code}")


def on_message(client, userdata, msg):
    global ultimo_estado_registrado

    payload = msg.payload.decode(errors="ignore").strip()
    estado_sistema["ultima_actualizacion"] = datetime.now().strftime("%H:%M:%S")

    if msg.topic == TOPIC_TEMPERATURA:
        estado_sistema["temperatura"] = payload
        agregar_log(f"Temperatura recibida: {payload} C")

    elif msg.topic == TOPIC_NIVEL:
        estado_sistema["nivel"] = payload
        agregar_log(f"Nivel recibido: {payload}")

    elif msg.topic == TOPIC_ESTADO:
        estado_sistema["estado"] = payload
        agregar_log(f"Estado del sistema: {payload}")

        # Solo registrar en Supabase cuando el estado CAMBIA (evita duplicar cada 5s)
        if payload != ultimo_estado_registrado:
            registrar_evento_supabase("ESTADO", payload)
            ultimo_estado_registrado = payload


def iniciar_mqtt():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="SHJPC_flask")
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)

    app.config["mqtt_client"] = client
    client.loop_forever()


# ---------- Rutas Flask ----------
@app.route("/")
def index():
    return render_template(
        "monitor.html",
        estado=estado_sistema,
        log=list(log_terminal),
    )


@app.route("/comando", methods=["POST"])
def comando():
    accion = request.form.get("accion")
    client = app.config.get("mqtt_client")

    if accion in ("ABRIR", "CERRAR") and client is not None:
        client.publish(TOPIC_SERVO_CMD, accion)
        agregar_log(f"Comando enviado desde Flask: {accion}")
        registrar_evento_supabase("COMANDO_SERVO", accion)

    return redirect(url_for("index"))


if __name__ == "__main__":
    hilo_mqtt = threading.Thread(target=iniciar_mqtt, daemon=True)
    hilo_mqtt.start()

    app.run(host="0.0.0.0", port=5000, debug=False)
