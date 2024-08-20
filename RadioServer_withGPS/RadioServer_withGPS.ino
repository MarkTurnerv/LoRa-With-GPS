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
long timeSinceLastPacket = 0; //Tracks the time stamp of last packet received
// The broadcast frequency is set to 921.2, but the SADM21 ProRf operates
// anywhere in the range of 902-928MHz in the Americas.
// Europe operates in the frequencies 863-870, center frequency at 
// 868MHz.This works but it is unknown how well the radio configures to this frequency:
//float frequency = 864.1;
float frequency = 921.2;
int safeMode = 1;
char header[] = "Date, Time, SatelliteCount, Latitude, Longitude, Speed (kmph), Altitude (m), SNR, RSSI";
String DEBUG_Buff;  //buffer for the USB Serial monitor
int safeModeTimer = 0;
int safeModeTimeout = 0;  //Used to track whether tranceiver has entered safemode because of timeout

void setup()
{
  safeTransmission(); //initialize server to safeTransmission mode
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
  //rf95.setSignalBandwidth(65000); //62.5kHz, see RH_RF95.h for documentation
  //rf95.setSpreadingFactor(9);
  //rf95.setCodingRate4(8);
 // The default transmitter power is 13dBm, using PA_BOOST.
 // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
 // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(21, false);
}

