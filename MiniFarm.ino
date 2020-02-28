#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <MicroGear.h>
#include "DHT.h"

#define ESP_AP_NAME "MiniFarm Config" //ชื่อ wifi 

//OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//netpie
#define APPID   "	"
#define KEY     "	" // device1
#define SECRET  "	"
#define ALIAS   "esp8266"
#define FEEDID   "	"           // ***** FeedID
#define FEEDAPI  "	"        //  FeedAPI
#define TARGETNAME "	"

//sensor Pin ,value,status
#define DHTPIN    D3
#define DHTTYPE   DHT11       // ***** e.g. DHT11, DHT21, DHT22
DHT dht(DHTPIN, DHTTYPE);

float humid = 0;
float temp  = 0;

long lastDHTRead = 0;
long lastTimeWriteFeed = 0;

int cout = 0;
int startcout = 0;
int sensorValue = 0;
int LDRReading = 0;

#define sensorPin  A0
#define LDR_Pin  D4
#define relay1  D7
#define relay2  D8

String status_LEDrelay1; // Water pump (ระบบปั้มน้ำ)
String status_LEDrelay2; // Ventilation system (ระบบระบายอากาศ)
String status_LEDrelay3; //Lighting (ระบบแสงสว่าง)
String status_esp8266 = "True"; // esp8266 system



WiFiClient client;
MicroGear microgear(client);

void onConnected(char *attribute, uint8_t* msg, unsigned int msglen) {
  Serial.println("Connected to NETPIE...");
  microgear.setAlias(ALIAS);
}

void setup()
{
  Serial.begin(115200);
  if (!OLED.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
  } else {
    Serial.println("OLED Start Work !!!");
  }
  OLEDSETUP();
  WiFiManager wifiManager;
  wifiManager.resetSettings(); // go to ip 192.168.4.1 to config
  wifiManager.autoConnect(ESP_AP_NAME);
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  microgear.on(CONNECTED, onConnected);
  microgear.init(KEY, SECRET, ALIAS); // กำหนดค่าตันแปรเริ่มต้นให้กับ microgear
  microgear.connect(APPID);           // ฟังก์ชั่นสำหรับเชื่อมต่อ NETPIE
  dht.begin(); // initialize โมดูล DHT

  //LED traffic
  pinMode(D6, OUTPUT); // R
  pinMode(D5, OUTPUT); // Y
  pinMode(D0, OUTPUT); // G
  //Relay
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);

}
void loop() {

  humid = dht.readHumidity();
  temp  = dht.readTemperature();

  sensorValue = map(analogRead(sensorPin), 1023, 0, 0, 100);

  LDRReading = digitalRead(LDR_Pin);
  OLEDPRINT();
  workstatus();

  if (microgear.connected()) {
    microgear.loop();

    if (millis() - lastDHTRead > 2000) {
      lastDHTRead = millis();

      Serial.print("Humid: ");
      Serial.print(humid);
      Serial.print(" %, ");
      Serial.print("Temp: ");
      Serial.print(temp);
      Serial.print(" C, ");

      Serial.print("lucidity: ");
      Serial.print(LDRReading );
      Serial.print(" , ");

      Serial.print("humidity_Soil: ");
      Serial.print(sensorValue);
      Serial.println(" % ");

      // ตรวจสอบค่า humid และ temp เป็นตัวเลขหรือไม่
      if (isnan(humid) || isnan(temp) || isnan(sensorValue)) {
        Serial.println("Failed to read sensor!");
      }
    }

    if (millis() - lastTimeWriteFeed > 15000) {
      lastTimeWriteFeed = millis();
      if (humid != 0 && temp != 0 && sensorValue != 0) {

        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        root["temp"] = temp;
        root["humidity_Air"] = humid;

        root["lucidity"] = LDRReading;
        root["humidity_Soil"] = sensorValue;

        String jsonData;
        root.printTo(jsonData);

        Serial.print("Write Feed --> ");
        Serial.println(jsonData);
        microgear.writeFeed(FEEDID, jsonData, FEEDAPI);
        /*
          // ส่งแบบระบุตำแหน่ง
          String datafeed = "{\"temp\":" + (String)temp + ",\"humidity_Air\":" + (String)humid + ",\"lucidity\":" + (String)LDRReading + ",\"humidity_Soil\":" + (String)sensorValue + "}";
          Serial.println (datafeed);
          microgear.writeFeed (FEEDID, datafeed);
        */
        String data2freeboard = (String)temp + "," + (String)humid + "," + (String)LDRReading + "," + (String)sensorValue + "," + status_esp8266 + "," + status_LEDrelay1 + "," + status_LEDrelay2 + "," + status_LEDrelay3;
        microgear.chat (TARGETNAME, data2freeboard);
        Serial.println (data2freeboard);
      }
    }
  }
  else {
    Serial.println("connection lost, reconnect...");
    microgear.connect(APPID);
  }
}


