#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

#define SS_PIN 2  // SDA pin connected to D4
#define RST_PIN 0 // RST pin connected to D3
#define BUZZER_PIN 5 // Pin connected to the buzzer (D1)

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

const char *ssid = "";
const char *password = "";

void setup() {
  Serial.begin(115200);
  Serial.println(".");
  Serial.println(".");
  Serial.print("Conectandose al Wi-Fi ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.print("Conectado con la IP: ");
  Serial.println(WiFi.localIP());
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522

  Serial.println("RFID-RC522 inicializado.");

  pinMode(BUZZER_PIN, OUTPUT); // Set the buzzer pin as output
}

void loop() {
  // Look for new cards
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("NFC tag detectado!");

    // Get NFC tag UID
    String tagUID = getTagUID();

    // Connect to the server and send the UID
    if (tagUID != "") {
      if (sendTagUID(tagUID)) {
        Serial.println("Tag UID enviado correctamente.");
        // Sound the buzzer
        tone(BUZZER_PIN, 2000); // 2000 Hz frequency
        delay(1000); // Sound duration
        noTone(BUZZER_PIN); // Turn off the buzzer
      } else {
        tone(BUZZER_PIN, 1000); // 1000 Hz frequency
        delay(500); // Sound duration
        noTone(BUZZER_PIN); // Turn off the buzzer
        Serial.println("Error al enviar el Tag UID.");
      }
    } else {
      Serial.println("Error al obtener el Tag UID.");
    }
  }

  mfrc522.PICC_HaltA(); // Stop reading
  delay(1000);
}

String getTagUID() {
  String tagUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    tagUID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    tagUID += String(mfrc522.uid.uidByte[i], HEX);
  }
  return tagUID;
}

bool sendTagUID(String tagUID) {
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient http;
  String url = "https://api-lector-nfc.exdev.cl/" + tagUID;
  Serial.print("Enviando solicitud a: ");
  Serial.println(url);

  if (http.begin(*client, url)) {
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, http.getString());
        if (!error) {
          String status = doc["status"];
          if (status == "ok") {
            String nombre = doc["nombre"];
            String puesto = doc["puesto"];
            Serial.println("Nombre: " + nombre);
            Serial.print("Puesto: " + puesto);
            Serial.println();
            return true;
          } else {
              Serial.println("Error desconocido en la respuesta JSON.");
          }
        } else {
          Serial.println("Error al analizar el JSON.");
        }
      } else {
        if(httpCode == 404) {
          DynamicJsonDocument doc(1024);
          DeserializationError error = deserializeJson(doc, http.getString());
          if(!error) {
            String errorMessage = doc["error"]["mensaje"];
            Serial.println(errorMessage);
          } else {
            Serial.println("Error al analizar el JSON.");
          }
        } else {
          Serial.print("Obtenido codigo de error: ");
          Serial.println(httpCode);
        }
      }
    } else {
      Serial.println("Conexión fallida. Intenta más tarde.");
    }
    http.end();
  } else {
    Serial.println("Failed to connect to server.");
  }
  return false;
}
