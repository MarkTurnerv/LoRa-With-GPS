/*
LoRa GPS transmitter
Created: 7/10/2024
Author: Mark Turner
To be used with RadioServer_withGPS.ino
Based off example from SparkFun:
https://learn.sparkfun.com/tutorials/sparkfun-samd21-pro-rf-hookup-guide/point-to-point-radio-arduino-examples
Which is in turn based off rf95 examples from PaulStoffregen RadioHead
  -rf95_client
  -rf95_server
https://github.com/PaulStoffregen/RadioHead/tree/master/examples/rf95

RadioHead refrence: https://www.airspayce.com/mikem/arduino/RadioHead/classRHGenericDriver.html#a9269fb2eddfaa055c31769619d808dbe

To do:
  remove unnecessary GPS transmissions?
  test different transmitter powers
  increase C/R, increase Spreading Factor, minimize? Bandwidth
  https://medium.com/home-wireless/testing-lora-radios-with-the-limesdr-mini-part-2-37fa481217ff

  Both the TX and RX ProRF boards will need a wire antenna. We recommend a 3" piece of wire.
*/

#include <SPI.h>

//Radio Head Library:
#include <RH_RF95.h> 
#include "TinyGPS++.h"

// We need to provide the RFM95 module's chip select and interrupt pins to the
// rf95 instance below.On the SparkFun ProRF those pins are 12 and 6 respectively.
RH_RF95 rf95(12, 6);

int LED = 13; //Status LED is on pin 13

//int packetCounter = 0; //Counts the number of packets sent
//long timeSinceLastPacket = 0; //Tracks the time stamp of last packet received

// The broadcast frequency is set to 921.2, but the SADM21 ProRf operates
// anywhere in the range of 902-928MHz in the Americas.
// Europe operates in the frequencies 863-870, center frequency at 868MHz.
// This works but it is unknown how well the radio configures to this frequency:
//float frequency = 864.1; 
float frequency = 921.2; //Broadcast frequency
byte sendLen;   //length of the GPS message to be sent
const int maxCharLen = 150; //amount of memory allocated to fit GPS data
//maximum LoRa transmission = 255 octets - 1 byte for message length - 4 bytes for header - 2 bytes FCS = 250 max transmission length
char GPStransmit[150];    //variable to contain the GPS data to be send over LoRa
TinyGPSPlus gps;    //instance of TinyGPSPlus
bool locUpd=0;    //booleans to check whether the location, altitude, and number of available satellites has been updated
bool altUpd = 0;
bool satCntUpd = 0;
int timeout=0;    //timeout check for if only part of satellite data is updated within 5 seconds
int GPSreceivingTimeout = 0;    //timeout check for if no satellite data is updated within 8 seconds
int safeMode = 1;   //set the initial transmission mode tracker to safe mode
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; //buffer to receive general transmissions
uint8_t len = sizeof(buf);    // size of general receive buffer

//Teensy setup
//int pinLoRaPower = ;


void setup()
{
  safeTransmission(); //initialize client to safeTransmission mode
  pinMode(LED, OUTPUT);

  SerialUSB.begin(9600);  //Serial port to talk to laptop
  Serial1.begin(9600);  //Serial port to talk to GPS data
  // It may be difficult to read serial messages on startup. The following line
  // will wait for serial to be ready before continuing. Comment out if not needed.
  //while(!SerialUSB); 
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
  //rf95.setSignalBandwidth(65000); //62.5kHz, see RH_RF95.h for documentation
  //rf95.setSpreadingFactor(9);  //increasing SF increases range at cost of data transmission rate
  //at SF=11 server drops half of the messages when sending at maximum speed (~2-3 sec/transmission)
  //rf95.setCodingRate4(8);

   // The default transmitter power is 13dBm, using PA_BOOST.
   // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
   // you can set transmitter powers from 5 to 23 dBm:
   // Transmitter power can range from 14-20dbm.
   rf95.setTxPower(21, false);

   //send header (can be saved on receiving end using GPSfileSaver.py)
   uint8_t initSend[] = "Date, Time, SatelliteCount, Latitude, Longitude, Speed (kmph), Altitude (m), SNR";
  //sprintf(toSend, "Hi, my counter is: %d", packetCounter++);
  SerialUSB.println("Sending Initial Message: ");
  rf95.send(initSend, sizeof(initSend));
}


