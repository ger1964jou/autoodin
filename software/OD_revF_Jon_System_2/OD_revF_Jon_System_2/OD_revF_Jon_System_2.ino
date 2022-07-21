#include <Wire.h>
//#include <SPI.h>
#include <Math.h>
#include <Adafruit_ADS1015.h>
#include <Adafruit_MCP4725.h>

Adafruit_ADS1115 ads1_4(0x48);
Adafruit_ADS1115 ads5_8(0x49);
Adafruit_MCP4725 dac;

// constants
const uint8_t channels = 8;                         // channels in use, up to 16
const uint8_t channelstart = 0;                      // the first channel to probe (normally 0);
const uint8_t addr_bits = 3;                         // number of bits required to encode 
const uint8_t adc_samples = 30;                      // number of repeated adc samples per channel

const uint8_t mux_addr_pins[] = {39, 40, 41};        // mux addr pins
const uint8_t mux_enablenot_pin = 38;                 // mux enable pin is inverted;
const uint8_t relay_pin = 36;                         // when high, shaker is on, when low, shaker is off

const uint32_t channel_select_delay_ms = 20;         // time to allow the mux to settle after channel selection in ms
const uint32_t shaker_shutdown_ms = 20000;           // time in ms to allow shaker table to stop moving
const uint32_t timestep = 23500;                   // time in ms between scans                          150000                                // re set to 1300000 after debugging

// global variables
uint32_t start_time;                      // execution time in ms since end of setup
struct data {                             // data from current scan of channels
    uint32_t time;
    uint16_t adc[64];
} data;

uint16_t calibration_voltage[64];
uint16_t best_calibration_value[64];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
    Serial.begin(115200);
    pinMode(mux_addr_pins[0], OUTPUT);
    pinMode(mux_addr_pins[1], OUTPUT);
    pinMode(mux_addr_pins[2], OUTPUT);    
    
    pinMode(mux_enablenot_pin, OUTPUT);      //Configure mux enable not pin as outp
    digitalWrite(mux_enablenot_pin, LOW);    //Enables the mux
    pinMode(relay_pin, OUTPUT);              //Configure relay control pin as output
    digitalWrite(relay_pin, LOW);            //Open the shaker relay
    
    ads1_4.setGain(GAIN_TWOTHIRDS); // this is also the default value
    ads5_8.setGain(GAIN_TWOTHIRDS); // this is also the default value
    ads1_4.begin();
    ads5_8.begin();
    dac.begin(0x62);
    
    for (int k = 0; k < 64; k++) {
        data.adc[k]=1;
    }
    data.time = 1;
    calibrate();
    Serial.print("Cal Volt");
    for (int i = 0; i < channels - channelstart; i++) {
      Serial.print("\t");
      Serial.print(calibration_voltage[i]);
    }
    Serial.print("\nCal Valu");
    for (int i = 0; i < channels - channelstart; i++) {
      Serial.print("\t");
      Serial.print(best_calibration_value[i]);
    }
    Serial.println("\nsetup: complete");
    start_time = millis();
}

void calibrate() {
    Serial.println("Begining calibration");
    for (int k = 0; k < channels - channelstart; k++) {             // re-set |k < channels| after testing
        Serial.print("\nChannel ");
        Serial.print(k + channelstart + 1);          // +1 added to keep it to the 1-16 range as opposed to the programmical 0-15 range
        Serial.print("\n");
        channel_select(k + channelstart);
        uint16_t low = 0;
        uint16_t mid = 2047;
        uint16_t high = 4095;
        double value = 0.0;
        boolean done = false;
        
        double best_value = 100000.0; // out of bounds value to start so that the first value tested replaces it
        int best_calibration_voltage = 0;
        while(!done) {
          Serial.print("mid: ");
          Serial.println(mid);
            dac.setVoltage(mid, false);
            delay(20);
            value = average(k + channelstart);
            Serial.print("low: ");
            Serial.print(low);
            Serial.print("  mid: ");
            Serial.print(mid);
            Serial.print("  high: ");
            Serial.print(high);
            Serial.print("  value: ");
            Serial.println(value);
            Serial.println();
            if (abs(value - 1600) < abs(best_value - 1600)) {
              best_value = value;
              best_calibration_voltage = mid;
            }
            if (value > 1600) {
                high = mid;
                mid = (low + high) / 2;
            } else {
                low = mid;
                mid = (low + high) / 2;
            }
            if (low == mid || mid == high) {
                done = true;
            }
        }
        Serial.print("Best PWM: ");
        Serial.print(best_calibration_voltage);
        Serial.print("  Best Value: ");
        Serial.println(best_value);
        
        
        data.adc[k + channelstart] = best_calibration_voltage; // was mid
        calibration_voltage[k + channelstart] = best_calibration_voltage; // was mid
        best_calibration_value[k + channelstart] = best_value;
        
    }
}

