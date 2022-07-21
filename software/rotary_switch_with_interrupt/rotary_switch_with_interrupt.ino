volatile int flag = 0;
int count = 0;

void setup() {
  Serial.begin(115200);
  attachInterrupt(4, set, RISING);
}

void loop() {
  if(flag == 1)
  {
    flag = 0;
    count++;
    int sensorValue = analogRead(A0);
    float voltage = sensorValue * (4.96 / 1023.0);
    Serial.print(count);
    Serial.print(" ");
    Serial.println(voltage);
  }
}

void set()
{
  flag = 1;
}
