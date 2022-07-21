/*
  Software for Optical Density INvestigator
  System 2
  Written by Jonathan Forbes
  Last updated Dec 26, 2018 (Jessica Hardwicke)
  Based on System 1 (ROGR) software written by Bob Petersen and David Beck
*/

#include <Wire.h>
#include <SPI.h>
#include <ccspi.h>
#include <string.h>
#include "utility/debug.h"
#include "utility/sntp.h"
#include "RTClib.h"
#include <SD.h>
#include <Math.h>
#include <LiquidTWI2.h>
#include <Adafruit_ADS1015.h>
#include <Adafruit_MCP4725.h>
#include <Ethernet.h>

Adafruit_ADS1115 ads1_4(0x48);                     // Create the ADC object interacting with tube boxes 1-4, at I2C address 0x48
Adafruit_ADS1115 ads5_8(0x49);                     // Create the ADC object interacting with tube boxes 5-8, at I2C address 0x49
Adafruit_MCP4725 dac;                              // Create the DAC object, at I2C address 0x62 (default address)

LiquidTWI2 lcd(0); // 0 = i2c address
// These are the pins (PWM) for each color of the RGB backlit LCD display
// According to the datasheet they are supposed to be laid out as such: Red(6), Green(7), Blue(8)
// However it didn't appear to be so, though that could be a consequence of the power issues we've been having on the arduino
const int LCD_Red_PWM = 7;    // 6
const int LCD_Green_PWM = 8;  // 7
const int LCD_Blue_PWM = 6;   // 8
int normal_backlight[]       = {0,   255, 0  };   // Appears GREEN  Normal operation R,G,B values for the backlight
int paused_backlight[]       = {0,   0,   255};   // Appears BLUE   Paused operation R,G,B values for the backlight
int cannot_pause_backlight[] = {255, 0,   0  };   // Appears PURPLE Critical section R,G,B values for the backlight, where pause cannot be used
int debug_mode_backlight[]   = {255, 255, 0  };   // Appears RED    Debug mode R,G,B values for the backlight

// Data sampling time intervals, as selected by the 4 channel rotary switch, in minutes.
// Don't go below 2 minutes since it takes a minimum amount of time to perform the data sampling, and too low an interval will cause the timing to go negative, which cannot happen, resulting in it instead going extremely large, resulting in a sampling interval of many weeks!
// Also, don't exceed about 71,500 minutes (about 7 weeks), as that is about the capacity of an unsigned long in milliseconds.
// More to that point, the experiment shouldn't last longer than 7.1 weeks, due to the capacity of the unsigned long which is used by the millis() system function.
// Running the experiment past this point may work, but could exhibit some undefined behavior.
const int data_sampling_interval_1 = 10;
const int data_sampling_interval_2 = 20;
const int data_sampling_interval_3 = 40;
const int data_sampling_interval_4 = 60;

const int channels = 64;                       // channels in use, up to 64
const int channelstart = 0;                    // the first channel to probe (normally 0);
const int addr_bits = 3;                       // number of bits required to encode (2^3=8, and we're using an 8-bit multiplexer)
const int adc_samples = 40;                    // number of repeated adc samples per each scan of each test tube, there can be an occasional spike in reading, this evens it out, but too much slows down the sampling

const int mux_addr_pins[] = {39, 40, 41};      // pins on the arduino for addressing the multiplexer (picking which channel for the multiplexer to output)
const int mux_enablenot_pin = 38;              // pin on the arduino for controlling the multiplexer NOT enable, when this pin is high the multiplexer is disabled, when low it is enabled

const int relay_pin = 36;                      // pin on the arduino which controls the relay, when high, shaker is on, when low, shaker is off
boolean shaker_shaking = false;                // flag for indicating the status of the shaker, initialized to false to indicate that the shaker starts immobilized

const int Motor_En_Pin = 44;                   // pin on the arduino which controls the motor enable, when LOW, the motor will not run
// There are two inputs on the linear actuator, but the motor controller is dual channel, therefore the two channels were bridged with wiring in order to maximize motor current (available power)
// In order to raise the linear actuator, you want pins 1 & 4 to be HIGH, and pins 2 & 3 to be LOW, in order to lower the linear actuator, you want the opposite.
const int Mtr_In_1_4_Pin = 45;                 // pin on the arduino which controls channels 1 & 4 of the motor controller, which both go to one of the channels on the linear actuator
const int Mtr_In_2_3_Pin = 46;                 // pin on the arduino which controls channels 2 & 3 of the motor controller, which both go to one of the channels on the linear actuator

const int debug_selection_pin = 32;            // input pin that reads the debug switch
const int normal_Green_LED = 28;               // output to green LED indicating if normal mode has been activated
const int debug_Red_LED = 29;                  // output to red LED indicating if debug mode has been activated

const int chipSelect = 10;                     // pin on the arduino which tells the SD Card system that it should listen to the data on the SPI bus (used by the SPI library, but needed to be called out here)
const int SD_Card_write_protected_or_missing = 30; // input pin which reads the write-protection status of the SD card, also whether the card is present or not, LOW unless card is present and not write protected, HIGH otherwise
const int SD_Card_not_physically_detected = 31;    // input pin which reads the physical status of the SD card, whether it is present in the slot, LOW unless the card is not present, then it is HIGH

const unsigned long channel_select_delay_ms = 20;  // time to allow the multiplexer to settle after channel selection in milliseconds
const unsigned long shaker_shutdown_ms = 8000;     // time in milliseconds to allow shaker table to stop moving after being shut off by relay
unsigned long timestep = data_sampling_interval_4 * 60000; // time in milliseconds between scans, the data sampling interval (don't change this, change "data_sampling_interval" above), initialized to the longest time sampling interval above

unsigned long start_time;                      // keeps track of how long the system took to perform all setup operations, so that time can be deducted from the total system time reported to the server in each loop iteration
File myFile;                                   // this is the variable that represents the file being written to the SD card containing experiment data
char filename[13];                             // variable holding the filename of the file being written to the SD card
int write_protected_or_missing = 1;            // keeps track of whether the SD card is in a write-protected state (or missing), used to store the result of the digitalRead on the wp pin
int card_not_physically_detected = 1;          // keeps track of whether the SD card is physically detected in the SD card slot, used to store the result of the digitalRead on the cd pin
boolean write_to_SD_card = false;              // lets the system keep track of whether to write data to the SD card or not, will change to TRUE if an SD Card is present (and not write-protected) at system setup
RTC_DS1307 rtc;                                // variable which represents the real-time-clock in the system

