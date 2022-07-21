#include <Wire.h>
#include <LiquidTWI2.h>
LiquidTWI2 lcd(0); // 0 = i2c address
unsigned long micro_time = 0;
unsigned long finish_micros = 0;

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
}

void loop() {
  lcd.print("HelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHello");
  lcd.setCursor(0,3);
  lcd.print("J");
  micro_time = micros();
  lcd.home();
  finish_micros = micros();
  Serial.print("Using home: ");
  Serial.println(finish_micros - micro_time);
  
  lcd.print("HelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHelloHello");
  lcd.setCursor(0,3);
  lcd.print("J");
  micro_time = micros();
  lcd.setCursor(0,0);
  finish_micros = micros();
  Serial.print("Using set cursor: ");
  Serial.println(finish_micros - micro_time);
  
}
