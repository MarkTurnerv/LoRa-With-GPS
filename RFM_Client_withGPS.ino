/*
LoRa GPS transmitter
Created: 7/10/2024
Author: Mark Turner

To do: truncate latitude and longitude?
  attach interupt to gps
  save data? to sd card? prob on server end
  timeout/watchdog
  figure out altitude problem: Check data isn't getting lost in buffer; read faster
      Transmit location, altitude, SC, date/time in separate transmissions?
  remove unnecessary GPS transmissions?

/*
  Both the TX and RX ProRF boards will need a wire antenna. We recommend a 3" piece of wire.
  This example is a modified version of the example provided by the Radio Head
  Library which can be found here:
  www.github.com/PaulStoffregen/RadioHeadd
*/

#include <SPI.h>

//Radio Head Library:
#include <RH_RF95.h> 
#include "TinyGPS++.h"

// We need to provide the RFM95 module's chip select and interrupt pins to the
// rf95 instance below.On the SparkFun ProRF those pins are 12 and 6 respectively.
RH_RF95 rf95(12, 6);

int LED = 13; //Status LED is on pin 13

int packetCounter = 0; //Counts the number of packets sent
long timeSinceLastPacket = 0; //Tracks the time stamp of last packet received

// The broadcast frequency is set to 921.2, but the SADM21 ProRf operates
// anywhere in the range of 902-928MHz in the Americas.
// Europe operates in the frequencies 863-870, center frequency at 868MHz.
// This works but it is unknown how well the radio configures to this frequency:
//float frequency = 864.1; 
float frequency = 921.2; //Broadcast frequency
String GPSdata;
byte sendLen;
const int maxCharLen = 150;
char GPStransmit[150];
TinyGPSPlus gps;

void setup()
{
  pinMode(LED, OUTPUT);

  SerialUSB.begin(9600);
  Serial1.begin(9600);
  // It may be difficult to read serial messages on startup. The following line
  // will wait for serial to be ready before continuing. Comment out if not needed.
  while(!SerialUSB); 
  SerialUSB.println("RFM Client!"); 

  //Initialize the Radio.
  if (rf95.init() == false){
    SerialUSB.println("Radio Init Failed - Freezing");
    while (1);
  }
  else{
    //An LED inidicator to let us know radio initialization has completed. 
    SerialUSB.println("Transmitter up!"); 
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
    delay(500);
  }

  // Set frequency
  rf95.setFrequency(frequency);

   // The default transmitter power is 13dBm, using PA_BOOST.
   // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
   // you can set transmitter powers from 5 to 23 dBm:
   // Transmitter power can range from 14-20dbm.
   rf95.setTxPower(14, false);

   //send header
   uint8_t toSend[] = "Date, Time, SatelliteCount, Latitude, Longitude, Speed (kmph), Altitude (m)";
  //sprintf(toSend, "Hi, my counter is: %d", packetCounter++);
  rf95.send(toSend, sizeof(toSend));
}


void loop()
{
  if(Serial1.available()) {
    gps.encode(Serial1.read());
  }
  if(gps.location.isUpdated()) {
    /*
    GPSdata += gps.date.month();
    GPSdata += "/";
    GPSdata += gps.date.day();
    GPSdata += "/";
    GPSdata += gps.date.year();
    GPSdata += ", ";
    GPSdata += gps.satellites.value();
    GPSdata += ", ";
    GPSdata += gps.location.lat();
    SerialUSB.print("Longitude: ");
    SerialUSB.println(gps.location.lng(), 6);
    SerialUSB.print("Speed (kmph): ");
    SerialUSB.println(gps.speed.kmph());
    SerialUSB.print("Altitude (meters): ");
    SerialUSB.println(gps.altitude.meters());
    */
  
    //SerialUSB.println(GPSdata);
    //SerialUSB.println(GPSdata.length());
    
    //Send a message to the other radio
    
    //GPSdata.toCharArray(&GPStransmit[0],(GPSdata.length()+1));
    SerialUSB.println(gps.altitude.meters());
    snprintf(GPStransmit,maxCharLen, "%d/%d/%d %02d:%02d:%02d SC:%d lat:%.6f lon:%.6f Sp:%f Alt:%f",
    gps.date.month(),gps.date.day(),gps.date.year(),gps.time.hour(),gps.time.minute(),gps.time.second(),gps.satellites.value(),
    gps.location.lat(),
    gps.location.lng(),
    gps.speed.kmph(),gps.altitude.meters());
    SerialUSB.print("Sending message: ");
    //SerialUSB.println(*GPStransmit);
    SerialUSB.println(GPStransmit);
    //uint8_t toSend[] = {*GPStransmit};
    sendLen = strlen(GPStransmit);
    //sprintf(toSend, "Hi, my counter is: %d", packetCounter++);
    //rf95.send((uint8_t *) toSend, sizeof(toSend));
    rf95.send((uint8_t *) GPStransmit, sendLen);
    
    GPSdata = "";
    //GPStransmit[0] = { 0 };
    memset(GPStransmit, 0, sendLen);
    rf95.waitPacketSent();

    // Now wait for a reply
    byte buf[RH_RF95_MAX_MESSAGE_LEN];
    byte len = sizeof(buf);

    if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      if (rf95.recv(buf, &len)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)buf);
        //SerialUSB.print(" RSSI: ");
        //SerialUSB.print(rf95.lastRssi(), DEC);
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
    delay(500);
  }/*
  else {
    SerialUSB.print("Sending message: ");
    uint8_t toSend[] = "Waiting for GPS";
    rf95.send( toSend, sizeof(toSend));
    rf95.waitPacketSent();

    // Now wait for a reply
    byte buf[RH_RF95_MAX_MESSAGE_LEN];
    byte len = sizeof(buf);

    if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      if (rf95.recv(buf, &len)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)buf);
        //SerialUSB.print(" RSSI: ");
        //SerialUSB.print(rf95.lastRssi(), DEC);
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
    delay(500);

  }*/
}
