#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#define DEBUG_MODE true  // Habilita logs detalhados no Serial Monitor

SoftwareSerial espSerial(D7, D8); // RX, TX (conectar ao Arduino)
String comando = "";
bool conectado = false;

void setup() {
  Serial.begin(115200);
  espSerial.begin(9600);
  WiFi.mode(WIFI_STA);
  
  if(DEBUG_MODE) {
    Serial.println();
    Serial.println("ESP8266 Inicializado");
    Serial.println("Aguardando comandos...");
  }
}

void loop() {
  if (espSerial.available()) {
    comando = espSerial.readStringUntil('\n');
    comando.trim();
    
    if(DEBUG_MODE) {
      Serial.print("Comando recebido: ");
      Serial.println(comando);
    }

    String resposta;

    if (comando == "LISTAR") {
      resposta = listarRedes();
    } 
    else if (comando.startsWith("LIGAR:")) {
      resposta = conectarRede(comando);
    } 
    else if (comando.startsWith("SENDWEB#:#")) {
      resposta = processarEnvioWeb(comando);
    } 
    else if (comando == "DESCONECTAR") {
      resposta = desconectarWiFi();
    } 
    else {
      resposta = "Comando inválido!";
    }

    espSerial.println(resposta + " OK");
    
    if(DEBUG_MODE) {
      Serial.print("Resposta enviada: ");
      Serial.println(resposta);
    }
  }
  delay(10);
}

String processarEnvioWeb(String comando) {
  String restante = comando.substring(10);
  int separador = restante.indexOf("|||");
  
  if (separador == -1) {
    return "Formato incorreto. Use: SENDWEB#:#URL|||JSON";
  }
  
  String url = restante.substring(0, separador);
  String json = restante.substring(separador + 3);
  
  if(DEBUG_MODE) {
    Serial.print("URL: ");
    Serial.println(url);
    Serial.print("JSON: ");
    Serial.println(json);
  }
  
  return sendweb_custom(url, json);
}

String sendweb_custom(String url, String jsonBody) {
  if (!conectado) {
    return "Não conectado ao Wi-Fi.";
  }

  WiFiClientSecure client;
  HTTPClient https;
  
  // Configurações para HTTPS
  client.setInsecure(); // Ignora verificação de certificado SSL
  client.setTimeout(15000); // 15 segundos de timeout
  
  if(DEBUG_MODE) {
    Serial.println("Iniciando conexão HTTPS...");
  }
  
  if (!https.begin(client, url)) {
    return "Falha ao iniciar conexão HTTPS";
  }

  // Configura cabeçalhos
  https.addHeader("Content-Type", "application/json");
  https.setTimeout(15000);
  
  if(DEBUG_MODE) {
    Serial.println("Enviando requisição POST...");
  }
  
  int httpCode = https.POST(jsonBody);

  if (httpCode > 0) {
    String payload = https.getString();
    
    if(DEBUG_MODE) {
      Serial.printf("Código HTTP: %d\n", httpCode);
      Serial.print("Resposta: ");
      Serial.println(payload);
    }
    
    https.end();
    return payload;
  } else {
    String erro = https.errorToString(httpCode);
    
    if(DEBUG_MODE) {
      Serial.print("Erro HTTP: ");
      Serial.println(erro);
    }
    
    https.end();
    return "Erro POST: " + erro;
  }
}

String listarRedes() {
  if(DEBUG_MODE) {
    Serial.println("Escaneando redes WiFi...");
  }
  
  String saida = "Redes disponíveis:\n";
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    return "Nenhuma rede encontrada";
  }
  
  for (int i = 0; i < n; ++i) {
    saida += String(i + 1) + ": " + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)";
    saida += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " [Aberta]\n" : " [Protegida]\n";
  }
  
  return saida;
}

String conectarRede(String comando) {
  comando.remove(0, 6); // Remove "LIGAR:"
  int separador = comando.indexOf(':');
  
  if (separador == -1) {
    return "Formato incorreto. Use: LIGAR:SSID:SENHA";
  }
  
  String ssid = comando.substring(0, separador);
  String senha = comando.substring(separador + 1);
  
  if(DEBUG_MODE) {
    Serial.print("Conectando a: ");
    Serial.println(ssid);
  }
  
  WiFi.begin(ssid.c_str(), senha.c_str());
  
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 30000) {
    delay(500);
    if(DEBUG_MODE) Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    conectado = true;
    String ip = WiFi.localIP().toString();
    
    if(DEBUG_MODE) {
      Serial.println("\nConectado!");
      Serial.print("IP: ");
      Serial.println(ip);
    }
    
    return "Conectado! IP: " + ip;
  } else {
    return "Falha ao conectar. Status: " + String(WiFi.status());
  }
}

String desconectarWiFi() {
  if (conectado) {
    WiFi.disconnect();
    conectado = false;
    delay(1000);
    return "WiFi desconectado";
  }
  return "Já desconectado";
}