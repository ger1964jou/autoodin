#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include "utility/sntp.h"
#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include <LiquidTWI2.h>
#include <Adafruit_ADS1015.h>
#include <Adafruit_MCP4725.h>

#define ADAFRUIT_CC3000_IRQ   2
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    49
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIVIDER);

Adafruit_ADS1115 ads1(0x48);  /* Use this for the 16-bit version */  // ADDR connected to GND (default)
Adafruit_ADS1115 ads2(0x49);  /* Use this for the 16-bit version */  // ADDR connected to VDD (+5V)
Adafruit_MCP4725 dac;
int ads_current_channel;
int ads_current_ads;
int ads_read;

sntp mysntp = sntp("bigben.cac.washington.edu", "time.nist.gov", (short)(-8 * 60), (short)(-8 * 60), false); // contructor only uses the dst offset value!
SNTP_Timestamp_t now;
NetTime_t timeExtract;

#define pF(string_pointer) (reinterpret_cast<const __FlashStringHelper *>(pgm_read_word(string_pointer)))

const prog_char janStr[] PROGMEM = "Jan";
const prog_char febStr[] PROGMEM = "Feb";
const prog_char marStr[] PROGMEM = "Mar";
const prog_char aprStr[] PROGMEM = "Apr";
const prog_char mayStr[] PROGMEM = "May";
const prog_char junStr[] PROGMEM = "Jun";
const prog_char julStr[] PROGMEM = "Jul";
const prog_char augStr[] PROGMEM = "Aug";
const prog_char sepStr[] PROGMEM = "Sep";
const prog_char octStr[] PROGMEM = "Oct";
const prog_char novStr[] PROGMEM = "Nov";
const prog_char decStr[] PROGMEM = "Dec"; 

PROGMEM const char* const monthStrs[] = { janStr, febStr, marStr, aprStr, mayStr, junStr,
                                          julStr, augStr, sepStr, octStr, novStr, decStr}; 

const prog_char sunStr[] PROGMEM = "Sun";
const prog_char monStr[] PROGMEM = "Mon";
const prog_char tueStr[] PROGMEM = "Tue";
const prog_char wedStr[] PROGMEM = "Wed";
const prog_char thuStr[] PROGMEM = "Thu";
const prog_char friStr[] PROGMEM = "Fri";
const prog_char satStr[] PROGMEM = "Sat"; 

PROGMEM const char* const dayStrs[] = { sunStr, monStr, tueStr,  wedStr,
                                        thuStr, friStr, satStr};

#define WLAN_SSID       "Stahl_Lab_Netgear"   // cannot be longer than 32 characters! // "StahlLabNetgear"
#define WLAN_PASS       "sulfate2010"
#define WLAN_SECURITY   WLAN_SEC_WPA2

volatile int four_flag = 1;
volatile int eight_flag = 1;
volatile int pause_button_flag = 0;

unsigned long millis_time_check;

unsigned int four_position_channel;
unsigned int eight_position_channel;
boolean debug_mode;
boolean paused = false;

LiquidTWI2 lcd(0); // 0 = i2c address

File myFile;

const int chipSelect = 10;

RTC_DS1307 rtc;

struct data {                             // data from current scan of channels
    uint32_t time;
    uint16_t adc[16];
} data;

