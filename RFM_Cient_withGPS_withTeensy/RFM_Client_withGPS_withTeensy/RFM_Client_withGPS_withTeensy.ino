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
  need to change definition of RS41_GPIO_PWR_PIN from 32 -> 36 (in RS41.h)?
  for longduration flight: millis will overflow after 50 days and reset; shouldn't cause problem
*/

#include <SPI.h>  //Serial Peripheral interface, used by LoRa
#include <SD.h>
#include <DS18B20.h>  //Library for Temperature Sensor on board
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
int pinDS = 22; //temperature sensor
int pin5V_MON = 26;//2;
int pin12V_IMON = 6;
int pin12V_MON = 27;//7;
int pin12V_ENABLE = 5;
int pinPCB_THERM = 22;
int pinV_ZYPHR_VMON = 40;
int pinHeater_DISABLE = 4;

RH_RF95 rf95(pinLoRaCS, pinLoRaINT);  //instance of radio module
DS18B20 ds(pinDS);  //instance of temperature sensor

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
File SDcard;
String Filename;
char filename[100];
String DEBUG_Buff;  //buffer for the USB Serial monitor

void setup()
{
  //safeTransmission(); //initialize client to safeTransmission mode
  pinMode(pin5V_MON,INPUT);
  pinMode(pin12V_IMON,INPUT);
  pinMode(pin12V_MON,INPUT);
  pinMode(pinPCB_THERM,INPUT);
  pinMode(pinV_ZYPHR_VMON,INPUT);
  pinMode(pin12V_ENABLE,OUTPUT);
  pinMode(pinHeater_DISABLE,OUTPUT);
  digitalWrite(pinHeater_DISABLE,LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN,LOW);

  SerialUSB.begin(9600);  //Serial port to talk to laptop
  GPSSERIAL.begin(9600);
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized");
  Filename = nameFile();
  Filename.toCharArray(filename, 100);
  SerialUSB.print("Filename: ");
  SerialUSB.println(filename);
  SerialUSB.println(Filename);
  SDcard = SD.open(filename, FILE_WRITE);
  
  //while(!SerialUSB); 
  delay(1000);
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
  if (SDcard) {
    SerialUSB.println("Writing to SD...");
    SDcard.println((char*)initSend);
  }
  SerialUSB.println("Sending Initial Message: ");
  rf95.send(initSend, sizeof(initSend));
  rs41.init();
}


void loop()
{
  //checkCmd(); #check if user command was sent from server
  if(GPSSERIAL.available()) {
    if(gps.encode(GPSSERIAL.read())) {
      //checkCmd();
      if(gps.location.isUpdated()) {  //check if location  has been updated since last loop
        locUpd = 1;
      }
      if(gps.altitude.isUpdated()) {  //check if altitude data has been updated since last loop
        altUpd = 1;
      }
      if(gps.satellites.isUpdated()) {  //check if number of available satellites has been updated since last loop
        satCntUpd = 1;
      }
      if(locUpd && altUpd){ //Send GPS data when location and altitude are both updated
        //updateMode(); //switch transmission modes
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
        setMaxReceiveTime();
        // Now wait for a reply
        //getLoRaReply(maxReceiveTime);
        checkCmd();
      }
      else {
        checkPartialGPSUpdate();
      }
    }
  }
  else {
    //checkCmd();
    //checkRS41;
    GPSTimeout();
  }
}


String nameFile() {
  int fileNum = 1;
  bool nameCheck = 0;
  char filename[20];
  String Filename;

  while (!nameCheck) {
    Filename = "";
    Filename = "ECUtest";
    Filename += fileNum;
    Filename.toCharArray(filename, 20);
    if (!SD.exists(filename)) {
      Serial.println(Filename);
      nameCheck = 1;
    } else {
      fileNum++;
    }
  }
  return filename;
}

void checkPartialGPSUpdate() {
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

void checkRS41(){
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
    //updateMode();
    sendRS41();
    timeout = millis();
  } else {
    Serial.println("Unable to obtain RS41 sensor data");
  }
}

void sendRS41(){  //send RS41 data packet over LoRa
snprintf(RS41transmit,maxCharLen, "R%d %f %f %f %f %f %d %d %f %f %d %f %f %f %f %f %f",  //save message in c-string
  sensor_data.frame_count,sensor_data.air_temp_degC,sensor_data.humdity_percent,sensor_data.hsensor_temp_degC,sensor_data.pres_mb,sensor_data.internal_temp_degC,
  sensor_data.module_status,sensor_data.module_error,sensor_data.pcb_supply_V,sensor_data.lsm303_temp_degC,sensor_data.pcb_heater_on,
  sensor_data.mag_hdgXY_deg,sensor_data.mag_hdgXZ_deg,sensor_data.mag_hdgYZ_deg,sensor_data.accelX_mG,sensor_data.accelY_mG,sensor_data.accelZ_mG);
  SerialUSB.print("Sending RS41 message: ");
  SerialUSB.println(RS41transmit);
  SDcard.println(RS41transmit);
  sendLen = strlen(RS41transmit);
  rf95.send((uint8_t *) RS41transmit, sendLen+1);  //add 1 to length to include terminating character
  memset(RS41transmit, 0, sendLen);
  rf95.waitPacketSent();
}

