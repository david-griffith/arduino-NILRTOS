
/* Swipe Tag for Minegem - Client
Basic functionality:
Init and get setup info from server
Monitor UART 1, On "In" swipe, report to server.
Monitor UART 2, On "Out" swipe, report to server.
Ping server occasionally to get updated.
*/

// Typedefs live in here due to a slightly annoying arduino compiler issue.
#include "MyTypes.h"

#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
//#include <PN532_HSU.h>
//#include <PN532.h>

// Use tiny unbuffered NilRTOS NilSerial library.
#include <NilSerial.h>


// Macro to redefine Serial as NilSerial.
// Remove definition to use standard Arduino Serial.
#define Serial NilSerial
//PN532_HSU pn532hsu(Serial);
//PN532 nfc(pn532hsu);


// LCD Setup.
// Init LCD 
LiquidCrystal lcd(8,9,4,5,6,7);
// Backlight Pin
const uint8_t LCD_BACKLIGHT_PIN = 3;
// Backlight level
uint8_t backlight = 128;
// LCD lines on display.
char LCDLine1[] = "                ";
char LCDLine2[] = "                ";
// LCD 'back buffer'
char line1[17] = "";
char line2[17] = "";  

// Network and ethernet
// Our UID for talking to the server.
// Later on we'll retrieve this from EEPROM?
const uint8_t myUID = 217;

// our Ethernet MAC/IP. Use the UID as the last digit.
const uint8_t mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, myUID }; 
const uint8_t ip[] = { 10, 20, 3, myUID};

// Tag count
int tagCount = 0;
// A few test tags for testing
const long mylist[] = {10402,35465,19297,40908,60609};

// Valid connection to server
volatile boolean connectionValid = false;

// Barricade control relay
const uint8_t BARRICADE_PIN = 4;

// Semaphore for comms lockout.
SEMAPHORE_DECL(semComms,1);



// Function to communicate with server.
recvPacket_t serverComms(xmitPacket_t myPacket){
  // Takes the xmitpacket struct
  // Returns a struct with data you can use to see if we successfully completed the task.

  // Modify default IP address with our UID
  //ip[3] = myUID;
  // Ethernet UDP
  EthernetUDP udpClient;
  recvPacket_t myRecvPacket;
  byte server[] = { 10, 20, 3, 204}; 
  // Init Ethernet.
  Ethernet.begin((uint8_t*)mac,ip);
  udpClient.begin(4000);
  char packetBuf[60];
  // Send a randomised transaction ID to server so it can tell if we ask again.
  myPacket.XID = random(256);
  // 3 attempts to communicate.
  byte transaction = 3; 
  while (transaction) {
    // send a request packet to the server.
    // Server should reply with current tag count.
    udpClient.beginPacket(server,4000);
    udpClient.write( (byte *) &myPacket, sizeof(myPacket));
    udpClient.endPacket();
    // now listen for a response for 100 milliseconds.
    nilThdSleepMilliseconds(100);  
    byte packetSize = udpClient.parsePacket();
    if(packetSize > 0)
      {
        // We got something back!
        // Update stuff.
        if (sizeof(myRecvPacket)==packetSize) {
          udpClient.read( (char *) &myRecvPacket, sizeof(myRecvPacket));          
          transaction = 0;    
          }
      }
     else
     {
       // Serial.println("Timeout");
       // No response? Set the receive packet accordingly.
       myRecvPacket.UID = myUID;
       strcpy(myRecvPacket.request,myPacket.request);
       strcpy(myRecvPacket.clientID,"");
       strcpy(myRecvPacket.groupID,"");
       myRecvPacket.success = false;
       myRecvPacket.currentTagCount = -1;     
       strcpy(myRecvPacket.returnMessage,"SERVER TIMEOUT");
       transaction--;        
     }
  }
return myRecvPacket;
};

void updateLCD(recvPacket_t myRecvPacket) {
// Update the LCD
    if (myRecvPacket.success==1) {
        sprintf(LCDLine1,"%s %s",myRecvPacket.groupID,myRecvPacket.clientID);
        sprintf(LCDLine2,"Tags:%d",myRecvPacket.currentTagCount);
       }
    else {
        sprintf(LCDLine1,"%s",myRecvPacket.returnMessage);
        sprintf(LCDLine2,"%s","CONTACT OPERATOR");
     }
    
  }



// Thread 1, Server Ping thread.
// Declare a stack with 256 bytes beyond context switch and interrupt needs. 
NIL_WORKING_AREA(waThread1, 256);
// Declare thread function for thread 1.
NIL_THREAD(Thread1, arg) {
  // Server Ping Thread. Talks to the server every second, gets a current count of tags.
  // Due to blocking in the ethernet library, we need to use UDP so we don't block if the 
  // server isn't there.
  // xmit data 
xmitPacket_t myXmitPacket;
// recv data 
recvPacket_t myRecvPacket;

  while(TRUE) {
    myXmitPacket.UID = myUID;
    strcpy(myXmitPacket.request,"PING");
    myXmitPacket.tagID = 1234; 
  // One at a time talking to the server.
    nilSemWait(&semComms);
    myRecvPacket = serverComms(myXmitPacket);  
    updateLCD(myRecvPacket);
    nilSemSignal(&semComms);
    // Update the current tag count regardless.
    tagCount = myRecvPacket.currentTagCount;
    nilThdSleepMilliseconds(5000);
  }

}