void setup () {
  Serial.begin(115200);
  Wire.begin();
  rtc.begin();
  
  attachInterrupt(1, set, RISING); // interrupt happens after button is let back up with notebook circuit diagram (more consistent than FALLING)
  attachInterrupt(5, set_four, RISING);
  attachInterrupt(4, set_eight, RISING);
  pinMode(32, INPUT); // debug mode pin
  pinMode(28, OUTPUT); // green normal mode LED
  pinMode(29, OUTPUT); // red debug mode LED
  pinMode(35, OUTPUT); // pause button LED
  pinMode(6, OUTPUT); // Red PWM for LCD
  pinMode(7, OUTPUT); // Green PWM for LCD
  pinMode(8, OUTPUT); // Blue PWM for LCD
  analogWrite(6, 25);
  analogWrite(7, 127);
  analogWrite(8, 220);
  lcd.setMCPType(LTI_TYPE_MCP23008); // must be called before begin()
  debug_mode = digitalRead(32);
  Serial.println(debug_mode);
  if(debug_mode)
  {
    digitalWrite(34, HIGH);
  }
  else
  {
    digitalWrite(33, HIGH);
  }
  lcd.begin(20,4);
  
  ads1.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 0.1875mV (default)
  ads2.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 0.1875mV (default)
  ads1.begin();
  ads2.begin();
  dac.begin(0x62);
  
  checkChannelFlags();

  lcdPrint("Free Ram:",String(getFreeRam(), DEC),"");
  
  lcdPrint("Initializing the","CC3000 ...","");
  if (!cc3000.begin())
  {
    lcdPrint("FAIL","Couldn't initialize","CC3000 :(");
    while(1);
  }
  
  lcdPrint("Deleting old","connection profiles","");
  if (!cc3000.deleteProfiles()) {
    lcdPrint("FAIL","Couldn't delete","old profiles :(");
    while(1);
  }
  
  /* Attempt to connect to an access point */
  char *ssid = WLAN_SSID;             /* Max 32 chars */
  lcdPrint("Attempting to","connect to",ssid);
  
  /* NOTE: Secure connections are not available in 'Tiny' mode! */
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    lcdPrint("FAIL","Couldn't connect! :(","");
    while(1);
  }
   
  lcdPrint("Connected!","","");
  
  /* Wait for DHCP to complete */
  lcdPrint("Request DHCP","","");
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  lcdPrint("UpdateNTPTime","","");
  mysntp.UpdateNTPTime();

  Serial.println(F("Current local time is:"));
  
  mysntp.ExtractNTPTime(mysntp.NTPGetTime(&now, true), &timeExtract);

  Serial.print(timeExtract.hour);
  Serial.print(F(":"));
  Serial.print(timeExtract.min);
  Serial.print(F(":"));
  Serial.print(timeExtract.sec);
  Serial.print(F("."));
  Serial.println(timeExtract.millis);
  
  Serial.print(pF(&dayStrs[timeExtract.wday]));
  Serial.print(F(", "));
  Serial.print(pF(&monthStrs[timeExtract.mon]));
  Serial.print(F(" "));
  Serial.print(timeExtract.mday);
  Serial.print(F(", "));
  Serial.println(timeExtract.year);
  
  String hours_minutes_seconds_milliseconds;
  hours_minutes_seconds_milliseconds += timeExtract.hour;
  hours_minutes_seconds_milliseconds += ":";
  hours_minutes_seconds_milliseconds += timeExtract.min;
  hours_minutes_seconds_milliseconds += ":";
  hours_minutes_seconds_milliseconds += timeExtract.sec;
  hours_minutes_seconds_milliseconds += ".";
  hours_minutes_seconds_milliseconds += timeExtract.millis;
  
  String day_month_date_year;
  day_month_date_year += pF(&dayStrs[timeExtract.wday]);
  day_month_date_year += ", ";
  day_month_date_year += pF(&monthStrs[timeExtract.mon]);
  day_month_date_year += " ";
  day_month_date_year += timeExtract.mday;
  day_month_date_year += ", ";
  day_month_date_year += timeExtract.year;
  
  lcdPrint("Current local time:",hours_minutes_seconds_milliseconds,day_month_date_year);
  
  //Serial.print(F("Day of year: "));
  //Serial.println(timeExtract.yday + 1); 

  /* You need to make sure to clean up after yourself or the CC3000 can freak out */
  /* the next time you try to connect ... */
  lcdPrint("Closing the","connection","");
  cc3000.disconnect();
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    lcdPrint("RTC is NOT running!","","");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  
  Serial.print("Initializing SD card...");
  lcdPrint("Initializing","SD card...","");
  pinMode(SS, OUTPUT);
  if (!SD.begin(10,11,12,13)) {
    Serial.println("initialization failed!");
    lcdPrint("initialization","failed!","");
    return;
  }
  Serial.println("initialization done.");
  lcdPrint("initialization done.","","");
  
  DateTime now = rtc.now();
  
  String time_date_stamp;
  time_date_stamp += now.month();
  time_date_stamp += "M";
  time_date_stamp += now.day();
  time_date_stamp += "D";
  
  String time_date_temp_check = time_date_stamp;
  time_date_temp_check += ".csv";
  char temp_filename_check[13];
  time_date_temp_check.toCharArray(temp_filename_check, 13);
  char filename[13];
  
  Serial.print("Checking if ");
  Serial.print(temp_filename_check);
  Serial.println(" exists already.");
  time_date_temp_check += "        ";
  lcdPrint("Checking if",time_date_temp_check,"exists already.");
  
  if(SD.exists(temp_filename_check))
  {
    Serial.println("Filename Found!");
    lcdPrint("Filename Found!","","");
    for(int i = 1; i < 100; i++) // assumption that there will be fewer than 10 new files created in a day (just one probably, maybe 2)
    {
      time_date_temp_check = time_date_stamp;
      time_date_temp_check += i;
      time_date_temp_check += ".csv";
      time_date_temp_check.toCharArray(temp_filename_check, 13);
      Serial.print("Checking if ");
      Serial.print(temp_filename_check);
      Serial.println(" exists already.");
      lcdPrint("Checking if",time_date_temp_check,"exists already.");
      if(!SD.exists(temp_filename_check))
      {
        break;
      }
    }
    Serial.print("Going with ");
    Serial.println(temp_filename_check);
    lcdPrint("Going with",time_date_temp_check,"");
    strcpy(filename, temp_filename_check);
  }
  else
  {
    time_date_stamp += ".csv";
    time_date_stamp.toCharArray(filename, 13);
  }
  