struct data {                                  // data structure that will be sent via UDP to the server, containing a single data sampling
  unsigned long time;                          // the time, since the end of system setup, in milliseconds, of the data sampling
  int adc[64];                                 // 64 integer values corresponding to the optical density of each emitter sensor unit as reported by the ADC
}
data;

int calibration_voltage[64];                   // each calibration voltage (in terms of DAC set points 0<->4095) for emitter sensor sampling unit
int actuator_position;                         // keeps track of where the linear actuator is from fully extended, to fully retracted
int actuator_current;                          // keeps track of the current flowing through the linear actuator, too much and it might be stuck on something!
unsigned long motor_run_time;                  // keeps track of how long the motor has been running, in milliseconds

int four_position_channel;                     // keeps track of which channel is currently set on the 4-channel rotary switch
int eight_position_channel;                    // keeps track of which channel is currently set on the 8-channel rotary switch
volatile int four_channel_flag = 1;            // signals that the 4-channel switch has been changed, but that change hasn't been handled by the software yet
volatile int eight_channel_flag = 1;           // signals that the 8-channel switch has been changed, but that change hasn't been handled by the software yet
volatile int pause_button_flag = 1;            // Signals that the pause button has been pressed, but that event hasn't been handled by the software yet. Set to 1 so the system investigates the pause status when initially asked at system start
boolean paused = false;                        // Keeps track of the pause state of the system.
                                               // Start the system in paused mode to let the LED's come up to temperature before calibrating and sending data to the server.
                         // Paused = FALSE here because the pause check will assume the system is coming from an unpaused state to begin the pause

boolean debug_mode;                            // Keeps track of the debug mode status of the system.
String ram;                                    // String to store the RAM status for periodic display (for debug purposes)

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  // MAC address for device (must be unique!)
IPAddress data_server_ip(172, 28, 236, 69);                // IP of remote host that will receive the data
const uint16_t data_server_port = 8577;                     // must be unique!
EthernetUDP udp;                                            // UDP instance for sending & receiving
const int PACKET_SIZE = sizeof(struct data);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Sets up system functions, runs once at system start
void setup()
{
  Serial.begin(115200); // starts serial communications (not related to SPI or I2C)
  lcd.setMCPType(LTI_TYPE_MCP23008); // must be called before lcd.begin(20,4), tells the lcd library which type of lcd controller is being used
  lcd.begin(20,4); // the lcd being used has 4 rows, each containing 20 characters
  Wire.begin(); // begin the wire library

  // set the lcd backlight pins to output
  pinMode(LCD_Red_PWM, OUTPUT);
  pinMode(LCD_Green_PWM, OUTPUT);
  pinMode(LCD_Blue_PWM, OUTPUT);

  // set the multiplexer pins to output, and enable the multiplexer
  pinMode(mux_addr_pins[0], OUTPUT);
  pinMode(mux_addr_pins[1], OUTPUT);
  pinMode(mux_addr_pins[2], OUTPUT);
  pinMode(mux_enablenot_pin, OUTPUT);
  digitalWrite(mux_enablenot_pin, LOW);    //Enables the multiplexer, because the pin on the multiplexer is NOT_ENABLE

  // setup the ADC's and DAC
  ads1_4.setGain(GAIN_TWOTHIRDS); // set the first ADC so that the range of input is (+-)6.144V, so the output from the tube box board doesn't fry the ADC. (this is also the default value)
  ads5_8.setGain(GAIN_TWOTHIRDS); // set the second ADC so that the range of input is (+-)6.144V, so the output from the tube box board doesn't fry the ADC. (this is also the default value)
  // startup both ADC's
  ads1_4.begin();
  ads5_8.begin();
  dac.begin(0x62); // startup the DAC and communicate with it via I2C address 0x62

  // initialize data container with a NULL value of -1, which cannot be replicated by the system,
  // so FileMakerPro can tell which tubes are in active use if all 64 values are sent, but only some are written to. (Not currently a feature, programming ahead)
  for (int k = 0; k < 64; k++)
  {
    data.adc[k] = -1;
  }
  // Configure the Ethernet Shield to send UDP data packets to the server
  Serial.println("setup: configuring ethernet via DHCP");
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("setup: Ethernet.begin failed, halting");
    lcd.setCursor(0,0);
    lcd.print("setup: Halted");
    lcd.setCursor(0,1);
    lcd.print("Ethernet.begin Fail");
    lcd.setCursor(0,0);
    while (1)
    {
      delay(1);          // Infinite halt. Waiting on reset button.
    }
  }
  Serial.print("setup: ethernet configuration complete, IP address is: ");
  Serial.println(Ethernet.localIP());
  if (udp.begin(data_server_port) == 0)
  {
    Serial.println("setup: udp.begin failed, halting");
    lcd.setCursor(0,0);
    lcd.print("setup: Halted");
    lcd.setCursor(0,1);
    lcd.print("udp.begin Fail");
    lcd.setCursor(0,0);
    while (1)
    {
      delay(1);          // Infinite halt. Waiting on reset button.
    }
  }

  // Pause the system, this allows the user to hold off on running the experiment before the LED's come up to temperature
  // Also, it gives the user one last chance to select either normal, or debug operating mode without having to restart the system
  // When the pause button is pressed, the signal coming from it goes low, before rising again as the button itself rises after the pressed
  // The button itself is noisy, and thus is used in concert with a debounce circuit to help there be only one button press event corresponding to each physical button press
  Serial.println("Waiting for Pause Button to be pressed");
  attachInterrupt(1, pause_button_interrupt_service_routine, RISING); // tells the system to run the pause button interrupt service routine when the button is released (more consistent results than FALLING which corresponds to when the button is initially pressed down)
  checkPauseButton(); // check the status of the pause button flag (which has been initialized to 1, so the system should pause, nevertheless if changed this code won't need to be modified)
  if (paused) // pause the system to reflect the pause state the system should be in
  {
    system_pause(); // run the pause routine
  }

  // Set the motor controller pins to output and enable the motor controller
  pinMode(Motor_En_Pin, OUTPUT); // Motor Controller Enable pin
  pinMode(Mtr_In_1_4_Pin, OUTPUT); // Motor Controller Input pin for channels 1/4
  pinMode(Mtr_In_2_3_Pin, OUTPUT); // Motor Controller Input pin for channels 2/3
  digitalWrite(Motor_En_Pin, HIGH); // enable the motor to be controlled

  debug_mode = digitalRead(debug_selection_pin); // read the operation mode selector
  if (debug_mode) // if the selector indicated debug mode, then branch off into debug operations
  {
    detachInterrupt(1);                             // disable the pause button from interrupting throughout debug mode
    attachInterrupt(4, eight_channel_isr, RISING);  // enable the 8 channel rotary switch, which selects which tube box to provide constant output from
    set_LCD_Color(debug_mode_backlight);            // change the backlight to debug lighting (RED)
    digitalWrite(debug_Red_LED, HIGH);              // power on the red debug LED to indicate the debug operating mode
    Serial.println("Debug Mode");
    lcd.setCursor(0,0);
    lcd.print("Debug Mode");
    lcd.setCursor(0,1);
    lcd.print("Raising Platform");
    lcd.setCursor(0,0);
    raise_platform();                               // raise the lifting platform so calibration can commence
    calibrate();                                    // calibrate the sensor channels
    lcdPrint("1:         5:       ","2:         6:       ","3:         7:       ","4:         8:       "); // sets up the static channel indicators on the LCD
    while (1)
    {
      debug_loop();                                   // skip the rest of the normal program and run only the debug routine
    }
  }

  // If NOT in debug mode, continue running program here
  digitalWrite(normal_Green_LED, HIGH); // light up the green, normal operations mode led
  Serial.println("Normal Mode");

  attachInterrupt(5, four_channel_isr, RISING); // enable the 4 channel rotary switch, which selects the data sampling interval from the choices set as constants above

  rtc.begin(); // begin the real-time-clock
  set_LCD_Color(normal_backlight); // set normal operating mode backlighting on the LCD

  pinMode(SS, OUTPUT); // set the SPI Slave Select pin on the Arduino as output
  // set the two SD Card check pins as Input Pullup, in order to avoid electrical "floating" (only way they work, they need this high resistance connection to ground)
  pinMode(SD_Card_write_protected_or_missing, INPUT_PULLUP);
  pinMode(SD_Card_not_physically_detected, INPUT_PULLUP);

  pinMode(relay_pin, OUTPUT);              //Configure relay control pin as output
  digitalWrite(relay_pin, LOW);            //Open the shaker relay, shutting the power off to the shaker table

  // Start the real-time-clock if it isn't running already
  // If it weren't running, you could tell by the serial output, or that the names of the SD card files keep referencing the same date
  // Note: There needs to be a battery in the rtc at all times, to avoid undefined behavior, even if the battery is dead
  if (! rtc.isrunning())
  {
    Serial.println("RTC is NOT running!");
    lcdPrint("RTC is NOT running!","","","");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  // Get the current date & time from the RTC.
  DateTime now = rtc.now();
  // Print the current date & time for RTC debug purposes if necessary.
  Serial.println("RTC current datetime:");
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

  set_critical_section_mode(); // sets the system to "critical section" mode, which disables interrupts so that nothing will bother the sensitive operations that follow
  initializeSDCardFile(); // determines if SD Card writing is possible, and comes up with a unique filename to use for this system session

  Serial.println("Raising the platform for calibration");
  lcd.setCursor(0,3);
  lcd.print("Raising Platform");
  lcd.setCursor(0,0);
  raise_platform(); // raises the lifting platform so calibration can commence
  data.time = 1; // sets the time in the data packet to 1, the very first sampling will be later than this due to the time it takes to perform the samples
  calibrate(); // calibrate the sensor channels
  Serial.println("setup: complete");
  start_time = millis(); // keep track of the time elapsed so far, so it can be subtracted from the system time to better indicate the time since the samples were first checked
}

