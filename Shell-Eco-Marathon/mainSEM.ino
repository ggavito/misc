// Shell Eco-Marathon data acquisition code
// pulls speed from NEO-6M gps module, current from DFrobot 50A current sensor, and voltage from 
// tiny BMS s516
// Written by Garet Gavito
#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <Wire.h>

#define SLAVE_ADDRESS 0x03 //SDA pin 18, SCL pin 19

SoftwareSerial gpsSerial(9, 10);
TinyGPS gps;

void readGPS(TinyGPS &gps);

float data[3];
float gpsSpd;
float power;

// current variables
float current;
const int numReadings = 30;
float readings[numReadings];      // the readings from the analog input
int x = 0;                  // the index of the current reading
float total = 0;                  // the running total
float average = 0;                // the average


void setup()  
{
  while(!Serial);
  Serial.begin(9600);
  Serial1.begin(115200);
  gpsSerial.begin(9600);
  pinMode(3, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  
  Wire.begin(SLAVE_ADDRESS);
  Wire.onRequest(sendDataPi);
  for (int thisReading = 0; thisReading < numReadings; thisReading++){
    readings[thisReading] = 0;     
}
  
}

void loop()

{
  Serial.println("in loop");
  bool newdata = false;
  unsigned long start = millis();
  while (millis() - start < 100) 
  {
    if (gpsSerial.available()) 
    
    {
      char c = gpsSerial.read();
      if (gps.encode(c)) 
      {
        newdata = true;
        break;
      }
    }
  }
  
  if (newdata) 
  {
    Serial.println("Acquired Data");
    Serial.println("-------------");
    readGPS(gps);
    Serial.println();
    Serial.println("-------------");
    Serial.println();
  }
  float voltage = readBMSVoltage();
  power = current * voltage;
  data[0] = gpsSpd;
  data[1] = power;
  data[2] = voltage;
  
  if (digitalRead(2)==LOW){ // dead man's switch
    digitalWrite(3, HIGH);
    Serial.println("hall effect active");
  } else {
    digitalWrite(3, LOW);
    Serial.println("hall effect inactive");
  }
}

void readGPS(TinyGPS &gps)
{
  gpsSpd = gps.f_speed_mph();
  
  Serial.print("gps (mph): ");
  Serial.print(gpsSpd);
}

void readCurrent()
// reads current from teensy pin 23
{
    total= total - readings[x];          
    readings[x] = analogRead(23);
    readings[x] = (readings[x]-510)*5/1024/0.04-0.04; // data processing:510-raw data from analogRead when the input is 0; 5-5v; the first 0.04-0.04V/A(sensitivity); the second 0.04-offset val;
    total= total + readings[x];       
    x = x + 1;                    
    if (x >= numReadings)              
      x = 0;                           
    average = total/numReadings;   // smoothing algorithm  
    current = average; // final variable is current
    delay(30);
}

float readBMSVoltage()
// reads pack voltage from tiny BMS s516. rx to pin 1, tx to pin 0, gnd to gnd
{
  byte receivedBytes[8] = {0};
  byte voltageBytes[4] = {0};
  float totalVoltage = 0;
  String content = "";
  int i = 0;
  byte message[] = {0xAA, 0x14, 0x7F, 0x1F}; // address, function code, CRC, CRC
  Serial1.write(message, sizeof(message));
  delay(10);
  
  while (Serial1.available()) {  
    Serial.println("Available"); 
    receivedBytes[i] = Serial1.read();
    Serial.println(receivedBytes[i], HEX);  
    i++;                   
  }                              
  for(int i = 0; i < 4; i++) {
    voltageBytes[i] = receivedBytes[i + 2];
  }
  totalVoltage = *((float*)voltageBytes);

  return totalVoltage;
}

void sendDataPi() {
  // send data array containing power and gps speed to raspberry pi
  // the teensy, arduino, and RPi are little-endian, no conversion needed
  Wire.write((byte*) &data, 3*sizeof(float));
  Serial.println("Send i2c");
}
