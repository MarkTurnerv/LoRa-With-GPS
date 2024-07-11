void setup()
{
  SerialUSB.begin(9600); // Initialize Serial Monitor USB
  Serial1.begin(9600); // Initialize hardware serial port, pins 0/1

  while (!SerialUSB) ; // Wait for Serial monitor to open

  // Send a welcome message to the serial monitor:
  SerialUSB.println("Send character(s) to relay it over Serial1");
}

void loop()
{
  if (SerialUSB.available()) // If data is sent to the monitor
  {
    String toSend = ""; // Create a new string
    while (SerialUSB.available()) // While data is available
    {
      // Read from SerialUSB and add to the string:
      toSend += (char)SerialUSB.read();
    }
    // Print a message stating what we're sending:
    SerialUSB.println("Sending " + toSend + " to Serial1");

    // Send the assembled string out over the hardware
    // Serial1 port (TX pin 1).
    Serial1.print(toSend);
  }

  if (Serial1.available()) // If data is sent from device
  {
    String toSend = ""; // Create a new string
    while (Serial1.available()) // While data is available
    {
      // Read from hardware port and add to the string:
      toSend += (char)Serial1.read();
    }
    // Print a message stating what we've received:
    SerialUSB.println("Received " + toSend + " from Serial1");
  }
}
