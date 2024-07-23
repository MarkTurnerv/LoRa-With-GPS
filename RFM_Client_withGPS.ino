/*
LoRa GPS transmitter
Created: 7/10/2024
Author: Mark Turner
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
bool safeMode = 0;
byte buf[RH_RF95_MAX_MESSAGE_LEN];
byte len = sizeof(buf);

//Teensy setup
//int pinLoRaPower = ;


void setup()
{
  pinMode(LED, OUTPUT);

  SerialUSB.begin(9600);
  Serial1.begin(9600);
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
  rf95.setSignalBandwidth(65000); //62.5kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(9);  //increasing SF increases range at cost of data transmission rate
  //at SF=11 server drops half of the messages when sending at maximum speed (~2-3 sec/transmission)
  rf95.setCodingRate4(8);

   // The default transmitter power is 13dBm, using PA_BOOST.
   // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
   // you can set transmitter powers from 5 to 23 dBm:
   // Transmitter power can range from 14-20dbm.
   rf95.setTxPower(23, false);

   //send header (can be saved on receiving end using GPSfileSaver.py)
   uint8_t initSend[] = "Date, Time, SatelliteCount, Latitude, Longitude, Speed (kmph), Altitude (m), SNR";
  //sprintf(toSend, "Hi, my counter is: %d", packetCounter++);
  SerialUSB.println("Sending Initial Message: ");
  rf95.send(initSend, sizeof(initSend));
}


void loop()
{
  /*if(safeMode){
    safeTransmission();
  }*
  else if (safeMode ==0){
    highDataRate();
  }*/
  if(Serial1.available()) {
    checkCmd();
    if(gps.encode(Serial1.read())) {
      if(gps.location.isUpdated()) {  //check if location data has been updated since last loop
        locUpd = 1;
      }
      if(gps.altitude.isUpdated()) {  //check if altitude data has been updated since last loop
        altUpd = 1;
      }
      if(gps.satellites.isUpdated()) {  //check if number of available satellites has been updated since last loop
        satCntUpd = 1;
      }
      if(locUpd && altUpd){
        locUpd = 0;
        altUpd = 0;
        satCntUpd = 0;
        timeout = millis();
        GPSreceivingTimeout = millis();
        
        //Send a message to the other radio
        snprintf(GPStransmit,maxCharLen, "%d/%d/%d %02d:%02d:%02d SC:%d lat:%.6f lon:%.6f Sp:%.5f Alt:%.1f",
        gps.date.month(),gps.date.day(),gps.date.year(),gps.time.hour(),gps.time.minute(),gps.time.second(),gps.satellites.value(),
        gps.location.lat(),
        gps.location.lng(),
        gps.speed.kmph(),gps.altitude.meters());
        SerialUSB.print("Sending message: ");
        SerialUSB.println(GPStransmit);
        sendLen = strlen(GPStransmit);
        //sprintf(toSend, "Hi, my counter is: %d", packetCounter++);
        //rf95.send((uint8_t *) toSend, sizeof(toSend));
        rf95.send((uint8_t *) GPStransmit, sendLen+1);  //add 1 to length to include terminating character
        
        //GPStransmit[0] = { 0 };
        memset(GPStransmit, 0, sendLen);
        rf95.waitPacketSent();

        // Now wait for a reply 
        /*  commented out waiting for reply so the transmitter continuously sends GPS data regardless of whether it is recieved
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
        }*/
      }
      else {
        if(locUpd || altUpd) {  //error message if either the location or altitude has been updated but the other has not
          if(millis() - timeout > 5000){  //if last complete update was longer than 5 seconds ago
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
              snprintf(GPStransmit,maxCharLen, "%d/%d/%d %02d:%02d:%02d SC:%d lat:%.6f lon:%.6f Sp:%.5f Alt:%.1f ",
              gps.date.month(),gps.date.day(),gps.date.year(),gps.time.hour(),gps.time.minute(),gps.time.second(),gps.satellites.value(),
              gps.location.lat(), gps.location.lng(), gps.speed.kmph(),gps.altitude.meters());
              SerialUSB.print("Sending message: ");
              SerialUSB.println(GPStransmit);
              sendLen = strlen(GPStransmit);
              rf95.send((uint8_t *) GPStransmit, sendLen);
              memset(GPStransmit, 0, sendLen);
              rf95.waitPacketSent();
            }
          }
        }
      }
    }
  }
  
  else {
    //checkCmd();
    SerialUSB.println("after chk");
    if (millis()-GPSreceivingTimeout > 8000) {    //if GPS reciever has not received any updates in 8 seconds
      timeout = millis();
      GPSreceivingTimeout = millis();
      SerialUSB.print("Sending wait message: ");
      uint8_t waitMessageSend[] = "Waiting for GPS";
      rf95.send(waitMessageSend, sizeof(waitMessageSend));
      rf95.waitPacketSent();

      // Now wait for a reply
      checkCmd();
      if (rf95.waitAvailableTimeout(2000)) {
        // Should be a reply message for us now
        if (rf95.recv(buf, &len)) {
          String BUF = (char*)buf;
          SerialUSB.print("Got reply: ");
          
          SerialUSB.println(BUF);
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
    }
  }
}


void safeTransmission(){
  rf95.sleep();
  rf95.setSignalBandwidth(62500); //62.5kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(7);
  rf95.setCodingRate4(8);
  safeMode = 1;
}

void highDataRate(){
  rf95.sleep();
  rf95.setSignalBandwidth(125000); //125kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(9);
  rf95.setCodingRate4(4);
  safeMode = 0;
}
//set interrupt upon receiving message (so also limit # of server messages sent)
//set commands to change freq, ^, sleepmode

void checkCmd(){
  SerialUSB.println("checking for Cmd");
  if (rf95.waitAvailableTimeout(1000)) {
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

void cmdParse(String BUF){
  char * strtokIndx; // this is used by strtok() as an index
  int bufInt=0;
  char CommandArray[64];
  if (BUF.startsWith("Cmd: safeTrans")){
    safeTransmission();
    uint8_t waitMessageSend[] = "Client Entering Safe Transmission Mode";
    rf95.send(waitMessageSend, sizeof(waitMessageSend));
    rf95.waitPacketSent();
    return;
  }
  if (BUF.startsWith("Cmd: HDR")){
    highDataRate();
    uint8_t waitMessageSend[] = "Client Entering Sleep Mode";
     rf95.send(waitMessageSend, sizeof(waitMessageSend));
    rf95.waitPacketSent();
    rf95.sleep();
    return;
  }
  if (BUF.startsWith("Cmd: sleep")){
    uint8_t waitMessageSend[] = "Client Entering Sleep Mode";
     rf95.send(waitMessageSend, sizeof(waitMessageSend));
    rf95.waitPacketSent();
    rf95.sleep();
    return;
  }
  if (BUF.startsWith("Cmd: setBW")){
    BUF.toCharArray(CommandArray, 64);
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
    SerialUSB.println(strtokIndx);
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    bufInt = atoi(strtokIndx);     // convert this part to a int for the bandwidth
    //rf95.setSignalBandwidth(bufInt);
    
    char setMsg[30];
    snprintf(setMsg,30, "Bandwidth set:%d", bufInt);
    SerialUSB.print("Sending set message: ");
    SerialUSB.println(setMsg);
    sendLen = strlen(setMsg);
    rf95.send((uint8_t *) setMsg, sendLen);
    memset(setMsg, 0, sendLen);
    rf95.waitPacketSent();
  }
  if (BUF.startsWith("Cmd: setSF")){
    int Blen = BUF.length();
    String Bnumber = "";
    for(int i = 0; i <= Blen-11; i++) {
      Bnumber += BUF.charAt(Blen+i-10);
    }
    rf95.setSpreadingFactor(Bnumber.toInt());
    char setMsg[30];
    snprintf(setMsg,30, "Spreading Factor set:%d", Bnumber.toInt());
    SerialUSB.print("Sending set message: ");
    SerialUSB.println(setMsg);
    sendLen = strlen(setMsg);
    rf95.send((uint8_t *) setMsg, sendLen);
    memset(setMsg, 0, sendLen);
    rf95.waitPacketSent();
  }
  if (BUF.startsWith("Cmd: setCR")){
    int Blen = BUF.length();
    String Bnumber = "";
    for(int i = 0; i <= Blen-11; i++) {
      Bnumber += BUF.charAt(Blen+i-10);
    }
    rf95.setCodingRate4(Bnumber.toInt());
    char setMsg[30];
    snprintf(setMsg,30, "Coding Ratio set:%d", Bnumber.toInt());
    SerialUSB.print("Sending set message: ");
    SerialUSB.println(setMsg);
    sendLen = strlen(setMsg);
    rf95.send((uint8_t *) setMsg, sendLen);
    memset(setMsg, 0, sendLen);
    rf95.waitPacketSent();   
  }
}