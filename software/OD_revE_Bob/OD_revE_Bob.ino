/*
** OD monitor, revision E
** Paired with OD_mark3 hardware.
** 16-channel LED/phototransistor optical density measurement device
** functional modes:
** ** serial (dead for unknown power reasons)
** ** ethernet (enabled by setting ethernetstatus to true)
*/

#include <SPI.h>
#include <Ethernet.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// constants
const boolean ethernetstatus = true;

const uint8_t channels = 16;                         // channels in use, up to 16
const uint8_t channelstart = 0;                      // the first channel to probe (normaly 0);
const uint8_t addr_bits = 4;                         // number of bits required to encode 
const uint8_t adc_samples = 30;                      // number of repeated adc samples per channel

const uint8_t pwm = 3;                               // pwm controll pin
const uint8_t adc_pin = 0;                           // adc channel (not the mux channel)
const uint8_t mux_addr_pins[] = {4, 5, 6, 7};        // mux addr pins
const uint8_t mux_enablenot_pin = 8;                 // mux enable pin is inverted;
const uint8_t relay_pin = 9;                         // when high, shaker is on, when low, shaker is off

const uint32_t channel_select_delay_ms = 20;         // time to allow the mux to settle after channel selection in ms
const uint32_t shaker_shutdown_ms = 20000;           // time in ms to allow shaker table to stop moving
const uint32_t timestep = 1300000;                   // time in ms between scans

// global variables
uint32_t start_time;                      // execution time in ms since end of setup
struct data {                             // data from current scan of channels
    uint32_t time;
    uint16_t adc[16];
} data;

uint16_t pwmlevel[16]; 

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  // MAC address for device (must be unique!)
IPAddress data_server_ip(128, 208, 236, 38);                // IP of remote host that will receive the data
const uint16_t data_server_port = 8577;                     // must be unique!
EthernetUDP udp;                                            // UDP instance for sending & receiving
const int PACKET_SIZE = sizeof(struct data);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void setup() {
    Serial.begin(9600);
    //Serial.println("setup: begins");
    uint8_t i;
    for (i = 0; i < 8; ++i) {
        pinMode(i , OUTPUT);                 //Configure all channel control pins as output
        digitalWrite(i, LOW);                //set all pins low
    }
    pinMode(mux_enablenot_pin, OUTPUT);      //Configure mux enable not pin as outp
    digitalWrite(mux_enablenot_pin, LOW);    //Enables the mux
    pinMode(relay_pin, OUTPUT);              //Configure relay control pin as output
    digitalWrite(relay_pin, LOW);            //Open the shaker relay
    if(ethernetstatus){
        Serial.println("setup: configuring ethernet via DHCP");
        if (Ethernet.begin(mac) == 0) {
            Serial.println("setup: unable to configure ethernet device, halting");
            while (1) { delay(1); }
        }
        Serial.print("setup: ethernet configuration complete, IP address is: ");
        Serial.println(Ethernet.localIP());
        udp.begin(data_server_port);
    }    
    for (int k = 0; k < 16; k = k + 1) {
        data.adc[k]=1;
    }
    data.time = 1;
    calibrate();
    Serial.println("setup: complete");
    start_time = millis();
}

void calibrate() {
    Serial.println("Begining calibration");
    for (int k = 0; k < channels; k = k + 1) {
        Serial.print("\nChannel ");
        Serial.print(k + channelstart);
        Serial.print("\n");
        channel_select(k + channelstart);
        uint16_t low = 0;
        uint16_t mid = 127;
        uint16_t high = 255;
        double value = 0.0;
        boolean done = false;
        while(!done) {
            analogWrite(pwm, mid);
            value = average(adc_pin);
            Serial.println(low);
            Serial.println(mid);
            Serial.println(high);
            Serial.println(value);
            if (value > 100) {
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
        data.adc[k + channelstart] = mid;
        pwmlevel[k + channelstart] = mid;
    }
    sendpacket(ethernetstatus);
}

double average(int8_t pin) {                            // averages the analog input 30 times to return a value
    double a = 0.0;
    delay(6000);
    for (int32_t k = 0;  k < 30; k = k + 1) {
        a = a + analogRead(pin);
    }
    a = a/30;
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
    for (i = 0; i < channels; ++i) {      // scan the channels and perform adc readings
        channel_select(i + channelstart);
        data.adc[i + channelstart] = adc_read(adc_pin, adc_samples);
    }
    Serial.println("");
    sendpacket(ethernetstatus);
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


void sendpacket(boolean on){
    if(on){
      // the data structure was filled during the scan, now create a udp packet and send it
      udp.beginPacket(data_server_ip, data_server_port);
      udp.write((byte *)&data, PACKET_SIZE);
      udp.endPacket();
    }
}


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
    //Serial.print("channel select: selecting channel ");
    //Serial.println(channel);
    digitalWrite(mux_enablenot_pin, HIGH);
    uint8_t i;
    //Serial.print("\t");
    for (i = 0; i < addr_bits; ++i) {
        digitalWrite(mux_addr_pins[i], bitRead(channel, i));
        //Serial.print(bitRead(channel, i));
        //delay(10);  //not sure if delay is needed between changing controll pins. 
    }
    digitalWrite(mux_enablenot_pin, LOW);
    delay(channel_select_delay_ms);
    analogWrite(pwm, pwmlevel[channel]);
    delay(6000);
}


uint16_t adc_read(uint8_t pin, uint8_t samples) {
    uint16_t a = 0;
    //Serial.print(samples);
    //Serial.println(pin);  
    for (int8_t k = 0; k < samples; k = k + 1) {
        a = a + analogRead(pin);
    }
    a = a/samples;
    Serial.print("\t");
    Serial.print(a);
    return a;
}

