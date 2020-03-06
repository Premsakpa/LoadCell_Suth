#include <FirebaseESP8266.h>
#include <FirebaseESP8266HTTPClient.h>
#include <FirebaseJson.h>
#include <jsmn.h>

//-------------------------------------------------------------------------------------
// HX711_ADC.h
// Arduino master library for HX711 24-Bit Analog-to-Digital Converter for Weigh Scales
// Olav Kallhovd sept2017
// Tested with      : HX711 asian module on channel A and YZC-133 3kg load cell
// Tested with MCU  : Arduino Nano, ESP8266
//-------------------------------------------------------------------------------------
// This is an example sketch on how to use this library
// Settling time (number of samples) and data filtering can be adjusted in the config.h file

#include <HX711_ADC.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <TridentTD_LineNotify.h>
#include <WiFiUdp.h>
#include <time.h>
//HX711 constructor (dout pin, sck pin):
HX711_ADC LoadCell(4, 5);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"pool.ntp.org",7*3600);//SET TIMEZONE
const int eepromAdress = 0;

long t;
#define FIREBASE_HOST "https://palert-4741c.firebaseio.com/"
#define FIREBASE_AUTH "lgGc0mXg4gve7egIWT6ifnUv7M5cXQDgEpZqiZOg"
#define WIFI_SSID "Test1"
#define WIFI_PASSWORD "12345678"
FirebaseData firebaseData;
FirebaseJson json;


String Name = "Pall";
float minA;
int i;
String statusW = "Normal Weight";//Lower Weight
int delayLine = 0 , delayData = 0;
String part = "";
String part2 = ""; 
int delaySet = 0;
//===========================================================
void setup() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  //Set database read timeout to 1 minute (max 15 minutes)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");

 
  
  float calValue; // calibration value
  calValue = 696.0; // uncomment this if you want to set this value in the sketch 
  #if defined(ESP8266) 
  //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch the value from eeprom
  #endif
  //EEPROM.get(eepromAdress, calValue); // uncomment this if you want to fetch the value from eeprom
  
  Serial.begin(115200); delay(10);
  Serial.println();
  Serial.println("Starting...");
  LoadCell.begin();
  
  long stabilisingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilising time
  LoadCell.start(stabilisingtime);
  if(LoadCell.getTareTimeoutFlag()) {
    Serial.println("Tare timeout, check MCU>HX711 wiring and pin designations");
  }
  else {
    LoadCell.setCalFactor(calValue); // set calibration value (float)
    Serial.println("Startup + tare is complete");
  }

  configTime(7*3600, 0, "pool.ntp.org", "time.nist.gov");     //ดึงเวลาจาก Server
    Serial.println("\nWaiting for time");
    while (!time(nullptr)) {
      Serial.print(".");
      delay(1000);
      }
   String lineToken;

  
  if (Firebase.get(firebaseData, "/SettingBoard/Token"))
    {
      Serial.println("PASSED");
      
      lineToken = firebaseData.stringData();
      Serial.println(lineToken);
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }

    configTime(7*3600,0, "pool.ntp.org", "time.nist.gov");     //ดึงเวลาจาก Server
    Serial.println("\nWaiting for time");
    while (!time(nullptr)) {
      Serial.print(".");
      delay(1000);
    }
    
  
  LINE.setToken(lineToken);
  //ใช่ส่งข้อความ
  LINE.notify("Welcome my BOSS."); 
pinMode(13, OUTPUT);
}
//==================================================================================


void loop() {
  
  weight();
  String da = date();
  String ti = times();
//light --------------------------------------------------------------------------------
if(i<minA){
digitalWrite(13, HIGH);   
  delay(500);  
}            
  else{digitalWrite(13, LOW);   
  delay(500);}            
 //light --------------------------------------------------------------------------------
    if(delaySet <= 0){
   settingloadcell();
    delaySet = 60*1;
    }
     delaySet--;
     aleartLine();
     
    part =  "/Data/" + Name + "/" + da + "/" + ti;
    part2 =  "/call/" + Name;

    if(delayData <= 0){
    saveData();
    delayData = 60*1;
    }
    delayData--;
  
  delay(1000*1);
}
//====================================================================================
//START dateTime
String date(){
  configTime(7*3600,0, "pool.ntp.org", "time.nist.gov");    //ดีงเวลาปัจจุบันจาก Server อีกครั้ง
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  //Serial.print(p_tm->tm_mon);
  String d = String((p_tm->tm_year)+1900)+":"+
             String((p_tm->tm_mon)+1)+":"+
             String((p_tm->tm_mday));
             
  Serial.println(d);
  Serial.println("-------------------------------");
             
  return(d);
}
String times(){
  
  timeClient.update();
  String t = String(timeClient.getFormattedTime());
  Serial.println(t);
  Serial.println("-------------------------------");

  return(t);
}
//END dateTime

//STRAT weight
void weight(){
  
  //update() should be called at least as often as HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS
  //use of delay in sketch will reduce effective sample rate (be carefull with use of delay() in the loop)
  LoadCell.update();

  //get smoothed value from data set
  if (millis() > t + 250) {
    i = LoadCell.getData()*3.175257732;
    Serial.print("Load_cell output val: ");
    Serial.println(i);
    t = millis();
  }

  //receive from serial terminal
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  //check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }
  
  }
//END weight

//START
void settingloadcell(){

  if (Firebase.get(firebaseData, "/SettingBoard/"+Name+"/SetMin"))
    {
      Serial.println("PASS");
      minA = firebaseData.floatData();
      Serial.println(minA);
    }
    else
    {
      Serial.println("FAIL");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("-------------------------------");

}
//END temperatureDTH

//START=========================================
  void aleartLine(){
   statusW = "Normal Weight";
   if(i<minA){
    if(delayLine <= 0){
    LINE.notify(String(i)+"น้ำหนักต่ำกว่ากำหนด");
    delayLine = 60*1;
    }
    statusW = "Lower Weight";
   }
   if(delayLine>0){
    delayLine--;
    }
  }
//END=======================================
 
  void saveData(){

   if (Firebase.set(firebaseData , part+"/weight" , i))
    {
      Serial.println("SAVE PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");

    if (Firebase.set(firebaseData , part+"/status" , statusW))
    {
      Serial.println("SAVE PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");

  
 //****************************************************************
   if (Firebase.set(firebaseData , part2+"/weight" , i))
    {
      Serial.println("SAVE PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");

    if (Firebase.set(firebaseData , part2+"/status" , statusW))
    {
      Serial.println("SAVE PASSED");
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
    Serial.println("------------------------------------");

  }  
 
