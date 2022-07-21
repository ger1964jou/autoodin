#include <Wire.h>
#include <LiquidTWI2.h>
LiquidTWI2 lcd(0); // 0 = i2c address
int start = 0;
int finish = 255;
int incrementor = 1;

void setup() {
  Serial.begin(115200);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  analogWrite(6, 25);
  analogWrite(7, 127);
  analogWrite(8, 220);
  lcd.setMCPType(LTI_TYPE_MCP23008); // must be called before begin()
  lcd.begin(20,4);
  lcd.print("HelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHello");
  // lcd.clear();
  // delay(500);
}

void loop() {
  for(int i = start; i != finish; i += incrementor){
    analogWrite(6, i);
    Serial.print("i = ");
    Serial.println(i);
  }
  for(int j = start; j != finish; j += incrementor){
    analogWrite(7, j);
    Serial.print("i = ");
    Serial.println(j);
  }
  for(int k = start; k != finish; k += incrementor){
    analogWrite(8, k);
    Serial.print("i = ");
    Serial.println(k);
  }
  int temp = start;
  start = finish;
  finish = temp;
  incrementor = incrementor * -1;
}
