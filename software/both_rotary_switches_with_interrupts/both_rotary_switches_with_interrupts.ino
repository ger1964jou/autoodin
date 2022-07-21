volatile int four_flag = 0;
volatile int eight_flag = 0;

void setup() {
  Serial.begin(115200);
  attachInterrupt(5, set_four, RISING);
  attachInterrupt(4, set_eight, RISING);
}

void loop() {
  if(four_flag == 1)
  {
    four_flag = 0;
    int voltage = analogRead(A0);
    if(voltage < 825)
    {
      Serial.println("4 Pos Channel 4");
    }
    else if(voltage < 904)
    {
      Serial.println("4 Pos Channel 3");
    }
    else if(voltage < 984)
    {
      Serial.println("4 Pos Channel 2");
    }
    else
    {
      Serial.println("4 Pos Channel 1");
    }
  }
  if(eight_flag == 1)
  {
    eight_flag = 0;
    int voltage = analogRead(A1);
    if(voltage < 896)
    {
      Serial.println("8 Pos Channel 8");
    }
    else if(voltage < 915)
    {
      Serial.println("8 Pos Channel 7");
    }
    else if(voltage < 935)
    {
      Serial.println("8 Pos Channel 6");
    }
    else if(voltage < 955)
    {
      Serial.println("8 Pos Channel 5");
    }
    else if(voltage < 974)
    {
      Serial.println("8 Pos Channel 4");
    }
    else if(voltage < 994)
    {
      Serial.println("8 Pos Channel 3");
    }
    else if(voltage < 1013)
    {
      Serial.println("8 Pos Channel 2");
    }
    else
    {
      Serial.println("8 Pos Channel 1");
    }
  }
}

void set_four()
{
  four_flag = 1;
}

void set_eight()
{
  eight_flag = 1;
}
