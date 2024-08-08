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
  need to change definition of RS41_GPIO_PWR_PIN from 32 -> 36?
  need to define COPI CIPO pins somewhere?
*/

#include <SPI.h>

//Radio Head Library:
#include <RH_RF95.h> 
#include "TinyGPS++.h"
#include <RS41.h>

#define GPSSERIAL Serial4
//#define TSENSERIAL Serial8    //for ECU on Teensy 4.1
#define TSENSERIAL Serial1  //for EFU_REVD on Teensy 3.6
#define SerialUSB Serial

//Teensy setup
int pinLoRaCS = 10; //SPI chip select pin for LoRa radio on ECU
//int pinLoRaCS = 19; //SPI chip select pin for LoRa radio on EFU
int pinLoRaCOPI = 11;   //Controller out Peripheral in (formerly MoSI)
int pinLoRaCIPO = 12; //Controller out Peripheral in (formerly MISO)
int pinLoRaINT = 14;// 18 for EFU, 14 for ECU
int pinRS41_ENABLE = 36;

// We need to provide the RFM95 module's chip select and interrupt pins to the
// rf95 instance below.On the Teensy 4.1 those pins are 10 and 14 respectively.
RH_RF95 rf95(pinLoRaCS, pinLoRaINT);

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
char RS41transmit[250];
TinyGPSPlus gps;    //instance of TinyGPSPlus
bool locUpd=0;    //booleans to check whether the location, altitude, and number of available satellites has been updated
bool altUpd = 0;
bool satCntUpd = 0;
int timeout=0;    //timeout check for if only part of satellite data is updated within 5 seconds
int GPSreceivingTimeout = 0;    //timeout check for if no satellite data is updated within 8 seconds
int safeMode = 1;   //set the initial transmission mode tracker to safe mode
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; //buffer to receive general transmissions
uint8_t len = sizeof(buf);    // size of general receive buffer
int maxReceiveTime = 1;

bool first_loop = true;
bool recondition = true;
RS41 rs41(Serial6);
RS41::RS41SensorData_t sensor_data;

void setup()
{
  //safeTransmission(); //initialize client to safeTransmission mode
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN,LOW);

  SerialUSB.begin(9600);  //Serial port to talk to laptop
  GPSSERIAL.begin(9600);
  while(!SerialUSB); 
  SerialUSB.println("RFM Client!"); 
  pinMode(pinRS41_ENABLE,OUTPUT);
  digitalWrite(pinRS41_ENABLE,HIGH);

  //Initialize the Radio.
  if (rf95.init() == false){
    SerialUSB.println("Radio Init Failed - Freezing");
    while (1);
  }
  else{
    //An LED inidicator to let us know radio initialization has completed. 
    SerialUSB.println("Transmitter up!"); 
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }

  // Set frequency
  rf95.setFrequency(frequency);
  SerialUSB.println("Safe Transmission Mode");
  rf95.sleep();
  rf95.setSignalBandwidth(125000); //125kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(9);
  rf95.setCodingRate4(8);
  rf95.available();
  //rf95.setSignalBandwidth(65000); //62.5kHz, see RH_RF95.h for documentation
  //rf95.setSpreadingFactor(9);  //increasing SF increases range at cost of data transmission rate
  //at SF=11 server drops half of the messages when sending at maximum speed (~2-3 sec/transmission)
  //rf95.setCodingRate4(8);

   // The default transmitter power is 13dBm, using PA_BOOST.
   // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
   // you can set transmitter powers from 5 to 23 dBm:
   // Transmitter power can range from 14-20dbm.
   rf95.setTxPower(21, false);  //maxTX generally 14 dBm in Europe https://mylorawan.blogspot.com/2016/05/spread-factor-vs-payload-size-on-lora.html

   //send header (can be saved on receiving end using GPSfileSaver.py)
   uint8_t initSend[] = "Date, Time, SatelliteCount, Latitude, Longitude, Speed (kmph), Altitude (m), SNR";
  //sprintf(toSend, "Hi, my counter is: %d", packetCounter++);
  SerialUSB.println("Sending Initial Message: ");
  rf95.send(initSend, sizeof(initSend));
  rs41.init();
}


