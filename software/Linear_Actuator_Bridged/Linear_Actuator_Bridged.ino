int c_A;
int c_B;

void setup() {
  Serial.begin(115200);
  pinMode(43, OUTPUT); // Motor Enable
  pinMode(44, OUTPUT); // Motor In 1/4
  pinMode(45, OUTPUT); // Motor In 2/3
  
//  delay(4000);
  
  digitalWrite(43, HIGH); // enable the motor
  // raise
  digitalWrite(44, HIGH);
  digitalWrite(45, LOW);
  
  // lower
//  digitalWrite(44, LOW);
//  digitalWrite(45, HIGH);
}

void loop()
{
//  c_A = analogRead(A2); // Motor A amperage
//  c_B = analogRead(A3); // Motor B amperage
//  Serial.print("A: ");
//  Serial.print(c_A);
//  Serial.print("  B: ");
//  Serial.println(c_B);
}

