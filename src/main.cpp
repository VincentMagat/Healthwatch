#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MAX1704x.h>
#include <Adafruit_LC709203F.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <WiFi.h>
#include <string>
#include "time.h"
#include <HttpClient.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64


 // your network password (use for WPA, or use as key for WEP)
char ssid[]= "";
char pass[]="";
const char kHostname[] = "";
// Path to download (this is the bit after the hostname in the URL
// that you want to download
const char kPath[] = "/?Avg=";
const int kport = 5000;

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

const char* ntpServer = "pool.ntp.org";
const long pstoffset_sec=0;
const int daylightOffset_sec = -25200;

MAX30105 particleSensor;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;

int avg;
int beat;
long irValue;
float temperature;
int batteryPin = A13; // analog input pin connected to the battery voltage
float batteryVoltage = 0.0; // variable to hold the battery voltage
int batteryPercentage = 0; // variable to hold the battery percentage




Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);




 

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %y %H:%M:%S");
  oled.setCursor(0,12);
  oled.print(&timeinfo, "%A, %B %d %y");
  oled.setCursor(0,22);
  oled.print(&timeinfo, "%H:%M:%S");
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
 
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());

  
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println("Failed to connect");
    while(1);

  }
  else{
    Serial.println("Success");
  }
 configTime(pstoffset_sec, daylightOffset_sec, ntpServer);
 printLocalTime(); 

 if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

   //Configure sensor with default settings
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0xFF);
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  particleSensor.setPulseAmplitudeIR(0);
  particleSensor.enableDIETEMPRDY();
  
 
}

void heartBeat(){
  long irValue = particleSensor.getRed();
  if (irValue > 50000)
  {
    //We sensed a beat!S
    
    long delta = millis() - lastBeat;
    Serial.println(delta);
    lastBeat = millis();

    beatsPerMinute =  55 + 60/(delta / 1000.0);
    Serial.print("Pulse: ");
    Serial.println(beatsPerMinute);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      avg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        avg += rates[x];
      avg /= RATE_SIZE;
    }
  }
  oled.setCursor(0,30);
  oled.print("IR=");
  oled.print(irValue);
  oled.setCursor(0,38);
  oled.print("BPM:");
  oled.print(beatsPerMinute);
  oled.setCursor(0,46);
  oled.print("Avg:");
  oled.print(avg);

  if (irValue < 40000)
    Serial.print(" No finger?");

  Serial.println();
}
void tempChecker(){
  temperature = particleSensor.readTemperatureF();
  Serial.print("Temperature: ");
  oled.setCursor(60, 38);
  oled.println("Temp:");
  oled.setCursor(90, 38);
  oled.print(temperature);
  Serial.println(temperature);
}
void IRAM_ATTR heartBeat_Interrupt()
{
  heartBeat();
}



void loop() {
  // put your main code here, to run repeatedly:
  
  int rawValue = analogRead(batteryPin);
  batteryVoltage = (rawValue /4095.0) * 3.3 * 2.138; // adjust for voltage divider and ESP32 voltage range
  
  // calculate the battery percentage
  batteryPercentage = ((batteryVoltage - 3.6) / (4.2 - 3.6)) * 100;
  // print the battery percentage   
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(55,0);
  oled.print("Battery:");
  oled.print(batteryPercentage);
  oled.println("%");
  oled.setCursor(2,22);
  oled.setTextSize(0);
  Serial.print("Battery percentage: ");
  Serial.print(batteryPercentage);
  Serial.println("%");
  Serial.println("Raw Value: ");
  Serial.println(rawValue);
  Serial.println(batteryVoltage);
  heartBeat();
  tempChecker();
  printLocalTime();
  oled.display();
  const char* kPathFormat = "/?beatsPerMinute=%.2f&temperature=%.2f";
  char kPath[50];
  snprintf(kPath, sizeof(kPath), kPathFormat, beatsPerMinute, temperature);
  int err =0;
  
  WiFiClient c;
  HttpClient http(c);
  
  err = http.get(kHostname,kport, kPath);
  if (err == 0)
  {
    Serial.println("startedRequest ok");

    err = http.responseStatusCode();
    if (err >= 0)
    {
      Serial.print("Got status code: ");
      Serial.println(err);

      // Usually you'd check that the response code is 200 or a
      // similar "success" code (200-299) before carrying on,
      // but we'll print out whatever response we get

      err = http.skipResponseHeaders();
      if (err >= 0)
      {
        int bodyLen = http.contentLength();
        Serial.print("Content length is: ");
        Serial.println(bodyLen);
        Serial.println();
        Serial.println("Body returned follows:");
        Serial.print("beatsPerMinute: ");
        Serial.println(avg);
      
      
        // Now we've got to the body, so we can print it out
        unsigned long timeoutStart = millis();
        char c;
        // Whilst we haven't timed out & haven't reached the end of the body
        while ( (http.connected() || http.available()) &&
               ((millis() - timeoutStart) < kNetworkTimeout) )
        {
            if (http.available())
            {
                c = http.read();
                // Print out this character
                Serial.print(c);
                
                
               
                bodyLen--;
                // We read something, reset the timeout counter
                timeoutStart = millis();
            }
            else
            {
                // We haven't got any data, so let's pause to allow some to
                // arrive
                delay(kNetworkDelay);
            }
        }
      }
      else
      {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err);
      }
    }
    else
    {    
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  }
  else
  {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }
  http.stop();
  
  
  // And just stop, now that we've tried a download

  

  
}

