# LoRa With GPS
Sends GPS data between two LoRa modules at a frequency of 921.2MHz.

### Libraries
Uses RadioHead and TinyGPSPlus Libraries.
RadioHead can be installed here: https://github.com/PaulStoffregen/RadioHead

Based off example from SparkFun:
https://learn.sparkfun.com/tutorials/sparkfun-samd21-pro-rf-hookup-guide/point-to-point-radio-arduino-examples
Which is in turn based off rf95 examples from PaulStoffregen RadioHead
  -rf95_client
  -rf95_server
https://github.com/PaulStoffregen/RadioHead/tree/master/examples/rf95


## Hardware
Communicates using two SparkFun Pro RF modules (SAMD21 board and RFM95W). One radio module is connected to a Goouuu Tech GT-U7 GPS reciever and 
a Taoglas PC81 868MHz ISM Mini PCB Antenna.
https://www.taoglas.com/product/pc81-868mhz-ism-mini-pcb-antenna/
