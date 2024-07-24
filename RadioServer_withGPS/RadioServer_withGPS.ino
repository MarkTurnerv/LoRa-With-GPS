#include <SPI.h>
//#include "CommandLineInterface.h"
//Radio Head Library: 
#include <RH_RF95.h>

// We need to provide the RFM95 module's chip select and interrupt pins to the 
// rf95 instance below.On the SparkFun ProRF those pins are 12 and 6 respectively.
RH_RF95 rf95(12, 6);

int LED = 13; //Status LED on pin 13

int packetCounter = 0; //Counts the number of packets sent
long timeSinceLastPacket = 0; //Tracks the time stamp of last packet received
// The broadcast frequency is set to 921.2, but the SADM21 ProRf operates
// anywhere in the range of 902-928MHz in the Americas.
// Europe operates in the frequencies 863-870, center frequency at 
// 868MHz.This works but it is unknown how well the radio configures to this frequency:
//float frequency = 864.1;
float frequency = 921.2;
bool safeMode = 0;
char header[] = "Date, Time, SatelliteCount, Latitude, Longitude, Speed (kmph), Altitude (m), SNR";
String DEBUG_Buff;  //buffer for the USB Serial monitor

void setup()
{
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
  rf95.setSignalBandwidth(65000); //62.5kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(9);
  rf95.setCodingRate4(8);
 // The default transmitter power is 13dBm, using PA_BOOST.
 // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
 // you can set transmitter powers from 5 to 23 dBm:
 // rf95.setTxPower(14, false);
}

void loop()
{
  /*if(safeMode){
  safeTransmission();
  }
  else if (safeMode ==0){
    highDataRate();
  }*/
  checkForCmd();
  if (rf95.available()){
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    checkForCmd();
    if (rf95.recv(buf, &len)){
      digitalWrite(LED, HIGH); //Turn on status LED
      timeSinceLastPacket = millis(); //Timestamp this packet

      //SerialUSB.print("Received message: ");
      SerialUSB.print((char*)buf);
      SerialUSB.println(rf95.lastSNR());
      //SerialUSB.print(" RSSI: ");
      //SerialUSB.print(rf95.lastRssi(), DEC);
      //SerialUSB.println();

      // Send a reply
      String BUF = (char*)buf;
      if (BUF.equals("Waiting for GPS")) {
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
    }
    else 
      SerialUSB.println("Recieve failed");
  }
  //Turn off status LED if we haven't received a packet after 1s
  if(millis() - timeSinceLastPacket > 1000){
    digitalWrite(LED, LOW); //Turn off status LED
    timeSinceLastPacket = millis(); //Don't write LED but every 1s
  }
}

void safeTransmission(){
  rf95.sleep();
  rf95.setSignalBandwidth(125000); //62.5kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(7);
  rf95.setCodingRate4(8);
  safeMode = 1;
  rf95.available();
}

void highDataRate(){
  rf95.sleep();
  rf95.setSignalBandwidth(125000); //125kHz, see RH_RF95.h for documentation
  rf95.setSpreadingFactor(9);
  rf95.setCodingRate4(4);
  safeMode = 0;
  rf95.available();
}

void checkForCmd() {
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
   * #setBW, - Sets LoRa client and server bandwidth
   * #setSF, - toggles to cummulative defined bins
   * #setCR, float - set a new pump speed
   * #send - send characters
   * #clear - clear saved pump settings
   */
  
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
  {
    commandToParse.toCharArray(CommandArray, 64); //copy the String() to a string
    strtokIndx = strtok(CommandArray,",");      // get the first part - the string we don't care about this
    SerialUSB.println(strtokIndx);
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    int1 = atoi(strtokIndx);     // convert this part to a int for the bandwidth
    SerialUSB.println(strtokIndx);
    SerialUSB.print("Setting Bandwidth to: ");
    SerialUSB.print(int1); SerialUSB.println("Hz");
    char cmdMsg[64];
    snprintf(cmdMsg,63, "Cmd: setBW,%d", int1);
    SerialUSB.write(cmdMsg);
    int cmdSendLen = strlen(cmdMsg);
    rf95.send((uint8_t *) cmdMsg, cmdSendLen+1);
    rf95.waitPacketSent();
    memset(cmdMsg, 0, cmdSendLen);
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
}