// Thread 2, Swipe in Monitor
// Declare a stack with 300 bytes beyond context switch and interrupt needs.
NIL_WORKING_AREA(waThread2, 300);
// Declare thread function for thread 2.
NIL_THREAD(Thread2, arg) {
int myval;
// xmit data 
xmitPacket_t myXmitPacket;
// recv data 
recvPacket_t myRecvPacket;
     
  while (TRUE) {
    // just for now, check analog0 (up button)
    myval = analogRead(0);
    if (myval > 140 && myval < 145) {
      // Call this a swipe in for now, send off to server.
      myXmitPacket.UID = myUID;
      myXmitPacket.tagID = mylist[random(5)];
      strcpy(myXmitPacket.request,"IN");
      nilSemWait(&semComms);
      myRecvPacket = serverComms(myXmitPacket); 
      updateLCD(myRecvPacket);
      nilSemSignal(&semComms);
      // Update the current tag count regardless.
      tagCount = myRecvPacket.currentTagCount;
    }
    nilThdSleepMilliseconds(1000);  
  }
}

// Thread 3, Swipe out monitor
// Declare a stack with 300 bytes beyond context switch and interrupt needs.
NIL_WORKING_AREA(waThread3, 300);
// Declare thread function for thread 3.
NIL_THREAD(Thread3, arg) {
  int myval;
  // xmit data 
xmitPacket_t myXmitPacket;
// recv data 
recvPacket_t myRecvPacket;

  while (TRUE) {
    // just for now, check analog0 (down button)
    // Install excellent tag reader code here later.
    myval = analogRead(0);
    if (myval < 330 && myval > 300) {
      // Call this a swipe in for now, send off to server.
      myXmitPacket.UID = myUID;
      myXmitPacket.tagID = mylist[random(5)];
      strcpy(myXmitPacket.request,"OUT");
      nilSemWait(&semComms);
      myRecvPacket = serverComms(myXmitPacket); 
      updateLCD(myRecvPacket);
      nilSemSignal(&semComms);
      // Update the current tag count regardless.
      tagCount = myRecvPacket.currentTagCount;
   }
    nilThdSleepMilliseconds(1000);  

  }
}


//------------------------------------------------------------------------------
/*
 * Threads static table, one entry per thread.  A thread's priority is
 * determined by its position in the table with highest priority first.
 * 
 * These threads start with a null argument.  A thread's name may also
 * be null to save RAM since the name is currently not used.
 */
NIL_THREADS_TABLE_BEGIN()
NIL_THREADS_TABLE_ENTRY("thread1", Thread1, NULL, waThread1, sizeof(waThread1))
NIL_THREADS_TABLE_ENTRY("thread2", Thread2, NULL, waThread2, sizeof(waThread2))
NIL_THREADS_TABLE_ENTRY("thread3", Thread3, NULL, waThread3, sizeof(waThread3))
NIL_THREADS_TABLE_END()


void setup()
{
  // Misc Setup Junk.
  // delay 250ms on boot to allow the ethernet chip to get it's act together
  delay(250);
  pinMode(BARRICADE_PIN,OUTPUT);
  pinMode(LCD_BACKLIGHT_PIN,OUTPUT);
  pinMode(A0, INPUT);
  lcd.begin(16, 2);
  digitalWrite(LCD_BACKLIGHT_PIN,HIGH);
  Serial.begin(9600);
  // Start Nil RTOS.  
  nilSysBegin();
  
}

void loop()
{
    if (LCDLine1 != line1) {
      // something's changed on line 1, unpdate it.
      // pad to 16 characters to avoid leftover junk on screen.
      sprintf(LCDLine1,"%-16s",LCDLine1);
      strcpy(line1,LCDLine1);
      lcd.setCursor(0, 0);
      lcd.print(LCDLine1);
      //Serial.println(LCDLine1);
      }
    if (LCDLine2 != line2) {
      // something's changed on line 2, unpdate it.
      sprintf(LCDLine2,"%-16s",LCDLine2);
      strcpy(line2,LCDLine2);
      lcd.setCursor(0, 1);
      lcd.print(LCDLine2);
      //Serial.println(LCDLine2);
      }
     // check tag count. Anything other than zero means lock out the barricade.
     if (tagCount != 0) {
       digitalWrite(BARRICADE_PIN,LOW);
     }
     else {
       digitalWrite(BARRICADE_PIN,HIGH);
     }

    if (tagCount > -1) {     
                   digitalWrite(LCD_BACKLIGHT_PIN,HIGH);
      }
    else {
      // blink the backlight REALLY ANNOYINGLY
     analogWrite(LCD_BACKLIGHT_PIN, backlight);
     if (backlight == 8) {backlight = 240;} else {backlight=8;}
      
    }
  // Try not to smash the stack. It makes things a little unpredicatable....
  //nilPrintUnusedStack(&Serial);
  delay(250);
}

