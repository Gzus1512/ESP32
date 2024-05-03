//OLED-----------------------------
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
//ESP32-IoT------------------------
#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
//RFID-----------------------------
#include <MFRC522.h>

/* Uncomment the initialize the I2C address , uncomment only one, If you get a totally blank screen try the other*/
#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's
//#define i2c_Address 0x3d //initialize with the I2C addr 0x3D Typically Adafruit OLED's

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SS_PIN 5
#define RST_PIN 4
MFRC522 mfrc522(SS_PIN, RST_PIN);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

int out = 2;
String type_access = "";
String url = "https://192.168.100.46:3443/api/access";
// String url = "https://192.168.1.79:3443/api/access";
String output;
String OldCardID = "";
unsigned long previousMillis = 0;
JsonDocument docRes;
JsonDocument doc;

void setup() {

  Serial.begin(115200);

  pinMode(out, OUTPUT);
  digitalWrite(out, LOW);

  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init();
  display.begin(i2c_Address, true); // Address 0x3C default

  // Clear the buffer.
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  displayMsg(3, "Iniciando...");

  initWiFi();
  Serial.println("Setup listo");
  displayMsg(3, "Presente tarjeta");
}

void initWiFi(){
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  if (!wm.autoConnect()) {
    Serial.println("#E1");
    display.clearDisplay();       // Draw white text
    display.setCursor(15,0);             // Start at top-left corner
    display.print("Error en WiFi");
    display.setCursor(0,20);
    display.print("Reiniciando...");
    delay(1000);
    ESP.restart();
  }
  else {
    delay(250);
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  display.clearDisplay();       // Draw white text
  display.setCursor(10,10);             // Start at top-left corner
  display.print(WiFi.softAPIP());
  display.setCursor(10,30);
  display.print(myWiFiManager->getConfigPortalSSID());
  display.display();
}

void loop() {
  if(!WiFi.isConnected()) {
    initWiFi();    //Retry to connect to Wi-Fi
  }

  displayMsg(3, "Presente tarjeta");

  if (millis() - previousMillis >= 5000) {
    previousMillis = millis();
    OldCardID="";
  }
  delay(50);

  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;//go to start of loop if there is no card present
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;//if read card serial(0) returns 1, the uid struct contians the ID of the read card.
  }

  String CardID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    char hexStr[3];
    sprintf(hexStr, "%02X", mfrc522.uid.uidByte[i]);
    CardID.concat(hexStr);
  }
  //---------------------------------------------
  Serial.println(CardID);
  if( CardID == OldCardID ){
    return;
  }
  else{
    OldCardID = CardID;
  }

  saveAccess(CardID);

  mfrc522.PICC_HaltA(); // Halt PICC
  mfrc522.PCD_StopCrypto1(); // Stop encryption on PCD

}

void displayMsg(int type, String user_name) {

  if(type == 1) {
    type_access = "Bienvenido";
  } else if(type == 2) {
    type_access = "Hasta luego";
  } else if(type == 0) {
    type_access = "Acceso denegado";
  } else {
    type_access = "Control de Acceso";
  }

  display.clearDisplay();       // Draw white text
  display.setCursor(10,10);             // Start at top-left corner
  display.print(type_access);
  display.setCursor(10,30);
  display.print(user_name);
  display.display();
}

void saveAccess(String cardCode) {
    if ((WiFi.status()== WL_CONNECTED)) {
    
      HTTPClient client;
      
      client.begin(url);
      client.addHeader("Content-Type", "application/json");

      doc["code_card"] = cardCode;

      doc.shrinkToFit();
      
      serializeJson (doc, output);
      
      int httpCode = client.POST(output);

        if(httpCode > 0) {
          String payload = client.getString();
          Serial.println(payload);
          client.end();
          digitalWrite(out, HIGH);
          delay(250);
          digitalWrite(out, LOW);
          DeserializationError error = deserializeJson(docRes, payload);

          if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
          }

          const char* name = docRes["name"]; // "Jurgen Mensoza"
          int access_type = docRes["access_type"]; // 1

          displayMsg(access_type, name);
        } else {
            Serial.println(httpCode);
            displayMsg(4, "Intente mas tarde");            
        }
      delay(3000);
    } else {
        Serial.println("#F0");
      }    
}