double average(int8_t channel) {                            // averages the analog input 30 times to return a value
    double a = 0.0;
    double low = 100000.0;
    double high = 0.0;
    double current_read;
    for (int32_t k = 0;  k < 30; k++) {
      if (channel / 8 < 4) {
        current_read = ads1_4.readADC_SingleEnded(channel / 8);
//        Serial.print("Reading from ADC 1_4 on ADC channel ");
//        Serial.println(channel / 8);
      } else {
        current_read = ads5_8.readADC_SingleEnded((channel - 32) / 8);
//        Serial.print("Reading from ADC 5_8 on ADC channel ");
//        Serial.println((channel - 32) / 8);
      }
//      Serial.println(current_read);
      if (current_read > high) {
        high = current_read;
      }
      if (current_read < low) {
        low = current_read;
      }
      a = a + current_read;
    }
    a = a/30;
    Serial.print("avg: ");
    Serial.print(a);
    Serial.print("  high: ");
    Serial.print(high);
    Serial.print("  low: ");
    Serial.println(low);
    return a;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void loop() {
    Serial.println("loop");
    uint32_t scan_time;
    scan_time = millis();                 // record starting time for this adc channel scan
    shaker_stop();                        // shut down the shaker table and wait
    data.time = millis() - start_time;    // record the time of the current set of readings
    Serial.print(data.time);
    uint8_t i;
    for (i = 0; i < channels - channelstart; i++) {      // scan the channels and perform adc readings
        channel_select(i + channelstart);
        dac.setVoltage(calibration_voltage[i], false);
        delay(20);
        data.adc[i + channelstart] = adc_read(i + channelstart, adc_samples);
    }
    Serial.println("");
    delay(500);
    shaker_start();                       // restart the shaker table
    scan_time = millis() - scan_time;     // compute time elapsed to shut down shaker, take readings and output
    printdelayinfo(scan_time);
    //delay(6000);
    Serial.println("here");
    delay(timestep - scan_time);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void shaker_stop(void) {
    //Serial.println("shaker_stop: shutting down shaker");
    digitalWrite(relay_pin, LOW);
    //Serial.println("shaker_stop: waiting for shaker to stop");
    delay(shaker_shutdown_ms);
    //Serial.println("shaker_stop: shaker stop is complete");
}


void shaker_start(void) {
    //Serial.println("shaker_start: starting up shaker");
    digitalWrite(relay_pin, HIGH);
}


void printdelayinfo(uint32_t scan_time) {
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


void channel_select(uint8_t channel) {
    digitalWrite(mux_enablenot_pin, HIGH);
    digitalWrite(mux_addr_pins[0], bitRead(channel, 0));
    digitalWrite(mux_addr_pins[1], bitRead(channel, 1));
    digitalWrite(mux_addr_pins[2], bitRead(channel, 2));
    digitalWrite(mux_enablenot_pin, LOW);
    delay(channel_select_delay_ms);
}


uint16_t adc_read(uint8_t channel, uint8_t samples) {
    double a = 0.0;
    double low = 100000.0;
    double high = 0.0;
    double current_read;
    for (int8_t k = 0; k < samples; k++) {
      if (channel / 8 < 4) {
      current_read = ads1_4.readADC_SingleEnded(channel / 8);
      } else {
        current_read = ads5_8.readADC_SingleEnded((channel - 32) / 8);
      }
      if (current_read > high) {
        high = current_read;
      }
      if (current_read < low) {
        low = current_read;
      }
      a = a + current_read;
    }
    a = a/samples;
    Serial.print("\t");
    Serial.print(a);
    return round(a);
}




















