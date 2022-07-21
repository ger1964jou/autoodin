int wp = 2;
int cd = 2;

void setup() {
  pinMode(30, INPUT_PULLUP);
  pinMode(31, INPUT_PULLUP);
  Serial.begin(115200);
}

void loop() {
  wp = digitalRead(30);
  cd = digitalRead(31);
  Serial.print("WP: ");
  Serial.print(wp);
  Serial.print("   CD: ");
  Serial.println(cd);
}
