#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <RTClib.h>
#include <AsyncDelay.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* token = "qdZuyRItKe5oal0V2pFNQSVUVXnUWQoRiex09lpc4Zr";

#define ver 0.01


#define TRIG_PIN  22 // ESP32 pin GPIO22 เชื่อมต่อเซนเซอร์ Ultrasonic Sensor's TRIG pin
#define ECHO_PIN  23 // ESP32 pin GPIO23 เชื่อมต่อเซนเซอร์ Ultrasonic Sensor's ECHO pin
#define DISTANCE_THRESHOLD 50 // ระยะระวัง ซม.

float duration_us, distance_cm;



#define I2C_SDA 19
#define I2C_SCL 18

#define displayAddress 0x3C
#define DHT_Type DHT22

#define refreshSpeed 10        //(ms) เวลาในการเลื่อน OLED
#define utcOffset -6 * 3600     //(hr) เวลาเชิงกล (UTC) offset
#define utcDST 0 * 1000         //(hr) รีเซ็ตวัน 
#define timeUpdate 24 * 360000  //(hr) เวลาอัพเดท

#define DHT_Pin 19
#define conFan 32
#define water 33
#define pump  35
#define peltierHot 6
#define peltireCold 7


RTC_DS1307 rtc;

TwoWire I2C(0);
DHT dht(DHT_Pin, DHT_Type);
Adafruit_SSD1306 display(128, 64, &I2C, -1);

AsyncDelay refreshDelay;
AsyncDelay timeFetch;

int Yscrl = 100;           //Screen Scroll offset
char sign = 'C';
float temp = 00.00f;
bool isF = true;
float humid = 00.00f;

/*Analog values for PWM control ระยะระหว่างตัวแปร */
int conFanSpeed = 255;    //Range from 0 to 255
int led = 255;            //Range from 0 to 255
int peltier = 0;          //Range from -255 to 255



void setup() {
  Serial.begin(115200);
  pinMode(conFan, OUTPUT);
  pinMode(water, OUTPUT);
  pinMode(pump, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT); // set ESP32 pin to output mode
  pinMode(ECHO_PIN, INPUT);  // set ESP32 pin to input mode

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.println(" READY ");

  I2C.begin(I2C_SDA, I2C_SCL, 400000);
  rtc.begin(&I2C);
  dht.begin();

  display.begin(SSD1306_SWITCHCAPVCC, displayAddress);
  display.cp437(true); //all printable ASCII
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.dim(true);
  display.clearDisplay();


  refreshDelay.start(refreshSpeed, AsyncDelay::MILLIS);
  timeFetch.start(timeUpdate, AsyncDelay::MILLIS);

}

void loop() {
  if (refreshDelay.isExpired()) {
    humid = dht.readHumidity();
    temp = dht.readTemperature();

    displayInfo();

    Yscrl = (Yscrl + 1) % 96;
    ultrasonic();
    automatic();
    refreshDelay.repeat();

  }
  lineNotify();
}

void scroll(String text, int X, int Y) {
  int pos = (Yscrl + Y) % 96;

  if (pos < 8 || pos > 56) {
    display.setCursor(X, pos - 7);
    display.println(text);
    display.setCursor(X, pos + 57);
    display.println(text);
  } else {
    display.setCursor(X, pos - 7);
    display.println(text);
  }
}

void displayInfo() {
  char time[] = "hh:mm:ssap";
  char date[] = "MM-DD-YY";

  display.clearDisplay();
  scroll("PlantEasy " + String(ver), 1, 0);
  display.drawFastHLine(0, (Yscrl) % 96 + 1, 128, WHITE);
  scroll(rtc.now().toString(time), 1, 16);
  scroll(rtc.now().toString(date), 78, 16);
  scroll("T:" + String(temp) + char(248) + 'C', 1, 32);
  scroll("Fan:" + String(String(double (conFanSpeed) / 255)), 80, 32);
  scroll("H:" + String(humid) + "%", 1, 48);
  wf();

  display.display();
}

void automatic() {
  if (temp > 50 ) { // ตั้งค่าอุณหภูมิเพื่อเปิดระบายอากาศ
    digitalWrite(conFan, 1);
  } else {
    digitalWrite(conFan, 0);
  }
  if (humid > 50 ) { // ตั้งค่าอุความชื่นเพื่อเปิดระบบรดน้ำหรือฉีดระอองน้ำ
    digitalWrite(water, 1);
  } else {
    digitalWrite(water, 0);
  }



}

void ultrasonic() {
  digitalWrite(TRIG_PIN, 1);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, 0);
  // measure duration of pulse from ECHO pin
  duration_us = pulseIn(ECHO_PIN, 1);
  // calculate the distance
  distance_cm = 0.017 * duration_us;


}

void wf() {
  if (distance_cm < DISTANCE_THRESHOLD) {
    digitalWrite(pump, 0); //
    scroll("W:FULL ", 80, 48);
  }
  else {
    digitalWrite(pump, 1);  //
    scroll("W:LOW ", 80, 48);
  }
}

void lineNotify() {

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin("https://notify-api.line.me/api/notify");
    http.addHeader("Authorization", "Bearer " + String(token));
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");


    if (temp > 40) {
      http.POST("message=ขณะนี้อุณหภูมิ: "+ String(temp) +" อุณหภูมิสูงเกิน 40 องศา🌡");
    } else if (temp < 0) {
      http.POST("message=ขณะนี้อุณหภูมิ: "+ String(temp) +" อุณหภูมิต่ำเกินไป🌡");
    }
  }

} 