void loop()
{
  //checkCmd(); #check if user command was sent from server
  if(Serial1.available()) {
    if(gps.encode(Serial1.read())) {
      //checkCmd();
      if(gps.location.isUpdated()) {  //check if location data has been updated since last loop
        locUpd = 1;
      }
      if(gps.altitude.isUpdated()) {  //check if altitude data has been updated since last loop
        altUpd = 1;
      }
      if(gps.satellites.isUpdated()) {  //check if number of available satellites has been updated since last loop
        satCntUpd = 1;
      }
      if(locUpd && altUpd){ //Send GPS data when location and altitude are both updated
        updateMode(); //switch transmission modes
        locUpd = 0;
        altUpd = 0;
        satCntUpd = 0;
        timeout = millis();
        GPSreceivingTimeout = millis();
        
        //Send a message to the other radio
        sendGPS(); //Send GPS packet over LoRa
        //checkCmd();
        // Now wait for a reply 
          //commented out waiting for reply so the transmitter continuously sends GPS data regardless of whether it is recieved
        byte buf[RH_RF95_MAX_MESSAGE_LEN];
        byte len = sizeof(buf);

        getLoRaReply();
      }
      else {
        if(locUpd || altUpd) {  //error message if either the location or altitude has been updated but the other has not
          if(millis() - timeout > 15000){  //if last complete update was longer than 15 seconds ago
            timeout = millis();
            GPSreceivingTimeout = millis();
            const char *timeoutSend;
            bool TimeoutMessage;
            if(!satCntUpd) {
              timeoutSend = "Satellite Count not updated";
              TimeoutMessage = 1;
            }
            if(!altUpd) {
              timeoutSend = "GPS Altitude not updated";
              TimeoutMessage = 1;
            }
            if(!locUpd) {
              timeoutSend = "GPS Location not updated";
              TimeoutMessage = 1;
            }
            else {  //this case should never be called
              timeoutSend = "System Timeout";
              TimeoutMessage = 1;
            }
            if (TimeoutMessage) { //send warning of what did not update
              rf95.send((uint8_t *) timeoutSend, strlen(timeoutSend)+1);  //add one to message char length to include terminating character 0
              rf95.waitPacketSent();
              delay(250);
              //send GPS data
              sendGPS();
              //checkCmd();
            }
          }
        }
      }
    }
  }
  
  else {
    //checkCmd();
    if (millis()-GPSreceivingTimeout > 30000) {    //if GPS reciever has not sent any updates in 30 seconds
      updateMode();
      timeout = millis();
      GPSreceivingTimeout = millis();
      SerialUSB.print("Sending wait message: ");
      uint8_t waitMessageSend[] = "Waiting for GPS";
      rf95.send(waitMessageSend, sizeof(waitMessageSend));
      rf95.waitPacketSent();

      // Now wait for a reply
      if (rf95.waitAvailableTimeout(20000)) {
        // Should be a reply message for us now
        if (rf95.recv(buf, &len)) {
          String BUF = (char*)buf;
          SerialUSB.print("Got reply: ");
          
          SerialUSB.println(BUF);
          SerialUSB.print(" SNR:"); SerialUSB.println(rf95.lastSNR());
          SerialUSB.print(" RSSI: "); SerialUSB.println(rf95.lastRssi(), DEC);
        }
        else {
          SerialUSB.println("Receive failed");
        }
      }
      else {
        SerialUSB.println("No reply, is the receiver running?");
      }
    }
  }
}


void safeTransmission(){ //define transmission mode that minimizes chance of data being corrupted
  SerialUSB.println("Safe Transmission Mode");
  rf95.sleep();
  rf95.setSignalBandwidth(125000); //62.5kHz, see RH_RF95.h for documentation
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
  rf95.setCodingRate4(7);
  rf95.available();
}

void updateMode(){  //switch between each of the three modes after every transmission
  bool upd = 0;
  if(safeMode == 2){
    longRange();
    safeMode = 0;
    upd = 1;
  }
  if(safeMode == 1 && upd == 0){
    safeTransmission();
    safeMode++;
    upd = 1;
  }
  if(safeMode == 0 && upd == 0){
    highDataRate();
    safeMode++;
    upd = 1;
  }
}

void checkCmd(){  //check if client has recieved any user commands from server
  SerialUSB.println("checking for Cmd");
  if (rf95.waitAvailableTimeout(2000)) {
    SerialUSB.println("checking for Cmd x2");
    // Should be a reply message for us now
    if (rf95.recv(buf, &len)) {
      String BUF = (char*)buf;
      SerialUSB.println((char*)buf);
      SerialUSB.println(BUF);
      if (BUF.startsWith("Cmd")){
        cmdParse(BUF);
      }
    }
  }
}

