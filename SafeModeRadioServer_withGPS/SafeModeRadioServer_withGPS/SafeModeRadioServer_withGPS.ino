//comment out all extraneous SerialUSB communication if saving to txt file with GPSfileSaver.py

#include <SPI.h>
//#include "CommandLineInterface.h"
//Radio Head Library: 
#include <RH_RF95.h>

// We need to provide the RFM95 module's chip select and interrupt pins to the 
// rf95 instance below.On the SparkFun ProRF those pins are 12 and 6 respectively.
RH_RF95 rf95(12, 6);

int LED = 13; //Status LED on pin 13

//int packetCounter = 0; //Counts the number of packets sent
long timeSinceLastPacket = 0;
float frequency = 921.2;
int safeMode = 1;
char header[] = "Date, Time, SatelliteCount, Latitude, Longitude, Speed (kmph), Altitude (m), SNR, RSSI, millis";
String DEBUG_Buff;  //buffer for the USB Serial monitor
int safeModeTimer = 0;
int safeModeTimeout = 0;  //Used to track whether tranceiver has entered safemode because of timeout

void setup()
{
  longRange(); //initialize server to safeTransmission mode
  pinMode(LED, OUTPUT);

  SerialUSB.begin(9600);
  // It may be difficult to read serial messages on startup. The following
  // line will wait for serial to be ready before continuing. Comment out if not needed.
  //while(!SerialUSB);
  //SerialUSB.println("RFM Server!");

  //Initialize the Radio. 
  if (rf95.init() == false){
    SerialUSB.println("Radio Init Failed - Freezing");
    while (1);
  }
  else{
  // An LED indicator to let us know radio initialization has completed.
    //SerialUSB.println("Receiver up!");
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
    delay(500);
  }

  rf95.setFrequency(frequency);
 // The default transmitter power is 13dBm, using PA_BOOST.
 // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
 // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(21, false);
  longRange();
}

void loop()
{
  if (rf95.available()){
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)){
      digitalWrite(LED, HIGH); //Turn on status LED
      timeSinceLastPacket = millis(); //Timestamp this packet
      safeModeTimeout = 0;
      //SerialUSB.print("Received message: ");
      SerialUSB.print((char*)buf); SerialUSB.print(" SNR:");
      SerialUSB.print(rf95.lastSNR());
      SerialUSB.print(" RSSI:"); SerialUSB.print(rf95.lastRssi());
      SerialUSB.print(" millis:"); SerialUSB.println(millis());
      digitalWrite(LED, LOW); //Turn off status LED
    }
    else 
      SerialUSB.println("Receive failed");
  }
  //Turn off status LED if we haven't received a packet after 1s
  if(millis() - timeSinceLastPacket > 1000){
    digitalWrite(LED, LOW); //Turn off status LED
    timeSinceLastPacket = millis(); //Don't write LED but every 1s
  }
}

void safeTransmission(){ //define transmission mode that minimizes chance of data being corrupted
  SerialUSB.println("Safe Transmission Mode");
  rf95.sleep();
  rf95.setSignalBandwidth(125000); //125kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(9);
  rf95.setCodingRate4(8);
  rf95.available();
}

void highDataRate(){  //define transmission mode that can send the largest amount of data
  SerialUSB.println("High Data Mode");
  rf95.sleep();
  rf95.setSignalBandwidth(250000); //125kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(7);
  rf95.setCodingRate4(4);
  rf95.available();
}

void longRange(){ //define transmission mode that can send the data over the longest range
  SerialUSB.println("Long Range Mode");
  rf95.sleep();
  rf95.setSignalBandwidth(40000); //decreasing BW increases link budget but decreases max crystal tolerance
  rf95.setSpreadingFactor(10);
  rf95.setCodingRate4(4);
  rf95.available();
}
