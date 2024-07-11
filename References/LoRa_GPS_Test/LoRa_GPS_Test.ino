const int sentenceSize = 80;
int temp;

char sentence[sentenceSize];

void setup() {
  SerialUSB.begin(9600);
  Serial1.begin(9600);
  while(!SerialUSB);
  SerialUSB.println("Initialized USB Serial");

}

void loop() {
  temp = Serial1.read();
  Serial.println(temp);
}

/*
void loop()
{
  static int i = 0;
  if (Serial1.available())
  {
    char ch = Serial1.read();
    if (ch != '\n' && i < sentenceSize)
    {
      sentence[i] = ch;
      i++;
    }
    else
    {
     sentence[i] = '\0';
     i = 0;
     displayGPS();
    }
  }
}

void displayGPS()
{
  char field[20];
  getField(field, 0);
  if (strcmp(field, "$GPRMC") == 0)
  {
    SerialUSB.print("Lat: ");
    getField(field, 3);  // number
    SerialUSB.print(field);
    getField(field, 4); // N/S
    SerialUSB.print(field);
    
    SerialUSB.print(" Long: ");
    getField(field, 5);  // number
    SerialUSB.print(field);
    getField(field, 6);  // E/W
    SerialUSB.println(field);
  }
}

void getField(char* buffer, int index)
{
  int sentencePos = 0;
  int fieldPos = 0;
  int commaCount = 0;
  while (sentencePos < sentenceSize)
  {
    if (sentence[sentencePos] == ',')
    {
      commaCount ++;
      sentencePos ++;
    }
    if (commaCount == index)
    {
      buffer[fieldPos] = sentence[sentencePos];
      fieldPos ++;
    }
    sentencePos ++;
  }
  buffer[fieldPos] = '\0';
} 
*/