void sendGPS(){ //Send GPS packet over LoRa
  snprintf(GPStransmit,maxCharLen, "%d/%d/%d %02d:%02d:%02d SC:%lu lat:%.6f lon:%.6f Sp:%.5f Alt:%.1f",  //save message in c-string
  gps.date.month(),gps.date.day(),gps.date.year(),gps.time.hour(),gps.time.minute(),gps.time.second(),gps.satellites.value(),
  gps.location.lat(),gps.location.lng(),gps.speed.kmph(),gps.altitude.meters());
  SerialUSB.print("Sending message: ");
  SerialUSB.println(GPStransmit);
  sendLen = strlen(GPStransmit);
  rf95.send((uint8_t *) GPStransmit, sendLen+1);  //add 1 to length to include terminating character
  memset(GPStransmit, 0, sendLen);
  rf95.waitPacketSent();
}

void getLoRaReply(){  //Get reply from LoRa server
  byte buf[RH_RF95_MAX_MESSAGE_LEN];
  byte len = sizeof(buf);
  if (rf95.waitAvailableTimeout(20000)) {
    if (rf95.recv(buf, &len)) {
      SerialUSB.print("Got reply: ");
      SerialUSB.println((char*)buf);
      SerialUSB.print(" SNR:"); SerialUSB.println(rf95.lastSNR());
      SerialUSB.print(" RSSI:");
      SerialUSB.print(rf95.lastRssi(), DEC);
    }
    else {
      SerialUSB.println("Receive failed");
    }
  }
  else {
    SerialUSB.println("No reply, is the receiver running?");
  }
}

void cmdParse(String BUF){  //interpret the user command
  char * strtokIndx; // this is used by strtok() as an index
  int bufInt=0;
  char CommandArray[64];
  if (BUF.startsWith("Cmd: safeTrans")){
    uint8_t waitMessageSend[] = "Client Entering Safe Transmission Mode";
    rf95.send(waitMessageSend, sizeof(waitMessageSend));
    rf95.waitPacketSent();
    safeTransmission();
  }
  if (BUF.startsWith("Cmd: HDR")){
    highDataRate();
    uint8_t waitMessageSend[] = "Client Entering High Data Rate Mode";
    rf95.send(waitMessageSend, sizeof(waitMessageSend));
    rf95.waitPacketSent();
    highDataRate();
  }
  if (BUF.startsWith("Cmd: sleep")){
    uint8_t waitMessageSend[] = "Client Entering Sleep Mode";
    rf95.send(waitMessageSend, sizeof(waitMessageSend));
    rf95.waitPacketSent();
    rf95.sleep();
  }
  if (BUF.startsWith("Cmd: setBW")){
    BUF.toCharArray(CommandArray, 64);
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    bufInt = atoi(strtokIndx);     // convert this part to a int for the bandwidth
    if (bufInt < 25000){
      bufInt = 125000;
    }
    char setMsg[maxCharLen];
    snprintf(setMsg,maxCharLen, "Bandwidth set:%d", bufInt);
    SerialUSB.print("Sending set message: ");
    SerialUSB.println(setMsg);
    byte cmdSendLen = strlen(setMsg);
    rf95.send((uint8_t *) setMsg, cmdSendLen+1);
    memset(setMsg, 0, cmdSendLen);
    rf95.waitPacketSent();
    rf95.setSignalBandwidth(bufInt);
  }
  if (BUF.startsWith("Cmd: setSF")){
    BUF.toCharArray(CommandArray, 64);
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
    SerialUSB.println(strtokIndx);
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    bufInt = atoi(strtokIndx);     // convert this part to a int for the bandwidth
    
    char setMsg[maxCharLen];
    snprintf(setMsg,maxCharLen, "Spreading Factor set:%d", bufInt);
    SerialUSB.print("Sending set message: ");
    SerialUSB.println(setMsg);
    byte cmdSendLen = strlen(setMsg);
    rf95.send((uint8_t *) setMsg, cmdSendLen+1);
    memset(setMsg, 0, cmdSendLen);
    rf95.waitPacketSent();
    rf95.setSpreadingFactor(bufInt);
  }
  if (BUF.startsWith("Cmd: setCR")){
    BUF.toCharArray(CommandArray, 64);
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
    SerialUSB.println(strtokIndx);
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    bufInt = atoi(strtokIndx);     // convert this part to a int for the bandwidth
    
    char setMsg[maxCharLen];
    snprintf(setMsg,maxCharLen, "Coding Ratio set:%d", bufInt);
    SerialUSB.print("Sending set message: ");
    SerialUSB.println(setMsg);
    sendLen = strlen(setMsg);
    rf95.send((uint8_t *) setMsg, sendLen+1);
    memset(setMsg, 0, sendLen);
    rf95.waitPacketSent();
    rf95.setCodingRate4(bufInt);
  }
}