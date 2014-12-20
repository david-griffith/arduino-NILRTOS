
#include <NilRTOS.h>
#include <Arduino.h>

// These typdefs have to be in their own header due to quirks with the Arduino dev environment.

typedef struct {
   byte UID;          //our UID
   byte XID;// insert a transaction ID here.
   char request[6];  // the request
   long tagID;        // tagID associated with request, 0 if not needed.
} xmitPacket_t;

typedef struct 
{
  byte UID;                // our UID as a sanity check!
  byte XID;
  char request[6];        // what we were requesting - 5 characters max
  char clientID[9];    // Client ID (eg barricade name) - 8 characters max
  char groupID[9];    // Group Id (eg level site is on) - 8 characters max
  int success;        // did it work?
  int currentTagCount;    // tag count - always returned if sucessful
  char returnMessage[16];  // return data - my name / my groupid if successful, error message on failure.
} recvPacket_t;
