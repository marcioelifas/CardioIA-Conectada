/*
 * CardioIA Conectada - Fase 3: Monitoramento Contínuo (IoT na Saúde)
 * Placas: ESP32-S3 DevKitC-1 N16R8 (bancada) | ESP32 DevKit V1 (Wokwi)
 *
 * Fluxo: captura -> buffer local (Edge) -> MQTT (nuvem) -> Node-RED (dashboard/alerta)
 *
 * Sensores:
 *   - DHT22 (AM2302): temperatura + umidade  (conta como 1 sensor)
 *   - Botão de pulso: cada toque = 1 batimento -> BPM calculado pelos intervalos
 *   - PIR HC-SR501: presença de movimento do paciente
 * Atuadores / interface:
 *   - LED vermelho de alerta
 *   - Display ILI9341 2.4" com os sinais vitais
 *   - Chave deslizante: simula Wi-Fi "conectado" / "desconectado"
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ---------------- Pinos ----------------
// O mesmo sketch roda nas duas placas: ESP32-S3 (bancada) e ESP32 clássico (Wokwi,
// onde o DHT22 não responde na simulação do S3). No clássico, GPIO 6-11 são da flash.
#if CONFIG_IDF_TARGET_ESP32S3
#define PIN_DHT     4
#define PIN_PIR     5
#define PIN_BEAT    6    // botão de pulso (INPUT_PULLUP, ativo em LOW)
#define PIN_WIFI_SW 7    // chave: HIGH = online, LOW = offline
#define PIN_LED     15   // LED de alerta
#define TFT_SCK     12
#define TFT_MOSI    11
#define TFT_MISO    13
#define TFT_CS      10
#define TFT_DC      9
#define TFT_RST     14
#else
#define PIN_DHT     15
#define PIN_PIR     27
#define PIN_BEAT    26
#define PIN_WIFI_SW 25
#define PIN_LED     2
#define TFT_SCK     18
#define TFT_MOSI    23
#define TFT_MISO    19
#define TFT_CS      5
#define TFT_DC      4
#define TFT_RST     22
#endif

// ---------------- Rede / MQTT ----------------
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
// ponytail: broker público sem TLS para facilitar a demo; em produção usar
// HiveMQ Cloud (porta 8883 + WiFiClientSecure + usuário/senha).
const char* MQTT_HOST  = "broker.hivemq.com";
const int   MQTT_PORT  = 1883;
const char* MQTT_TOPIC = "cardioia/g7f3a/paciente01/vitals";

// ---------------- Regras clínicas (limites do grupo) ----------------
const float TEMP_MAX   = 38.0;   // febre
const int   BPM_MAX    = 120;    // taquicardia
const int   BPM_MIN    = 50;     // bradicardia
const unsigned long NO_MOTION_MS = 60000;  // 1 min sem movimento

// ---------------- Edge: buffer offline ----------------
// 1 amostra a cada 2 s. 3600 amostras = 2 h de autonomia offline (~43 KB de RAM).
// Política: buffer circular - quando cheio, a amostra mais antiga é descartada,
// pois para o monitoramento a leitura mais recente é a mais relevante.
const unsigned long SAMPLE_MS = 2000;
const int BUFFER_CAP = 3600;

struct Sample {
  uint32_t t;      // segundos desde o boot
  int16_t  temp;   // °C x10
  uint8_t  hum;    // %
  uint8_t  bpm;
  bool     motion;
  bool     alert;
};

Sample buf[BUFFER_CAP];
int head = 0;      // índice da amostra mais antiga
int count = 0;     // amostras guardadas
unsigned long dropped = 0;

// ---------------- Estado ----------------
DHT dht(PIN_DHT, DHT22);
WiFiClient net;
PubSubClient mqtt(net);
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

bool wifiOnline = false;          // conectividade simulada (variável booleana)
unsigned long beats[5];           // instantes dos últimos 5 batimentos
int beatCount = 0;
bool lastBeatBtn = HIGH;
unsigned long lastBeatMs = 0;
unsigned long lastMotionMs = 0;
unsigned long lastSampleMs = 0;
unsigned long lastMqttTry = 0;

// ---------------- Batimentos ----------------
void readBeatButton() {
  bool now = digitalRead(PIN_BEAT);
  unsigned long ms = millis();
  if (lastBeatBtn == HIGH && now == LOW && ms - lastBeatMs > 250) {  // borda + debounce (máx 240 bpm)
    for (int i = 4; i > 0; i--) beats[i] = beats[i - 1];
    beats[0] = ms;
    if (beatCount < 5) beatCount++;
    lastBeatMs = ms;
  }
  lastBeatBtn = now;
}

// BPM = 60000 / média dos intervalos entre os últimos toques.
// Sem toque há mais de 5 s -> 0 (sem leitura).
int currentBpm() {
  if (beatCount < 2 || millis() - beats[0] > 5000) return 0;
  unsigned long avg = (beats[0] - beats[beatCount - 1]) / (beatCount - 1);
  return avg ? min(255UL, 60000UL / avg) : 0;
}

// ---------------- Buffer ----------------
void pushSample(const Sample& s) {
  int tail = (head + count) % BUFFER_CAP;
  buf[tail] = s;
  if (count < BUFFER_CAP) count++;
  else { head = (head + 1) % BUFFER_CAP; dropped++; }  // sobrescreve a mais antiga
}

void toJson(const Sample& s, char* out, size_t len) {
  snprintf(out, len,
    "{\"t\":%lu,\"temp\":%.1f,\"hum\":%u,\"bpm\":%u,\"motion\":%d,\"alert\":%d,\"pending\":%d}",
    (unsigned long)s.t, s.temp / 10.0, s.hum, s.bpm, s.motion, s.alert, count - 1);
}

// Envia do mais antigo ao mais novo. Só remove do buffer se o publish deu certo:
// se a conexão cair no meio, o restante continua guardado.
// ponytail: PubSubClient só publica QoS 0 (true = escrito no TCP, sem PUBACK);
// para entrega confirmada, migrar para esp-mqtt com QoS 1.
void syncBuffer() {
  while (count > 0) {
    char json[160];
    toJson(buf[head], json, sizeof(json));
    if (!mqtt.publish(MQTT_TOPIC, json)) break;
    Serial.printf("[CLOUD] %s\n", json);
    head = (head + 1) % BUFFER_CAP;
    count--;
  }
}

// ---------------- Conectividade ----------------
void updateConnectivity() {
  bool was = wifiOnline;
  wifiOnline = digitalRead(PIN_WIFI_SW) == HIGH && WiFi.status() == WL_CONNECTED;
  if (wifiOnline != was) Serial.printf("[NET] %s (chave=%d, wifi=%d)\n", wifiOnline ? "online" : "offline",
                                       digitalRead(PIN_WIFI_SW), WiFi.status() == WL_CONNECTED);
  if (!wifiOnline) { if (mqtt.connected()) mqtt.disconnect(); return; }
  if (!mqtt.connected() && millis() - lastMqttTry > 5000) {
    lastMqttTry = millis();
    String id = "cardioia-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.println(mqtt.connect(id.c_str()) ? "[MQTT] conectado" : "[MQTT] falha, tentando de novo em 5 s");
  }
}

// ---------------- Display ----------------
// Limpa a tela só quando o estado de alerta muda; no resto, o texto com fundo
// sobrescreve o anterior (redesenhar 320x240 a cada amostra deixa tudo lento).
void drawScreen(const Sample& s) {
  static int lastAlert = -1;
  uint16_t bg = s.alert ? ILI9341_RED : ILI9341_BLACK;
  if (s.alert != lastAlert) { tft.fillScreen(bg); lastAlert = s.alert; }
  tft.setTextColor(ILI9341_WHITE, bg);
  tft.setTextSize(2);
  tft.setCursor(10, 10);  tft.print("CardioIA - Paciente 01");
  tft.setTextSize(3);
  tft.setCursor(10, 60);  tft.printf("BPM:  %-3u", s.bpm);
  tft.setCursor(10, 100); tft.printf("Temp: %.1fC", s.temp / 10.0);
  tft.setCursor(10, 140); tft.printf("Umid: %u%%", s.hum);
  tft.setTextSize(2);
  tft.setCursor(10, 190); tft.print(s.motion ? "Movimento: SIM" : "Movimento: NAO");
  tft.setCursor(10, 220); tft.print(wifiOnline ? (mqtt.connected() ? "ONLINE    " : "CONECTANDO") : "OFFLINE   ");
  tft.setCursor(10, 250); tft.printf("Buffer: %-4d/%d", count, BUFFER_CAP);
  if (s.alert) { tft.setCursor(10, 285); tft.print("!!! ALERTA !!!"); }
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_BEAT, INPUT_PULLUP);
  pinMode(PIN_WIFI_SW, INPUT);
  pinMode(PIN_LED, OUTPUT);
  dht.begin();

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASS);  // conecta em segundo plano; a chave decide se "está online"
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(60);  // padrão é 15 s; com rede/simulador lento o broker derrubava a conexão
  Serial.println("CardioIA iniciado");
}

void loop() {
  delay(10);  // libera a CPU (no Wokwi acelera muito a simulação); 10 ms não perde toques do botão
  readBeatButton();
  if (digitalRead(PIN_PIR)) lastMotionMs = millis();
  updateConnectivity();
  mqtt.loop();
  if (wifiOnline && mqtt.connected()) syncBuffer();  // a cada volta: descarrega logo após reconectar

  if (millis() - lastSampleMs < SAMPLE_MS) return;
  lastSampleMs = millis();

  float temp = dht.readTemperature(), hum = dht.readHumidity();
  if (isnan(temp) || isnan(hum)) { Serial.println("[DHT] leitura inválida, amostra ignorada"); return; }

  Sample s;
  s.t      = millis() / 1000;
  s.temp   = (int16_t)(temp * 10);
  s.hum    = (uint8_t)hum;
  s.bpm    = currentBpm();
  s.motion = millis() - lastMotionMs < 3000;
  bool noMotion = millis() > NO_MOTION_MS && millis() - lastMotionMs > NO_MOTION_MS;
  s.alert  = temp > TEMP_MAX || s.bpm > BPM_MAX || (s.bpm > 0 && s.bpm < BPM_MIN) || noMotion;

  digitalWrite(PIN_LED, s.alert);   // alerta local funciona mesmo offline (Edge)
  pushSample(s);                     // toda leitura passa pelo buffer
  Serial.printf("[EDGE] T=%.1f H=%u BPM=%u MOV=%d ALERTA=%d buffer=%d descartadas=%lu %s\n",
                temp, s.hum, s.bpm, s.motion, s.alert, count, dropped,
                wifiOnline ? "online" : "offline");

  drawScreen(s);
}
