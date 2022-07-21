// Date and time functions using a DS1307 RTC connected via I2C and Wire lib

#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>

File myFile;

const int chipSelect = 10;

RTC_DS1307 rtc;

void setup () {
  Serial.begin(57600);
  Wire.begin();
  rtc.begin();

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  
  Serial.print("Initializing SD card...");
  pinMode(SS, OUTPUT);
  if (!SD.begin(10,11,12,13)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  
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
  
  if(SD.exists(temp_filename_check))
  {
    Serial.println("Filename Found!");
    for(int i = 1; i < 100; i++) // assumption that there will be fewer than 10 new files created in a day (just one probably, maybe 2)
    {
      time_date_temp_check = time_date_stamp;
      time_date_temp_check += i;
      time_date_temp_check += ".csv";
//      char temp_filename_check[13];
      time_date_temp_check.toCharArray(temp_filename_check, 13);
      Serial.print("Checking if ");
      Serial.print(temp_filename_check);
      Serial.println(" exists already.");
      if(!SD.exists(temp_filename_check))
      {
        break;
      }
    }
    Serial.print("Going with ");
    Serial.println(temp_filename_check);
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
    myFile.println("testing 1, 2, 3.");
	// close the file:
    myFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.print("error opening ");
    Serial.println(filename);
  }
  
  // re-open the file for reading:
  myFile = SD.open(filename);
  if (myFile) {
    Serial.println(filename);
    
    // read from the file until there's nothing else in it:
    while (myFile.available()) {
    	Serial.write(myFile.read());
    }
    // close the file:
    myFile.close();
  } else {
  	// if the file didn't open, print an error:
    Serial.print("error opening ");
    Serial.println(filename);
  }
}

void loop () {
  /*
    DateTime now = rtc.now();
    
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.println(now.second(), DEC);
    
    String time_date_stamp = "Y";
    time_date_stamp += now.year();
    time_date_stamp += "M";
    time_date_stamp += now.month();
    time_date_stamp += "D";
    time_date_stamp += now.day();
    time_date_stamp += "H";
    time_date_stamp += now.hour();
    time_date_stamp += "M";
    time_date_stamp += now.minute();
    time_date_stamp += "S";
    time_date_stamp += now.second();
    Serial.println(time_date_stamp);
    
    Serial.println();
    delay(3000);
    */
}
