#include "TinyGPS++.h"

TinyGPSPlus gps;

void setup() {
  SerialUSB.begin(9600);
  Serial1.begin(9600);
  while(!SerialUSB)
  SerialUSB.println("Initialization complete...");
}
/*
void loop() {
  SerialUSB.println(Serial1.read());
}
*/

void loop() {
  // put your main code here, to run repeatedly:
  if(Serial1.available()) {
    gps.encode(Serial1.read());
  }
  if(gps.location.isUpdated()) {
    SerialUSB.print("Satellite Count: ");
    SerialUSB.println(gps.satellites.value());
    SerialUSB.print("Latitude: ");
    SerialUSB.println(gps.location.lat(), 6);
    SerialUSB.print("Longitude: ");
    SerialUSB.println(gps.location.lng(), 6);
    SerialUSB.print("Speed (kmph): ");
    SerialUSB.println(gps.speed.kmph());
    SerialUSB.print("Altitude (meters): ");
    SerialUSB.println(gps.altitude.meters());
    
  }
}