//  char filename[13] = "M11D24#1.csv";
  myFile = SD.open(filename, FILE_WRITE);
  
  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print("Writing to ");
    Serial.print(filename);
    Serial.print("...");
    lcdPrint("Writing to",filename,"...");
    myFile.println("testing 1, 2, 3.");
	// close the file:
    myFile.close();
    Serial.println("done.");
    lcdPrint("Done.","","");
  } else {
    // if the file didn't open, print an error:
    Serial.print("error opening ");
    Serial.println(filename);
    lcdPrint("Error opening",filename,"");
  }
}

void loop ()
{
  lcdPrint("Done.","Free Ram:",String(getFreeRam(), DEC));
}

void lcdPrint(String first_line, String second_line, String third_line/*, String fourth_line*/)
{
  first_line = trimStringEndToTwentyChars(first_line);
  first_line = fillStringEndWithWhitespaceToMakeTwentyChars(first_line);
  second_line = trimStringEndToTwentyChars(second_line);
  second_line = fillStringEndWithWhitespaceToMakeTwentyChars(second_line);
  third_line = trimStringEndToTwentyChars(third_line);
  third_line = fillStringEndWithWhitespaceToMakeTwentyChars(third_line);
//  fourth_line = trimStringEndToTwentyChars(fourth_line);
//  fourth_line = fillStringEndWithWhitespaceToMakeTwentyChars(fourth_line);

  String fourth_line;
  fourth_line += "4:";
  fourth_line += four_position_channel;
  fourth_line += "        ";
  String ads_String;
  ads_String += ads_read;
  while(ads_String.length() < 5)
  {
    ads_String += " ";
  }
  fourth_line += ads_String;
  fourth_line += " 8:";
  fourth_line += eight_position_channel;
  
  String lcd_message;
  lcd_message += first_line;
  lcd_message += third_line;
  lcd_message += second_line;
  lcd_message += fourth_line;
  lcd.print(lcd_message);
  millis_time_check = millis();
  while(millis() - millis_time_check < 1250)
  {
    while(paused)
    {
      checkPauseButton();
    }
    checkChannelFlags();
    checkPauseButton();
  }
}

