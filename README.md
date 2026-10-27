# CardioIA Conectada

Projeto desenvolvido no âmbito da FIAP para monitoramento, análise e inteligência em saúde cardiovascular.

## 📌 Visão Geral
O **CardioIA Conectada** integra dispositivos, telemetria clínica e modelos inteligentes para apoiar a detecção precoce e acompanhamento de condições cardíacas.

---
*Repositório sincronizado e configurado via SSH.*

## Fase 3 – Monitoramento Contínuo (IoT)

| Pasta | Conteúdo |
|---|---|
| [`fase3/wokwi/`](fase3/wokwi/) | `sketch.ino`, `diagram.json`, `libraries.txt` (cole os 3 em um projeto **ESP32** novo no Wokwi; o mesmo sketch roda no ESP32-S3 da bancada) |
| [`fase3/node-red/flow.json`](fase3/node-red/flow.json) | Dashboard (requer `node-red-dashboard`) |
| [`fase3/docker-compose.yml`](fase3/docker-compose.yml) | Lab local: broker Mosquitto + Node-RED + simulador de sinais |
| [`fase3/painel/`](fase3/painel/) | Painel web extra (React + Gauge do 21st.dev), lê o MQTT via WebSocket |
| [`fase3/docs/`](fase3/docs/) | Relatórios da Parte 1 (Edge) e da Parte 2 (MQTT + dashboard) |

**Link do projeto Wokwi:** _adicionar após salvar o projeto no Wokwi_

### Lab local (Docker)

```bash
cd fase3
docker compose --profile sim up -d --build
```

- Dashboard Node-RED (entrega): http://localhost:1880/ui · Editor: http://localhost:1880
- Painel web (extra): http://localhost:8080
- O simulador publica sinais falsos a cada 2 s; pare-o com `docker compose stop simulador` quando o ESP32 real estiver ligado.
- ESP32 na bancada: aponte `MQTT_HOST` do firmware para o IP deste PC na rede local (porta 1883 liberada no firewall do Windows).
- **Wokwi → lab:** o ESP32 do Wokwi publica no HiveMQ público e o Mosquitto local tem uma ponte (`docker/mosquitto.conf`) que traz `cardioia/#` para o lab. Node-RED e painel funcionam sem mudar nada.