// Calibrates the emitter/sensor channels, tries to get the freshly inoculated media as close to a sensor output of 0.3V (ADC value 1600) as possible
// to allow for maximum capacity for growth sensing, as well as a little buffer for the media getting clearer before it occludes
void calibrate()
{
  Serial.println("Begining calibration");
  // This algorithm is logarithmic in nature, as it takes the middle calibration value, and after determining its suitability
  // sets low or high (depending) to that value, and once again tries the middle, in order to very quickly ascertain the best calibration voltage
  // The best value is also kept track of, in case the final value checked is slightly less suitable than one seen before, since it is difficult to get the calibration dead on.
  int low;
  int mid;
  int high;
  double value;
  boolean done; // algorithm is complete (true/false)
  double best_value;
  int best_calibration_voltage;

  // Start calibrating all channels!
  for (int k = 0; k < channels - channelstart; k++) // k is the number of channels to calibrate (minus one)
  {
    Serial.print("\nChannel ");
    Serial.println(k + channelstart);
    channel_select(k + channelstart); // set the multiplexer to output the voltage from the set channel

  // this block outputs to the LCD the current channel being calibrated, as well as the free system RAM at that moment
    String channel;
    channel += "Channel ";
    channel += (k + channelstart);
    ram = String(getFreeRam(), DEC);
    lcdPrint(channel,"","Free RAM",ram);

    low = 0;     // 0V on the DAC
    mid = 2047;  // 2.5V on the DAC
    high = 4095; // 5V on the DAC
    value = 0.0;
    done = false;
    best_value = 100000.0; // out of bounds value to start so that the first value tested will definitely replace it
    best_calibration_voltage = 0;

  // Start calibrating a channel!
    while (!done)
    {
      //Serial.print("mid: ");
      //Serial.println(mid);
      dac.setVoltage(mid, false); // set the voltage on the DAC to the midpoint value of the range
                                // false indicates that we don't want to set the DAC to automatically output that voltage every time it wakes up from now on
      delay(20); // wait a moment to let the DAC set the voltage, and allow that voltage to propagate throughout the system of sensors
      value = adc_read(k + channelstart); // read the output voltage from this channel being investigated
      //Serial.print("low: ");
      //Serial.print(low);
      //Serial.print("  mid: ");
      //Serial.print(mid);
      //Serial.print("  high: ");
      //Serial.print(high);
      //Serial.print("  value: ");
      //Serial.println(value);
      //Serial.println();
      if (abs(value - 1600) < abs(best_value - 1600)) // if the output value is the closest to 1600 yet, then keep track of it as the current best
      {
        best_value = value;
        best_calibration_voltage = mid;
      }
      if (value > 1600) // if the output value is less than 1600, then narrow the range by only considering values above the DAC value tested
      {
        high = mid;
        mid = (low + high) / 2;
      }
      else // if the output value is greater than 1600, then narrow the range by only considering values below the DAC value tested
      {
        low = mid;
        mid = (low + high) / 2;
      }
      if (low == mid || mid == high) // if there is no more range left, then all relevant values have been considered and one will be the best
      {
        done = true; // end calibration for this sensor
      }
    }
    Serial.print("Best PWM: ");
    Serial.print(best_calibration_voltage);
    Serial.print("  Best Value: ");
    Serial.println(best_value);

    // set the data field for this channel as well as a separate calibration array to what was determined to be the best calibration DAC value
    data.adc[k + channelstart] = best_calibration_voltage;
    calibration_voltage[k + channelstart] = best_calibration_voltage;
  }

  write_data_to_SD_card(); // write the calibration values to the SD card in the guise of the first packet sent, this is for debugging purposes
  delay(1000); // give the system a second to ensure all SD Card operations have completed before potentially using the SPI bus again

  sendpacket(); // send the calibration values to the server via the Ethernet Shield
  delay(1000); // give the system a second to ensure all SPI bus operations have completed
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constantly outputs the ADC values for a single tube box selected by the 4 Channel switch, to the LCD display
// Does NOT record any of the information anywhere!
void debug_loop()
{
  int count = 1; // keeps track of which channel in the individual tube box to look at
  checkEightChannelFlag(); // check which tube box the user has selected to investigate
  int channel = (eight_position_channel - 1) * 8; // determine the first channel in the selected tube box
  int tube_data; // keeps track of the read ADC data from each channel before it gets output to the display
  while (count <= 8) // read the ADC for each channel in the selected tube box
  {
    channel_select(channel); // set the multiplexers to the right channel
    dac.setVoltage(calibration_voltage[channel], false); // set the DAC to the calibration voltage for that channel
    delay(20); // short delay to allow the DAC to come to the set Voltage and allow it to propagate throughout the system
    tube_data = adc_read(channel); // read the ADC value for the set channel
    // Write the read value to the LCD, in the proper place
    if (count == 1)
    {
      lcdDebugPrint(3, 0, tube_data);
    }
    else if (count == 2)
    {
      lcdDebugPrint(3, 1, tube_data);
    }
    else if (count == 3)
    {
      lcdDebugPrint(3, 2, tube_data);
    }
    else if (count == 4)
    {
      lcdDebugPrint(3, 3, tube_data);
    }
    else if (count == 5)
    {
      lcdDebugPrint(14, 0, tube_data);
    }
    else if (count == 6)
    {
      lcdDebugPrint(14, 1, tube_data);
    }
    else if (count == 7)
    {
      lcdDebugPrint(14, 2, tube_data);
    }
    else // must be 8 then
    {
      lcdDebugPrint(14, 3, tube_data);
    }
    count++; // go to the next channel in the tube box
    channel++; // go to the next channel in the system
  }
}

// Takes a data sample, sends it in UDP packet form to the server, and writes it in CSV form to the SD Card
// Then waits until the data interval completes, while shaking the tube rack, before beginning over again.
void loop() {
  Serial.println("beginning sample");
  unsigned long scan_time;              // times how long it takes to complete a scan so that it can be subtracted from the data sampling interval (so the sampling times line up with the interval set)
  scan_time = millis();                 // record starting time for this sampling set
  Serial.println("Waiting for Shaker Table to stop");
  lcd3LinePrint("Waiting for Shaker"," Table to shut down","");
  shaker_stop();                        // shut down the shaker table and wait for it to stop and come to a rest
  Serial.println("Raising Platform");
  lcd3LinePrint("  Raising Platform","","");
  raise_platform();                     // raise the platform so readings can be taken
  data.time = millis() - start_time;    // record the time of the current set of readings, minus the time it took to initially set up the system
  lcd3LinePrint("Taking Data Readings","  From Tube Boxes","");
  Serial.println(data.time);
  for (int i = 0; i < channels - channelstart; i++) // scan the channels and perform adc readings
  {
    if (i % 8 == 0)
    {
      Serial.println("\n");
      for (int k = i; k < i+8; k++) // this loop helps set up a matrix for easy Serial output readability
      {
        Serial.print(k);
        Serial.print("\t");
      }
      Serial.println();
    }
    channel_select(i + channelstart); // set the multiplexer to the channel to be investigated
    dac.setVoltage(calibration_voltage[i], false); // set the DAC to the calibration voltage for that channel
    delay(20); // short delay to allow the DAC to come to the set Voltage and allow it to propagate throughout the system
    int tube_data = adc_read(i + channelstart); // read, and save the ADC value from the channel
    data.adc[i + channelstart] = tube_data; // set that ADC value read to the data slot for that channel in the data structure
    Serial.print(tube_data);
    Serial.print("\t");
  }
  Serial.println("\n");
  write_data_to_SD_card(); // writes to the SD card first in case things take awhile with the WiFi
  delay(1000); // give the system a second to ensure all SD Card operations have completed before potentially using the SPI bus again

  boolean send_to_server = true; // assume we'll be able to send the data to the server until proven otherwise
  byte ethernet_maintenance_return = Ethernet.maintain();
  switch(ethernet_maintenance_return)
  {
    case 1: Serial.println("Ethernet DHCP Renewal Failed");
            lcd3LinePrint("Ethernet DHCP","Renewal Failed","Reconfiguring");
            delay(1500); // delay so that the LCD message will get noticed (if it's a problem for the user)
            send_to_server = false;
            break;
    case 2: Serial.println("Ethernet DHCP Renewal Succeeded");
            break;
    case 3: Serial.println("Ethernet DHCP Rebind Failed");
            lcd3LinePrint("Ethernet DHCP","Rebind Failed","Reconfiguring");
            delay(1500); // delay so that the LCD message will get noticed (if it's a problem for the user)
            send_to_server = false;
            break;
    case 4: Serial.println("Ethernet DHCP Rebind Succeeded");
            break;
  }
  if (!send_to_server) // If the Ethernet failed, try to fix it
  {
    udp.stop();
    // Reconfigure the Ethernet Shield to send UDP data packets to the server
    Serial.println("loop: reconfiguring ethernet via DHCP");
    if (Ethernet.begin(mac) == 0)
    {
      Serial.println("loop: Ethernet.begin failed, skipping write to server.");
      lcd3LinePrint("loop: Ethernet","reconfiguration fail","Ethernet.begin fail");
      delay(1500); // delay so that the LCD message will get noticed (if it's a problem for the user)
    }
    else
    {
      Serial.print("loop: ethernet reconfiguration complete, IP address is: ");
      Serial.println(Ethernet.localIP());
      if (udp.begin(data_server_port) == 0)
      {
        Serial.println("loop: udp.begin failed, skipping write to server.");
        lcd3LinePrint("loop: Ethernet","reconfiguration fail","udp.begin fail");
        delay(1500); // delay so that the LCD message will get noticed (if it's a problem for the user)
      }
      else
      {
        lcd3LinePrint("loop: Ethernet","reconfiguration","success");
        delay(1500); // delay so that the LCD message will get noticed (user info regarding successful reconfiguration)
      }
    }
  }

  if (send_to_server)
  {
    sendpacket(); // send the data packet to the server via the Ethernet Shield
  }
  
  delay(1500); // give the system a some time to ensure all SPI bus operations have completed, and that error messages can be read

  Serial.println("Lowering Platform");
  lcd3LinePrint("Lowering the","       Platform","");
  lower_platform(); // lower the lifting platform so that shaking can commence
  Serial.println("Starting Shaker Table");
  lcd3LinePrint("Starting the","    Shaker Table","");
  shaker_start();                       // restart the shaker table
  scan_time = millis() - scan_time;     // compute time elapsed to shut down shaker, take readings and output
  printdelayinfo(scan_time);            // prints to Serial details of the scan timing intervals
  Serial.println("waiting until next sampling time");

  pause_button_flag = 0; // clear any presses that might have occurred during the sampling (could be spurious)

  resume_normal_mode(); // re-enable interrupts, as the system is no longer in "critical-section" mode

  unsigned long scan_end_time = millis(); // record the system time when sampling operations were complete

  // shake while the system time is less than the time recorded at the end of the last sampling run, plus the data interval, minus the time it takes to sample the data
  while (millis() < (scan_end_time + (timestep - scan_time)))
  {
    checkFourChannelFlag(); // check to see if a new data sampling interval has been set
    //checkPauseButton(); // check to see if the pause button has been pressed

  /*
  // System pause loop, shuts down the shaker table, and waits for the pause button to be pressed again
    if (paused)
    {
      digitalWrite(relay_pin, LOW); // turn off the shaker (didn't call shaker_stop() because we want to pause before waiting for the shaker to stop
      shaker_shaking = false; // let the system know the shaker table has been turned off (since we didn't use shaker_stop()
      system_pause();  // pause the system until the pause button is pressed again
      shaker_start();  // start the shaker table back up again
    }
    delay(100); // wait a small moment so the LCD doesn't flash so fast it is unreadable...
  */

  // Output the remaining time to the display, which is in milliseconds
    String lcd_loop_first_line = "";
    lcd_loop_first_line += "Interval Mins: ";
    lcd_loop_first_line += (timestep/60000); // Display the interval time in minutes
    lcd3LinePrint(lcd_loop_first_line, "Remaining Millisecs", String((scan_end_time + (timestep - scan_time)) - millis())); // This *could* be set to seconds/minutes/etc. by dividing the calculated number inside the string cast by the appropriate constant

    checkFourChannelFlag(); // check if the user changed the data sampling interval while the system was paused, this is the last chance before the loop can potentially end and the system takes another sample
  }
  set_critical_section_mode(); // since the waiting/shaking period is over and the loop is set to begin anew, disable all interrupts as the system is once again in "critical-section" mode
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Returns the ADC value of the optical density of a single tube channel, performing as many ADC readings as specified by the global constant "adc_samples", and computing the average of those samples
int adc_read(int channel)
{
  double a = 0.0;               // stores the eventual output of this method, each ADC reading summed together, divided by the number of readings taken to compute an average reading of a sample
  //  double low = 100000.0;        // keeps track of the lowest value read by the ADC for this channel, during this sampling, initialized to a very high value so the first reading will replace it
  //  double high = 0.0;            // keeps track of the highest value read by the ADC for this channel, during this sampling
  double current_read;          // stores each ADC reading as they occur
  for (unsigned int k = 0; k < adc_samples; k++) // take as many readings as specified by the constant "adc_samples", extra readings are sometimes required in order to smooth out anomolous readings
  {
    if (channel / 8 < 4)        // determine which of the two ADC chips to read, and which channel on those chips to read for each of the tubes being read
    {
      current_read = ads1_4.readADC_SingleEnded(channel / 8);
    }
    else
    {
      current_read = ads5_8.readADC_SingleEnded((channel - 32) / 8);
    }
    /*
    if (current_read > high) {  // If the current reading for this tube sample is higher than the highest for this tube sample, then replace the highest with this reading
      high = current_read;
    }
    if (current_read < low) {   // If the current reading for this tube sample is higher than the highest for this tube sample, then replace the highest with this reading
      low = current_read;
    }
    */
    a = a + current_read;       // sum all the readings for each tube, during this sampling together
  }
  a = a / adc_samples;            // divide the accumulated sum by the number of readings taken, to compute an average
  /*
  Serial.print("High: ");
  Serial.print(high);
  Serial.print("   Avg: ");
  Serial.print(a);
  Serial.print("   Low: ");
  Serial.println(low);
  */
  return round(a); // return the rounded average since it will be stored as an integer
}

// Writes the current time and channel output values from the data structure to the current file on the SD Card
void write_data_to_SD_card()
{
  // Check if the SD card is physically present, and not write protected
  write_protected_or_missing = digitalRead(SD_Card_write_protected_or_missing);
  card_not_physically_detected = digitalRead(SD_Card_not_physically_detected);

  // if writing to the SD Card is possible, and the system is set for it (write_to_SD_card = true), perform the write
  if (write_to_SD_card && !write_protected_or_missing && !card_not_physically_detected)
  {
    myFile = SD.open(filename, FILE_WRITE); // open the file on the SD Card

    // if the file opened okay, write to it:
    if (myFile)
    {
      Serial.print("Writing to ");
      Serial.print(filename);
      Serial.print("...");
      lcdPrint("Writing to",filename,"...","");

      DateTime now = rtc.now(); // Get the current Date/Time as perceived by the real-time-clock
      myFile.print(now.year(), DEC);
      myFile.print('/');
      myFile.print(now.month(), DEC);
      myFile.print('/');
      myFile.print(now.day(), DEC);
      myFile.print(' ');
      myFile.print(now.hour(), DEC);
      myFile.print(':');
      myFile.print(now.minute(), DEC);
      myFile.print(':');
      myFile.print(now.second(), DEC);
      myFile.print(",");              // write a comma to the file (so it imports to excel better)
      myFile.print(data.time);        // write the experiment time (in milliseconds) to the file
      myFile.print(",");              // print the comma separation
      for (int i = 0; i < 64; i++)
      {
        myFile.print(data.adc[i]);    // write the channel data to the file
        myFile.print(",");            // print the comma separation
      }
      myFile.println();  //write a line ending so that the next data sampling to be written ends up on a new line
      myFile.close(); // close the file:
      Serial.println("done.");
      lcdPrint("Done.","","","");
    }
    else
    {
      // if the file didn't open, print an error to Serial:
      Serial.print("error opening ");
      Serial.println(filename);
      lcdPrint("Error opening",filename,"","");
    }
  }
  // SD Card is either missing, or write protected, or the system isn't set to write to the SD Card (write_to_SD_card = false)
  else
  {
    Serial.println("SD Card write-protected or missing or was either at system start, skipping write to SD Card");
    lcdPrint("Skipping Write to SD","Card","","");
    delay(1500); // delay so that the LCD message will get noticed (if it's a problem for the user)
  }
}

// Sends the data packet to the server via the Ethernet Shield
void sendpacket()
{
  Serial.println("Sending data via Ethernet.");
  lcdPrint("Sending data","via Ethernet","","");

  // the data structure was filled during the scan, now create a udp packet and send it
  if (udp.beginPacket(data_server_ip, data_server_port) == 0)
  {
    Serial.println("Ethernet sendpacket error: udp.beginPacket failed, skipping packet send.");
    lcdPrint("Ethernet Problem","udp.beginPacket Fail","Skipping Packet","Sending to Server");
    delay(1500); // delay so that the LCD message will get noticed (if it's a problem for the user)
  }
  else
  {
    udp.write((byte *)&data, PACKET_SIZE);
    if (udp.endPacket() == 0)
    {
      Serial.println("Ethernet sendpacket error: udp.endPacket failed, skipping packet send.");
      lcdPrint("Ethernet Problem","udp.endPacket Fail","Skipping Packet","Sending to Server");
      delay(1500); // delay so that the LCD message will get noticed (if it's a problem for the user)
    }
  }
}

// Turns off the relay, which in turn shuts off the shaker table, also waits until the shaker table comes to a rest
void shaker_stop()
{
  digitalWrite(relay_pin, LOW); // turn off the relay (not inside conditional just to make sure it happens (takes no time at all))
  if (shaker_shaking) // if the system thinks the shaker table is shaking
  {
    shaker_shaking = false; // set the mode to non-shaking
    delay(shaker_shutdown_ms);    // wait for the shaker to stop shaking
  }
}

// Turns on the relay, which in turn powers on the shaker table, which holds onto the last set value, and spins up to that rpm
// This function doesn't wait for the shaker table to get up to speed
void shaker_start()
{
  digitalWrite(relay_pin, HIGH);  // turn the relay on, turning the shaker table on
  shaker_shaking = true; // set the mode to shaking
}

// Raises the lifting platform, and waits until it is completely lifted
void raise_platform()
{
  digitalWrite(Motor_En_Pin, HIGH); // enable the motor controller
  // extend the linear actuator, lifting the table (a limit switch on the linear actuator will automatically stop the motor when fully extended)
  digitalWrite(Mtr_In_1_4_Pin, HIGH);
  digitalWrite(Mtr_In_2_3_Pin, LOW);

  actuator_position = analogRead(A2); // read the actuator position
  motor_run_time = millis(); // save the time when the motor started extending
  while ((millis() - motor_run_time) < 120000)  // run motor until 120 seconds has elapsed (to clear bubbles)
  {
    actuator_position = analogRead(A2); // read the actuator position
    actuator_current = analogRead(A3); // read the current going to the motor
    if (actuator_current > 500) // if the current is too high (500 is just arbitrary, figure out what a better number is)
    {
      digitalWrite(Motor_En_Pin, LOW); // disable the motor
      lcdPrint("OBSTRUCTION!!!","","","");
      while (1) {} // pause forever (there needs to be programmed a way to gracefully recover from this situation)
    }
  }
  digitalWrite(Motor_En_Pin, LOW); // disable the motor
  Serial.print("Actuator Position: ");
  actuator_position = analogRead(A2); // read the actuator position
  Serial.println(actuator_position);
}

// Lowers the lifting platform, and waits until it is completely lowered
void lower_platform()
{
  digitalWrite(Motor_En_Pin, HIGH); // enable the motor controller
  // retract the linear actuator, lowering the table (a limit switch on the linear actuator will automatically stop the motor when fully retracted)
  digitalWrite(Mtr_In_1_4_Pin, LOW);
  digitalWrite(Mtr_In_2_3_Pin, HIGH);

  actuator_position = analogRead(A2); // read the actuator position
  motor_run_time = millis(); // save the time when the motor started extending
  while ((millis() - motor_run_time) < 20000)  // run motor until 20 seconds has elapsed (to park it before enabling the shaker table)
  {
    actuator_position = analogRead(A2); // read the actuator position
    actuator_current = analogRead(A3); // read the current going to the motor
    if (actuator_current > 500) // if the current is too high (500 is just arbitrary, figure out what a better number is)
    {
      digitalWrite(Motor_En_Pin, LOW); // disable the motor
      lcdPrint("OBSTRUCTION!!!","","","");
      while (1){} // pause forever (there needs to be programmed a way to gracefully recover from this situation)
    }
  }
  digitalWrite(Motor_En_Pin, LOW); // disable the motor
  Serial.print("Actuator Position: ");
  actuator_position = analogRead(A2); // read the actuator position
  Serial.println(actuator_position);
}

// Prints to the Serial console the timing information currently set (and passed)
void printdelayinfo(unsigned long scan_time)
{
  Serial.println("");
  Serial.print("The scan time is ");
  Serial.print(scan_time);
  Serial.println("");
  Serial.print("The timestep is ");
  Serial.print(timestep);
  Serial.println("");
  Serial.print("The delay time is ");
  Serial.print(timestep - scan_time);
  Serial.println("");
}

// Sets the multiplexer channel select pins to the channel passed
void channel_select(int channel)
{
  digitalWrite(mux_enablenot_pin, HIGH); // disable the multiplexer
  // set the select pins
  digitalWrite(mux_addr_pins[0], bitRead(channel, 0));
  digitalWrite(mux_addr_pins[1], bitRead(channel, 1));
  digitalWrite(mux_addr_pins[2], bitRead(channel, 2));
  digitalWrite(mux_enablenot_pin, LOW); // re-enable the multiplexer
  delay(channel_select_delay_ms); // delay for the multiplexer to make the connection
}

// Prints the passed messages to the lines on the LCD
// first 20 characters of the first passed String will be displayed on the first line of the LCD
// first 20 characters of the second passed String will be displayed on the second line of the LCD
// first 20 characters of the third passed String will be displayed on the third line of the LCD
// first 20 characters of the fourth passed String will be displayed on the fourth line of the LCD
void lcdPrint(String first_line, String second_line, String third_line, String fourth_line)
{
  // each line is trimmed to 20 characters, and/or filled with spaces to make 20 characters if short
  first_line = trimStringEndToTwentyChars(first_line);
  first_line = fillStringEndWithVariableWhitespace(first_line, 20);
  second_line = trimStringEndToTwentyChars(second_line);
  second_line = fillStringEndWithVariableWhitespace(second_line, 20);
  third_line = trimStringEndToTwentyChars(third_line);
  third_line = fillStringEndWithVariableWhitespace(third_line, 20);
  fourth_line = trimStringEndToTwentyChars(fourth_line);
  fourth_line = fillStringEndWithVariableWhitespace(fourth_line, 20);
  String lcd_message;
  lcd_message += first_line;
  lcd_message += third_line;
  lcd_message += second_line;
  lcd_message += fourth_line;
  lcd.print(lcd_message);
}

// Prints the passed messages to the  first three lines on the LCD, using the fourth line as a place for PAUSE and RAM information
// first 20 characters of the first passed String will be displayed on the first line of the LCD
// first 20 characters of the second passed String will be displayed on the second line of the LCD
// first 20 characters of the third passed String will be displayed on the third line of the LCD
// The fourth line on the LCD will display PAUSE and RAM information
void lcd3LinePrint(String first_line, String second_line, String third_line)
{
  // each line is trimmed to 20 characters, and/or filled with spaces to make 20 characters if short
  first_line = trimStringEndToTwentyChars(first_line);
  first_line = fillStringEndWithVariableWhitespace(first_line, 20);
  second_line = trimStringEndToTwentyChars(second_line);
  second_line = fillStringEndWithVariableWhitespace(second_line, 20);
  third_line = trimStringEndToTwentyChars(third_line);
  third_line = fillStringEndWithVariableWhitespace(third_line, 20);
  String lcd_message;
  lcd_message += first_line;
  lcd_message += third_line;
  lcd_message += second_line;
  lcd.print(lcd_message);
  ram = String(getFreeRam(), DEC);
  lcd.setCursor(0,3);
  if (paused)
  {
    lcd.print("PAUSED");
  }
  else
  {
    lcd.print("      ");
  }
  lcd.setCursor(12,3);
  lcd.print("RAM     ");
  lcd.setCursor(16,3);
  lcd.print(ram);
  lcd.setCursor(0,0);
}

void lcdDebugPrint(int column, int row, int data)
{
  String debug_tube_data = "";
  debug_tube_data += data;
  debug_tube_data = fillStringEndWithVariableWhitespace(debug_tube_data, 5);
  lcd.setCursor(column, row);
  lcd.print(debug_tube_data);
}

// Returns up to the first 20 characters of the passed String
String trimStringEndToTwentyChars(String msg)
{
  if (msg.length() > 20)
  {
    msg.remove(20);
  }
  return msg;
}

// Returns a 20 character String, the first part containing the passed String, with the remainder filled with spaces
String fillStringEndWithVariableWhitespace(String msg, int desired_length)
{
  int string_length = msg.length();
  if (string_length < desired_length)
  {
    for (int i = 0; i < desired_length - string_length; i++)
    {
      msg += " ";
    }
  }
  return msg;
}

// Determines if SD Card access is possible, and if so sets the system to do so and figures out which file to write to on the SD Card
// Filenames conform to the following formatting:
// 1M30D.csv   - this file would be the first file written to the card on January 30th
// 1M30D1.csv  - this file would be the second file written to the card on January 30th
// 1M30D99.csv - this file would be the last acceptable file written to the card on January 30th since filenames must conform to 8.3 format
// 12M9D7.csv  - this file would be the eighth file written to the card on December 9th
void initializeSDCardFile()
{
  // Determine if the SD Card is physically present and non write-protected
  write_protected_or_missing = digitalRead(SD_Card_write_protected_or_missing);
  card_not_physically_detected = digitalRead(SD_Card_not_physically_detected);

  // If the SD Card is not absent and not write-protected then begin communication with it
  if (!write_protected_or_missing && !card_not_physically_detected)
  {
    Serial.print("Initializing SD card...");
    lcdPrint("Initializing","SD card...","","");
    if (!SD.begin(10,11,12,13)) // start communicating with the SD Card (Put these "magic numbers" up top where they belong!)
    {
      Serial.println("SD Card initialization failed!");
      lcdPrint("initialization","failed!","","");
    }
    else
    {
      Serial.println("initialization done.");
      lcdPrint("initialization done.","","","");

      DateTime now = rtc.now(); // Get the current Date/Time as perceived by the real-time-clock

      // Convert the Date/Time into a filename
      String time_date_stamp;
      time_date_stamp += now.month();
      time_date_stamp += "M";
      time_date_stamp += now.day();
      time_date_stamp += "D";

      String time_date_temp_check = time_date_stamp;
      time_date_temp_check += ".csv"; // add the file-extension
      char temp_filename_check[13];
      time_date_temp_check.toCharArray(temp_filename_check, 13); // convert the filename to an 8.3 character array
      Serial.print("Checking if ");
      Serial.print(temp_filename_check);
      Serial.println(" exists already.");
      time_date_temp_check += "        ";
      lcdPrint("Checking if",time_date_temp_check,"exists already.","");

      if (SD.exists(temp_filename_check)) // check to see if the filename is present already on the SD Card
      {
        Serial.println("Filename Found!");
        lcdPrint("Filename Found!","","","");
        for (int i = 1; i < 100; i++) // assumption that there will be fewer than 100 new files created in a day (just one probably, maybe a couple (of course more during testing!))
        {
          time_date_temp_check = time_date_stamp;
          time_date_temp_check += i;
          time_date_temp_check += ".csv";
          time_date_temp_check.toCharArray(temp_filename_check, 13);
          Serial.print("Checking if ");
          Serial.print(temp_filename_check);
          Serial.println(" exists already.");
          lcdPrint("Checking if",time_date_temp_check,"exists already.","");
          if (!SD.exists(temp_filename_check))
          {
            break;
          }
        }
        // Should make a FAILURE state here for when every option is already on the card, something that tells the system not to write to the SD card, or maybe starts using lowercase letters???
        Serial.print("Going with ");
        Serial.println(temp_filename_check);
        lcdPrint("Going with",time_date_temp_check,"","");
        strcpy(filename, temp_filename_check);
      }
      else // the first file of the day isn't there yet, so make this it
      {
        time_date_stamp += ".csv";
        time_date_stamp.toCharArray(filename, 13);
      }
      write_to_SD_card = true; // set the system to write to this file in the future
    }
  }
  else
  {
    Serial.println("SD Card write-protected or missing, skipping write to SD Card");
  }
}

// Sets the background color of the LCD to the RGB values passed in the integer array (in that order)
void set_LCD_Color(int colors[])
{
  // writes the RGB values as PWM settings to the relevant pins
  analogWrite(LCD_Red_PWM, colors[0]);
  analogWrite(LCD_Green_PWM, colors[1]);
  analogWrite(LCD_Blue_PWM, colors[2]);
}

// When the pause button interrupt is triggered, this service routine sets the pause flag
void pause_button_interrupt_service_routine()
{
  pause_button_flag = 1;    // set the pause button indicator so it can be checked by the system at an appropriate moment
}

// If the pause flag is set, pauses the system if it was unpaused, and unpauses the system if it was paused
void checkPauseButton()
{
  if (pause_button_flag == 1) // if the pause flag is set
  {
    if (paused) // if the system was paused
    {
      set_LCD_Color(normal_backlight); // return the LCD backlight to normal
      digitalWrite(35, LOW); // turn off the PAUSE button LED
      lcd.setCursor(0,3);
      lcd.print("          "); // remove the PAUSED message from the LCD
      lcd.setCursor(0,0);
      paused = false; // set the system to unpaused
    }
    else // if the system was unpaused
    {
      set_LCD_Color(paused_backlight); // set the LCD backlight to paused
      digitalWrite(35, HIGH); // turn on the PAUSE button LED
      lcd.setCursor(0,3);
      lcd.print("PAUSED    "); // display the PAUSED message on the LCD
      lcd.setCursor(0,0);
      paused = true; // set the system to PAUSED
    }
  delay(100); // Some extra debouncing (pause button is REALLY susceptible to this, even with the debounce chip.
              // Sometimes an uncaught button bounce would cause the flag to set after the pause, resulting in the sampling loop to begin paused.
              // This would cause problems for the user if they walked away before the system started shaking, it could be paused without them realizing it.
  pause_button_flag = 0; // unset the pause flag
  }
}

// Halts system activity until the pause button is pressed, if the system is in a paused state
void system_pause()
{
  while (paused) // while the system is paused
  {
    checkPauseButton(); // keep checking if the pause button has been pressed
    delay(100); // not sure what I was trying to avoid here with this delay...
  }
}

// Disables pause button and interval knob so that nothing can disturb the delicate tasks performed while the system is in this mode
// Also sets "critical-section" backlight color on LCD
void set_critical_section_mode()
{
  //detachInterrupt(1); // disable the pause button from interrupting
  detachInterrupt(5); // disable the 4 channel rotary switch from interrupting
  set_LCD_Color(cannot_pause_backlight);
}

// Re-enables pause button and interval knob, also restores normal mode backlight color
void resume_normal_mode()         // returns the system from critical mode, re-enabling the pause button and interval knob
{
  //attachInterrupt(1, pause_button_interrupt_service_routine, RISING); // re-enable pause button interrupt
  attachInterrupt(5, four_channel_isr, RISING); // re-enable data sample interval rotary switch interrupt
  set_LCD_Color(normal_backlight);
}

// Checks the 4-ch rotary knob and sets the data interval to what is set by the knob, if the four_channel_flag is set
void checkFourChannelFlag()
{
  if (four_channel_flag == 1) // if the flag was set (by the 4-ch interrupt)
  {
    four_channel_flag = 0; // reset the flag
    int voltage = analogRead(A0); // read the voltage from the 4-ch rotary switch voltage ladder
    // Determine the interval set by the voltage read, set the timestep to the correct interval and display the interval (in minutes) on the LCD
    if (voltage < 825)
    {
      timestep = data_sampling_interval_4 * 60000;
      lcd3LinePrint("Interval", String(data_sampling_interval_4), "");
    }
    else if (voltage < 905)
    {
      lcd3LinePrint("Interval", String(data_sampling_interval_3), "");
      timestep = data_sampling_interval_3 * 60000;
    }
    else if (voltage < 984)
    {
      lcd3LinePrint("Interval", String(data_sampling_interval_2), "");
      timestep = data_sampling_interval_2 * 60000;
    }
    else
    {
      lcd3LinePrint("Interval", String(data_sampling_interval_1), "");
      timestep = data_sampling_interval_1 * 60000;
    }
  }
}

// Checks the 8-ch rotary knob and sets the tube box to take continuous readings from in debug mode to that which is set by the knob, if the eight_channel_flag is set
void checkEightChannelFlag()
{
  int voltage = analogRead(A1); // read the voltage from the 8-ch rotary switch voltage ladder
  // Determine the tube box set by the voltage read, set the tube box to be interrogated by the debug loop to the one set by the user
  if (voltage < 906)
  {
    eight_position_channel = 8;
  }
  else if (voltage < 924)
  {
    eight_position_channel = 7;
  }
  else if (voltage < 942)
  {
    eight_position_channel = 6;
  }
  else if (voltage < 960)
  {
    eight_position_channel = 5;
  }
  else if (voltage < 978)
  {
    eight_position_channel = 4;
  }
  else if (voltage < 997)
  {
    eight_position_channel = 3;
  }
  else if (voltage < 1015)
  {
    eight_position_channel = 2;
  }
  else
  {
    eight_position_channel = 1;
  }
}

// Sets the four_channel_flag upon 4-ch switch interrupt interception
void four_channel_isr()
{
  four_channel_flag = 1;
}

// Sets the eight_channel_flag upon 8-ch switch interrupt interception
// DEPRECATED, no longer in use because the interrupt has become faulty.
void eight_channel_isr()
{
  eight_channel_flag = 1;
}
