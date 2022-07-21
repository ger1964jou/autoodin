volatile int flag = 0;
int first = 0;
int count = 0;
unsigned long micro_time = 0;
unsigned long first_micro_time = 0;

void setup() {
  Serial.begin(115200);
  attachInterrupt(1, set, RISING); // interrupt happens after button is let back up with notebook circuit diagram (more consistent than FALLING)
}

void loop() {
  if(flag == 1)
  {
    micro_time = micros();
    if(first == 0)
    {
      first_micro_time = micro_time;
      first = 1;
    }
    flag = 0;
    count++;
    Serial.print(count);
    Serial.print(" CHANGE!!! ");
    Serial.print(micro_time - first_micro_time);
    Serial.println(" microseconds");
  }
}

void set()
{
  flag = 1;
}