void loop()
{
  //checkCmd(); #check if user command was sent from server
  /*if(GPSSERIAL.available()) {
    if(gps.encode(GPSSERIAL.read())) {
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
        sendGPS();
        //checkCmd();
        // Now wait for a reply 
          //commented out waiting for reply so the transmitter continuously sends GPS data regardless of whether it is recieved
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
              sendGPS();  //send (partially updated) GPS data
              //checkCmd();
            }
          }
        }
      }
    }
  }
  else {
    //checkCmd();
    if (millis()-GPSreceivingTimeout > 8000) {    //if GPS reciever has not sent any updates over LoRa in 30 seconds
      updateMode();
      timeout = millis();
      GPSreceivingTimeout = millis();
      SerialUSB.print("Sending wait message: ");
      uint8_t waitMessageSend[] = "Waiting for GPS";
      rf95.send(waitMessageSend, sizeof(waitMessageSend));
      rf95.waitPacketSent();

      if (safeMode == 0){ //Long Range Mode
        maxReceiveTime = 20000;
      }
      else if (safeMode == 2){  //Safe Transmission Mode
        maxReceiveTime = 18000;
      }
      else{ //High Data Rate Mode
        maxReceiveTime = 8000;
      }
      // Now wait for a reply
      if (rf95.waitAvailableTimeout(maxReceiveTime)) {
        // Should be a reply message for us now
        if (rf95.recv(buf, &len)) {
          String BUF = (char*)buf;
          SerialUSB.print("Got reply: ");
          
          SerialUSB.println(BUF);
          SerialUSB.print("SNR:"); SerialUSB.print(rf95.lastSNR());
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
  }*/

  if (first_loop) {
  Serial.println(rs41.banner());
  Serial.println("RS41 meta data: " + rs41.meta_data());
  Serial.println(rs41.sensor_data_var_names);
  first_loop = false;
  }

  delay(2000);

  if (recondition){
    String recond = rs41.recondition();
    if (!recond.length()){
      Serial.println("RS41 did not respond to RHS");
    } else {
      Serial.print("Recondition: ");
      Serial.println(recond);
      recondition = false;
    }
  }

  sensor_data = rs41.decoded_sensor_data(false);
  if (sensor_data.valid) {
    updateMode();
    sendRS41();
  } else {
    Serial.println("Unable to obtain RS41 sensor data");
  }
}

void sendRS41(){  //send RS41 data packet over LoRa
snprintf(RS41transmit,maxCharLen, "%d %f %f %f %f %f %d %d %f %f %d %f %f %f %f %f %f",  //save message in c-string
  sensor_data.frame_count,sensor_data.air_temp_degC,sensor_data.humdity_percent,sensor_data.hsensor_temp_degC,sensor_data.pres_mb,sensor_data.internal_temp_degC,
  sensor_data.module_status,sensor_data.module_error,sensor_data.pcb_supply_V,sensor_data.lsm303_temp_degC,sensor_data.pcb_heater_on,
  sensor_data.mag_hdgXY_deg,sensor_data.mag_hdgXZ_deg,sensor_data.mag_hdgYZ_deg,sensor_data.accelX_mG,sensor_data.accelY_mG,sensor_data.accelZ_mG);
  SerialUSB.print("Sending message: ");
  SerialUSB.println(RS41transmit);
  sendLen = strlen(RS41transmit);
  rf95.send((uint8_t *) RS41transmit, sendLen+1);  //add 1 to length to include terminating character
  memset(RS41transmit, 0, sendLen);
  rf95.waitPacketSent();
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
  if (rf95.waitAvailableTimeout(10000)) {
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
  if (BUF.startsWith("Cmd: RS41On")){ //Remote command to power RS41 on
    digitalWrite(pinRS41_ENABLE,HIGH);
    uint8_t toSend[] = "RS41 powered off";
    rf95.send(toSend, sizeof(toSend)+1);
    rf95.waitPacketSent();
  }
  if (BUF.startsWith("Cmd: RS41Off")){ //Remote command to power RS41 off
    digitalWrite(pinRS41_ENABLE,LOW);
    uint8_t toSend[] = "RS41 powered off";
    rf95.send(toSend, sizeof(toSend)+1);
    rf95.waitPacketSent();
  }
}