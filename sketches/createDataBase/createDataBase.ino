#include "Arduino.h"
#include <EDB.h>
#include <SD.h>

 
File dbFile;

PROGMEM int TABLE_SIZE_DOOR=6885;
PROGMEM int TABLE_SIZE_CH=8925;
PROGMEM int TABLE_SIZE_DLE=51000;
PROGMEM int TABLE_SIZE_ACCESS=6000;

struct door{
  int id; 
  char name[20];
  char coding[5]; //Building E, door15 = "E015"
  uint8_t ip; 
}door;  // 27 bytes

struct cardHolder{
  int id;
  char card[9];  
  char name[25];
}cardHolder; //35 bytes

struct DoorLogEvent{
  int access;
  char card[9];
  char name[25]; // Do we need this?
  int hour;
  int minute;
  int day;
  int month;
  int year;
  char doorcoding[5];
}DoorLogEvent;  //51bytes


struct access{
  int id;
  int door_id;
  int cardHolder_id;
}access; //6 bytes


void writer(unsigned long address, byte data)
{
  dbFile.seek(address); 
  dbFile.write(data); 
  dbFile.flush();
}
 
byte reader(unsigned long address)
{
  dbFile.seek(address); 
  char c = dbFile.read(); 
  return c;
}
 
 
EDB db(&writer, &reader);
 
void setup()
{
  Serial.begin(57600);
  Serial.println("Type any character to start");
  while (Serial.read() <= 0) {}
    
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    if (!SD.begin(4)) {
        Serial.println("ERROR - SD card initialization failed!");
        return;    // init failed
    }Serial.println("SUCCESS - SD card initialized.");
    
  
  dbFile = SD.open("dbase1.db", FILE_WRITE);
  db.create(0, TABLE_SIZE_DOOR, sizeof(door));
  dbFile.close();
  
  dbFile = SD.open("dbase2.db", FILE_WRITE);
  db.create(0, TABLE_SIZE_CH, sizeof(cardHolder));
  dbFile.close();
  
  dbFile = SD.open("dbase3.db", FILE_WRITE);
  db.create(0, TABLE_SIZE_DLE, sizeof(DoorLogEvent));  
  dbFile.close();

  dbFile = SD.open("dbase4.db", FILE_WRITE); 
  db.create(0, TABLE_SIZE_ACCESS, sizeof(access));  
  dbFile.close();*/
}
 
void loop(){
  
}
