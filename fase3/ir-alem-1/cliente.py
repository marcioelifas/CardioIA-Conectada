"""Cliente REST + automação de e-mail (RPA) do CardioIA.

Assina o MQTT, envia cada amostra para a API (POST), lê a avaliação de risco da resposta
e, quando surge um alerta novo, busca o histórico recente (GET) e dispara um e-mail.
O SMTP é o Mailpit do lab (caixa de entrada falsa em http://localhost:8025): nenhum e-mail sai de verdade.
"""
import json
import os
import smtplib
from email.message import EmailMessage

import paho.mqtt.client as mqtt
import requests

API_URL = os.getenv("API_URL", "http://localhost:8000")
MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
SMTP_HOST = os.getenv("SMTP_HOST", "localhost")
SMTP_PORT = int(os.getenv("SMTP_PORT", "1025"))
TOPICO = "cardioia/g7f3a/+/vitals"       # + = qualquer paciente
DESTINO = "plantao@hospital.local"

ultimos_riscos: dict[str, list] = {}       # último alerta enviado por paciente (evita e-mail a cada 2 s)


def enviar_email(paciente: str, riscos: list[str]):
    recentes = requests.get(f"{API_URL}/pacientes/{paciente}/vitals", params={"limite": 5}, timeout=5).json()
    linhas = "\n".join(f"  {r['recebido_em']}  BPM={r['bpm']:<3} T={r['temp']} °C  mov={r['motion']}" for r in recentes)
    msg = EmailMessage()
    msg["From"] = "CardioIA <alertas@cardioia.local>"
    msg["To"] = DESTINO
    msg["Subject"] = f"[CardioIA] ALERTA {paciente}: {', '.join(riscos)}"
    msg.set_content(
        f"Paciente: {paciente}\nRiscos detectados: {', '.join(riscos)}\n\n"
        f"Últimas leituras:\n{linhas}\n\n"
        "Mensagem automática do monitoramento CardioIA. Avalie o paciente."
    )
    with smtplib.SMTP(SMTP_HOST, SMTP_PORT, timeout=5) as smtp:
        smtp.send_message(msg)
    print(f"[EMAIL] {msg['Subject']}")


def ao_receber(cliente, _userdata, m):
    paciente = m.topic.split("/")[2]
    try:
        resp = requests.post(f"{API_URL}/pacientes/{paciente}/vitals", json=json.loads(m.payload), timeout=5)
        resp.raise_for_status()
        riscos = resp.json()["riscos"]
        print(f"[API] {paciente} {m.payload.decode()} -> {riscos or 'normal'}")
        # Só notifica quando muda o TIPO de risco (135 -> 138 bpm não gera outro e-mail);
        # voltar ao normal rearma o envio.
        tipos = [r.split(" (")[0] for r in riscos]
        if tipos and tipos != ultimos_riscos.get(paciente):
            enviar_email(paciente, riscos)
        ultimos_riscos[paciente] = tipos
    except (requests.RequestException, smtplib.SMTPException, OSError, ValueError) as e:
        print(f"[ERRO] {paciente}: {e}")   # não derruba o cliente; a próxima amostra tenta de novo


def ao_conectar(cliente, _userdata, _flags, rc, _props):
    print(f"[MQTT] conectado ({rc}), assinando {TOPICO}")
    cliente.subscribe(TOPICO)


if __name__ == "__main__":
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="cardioia-cliente-rest")
    c.on_connect = ao_conectar
    c.on_message = ao_receber
    c.connect(MQTT_HOST, 1883, 60)
    c.loop_forever(retry_first_connection=True)