int settemp = 35  ;  //อุณภูมิที่กำหนด
int setwater = 50;  //ความชื่นที่กำหนด


void workstatus() { //ควบคุมการทำงานของ รีเลย์


  if (temp <= settemp && sensorValue >= setwater && LDRReading == 0) { // ปกติ ไฟเขียวขึ้น
    ledOn(0, 0, 1);
    digitalWrite(relay1, 1);
    digitalWrite(relay2, 1);
    status_LEDrelay1 = "false";
    status_LEDrelay2 = "false";
    status_LEDrelay3 = "false";
    Serial.println ("ไฟ : เขียว => ปกติ");
  }

  
  if (sensorValue < setwater  ) { // water
    ledOn(0, 1, 0);
    digitalWrite(relay1, 0);
    status_LEDrelay1 = "True";
    Serial.println ("ไฟ : เหลือง  => ดินแห้ง ");
    Serial.println ("relay1");
  }else{
    digitalWrite(relay1, 1);
    status_LEDrelay1 = "false";
  }
  if (LDRReading == 1 ) { // lucidity
    ledOn(1, 1, 0);
    status_LEDrelay3 = "True";
    Serial.println ("ไฟ : แดง เหลือง  => แสงน้อย ");
  }else{
    status_LEDrelay3 = "false";
  }
 
  if (temp > settemp) { // temp
    ledOn(1, 0, 0);
    digitalWrite(relay2, 0);
    status_LEDrelay2 = "True";
    Serial.println ("ไฟ : แดง  => อุณภูมิสูง");
    Serial.println ("relay2");
  }else{
    digitalWrite(relay2, 1);
    status_LEDrelay2 = "false";
  }
}
void ledOn(int pin1, int pin2, int pin3) { // R Y G
  digitalWrite(D0, 0);
  digitalWrite(D5, 0);
  digitalWrite(D6, 0);

  digitalWrite(D6, pin1);
  digitalWrite(D5, pin2);
  digitalWrite(D0, pin3);
  delay(1000);
}

void OLEDSETUP () { //แสดงสถานะก่อนเริ่มทำงาน
  OLED.clearDisplay();
  OLED.setCursor(10, 0);
  OLED.setTextColor(BLACK, WHITE);
  OLED.setTextSize(2);

  OLED.println("MiniFarm");
  OLED.setTextColor(WHITE, BLACK);

  OLED.setTextSize(1);
  OLED.setCursor(0, 20);
  OLED.println("WiFi disconnect");
  OLED.println("AP : MiniFarm Config");
  OLED.println("IP address: ");
  OLED.println("192.168.4.1");
  delay(500);
  OLED.display();
}

void OLEDPRINT () {  //แสดงสถานะทำงาน
  OLED.setCursor(25, 0);
  OLED.clearDisplay();
  OLED.setTextColor(BLACK, WHITE);
  OLED.setTextSize(1);

  OLED.println("MiniFarm");
  OLED.setTextColor(WHITE, BLACK);

  OLED.setCursor(100, 0);
  OLED.println(cout);

  OLED.setTextSize(1);
  OLED.setCursor(0, 10);
  //OLED.println("DHL ");
  OLED.print("Temp : ");
  OLED.println(temp);
  OLED.print("Humidity : ");
  OLED.println(humid);

  OLED.print("Soil Moisture : ");
  OLED.println(sensorValue);

  OLED.print("LDR : ");

  if (LDRReading == 1) {
    OLED.println("Low light :X");
  } else {
    OLED.println("Very light :) ");
  }
  cout++;
  if (WiFi.status() == WL_CONNECTED) {
    OLED.print("WiFi: ");
    OLED.println(WiFi.localIP());
  } else {
    OLED.print("WiFi failed");
  }
  if (microgear.connected()) {
    OLED.print("NETPIE connected :) ");
  } else {
    OLED.print("NETPIE notconnected :( ");
  }
  delay(500);
  OLED.display();
}