String trimStringEndToTwentyChars(String msg)
{
  if(msg.length() > 20)
  {
    msg.remove(20);
  }
  return msg;
}

String fillStringEndWithWhitespaceToMakeTwentyChars(String msg)
{
  int string_length = msg.length();
  if(string_length < 20)
  {
    for(int i = 0; i < 20 - string_length; i++)
    {
      msg += " ";
    }
  }
  return msg;
}

void checkChannelFlags()
{
  if(four_flag == 1)
  {
    four_flag = 0;
    int voltage = analogRead(A0);
    if(voltage < 825)
    {
      four_position_channel = 4;
    }
    else if(voltage < 905)
    {
      four_position_channel = 3;
    }
    else if(voltage < 984)
    {
      four_position_channel = 2;
    }
    else
    {
      four_position_channel = 1;
    }
    Serial.print("Four Position Voltage: ");
    Serial.println(voltage);
    dac.setVoltage((four_position_channel * 1000), false); // 1:1221mV (6512)  2:2442mV (13024)  3:3663mV (19536)  4:4884mV (26048)
    lcd.setCursor(2,3);
    lcd.print(four_position_channel);
    lcd.setCursor(11,3);
    if(ads_current_ads == 1)
    {
      ads_read = ads1.readADC_SingleEnded(ads_current_channel);
    }
    else
    {
      ads_read = ads2.readADC_SingleEnded(ads_current_channel);
    }
    lcd.print(ads_read);
    lcd.setCursor(0,0);    
  }
  if(eight_flag == 1)
  {
    eight_flag = 0;
    int voltage = analogRead(A1);
    if(voltage < 906)
    {
      eight_position_channel = 8;
      ads_current_ads = 2;
      ads_current_channel = 3;
    }
    else if(voltage < 924)
    {
      eight_position_channel = 7;
      ads_current_ads = 2;
      ads_current_channel = 2;
    }
    else if(voltage < 942)
    {
      eight_position_channel = 6;
      ads_current_ads = 2;
      ads_current_channel = 1;
    }
    else if(voltage < 960)
    {
      eight_position_channel = 5;
      ads_current_ads = 2;
      ads_current_channel = 0;
    }
    else if(voltage < 978)
    {
      eight_position_channel = 4;
      ads_current_ads = 1;
      ads_current_channel = 3;
    }
    else if(voltage < 997)
    {
      eight_position_channel = 3;
      ads_current_ads = 1;
      ads_current_channel = 2;
    }
    else if(voltage < 1015)
    {
      eight_position_channel = 2;
      ads_current_ads = 1;
      ads_current_channel = 1;
    }
    else
    {
      eight_position_channel = 1;
      ads_current_ads = 1;
      ads_current_channel = 0;
    }
    Serial.print("Eight Position Voltage: ");
    Serial.println(voltage);
    if(ads_current_ads == 1)
    {
      ads_read = ads1.readADC_SingleEnded(ads_current_channel);
    }
    else
    {
      ads_read = ads2.readADC_SingleEnded(ads_current_channel);
    }
    lcd.setCursor(11,3);
    lcd.print(ads_read);
    lcd.setCursor(19,3);
    lcd.print(eight_position_channel);
    lcd.setCursor(0,0);
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

void set()
{
  pause_button_flag = 1;
}

void checkPauseButton()
{
  if(pause_button_flag == 1)
  {
    Serial.println("Flag is 1");
    pause_button_flag = 0;
    if(paused)
    {
      digitalWrite(35, LOW);
      lcd.setCursor(4,3);
      lcd.print("      ");
      lcd.setCursor(0,0);
    }
    else
    {
      digitalWrite(35, HIGH);
      lcd.setCursor(4,3);
      lcd.print("PAUSED");
      lcd.setCursor(0,0);
    }
    paused = !paused;
  }
}





















