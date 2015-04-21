
/* S0VZ
 * Web Client
 * armin@langhofer.net
 * last changed: 2015-03-17
 *
 * REQUIRED HARDWARE
 * =================
 * - Arduino Pro Mini (I used 3.3V 8 MHz version (3$), should be fine with 5V as well) http://arduino.cc/en/Main/ArduinoBoardProMini
 * - Cheap Ethernet module with the ENC28J60 chip connected via SPI (I used a mini HanRun (4$))
 *
 * - for programming, debugging & logging (RS232) (4$): FT232RL FTDI 3.3V 5.5V USB to TTL Serial Adapter Module
 *   I used the ones on ebay but this one should work as well: https://www.sparkfun.com/products/9873
 *  
 * REQUIRED SOFTWARE
 * =================
 * - I used: Arduino IDE 1.5.8 Beta (which is compatible with the most current version of Mac OS (Yosemite, v10.10))
 * - Ethercard http://jeelabs.net/pub/docs/ethercard/
 * - TrueRandom https://code.google.com/p/tinkerit/wiki/TrueRandom
 *
 * DESCRIPTION
 * ===========
 * The Sketch first looks up the MAC address stored in EEProm. If not found, it generates the three last octets of a  MAC using 
 * a random generator and stores this in the EEProm. Please note that the bits for local addresses are cleared so that this MAC 
 * is safe to be used in your local netwok. If you have your own OUI address space simply replace the first three octes and you're done.
 * 
 * After MAC generation the ethernet interface is started and requests IP, Netmask, Router-IP from DHCP Server and begins Looping.
 * Within the loop the inputs D2-D9 and A0 - A1 are continuously read every 3 seconds and falling edges are sent to the volskzaehler backend.
 * 
 * The workflow of this program can be debugged via RS232 at 115200baud (when connected to the ftdi chip and usb host)
 *
 * CONFIGURATION
 * =============
 * MAC Address:
 * ------------
 * if you'd like to use the MAC space of e.g. BCT ELECTRONIC '2C-2D-48' then you'd have to setup
 * static byte mymac[] = { 0x2C,0x2D,0x48,0x04,0x05,0x06 };
 * instead of 
 * static byte mymac[] = { 0x06,0x02,0x03,0x04,0x05,0x06 };
 * please note that the last three octes still are randomized at first boot so if you'd like to provide a MAC range this
 * sketch has to be extended.
 * 
 * Volkszaehler Server
 * -------------------
 * This Webclient sends the read values to the host specified at:
 * const char website[] PROGMEM = "vz.example.com";
 * Please feel free to change this for your needs
 *
 * Volkszaehler UUIDs
 * -------------------
 * For temperature, please change
 * ether.browseUrl(PSTR("/middleware.php/data/fd19aa80-56d9-11e3-933b-dbLED_LAN_ROOCESSING96b402cb.json?operation=add&value="), b, website, my_callback);
 * For humidity, please change
 * ether.browseUrl(PSTR("/middleware.php/data/8d360e60-56d0-11e3-9108-07b3ed1dcf49.json?operation=add&value="), b, website, my_callback);
 *
 */

#include <EtherCard.h>
#define CS_PIN 10

#include <EEPROM.h>
#include <TrueRandom.h>
//#include "Timer.h"
//Timer t;


#include <avr/wdt.h>




/*
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
*/




// Universally administered and locally administered addresses are distinguished by setting the second least significant bit of the most significant byte of the address. If the bit is 0, the address is universally administered. If it is 1, the address is locally administered. In the example address 02-00-00-00-00-01 the most significant byte is 02h. The binary is 00000010 and the second least significant bit is 1. Therefore, it is a locally administered address.[3] The bit is 0 in all OUIs.
// so mac should be:
// x2-xx-xx-xx-xx-xx
// x6-xx-xx-xx-xx-xx
// xA-xx-xx-xx-xx-xx
// xE-xx-xx-xx-xx-xx
static byte mymac[] = { 0x06,0x02,0x03,0x04,0x05,0x06 };
char macstr[18];

byte Ethernet::buffer[700];
static uint32_t timer = 0;

const char website[] PROGMEM = "vz.langhofer.net";

int LED_LAN_RDY =2;
int LED_LAN_ROOCESSING =3;


int ResetGPIO = A3;


bool requestPending=false;
int heartbeatreset = 0;

/*
void displaystuff(char* releasedetails, char* ipaddress, char* macaddress)   {                
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  // init done
  
  // Clear the buffer.
  display.clearDisplay();


  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(releasedetails);
  display.println(ipaddress);
  display.setTextSize(2);
  display.println(macaddress);
  display.display();
  delay(2000);
}
*/


static void my_callback (byte status, word off, word len) {
  requestPending=false;
  
  Serial.println(">>>");
  Ethernet::buffer[off+300] = 0;
  Serial.print((const char*) Ethernet::buffer + off);
  Serial.println("...");
  //wdt_reset();
  
  
  
}

uint16_t values[10];
uint8_t debounce[10];

String a,c;
int heartbeat=0;

void clearvalues() {
   values[0] = 0;
   values[1] = 0;
   values[2] = 0;
   values[3] = 0;
   values[4] = 0;
   values[5] = 0;
   values[6] = 0;
   values[7] = 0;
   values[8] = 0;
   values[9] = 0;
}

