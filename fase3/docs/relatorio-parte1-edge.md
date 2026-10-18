# Relatório – Parte 1: Armazenamento e Processamento Local (Edge Computing)

**Projeto:** CardioIA Conectada – Fase 3: Monitoramento Contínuo
**Plataforma:** ESP32-S3 DevKitC-1 (N16R8), simulado no Wokwi
**Código:** [`wokwi/sketch.ino`](../wokwi/sketch.ino)

## 1. Objetivo

Simular um dispositivo vestível de monitoramento cardíaco que continue funcionando sem internet. O dispositivo coleta sinais vitais, avalia riscos localmente e guarda as leituras até conseguir enviá-las para a nuvem. Esse é o papel do Edge Computing em saúde: a decisão crítica (o alerta) não pode depender da rede.

## 2. Sensores e componentes

| Componente | GPIO | Função |
|---|---|---|
| DHT22 (AM2302) | 4 | Temperatura corporal simulada e umidade do ambiente |
| Botão "Pulso" | 6 | Cada toque representa um batimento; o BPM é calculado pelos intervalos |
| PIR HC-SR501 | 5 | Detecta movimento do paciente |
| Chave deslizante | 7 | Simula a conectividade Wi-Fi (variável booleana `wifiOnline`) |
| LED vermelho + 220 Ω | 15 | Alerta local |
| Display ILI9341 2.4" (SPI) | 9–14 | Mostra sinais vitais, status de rede e ocupação do buffer |

O DHT22 conta como um sensor (temperatura + umidade). O segundo sensor obrigatório é o botão de pulso, e o PIR é um terceiro sensor, que permite detectar a "ausência de movimento".

## 3. Fluxo de funcionamento

```
 loop() contínuo
   ├─ lê botão de pulso (detecção de borda + debounce de 250 ms)
   ├─ lê PIR (registra o último instante com movimento)
   ├─ atualiza conectividade (chave + Wi-Fi + MQTT)
   └─ a cada 2 s:
        1. lê DHT22 (leitura inválida → amostra descartada e registrada no Serial)
        2. calcula BPM = 60000 / média dos intervalos dos últimos 5 toques
        3. aplica as regras de alerta
        4. acende/apaga o LED de alerta   ← funciona offline
        5. grava a amostra no buffer local
        6. se online → sincroniza o buffer (do mais antigo ao mais novo)
        7. atualiza o display
```

**Regras de alerta (processadas no próprio ESP32):**
- Febre: temperatura > 38,0 °C
- Taquicardia: BPM > 120
- Bradicardia: 0 < BPM < 50 (BPM 0 significa "sem leitura", não "coração parado")
- Ausência de movimento: mais de 60 s sem acionar o PIR

## 4. Lógica de resiliência offline

**Princípio:** toda leitura passa pelo buffer, esteja o dispositivo online ou não. Assim existe um único caminho de dados: online o buffer é esvaziado na hora, offline ele acumula. Não existe um "modo offline" separado que possa falhar.

**Armazenamento limitado (dimensionamento):**
- Taxa: 1 amostra a cada 2 s (30 por minuto)
- Capacidade: **3.600 amostras = 2 horas de autonomia offline**
- Cada amostra ocupa cerca de 12 bytes (tempo, temperatura ×10, umidade, BPM, movimento, alerta), o que dá cerca de 43 KB de RAM

**Por que 2 horas?** O cenário de negócio é um paciente em casa usando o vestível. Quedas de Wi-Fi domésticas costumam durar minutos. Duas horas cobrem com folga uma queda de energia ou uma saída do paciente para fora do alcance do roteador. No ESP32-S3 N16R8 real, os 8 MB de PSRAM permitem ampliar para mais de 500 mil amostras (cerca de 12 dias) sem mudar a lógica, só a constante `BUFFER_CAP`.

**Política quando o buffer enche:** buffer circular (FIFO). A amostra mais antiga é sobrescrita e o contador `descartadas` é incrementado e exibido no Serial. A escolha prioriza os dados recentes, que são os relevantes para a conduta clínica. O alerta não se perde, porque é sinalizado localmente pelo LED e pelo display no momento em que ocorre.

**Sincronização:** quando a chave volta para "online" e o MQTT conecta, `syncBuffer()` envia as amostras **do mais antigo para o mais novo**, preservando a ordem temporal. Cada amostra só é removida do buffer **depois** que o `publish` retorna sucesso. Se a conexão cair no meio da sincronização, o envio para e o restante continua guardado. Cada envio também é impresso via `Serial.println("[CLOUD] ...")`, como pede o enunciado.

## 5. SPIFFS e limitação do simulador

O SPIFFS é volátil no Wokwi: os dados se perdem ao encerrar a simulação. Por isso o buffer foi implementado em RAM, e o Monitor Serial serve como registro da resiliência (linhas `[EDGE]` com o tamanho do buffer e `[CLOUD]` na sincronização). No chip físico, a evolução natural é gravar o mesmo buffer em LittleFS/SPIFFS: o ESP32-S3 N16R8 tem 16 MB de flash, o que permite persistir os dados mesmo após uma reinicialização.

## 6. Como testar no Wokwi

1. Inicie a simulação com a chave em **online** e toque em "Pulso" repetidamente. O BPM aparece no display e no Serial.
2. Mude a chave para **offline**. O Serial mostra `buffer=` crescendo e o display mostra "OFFLINE".
3. Ajuste a temperatura do DHT22 para 39 °C. O LED acende e a tela fica vermelha, mesmo offline.
4. Volte a chave para **online**. O Serial mostra a rajada de linhas `[CLOUD]` e o buffer volta a 0.
