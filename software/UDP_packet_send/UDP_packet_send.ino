#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   2  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  26
#define ADAFRUIT_CC3000_CS    49
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed but DI

#define WLAN_SSID       "Stahl_Lab_Netgear"        // cannot be longer than 32 characters!        // The two wifi networks are "Stahl_Lab_Linksys_1" and "Stahl_Lab_Netgear"
#define WLAN_PASS       "sulfate2010"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

struct data {                             // data from current scan of channels
    uint32_t time;
    uint16_t adc[64];
} data;

void setup(void)
{
  Serial.begin(115200);
  Serial.println(F("Hello, CC3000!\n")); 

  displayDriverMode();
  Serial.print("Free RAM: ");
  Serial.println(getFreeRam(), DEC);
  
  /* Initialise the module */
  Serial.println(F("\nInitialising the CC3000 ..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    while(1);
  }
  
  uint8_t macAddress[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  if (!cc3000.setMacAddress(macAddress))
  {
    Serial.println(F("Failed trying to update the MAC address"));
    while(1);
  }
  
  uint16_t firmware = checkFirmwareVersion();
  if (firmware < 0x113) {
    Serial.println(F("Wrong firmware version!"));
    for(;;);
  } 
  
  displayMACAddress();
  listSSIDResults();
  
  /* Delete any old connection data on the module */
  Serial.println(F("\nDeleting old connection profiles"));
  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Failed!"));
    while(1);
  }

  /* Attempt to connect to an access point */
  char *ssid = WLAN_SSID;             /* Max 32 chars */
  Serial.print(F("\nAttempting to connect to ")); Serial.println(ssid);
  
  /* NOTE: Secure connections are not available in 'Tiny' mode!
     By default connectToAP will retry indefinitely, however you can pass an
     optional maximum number of retries (greater than zero) as the fourth parameter.
  */
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  /* Display the IP address DNS, Gateway, etc. */  
  while (! displayConnectionDetails()) {
    delay(1000);
  }
  
  //////////////////////////////////////////////////////////////////////////////////////////

  data.time = millis();
  data.adc[0] = 1;
  data.adc[1] = 2;
  data.adc[2] = 3;
  data.adc[3] = 4;
  data.adc[4] = 5;
  data.adc[5] = 6;
  data.adc[6] = 7;
  data.adc[7] = 8;
  data.adc[8] = 9;
  data.adc[9] = 10;
  data.adc[10] = 11;
  data.adc[11] = 12;
  data.adc[12] = 13;
  data.adc[13] = 14;
  data.adc[14] = 15;
  data.adc[15] = 16; 
  data.adc[16] = 17;
  data.adc[17] = 18;
  data.adc[18] = 19;
  data.adc[19] = 20;
  data.adc[20] = 21;
  data.adc[21] = 22;
  data.adc[22] = 23;
  data.adc[23] = 24;
  data.adc[24] = 25;
  data.adc[25] = 26;
  data.adc[26] = 27;
  data.adc[27] = 28;
  data.adc[28] = 29;
  data.adc[29] = 30;
  data.adc[30] = 31;
  data.adc[31] = 32;
  data.adc[32] = 33;
  data.adc[33] = 34;
  data.adc[34] = 35;
  data.adc[35] = 36;
  data.adc[36] = 37;
  data.adc[37] = 38;
  data.adc[38] = 39;
  data.adc[39] = 40;
  data.adc[40] = 41;
  data.adc[41] = 42;
  data.adc[42] = 43;
  data.adc[43] = 44;
  data.adc[44] = 45;
  data.adc[45] = 46;
  data.adc[46] = 47;
  data.adc[47] = 48;
  data.adc[48] = 49;
  data.adc[49] = 50;
  data.adc[50] = 51;
  data.adc[51] = 52;
  data.adc[52] = 53;
  data.adc[53] = 54;
  data.adc[54] = 55;
  data.adc[55] = 56;
  data.adc[56] = 57;
  data.adc[57] = 58;
  data.adc[58] = 59;
  data.adc[59] = 60;
  data.adc[60] = 61;
  data.adc[61] = 62;
  data.adc[62] = 63;
  data.adc[63] = 64;
  Serial.println("Set the data");

  uint32_t ip = cc3000.IP2U32(128, 208, 236, 38);   // 128, 208, 236, 38
  Serial.println("Set the Server IP");
  uint16_t port = 8577;   // 8577
  Serial.println("Set the Server Port");
  Adafruit_CC3000_Client udp = cc3000.connectUDP(ip, port);
  
  Serial.println(F("Waiting for UDP connection"));
  while(!udp.connected()) {}
  Serial.println("Sending UDP Packet");
  if (!udp.write((byte *)&data, sizeof(struct data))){
    Serial.println("UDP SEND FAILED!!!");
  } else {
    Serial.println(F("Connected to UDP"));
  }
  
  //////////////////////////////////////////////////////////////////////////////////////////

  /* You need to make sure to clean up after yourself or the CC3000 can freak out */
  /* the next time you try to connect ... */
  Serial.println(F("\n\nClosing the connection"));
  cc3000.disconnect();
}

void loop(void)
{
  delay(1000);
}

/**************************************************************************/
/*!
    @brief  Displays the driver mode (tiny of normal), and the buffer
            size if tiny mode is not being used

    @note   The buffer size and driver mode are defined in cc3000_common.h
*/
/**************************************************************************/
void displayDriverMode(void)
{
  #ifdef CC3000_TINY_DRIVER
    Serial.println(F("CC3000 is configure in 'Tiny' mode"));
  #else
    Serial.print(F("RX Buffer : "));
    Serial.print(CC3000_RX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
    Serial.print(F("TX Buffer : "));
    Serial.print(CC3000_TX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
  #endif
}

/**************************************************************************/
/*!
    @brief  Tries to read the CC3000's internal firmware patch ID
*/
/**************************************************************************/
uint16_t checkFirmwareVersion(void)
{
  uint8_t major, minor;
  uint16_t version;
  
#ifndef CC3000_TINY_DRIVER  
  if(!cc3000.getFirmwareVersion(&major, &minor))
  {
    Serial.println(F("Unable to retrieve the firmware version!\r\n"));
    version = 0;
  }
  else
  {
    Serial.print(F("Firmware V. : "));
    Serial.print(major); Serial.print(F(".")); Serial.println(minor);
    version = major; version <<= 8; version |= minor;
  }
#endif
  return version;
}

/**************************************************************************/
/*!
    @brief  Tries to read the 6-byte MAC address of the CC3000 module
*/
/**************************************************************************/
void displayMACAddress(void)
{
  uint8_t macAddress[6];
  
  if(!cc3000.getMacAddress(macAddress))
  {
    Serial.println(F("Unable to retrieve MAC Address!\r\n"));
  }
  else
  {
    Serial.print(F("MAC Address : "));
    cc3000.printHex((byte*)&macAddress, 6);
  }
}


/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

/**************************************************************************/
/*!
    @brief  Begins an SSID scan and prints out all the visible networks
*/
/**************************************************************************/

void listSSIDResults(void)
{
  uint32_t index;
  uint8_t valid, rssi, sec;
  char ssidname[33]; 

  if (!cc3000.startSSIDscan(&index)) {
    Serial.println(F("SSID scan failed!"));
    return;
  }

  Serial.print(F("Networks found: ")); Serial.println(index);
  Serial.println(F("================================================"));

  while (index) {
    index--;

    valid = cc3000.getNextSSID(&rssi, &sec, ssidname);
    
    Serial.print(F("SSID Name    : ")); Serial.print(ssidname);
    Serial.println();
    Serial.print(F("RSSI         : "));
    Serial.println(rssi);
    Serial.print(F("Security Mode: "));
    Serial.println(sec);
    Serial.println();
  }
  Serial.println(F("================================================"));

  cc3000.stopSSIDscan();
}
