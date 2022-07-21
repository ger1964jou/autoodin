int sensorPin = A0;    // select the input pin for the potentiometer
int sensorValue = -1;  // variable to store the value coming from the sensor

void setup() {
  Serial.begin(115200); 
}

void loop() {
  sensorValue = analogRead(sensorPin);
  Serial.println(sensorValue);
}