void loop()
{
  checkSafeMode();
  //checkForCmd();
  if (rf95.available()){
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    //checkForCmd();
    if (rf95.recv(buf, &len)){
      digitalWrite(LED, HIGH); //Turn on status LED
      timeSinceLastPacket = millis(); //Timestamp this packet
      safeModeTimeout = 0;
      //SerialUSB.print("Received message: ");
      SerialUSB.print((char*)buf); SerialUSB.print(" SNR:");
      SerialUSB.print(rf95.lastSNR());
      SerialUSB.print(" RSSI:"); SerialUSB.println(rf95.lastRssi());
      //SerialUSB.print(" RSSI: ");
      //SerialUSB.print(rf95.lastRssi(), DEC);
      //SerialUSB.println();

      // Send a reply
      String BUF = (char*)buf;
      if (BUF.startsWith("Waiting for GPS")) {
        uint8_t toSend[] = "Ack Waiting for GPS"; 
        rf95.send(toSend, sizeof(toSend));
        rf95.waitPacketSent();
        //SerialUSB.println("Sent a reply");
        digitalWrite(LED, LOW); //Turn off status LED
      }
      if (BUF.equals("Satellite Count not updated") || BUF.equals("GPS Altitude not updated") || BUF.equals("GPS Location not updated") || BUF.equals("System Timeout")) {
        uint8_t toSend[] = "Ack Partial Packet Update"; 
        rf95.send(toSend, sizeof(toSend));
        rf95.waitPacketSent();
        //SerialUSB.println("Sent a reply");
        digitalWrite(LED, LOW); //Turn off status LED
      }
      else{
        uint8_t toSend[] = "Data Recieved"; 
        rf95.send(toSend, sizeof(toSend));
        rf95.waitPacketSent();
        //SerialUSB.println("Sent a reply");
        digitalWrite(LED, LOW); //Turn off status LED
      }
    updateMode(); //switch between the 3 transmission modes after each successful reception and ack
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

void updateMode(){  //switch between each of the three modes after each successful reception
  safeModeTimer = millis();
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

void checkSafeMode(){ //if no client transmissions are successfully recieved within a minute (two cycles 
  //through each of the modes) switch to safeTransmission mode to maximize chance of receiving a signal
  if (millis() - safeModeTimer > 60000 && safeModeTimeout == 0){
    SerialUSB.println("Timeout entering Safe Transmission");
    safeTransmission();
    safeMode = 2;
    safeModeTimer = millis();
    safeModeTimeout = 1;
  }
  else if (millis() - safeModeTimer > 60000 && safeModeTimeout == 1) {
    SerialUSB.println("Timeout entering Long Range");
    longRange();
    safeMode = 0;
    safeModeTimer = millis();
    safeModeTimeout = 0;
  }
}
/*
void checkForCmd() {  //check serialUSB for user commands
  if (SerialUSB.available()) {
   char DEBUG_Char = SerialUSB.read();
   
   if((DEBUG_Char == '\n') || (DEBUG_Char == '\r'))
   {
    parseCommand(DEBUG_Buff);
    DEBUG_Buff = "";
   } else
   {
    DEBUG_Buff += DEBUG_Char;
   }   
  }
}

void parseCommand(String commandToParse) {
  /* This is where all the commands are interpreted and is the meat of the control system
   * so far
   * #header - print a header to the terminal
   * #safeTrans - enter safeTransmission mode
   * #HDR - enter HighDataRate
   * #sleep  - puts LoRa radio client and server to sleep
   * #setBW,[int] - Sets LoRa client and server bandwidth
   * #setSF,[int] - Sets LoRa client and server spreading factor
   * #setCR,[int] - Sets LoRa client and server coding ratio
   * #send - send all characters entered into the Serial Monitor port
   * #setSerBW - sets the bandwidth of just the server side (used to re-establish connection
                  if client changes to a known bandwidth)
   */
  /*
  char * strtokIndx; // this is used by strtok() as an index
  char CommandArray[64];

  int int1 = 0;

  if(commandToParse.startsWith("#header"))
  {
     SerialUSB.println(header);  
  }
  else if(commandToParse.startsWith("#safeTrans")) {
      uint8_t toSend[] = "Cmd: safeTrans";
      rf95.send(toSend, sizeof(toSend)+1);
      rf95.waitPacketSent();
      if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      if (rf95.recv(buf, &len)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)buf);
        safeTransmission();
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
    rf95.sleep();
  }
  else if(commandToParse.startsWith("#HDR")) {
      uint8_t toSend[] = "Cmd: HDR";
      rf95.send(toSend, sizeof(toSend)+1);
      rf95.waitPacketSent();
      highDataRate();
      if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      if (rf95.recv(buf, &len)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)buf);
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
    rf95.sleep();
  }
  else if(commandToParse.startsWith("#sleep")) {
      uint8_t toSend[] = "Cmd: sleep";
      rf95.send(toSend, sizeof(toSend)+1);
      rf95.waitPacketSent();
      if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      if (rf95.recv(buf, &len)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)buf);
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
    rf95.sleep();
  }
  else if(commandToParse.startsWith("#setBW"))  //7800-250000
  { //according to datasheet RFM95W supports BW from 7.8-500kHz; in testing it was losing signals below 25kHz
    commandToParse.toCharArray(CommandArray, 64); //copy the String() to a string
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
    SerialUSB.println(strtokIndx);
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    int1 = atoi(strtokIndx);     // convert this part to a int for the bandwidth
    SerialUSB.println(strtokIndx);
    SerialUSB.print("Setting Bandwidth to: ");
    SerialUSB.print(int1); SerialUSB.println("Hz");
    char cmdMsg[64];
    snprintf(cmdMsg,64, "Cmd: setBW,%d", int1);
    SerialUSB.write(cmdMsg);
    int cmdSendLen = strlen(cmdMsg);
    rf95.send((uint8_t *) cmdMsg, cmdSendLen+1);
    rf95.waitPacketSent();
    memset(cmdMsg, NULL, cmdSendLen);
    commandToParse = "";
    if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      uint8_t cmdBuf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t cmdLen = sizeof(cmdBuf);
      if (rf95.recv(cmdBuf, &cmdLen)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)cmdBuf);
        rf95.setSignalBandwidth(int1);
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
  }
  else if(commandToParse.startsWith("#setSF"))  //valid inputs: integer 6-12 (will limit to 6 or 12 if outside of range)
  {//Client becomes unresponsive at 6; do NOT set SF below 7
  //max SF of 10 in North America (12 in Europe)
    commandToParse.toCharArray(CommandArray, 64); //copy the String() to a string
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
  
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    int1 = atoi(strtokIndx);     // convert this part to a int for the spreading factor

    SerialUSB.print("Setting Spreading Factor to: ");
    SerialUSB.println(int1);
    char cmdMsg[64];
    snprintf(cmdMsg,64, "Cmd: setSF,%d", int1);
    int cmdSendLen = strlen(cmdMsg);
    rf95.send((uint8_t *) cmdMsg, cmdSendLen+1);
    memset(cmdMsg, 0, cmdSendLen);
    commandToParse = "";
    rf95.waitPacketSent();
    if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      uint8_t cmdBuf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t cmdLen = sizeof(cmdBuf);
      if (rf95.recv(cmdBuf, &cmdLen)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)cmdBuf);
        rf95.setSpreadingFactor(int1);
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
  }
  else if(commandToParse.startsWith("#setCR"))  //Valid denominator values are 5, 6, 7 or 8.
  {
    commandToParse.toCharArray(CommandArray, 64); //copy the String() to a string
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
  
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    int1 = atoi(strtokIndx);     // convert this part to a int for the bandwidth

    SerialUSB.print("Setting Coding Rate to: ");
    SerialUSB.print("4/"); SerialUSB.println(int1);
    char cmdMsg[64];
    snprintf(cmdMsg,64, "Cmd: setCR,%d", int1);
    int cmdSendLen = strlen(cmdMsg);
    rf95.send((uint8_t *) cmdMsg, cmdSendLen+1);
    memset(cmdMsg, 0, cmdSendLen);
    commandToParse = "";
    rf95.waitPacketSent();
    if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      uint8_t cmdBuf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t cmdLen = sizeof(cmdBuf);
      if (rf95.recv(cmdBuf, &cmdLen)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)cmdBuf);
        rf95.setCodingRate4(int1);
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
  }
  else if (commandToParse.startsWith("#send")) {
    char sendMes[251];
    commandToParse.toCharArray(sendMes,251);
    SerialUSB.println(sendMes);
    rf95.send((uint8_t*)sendMes, 251);
    rf95.waitPacketSent();
        if (rf95.waitAvailableTimeout(2000)) {
      // Should be a reply message for us now
      uint8_t cmdBuf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t cmdLen = sizeof(cmdBuf);
      if (rf95.recv(cmdBuf, &cmdLen)) {
        SerialUSB.print("Got reply: ");
        SerialUSB.println((char*)cmdBuf);
        rf95.setCodingRate4(int1);
      }
      else {
        SerialUSB.println("Receive failed");
      }
    }
    else {
      SerialUSB.println("No reply, is the receiver running?");
    }
  }
  else if (commandToParse.startsWith("#setSerBW")){
    commandToParse.toCharArray(CommandArray, 64); //copy the String() to a string
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
  
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    int1 = atoi(strtokIndx);
    SerialUSB.print("Setting Server BW to: "); SerialUSB.println(int1);
    rf95.setSignalBandwidth(int1);
  }
}*/