void cleardebounce() {
   debounce[0] = 0;
   debounce[1] = 0;
   debounce[2] = 0;
   debounce[3] = 0;
   debounce[4] = 0;
   debounce[5] = 0;
   debounce[6] = 0;
   debounce[7] = 0;
   debounce[8] = 0;
   debounce[9] = 0;
}

void(* resetFunc) (void) = 0;


void setup () {
  
  Serial.begin(115200);
  Serial.println(F("\ns0vz v2 starting..."));
  
  
  
  
  //displaystuff("s0vz v2 build 1702", "192.168.2.3", "13:22:23:12:23:42");
 
  
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  

  pinMode(LED_LAN_RDY, OUTPUT);
  pinMode(LED_LAN_ROOCESSING, OUTPUT);
  
  // LED test
  delay(200);
  digitalWrite(LED_LAN_RDY, HIGH);
  delay(200);
  digitalWrite(LED_LAN_RDY, LOW);
  digitalWrite(LED_LAN_ROOCESSING, HIGH);
  delay(200);
  digitalWrite(LED_LAN_ROOCESSING, LOW);
  digitalWrite(LED_LAN_RDY, LOW);
  
  
  
  
  
  
  if (EEPROM.read(1) != '#') {
    Serial.println(F("\nWriting EEProm..."));
  
    EEPROM.write(1, '#');
    
    for (int i = 3; i < 6; i++) {
      EEPROM.write(i, TrueRandom.randomByte());
    }
  }
  
  // read 3 last MAC octets
  for (int i = 3; i < 6; i++) {
      mymac[i] = EEPROM.read(i);
  }
  
Serial.print("MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mymac[i], HEX);
        if (i<5)
          Serial.print(':');
    }
  Serial.println();
  
  Serial.print("Access Ethernet Controller... ");
  if (ether.begin(sizeof Ethernet::buffer, mymac, CS_PIN) == 0) {
    Serial.println(F("FAILED"));
  } else {
    Serial.println("DONE");
  }
  
  
  Serial.print("Requesting IP from DHCP Server... ");
  if (!ether.dhcpSetup()) {
    Serial.println(F("FAILED"));
  } else {
    Serial.println("DONE");
  }
  
  
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  

  if (!ether.dnsLookup(website))
    Serial.println("DNS failed");
    
  ether.printIp("SRV: ", ether.hisip);
  
  clearvalues();
  cleardebounce();
  
  heartbeat=0;
  
  a = "?mac=" + String(mymac[0], HEX) + ":" + String(mymac[1], HEX) + ":" + String(mymac[2], HEX) + 
  ":" + String(mymac[3], HEX) + ":" +String( mymac[4], HEX) + ":" + String(mymac[5], HEX);
  
  digitalWrite(LED_LAN_RDY, HIGH);
  // t.every(1000, takeReading); 

}

bool foo;
void blinkLED() {
  foo=!foo;
  if (foo)
    digitalWrite(LED_LAN_RDY,HIGH);
  else
    digitalWrite(LED_LAN_RDY,LOW);
  
}



void dodebounce(int channel, int pin) {
  int newvalue = digitalRead(pin);
  
  if (debounce[channel] == 1 && newvalue == false)
  {
    // falling edge
    values[channel]++;
  }
  debounce[channel] = newvalue;
}

int MAX_HEARTBEAT=200;

void loop () {
  
  //t.update();
  
  
  word len = ether.packetLoop(ether.packetReceive());
  
  word pos = ether.packetLoop(len);
  if(strstr((char *)Ethernet::buffer + pos, "GET /?LED2=ON") != 0) {
    Serial.println("Received ON for LED2 command");
  }
  if(strstr((char *)Ethernet::buffer + pos, "GET /?LED3=ON") != 0) {
    Serial.println("Received ON for LED3 command");
  }
 
    
    
  dodebounce(0,4);
  dodebounce(1,5);
  dodebounce(2,6);
  dodebounce(3,7);
  dodebounce(4,8);
  dodebounce(5,9);
  dodebounce(6,A0);
  dodebounce(7,A1);
  dodebounce(8,A2);
  dodebounce(9,A3);
   
  if (millis() > timer) {
    heartbeat++;
    heartbeatreset++;
    
    timer = millis() + 100;
    
    blinkLED();
    
    // heartbeat + 10000
    if (requestPending && heartbeatreset > MAX_HEARTBEAT * 10) {
      Serial.println("no http answer within given timeout. resetting ...");
      delay(500); // wait until serial string is written 
      resetFunc();
    }
    
    
    c = a;
    c += "&millis=";
    c += millis();
    int found=0;
    for (int i=0; i<10; i++) {
      if (values[i] > 0) {
        c += "&c";
        c += String(i);
        c += "=";
        c += String(values[i]);
        found++;
      }
    }
    if (found > 0 || heartbeat > MAX_HEARTBEAT ) {
      
      digitalWrite(LED_LAN_ROOCESSING, HIGH);
      
      heartbeat =0;
      
      char b[100];
      c.toCharArray(b, sizeof(b));


      requestPending = true;
      ether.browseUrl(PSTR("/s0.php"), b, website, my_callback);
      Serial.print("/s0.php");
      Serial.print(b);
      
      clearvalues();
      
      
      
      Serial.println();
      digitalWrite(LED_LAN_ROOCESSING, LOW);
    }
  }
}




