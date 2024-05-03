#include "Arduino.h"
#include "esp_camera.h"
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <addons/TokenHelper.h>
#include <ESP32Time.h>
#include <WiFiManager.h>
#include <Ticker.h>

Ticker ticker;

ESP32Time rtc;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -6*3600;
const int daylightOffset_sec = 0;

const char* filePath = "";
const char* bucketPath = "";
String fileName = "";
String bucketName = "";

String url = "https://192.168.100.46:3443/api/access";
String output;
int userId = 25;
bool takeNewPhoto = true;
int btn = 2;
int LED = 33;
int FLASH = 4;

// Insert Firebase project API Key
#define API_KEY "AIzaSyC00R4UpqQpb5QWWzEPyxngSD2Ezf2dlzI"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "jrb.2512@esimez.mx"
#define USER_PASSWORD "putClion2512."

// Insert Firebase storage bucket ID e.g bucket-name.appspot.com
#define STORAGE_BUCKET_ID "access-point-a7882.appspot.com"
// For example:
//#define STORAGE_BUCKET_ID "esp-iot-app.appspot.com"

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

//Define Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

void tick()
{
  digitalWrite(LED, !digitalRead(LED));
}

void fcsUploadCallback(FCS_UploadStatusInfo info);

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  pinMode(btn, INPUT_PULLDOWN);
  pinMode(LED, OUTPUT);
  pinMode(FLASH, OUTPUT);
  ticker.attach(0.6, tick);

  initWiFi();  
  initLittleFS();
  initRtc();
  initCamera();

  //Firebase
  // Assign the api key
  configF.api_key = API_KEY;
  //Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  //Assign the callback function for the long running token generation task
  configF.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("#OK");
}

void initWiFi(){
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  if (!wm.autoConnect()) {
    Serial.println("#E1");
    ESP.restart();
  }
  else {
    ticker.detach();
    delay(250);    
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  // Serial.println("Entered config mode");  
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.print("#C_");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

void initLittleFS(){
  if (!LittleFS.begin(true)) {
    Serial.println("#E2");
    ESP.restart();
  }
  else {
    delay(250);    
  }
}

void initRtc(){
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)){
    rtc.setTimeStruct(timeinfo);
  } else {
    Serial.println("#E3");
    ESP.restart();
  }
}

void initCamera(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Turn-off the 'brownout detector'
  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;    
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;    
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.println("#E4");
    ESP.restart();
  } 
}

void loop() {
  if (digitalRead(btn)){
    takeNewPhoto = false;
    capturePhotoSaveLittleFS();
    savePhoto();
    saveAccess();
  }    
}

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS( void ) {
  // Dispose first pictures because of bad quality
  digitalWrite(FLASH, HIGH);
  camera_fb_t* fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }
    
  // Take a new photo
  fb = NULL;  
  fb = esp_camera_fb_get();  
  if(!fb) {
    // Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  fileName = "/" + rtc.getTime("%F_%T") + ".jpg";
  filePath = (fileName).c_str();
  bucketName = "/data" + fileName;
  bucketPath = (bucketName).c_str();

  // Photo file name
  // Serial.printf("Picture file name: %s\n", filePath);
  File file = LittleFS.open(filePath, FILE_WRITE);

  // Insert the data in the photo file
  if (file) {
    file.write(fb->buf, fb->len); // payload (image), payload length
    // Serial.print("The picture has been saved in ");
    // Serial.print(filePath);
    // Serial.print(" - Size: ");
    // Serial.print(fb->len);
    // Serial.println(" bytes");    
  }
  // else {
  //  Serial.println("Failed to open file in writing mode");
  // }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);

  takeNewPhoto = true;
  digitalWrite(FLASH, LOW);
}

void savePhoto(){
  if (Firebase.ready()){
    // Serial.print("Uploading picture... ");

    //MIME type should be valid to avoid the download problem.
    //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.    
    Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, filePath /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, bucketPath /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */,fcsUploadCallback);
    // if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, filePath /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, bucketPath /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */,fcsUploadCallback)){
    //   Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
    // }
    // else{
    //   Serial.println(fbdo.errorReason());
    // }
  }
}

void saveAccess(){
    if ((WiFi.status()== WL_CONNECTED)) {
    
      HTTPClient client;
      
      client.begin(url);
      client.addHeader("Content-Type", "application/json");

      JsonDocument doc;
      doc["photo_url"] = fbdo.downloadURL();

      doc.shrinkToFit();
      
      serializeJson (doc, output);
      
      int httpCode = client.PATCH(output);

        if(httpCode > 0){
          String payload = client.getString();
          Serial.println("#S");          
          client.end(); 
        } else {
            Serial.printf("[HTTP] PATCH... failed, error: %s\n", client.errorToString(httpCode).c_str());
        }
    } else {
        Serial.println("#F0");
      }
}

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info){
    if (info.status == firebase_fcs_upload_status_init){
        Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
    }
    else if (info.status == firebase_fcs_upload_status_upload)
    {
        Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_upload_status_complete)
    {
        Serial.println("Upload completed\n");
        // FileMetaInfo meta = fbdo.metaData();
        // Serial.printf("Name: %s\n", meta.name.c_str());
        // Serial.printf("Bucket: %s\n", meta.bucket.c_str());
        // Serial.printf("contentType: %s\n", meta.contentType.c_str());
        // Serial.printf("Size: %d\n", meta.size);
        // Serial.printf("Generation: %lu\n", meta.generation);
        // Serial.printf("Metageneration: %lu\n", meta.metageneration);
        // Serial.printf("ETag: %s\n", meta.etag.c_str());
        // Serial.printf("CRC32: %s\n", meta.crc32.c_str());
        // Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
        // Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
        removeFile();
    }
    else if (info.status == firebase_fcs_upload_status_error){
        // Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
        removeFile();
    }
}

void removeFile(){
  if (LittleFS.exists(filePath)){
    LittleFS.remove(filePath);    
  }
}