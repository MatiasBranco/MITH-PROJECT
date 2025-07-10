#include <Wire.h>
#include <SoftwareSerial.h>
#include <HDC2080.h>

#define INTERVALO_LEITURA 30000 // 10 segundos entre leituras
#define INTERVALO_ENVIO 600000  // 10 minutos entre envios (600000 ms)
#define NUM_LEITURAS (INTERVALO_ENVIO / INTERVALO_LEITURA) // Número de leituras para média
#define BAUDRATE 9600 // Velocidade de comunicação
#define ADDR 0x40 // Endereço I2C do HDC2080

HDC2080 sensor(ADDR); // Usando a mesma instância do sensor
SoftwareSerial espSerial(11, 10); // RX, TX

bool wifiConectado = false;
unsigned long ultimoEnvio = 0;
unsigned long ultimaLeitura = 0;

float temperaturas[NUM_LEITURAS];
float umidades[NUM_LEITURAS];
int indiceLeitura = 0;
bool bufferCheio = false;

void setup() {
  Serial.begin(BAUDRATE);
  espSerial.begin(BAUDRATE);
  
  // Inicializa o sensor com configurações do programa 2
  Wire.begin();
  sensor.begin();
  
  // Configurações do programa 2
  sensor.reset();
  sensor.setHighTemp(28);         // Temperatura alta: 28 ºC
  sensor.setLowTemp(22);          // Temperatura baixa: 22 ºC
  sensor.setHighHumidity(55);     // Umidade alta: 55%
  sensor.setLowHumidity(40);      // Umidade baixa: 40%
  
  // Configurações de medição (combinando ambos)
  sensor.setMeasurementMode(TEMP_AND_HUMID);
  sensor.setRate(ONE_HZ);
  sensor.setTempRes(FOURTEEN_BIT);
  sensor.setHumidRes(FOURTEEN_BIT);
  sensor.triggerMeasurement();
  
  // Inicializa arrays
  for (int i = 0; i < NUM_LEITURAS; i++) {
    temperaturas[i] = 0;
    umidades[i] = 0;
  }
  
  delay(2000); // Tempo para estabilização
  conectarWiFi("ZTE_B41A62", "74687949");
  //conectarWiFi("NET MB", "Matias#2025");
}

void conectarWiFi(String ssid, String password) {
  String comando = "LIGAR:" + ssid + ":" + password;
  enviarComando(comando, 30000);
}

void enviarComando(String comando, unsigned long timeout) {
  Serial.println("Enviando: " + comando);
  espSerial.println(comando);
  
  unsigned long inicio = millis();
  while (millis() - inicio < timeout) {
    if (espSerial.available()) {
      String resposta = espSerial.readStringUntil('\n');
      resposta.trim();
      Serial.println("Resposta: " + resposta);
      
      if (resposta.indexOf("Conectado") >= 0) {
        wifiConectado = true;
        return;
      }
    }
    delay(100);
  }
  Serial.println("Timeout ao aguardar resposta");
}

String formatarDateTime() {
  static unsigned long lastIncrement = 0;
  static int second = 0, minute = 0, hour = 0, day = 19, month = 6, year = 2025;
  
  if (millis() - lastIncrement >= 1000) {
    lastIncrement = millis();
    second++;
    if (second >= 60) {
      second = 0;
      minute++;
      if (minute >= 60) {
        minute = 0;
        hour++;
        if (hour >= 24) {
          hour = 0;
          day++;
        }
      }
    }
  }

  char buffer[20];
  sprintf(buffer, "%02d-%02d-%04d %02d:%02d:%02d", day, month, year, hour, minute, second);
  return String(buffer);
}

void fazerLeitura() {
  if (millis() - ultimaLeitura >= INTERVALO_LEITURA) {
    // Lê os valores do sensor
    float tempAtual = sensor.readTemp();
    float umidAtual = sensor.readHumidity();
    
    // Verifica se as leituras são válidas
    if (isnan(tempAtual)) {
      Serial.println("Erro na leitura de temperatura");
      tempAtual = 0;
    }
    if (isnan(umidAtual)) {
      Serial.println("Erro na leitura de umidade");
      umidAtual = 0;
    }
    
    // Armazena os valores nos arrays
    temperaturas[indiceLeitura] = tempAtual;
    umidades[indiceLeitura] = umidAtual;
    
    // Exibe os dados no monitor serial (estilo programa 2)
    Serial.print("Temperatura (C): ");
    Serial.print(tempAtual, 2);
    Serial.print("\t\tUmidade (%): ");
    Serial.println(umidAtual, 2);
    
    // Atualiza o índice de leitura
    indiceLeitura++;
    if (indiceLeitura >= NUM_LEITURAS) {
      indiceLeitura = 0;
      bufferCheio = true;
    }
    
    ultimaLeitura = millis();
  }
}

void enviarDadosSensor() {
  // Calcula as médias
  float tempMedia = 0;
  float umidMedia = 0;
  int numLeituras = bufferCheio ? NUM_LEITURAS : indiceLeitura;

  if (numLeituras == 0) {
    Serial.println("Nenhuma leitura disponível para enviar");
    return;
  }

  for (int i = 0; i < numLeituras; i++) {
    tempMedia += temperaturas[i];
    umidMedia += umidades[i];
  }

  tempMedia /= numLeituras;
  umidMedia /= numLeituras;

  Serial.print("Enviando médias - Temp: ");
  Serial.print(tempMedia, 2);
  Serial.print("°C, Umid: ");
  Serial.print(umidMedia, 2);
  Serial.println("%");

  // Monta JSON com só temperatura e umidade numéricos
  String json = "{";
  json += "\"temperatura\":" + String(tempMedia, 2) + ",";
  json += "\"umidade\":" + String(umidMedia, 2);
  json += "}";

  // Monta comando com URL correta
  String comando = "SENDWEB#:#https://mith2025.atspace.cc/config.php|||" + json;
  delay(5000);
  enviarComando(comando, 15000);

  indiceLeitura = 0;
  bufferCheio = false;
}


void loop() {
  fazerLeitura();
  
  if (wifiConectado && millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    enviarDadosSensor();
    ultimoEnvio = millis();
  }
  
  if (espSerial.available()) {
    String resposta = espSerial.readStringUntil('\n');
    resposta.trim();
    Serial.println("Mensagem recebida: " + resposta);
  }
}