void sendGPS(){ //Send GPS packet over LoRa
  snprintf(GPStransmit,maxCharLen, "G%d/%d/%d %02d:%02d:%02d SC:%lu lat:%.6f lon:%.6f Sp:%.5f Alt:%.1f",  //save message in c-string
  gps.date.month(),gps.date.day(),gps.date.year(),gps.time.hour(),gps.time.minute(),gps.time.second(),gps.satellites.value(),
  gps.location.lat(),gps.location.lng(),gps.speed.kmph(),gps.altitude.meters());
  SerialUSB.print("Sending GPS message: ");
  SerialUSB.println(GPStransmit);
  SDcard.println(GPStransmit);
  sendLen = strlen(GPStransmit);
  rf95.send((uint8_t *) GPStransmit, sendLen+1);  //add 1 to length to include terminating character
  memset(GPStransmit, 0, sendLen);
  rf95.waitPacketSent();
}

void getLoRaReply(int maxReceiveTime){  //Get reply from LoRa server
  byte buf[RH_RF95_MAX_MESSAGE_LEN];
  byte len = sizeof(buf);
  if (rf95.waitAvailableTimeout(maxReceiveTime)) {
    if (rf95.recv(buf, &len)) {
      SerialUSB.print("Got reply: ");
      SDcard.print(("Got reply: "));
      SerialUSB.println((char*)buf);
      SDcard.print((char*)buf);
      SerialUSB.print(" SNR:"); SerialUSB.println(rf95.lastSNR());
      SDcard.print(" SNR:"); SDcard.print(rf95.lastSNR());
      SerialUSB.print(" RSSI:");
      SDcard.print(" RSSI:");
      SerialUSB.println(rf95.lastRssi(), DEC);
      SDcard.println(rf95.lastRssi(), DEC);
      String BUF = (char*)buf;
      if (BUF.startsWith("Cmd")){
        cmdParse(BUF);
      }
    }
    else {
      SerialUSB.println("Receive failed");
    }
    timeout = millis();
  }
  else {
    SerialUSB.println("No reply, is the receiver running?");
    timeout = millis();
  }
}

void setMaxReceiveTime(){
  if (safeMode == 0){ //Long Range Mode
    maxReceiveTime = 20000;
  }
  else if (safeMode == 2){  //Safe Transmission Mode
    maxReceiveTime = 18000;
  }
  else{ //High Data Rate Mode
    maxReceiveTime = 8000;
  }
}

void GPSTimeout(){
  if (millis()-GPSreceivingTimeout > 8000) {    //if GPS reciever has not sent any updates over LoRa in 30 seconds
    //updateMode();
    timeout = millis();
    GPSreceivingTimeout = millis();
    SerialUSB.print("Sending wait message: ");
    uint8_t waitMessageSend[] = "Waiting for GPS";
    rf95.send(waitMessageSend, sizeof(waitMessageSend));
    rf95.waitPacketSent();
    setMaxReceiveTime();
    //getLoRaReply(maxReceiveTime);
    checkCmd();
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

void boardMon() {
  String outputString = "5V:";
  float bin = analogRead(pin5V_MON);
  float calc = (bin/1024.0) * 4 * 3.293;  //3.475 in testing
  outputString += calc;
  outputString += ", ";
  outputString += "12V_I:";
  outputString += analogRead(pin12V_IMON);
  outputString += ", ";
  outputString += "12V:";
  bin =  analogRead(pin12V_MON);
  calc = (bin / 1024.0) * 4.87 * 3.293; //3.854 in testing
  outputString += calc;
  outputString += ", ";
  outputString += "PCB_THERM (C):";
  outputString += ds.getTempC();
  outputString += ", ";
  outputString += "Zephr_V:";
  bin =  analogRead(pinV_ZYPHR_VMON);
  calc = (bin / 1024.0) * 50.9* 3.28;
  outputString += calc;
  char boardMonMes[100];
  outputString.toCharArray(boardMonMes, 100);
  SerialUSB.println((char*)boardMonMes);
  rf95.send((uint8_t *)boardMonMes, sizeof(boardMonMes));
  rf95.waitPacketSent();
  Serial.println(outputString);
  SDcard = SD.open(filename, FILE_WRITE);
    if (SDcard) {
    Serial.println("Writing to SD...");
    SDcard.println(outputString);
  }
  SDcard.close();
  outputString = "";
}

void checkCmd(){  //check if client has recieved any user commands from server
  //SerialUSB.println("checking for Cmd");
  if (rf95.waitAvailableTimeout(2000)) {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len)) {
      String BUF = (char*)buf;
      SerialUSB.println((char*)buf);
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
  if(BUF.startsWith("Cmd: enable12V")) {
    digitalWrite(pin12V_ENABLE,LOW);  //prototype ECU board has 12V active low
    uint8_t toSend[] = "12V powered on";
    rf95.send(toSend, sizeof(toSend)+1);
    rf95.waitPacketSent();
  }
  if(BUF.startsWith("Cmd: disable12V")) {
    digitalWrite(pin12V_ENABLE,HIGH);
    uint8_t toSend[] = "12V powered off";
    rf95.send(toSend, sizeof(toSend)+1);
    rf95.waitPacketSent();
  }
  if(BUF.startsWith("Cmd: enableHeater")) {
    digitalWrite(pinHeater_DISABLE,LOW);
    uint8_t toSend[] = "Heater on";
    rf95.send(toSend, sizeof(toSend)+1);
    rf95.waitPacketSent();
  }
  if(BUF.startsWith("Cmd: disableHeater")) {
    digitalWrite(pinHeater_DISABLE,HIGH);
    uint8_t toSend[] = "Heater off";
    rf95.send(toSend, sizeof(toSend)+1);
    rf95.waitPacketSent();
  }
  if(BUF.startsWith("Cmd: boardMon")) {
    boardMon();
  }
}