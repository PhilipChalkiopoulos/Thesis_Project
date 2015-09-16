#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Wire.h>
#include <DS1307.h>
#include <SPI.h>
#include <Ethernet.h>
#include <IPaddress.h>
#include <EDB.h>
#include <SdFat.h>
#include <SdFatUtil.h>


const uint8_t SD_CHIP_SELECT = 4;
SdFat sd;
File webFile;

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   50

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 101); // IP address of Ethernet/Server

// IP address of every IP Reader/Wifi Device
uint8_t first_octet=192;
uint8_t second_octet=168;
uint8_t third_octet=1;
uint8_t fourth_octet=100;
IPAddress doorIP(first_octet, second_octet, third_octet, fourth_octet); 

EthernetServer server(80);  // create a server at port 80 (HTTP)
EthernetServer doorserver(3333); //Server to listen every IP Reader/Wifi devices
int wificonnect=0;
int device_count=0;
int delete_user=0;

EthernetClient wifiClient;  //Use Ethernet Shield as client to sent data to every door/Wifi devices

char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request. 
                                //First line of every HTTP request. Stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer

char buff2[6]={0};  // buffer to store the Content Length of every POST message
char postcredentials[100]={0}; // buffer to store data of the POST message
char contLbuffer[50]={0};   //buffer to check if content length exists in line
int count=0;
int len_index=0;
int d=0; //content length

int i;

byte clientBuf[128]; // buffer for the data to send from the Server. 
int clientCount = 0;

char tag[14]; /*Array to store data that comes from door/Wifi devices
data format will be like:"ABABABAB1001" and they been used for log Event
first 8 bytes the card code, 3 next is the number of IP, last is for access*/

char tag_to_send[11]={0};/* Tag to send to every door/Wifi devices.
Format will be like: "aABABABAB" to add a card or "bABABABAB" to delete a card*/

int cnt;
int gif;
char noname[]={"No_user_match"};

//Max Size of databases
PROGMEM int TABLE_SIZE_DOOR=6885;
PROGMEM int TABLE_SIZE_CH=8925;
PROGMEM int TABLE_SIZE_DLE=51000;
PROGMEM int TABLE_SIZE_ACCESS=6000;

//Struct to use to store the information of every doorWifi device
struct door{
  int id;          //Unique ID of every door record
  char name[20];  //"CEO_office"
  char coding[5]; //Building E, door15 = "E015"
  uint8_t ip;     //last octec of the IP Address of the device
}door;  //27 bytes 

struct cardHolder{
  int id;          //Unique ID of every cardHolder record
  char card[9];    //Here we store the unique card number
  char name[25];  //Name of card holder. "Philip_Chalkiopoulos"
}cardHolder; //35 bytes

struct DoorLogEvent{
  int access; //
  char card[9];   //Here we store the unique card number
  char name[25];  //Name of card holder. "Philip_Chalkiopoulos"
  int hour;       //Time & Date are stored here
  int minute;
  int day;
  int month;
  int year;
  char doorcoding[5]; //Building E, door15 = "E015"
}DoorLogEvent;  //51bytes

// Struct to store the joined card-door. Here we store the access 
//of every card to every IP Reader/door. Every one access uses a unique record
struct access{
  int id;
  int door_id;  //The unique ID of every door
  int cardHolder_id;  //The unique ID of every cardHolder
}access; //6 bytes

//This function is required from the Database 
//constructor to write data to the SD card.
void writer(unsigned long address, byte data)
{
  webFile.seek(address); 
  webFile.write(data); 
  webFile.flush();
}

//This function is required from the Database 
//constructor to read data from the SD card.
byte reader(unsigned long address)
{
  webFile.seek(address); 
  return webFile.read();
}
 
// The database instanse.
EDB db(&writer, &reader);
 


void setup(){
  
  //enable SPI Interface on MEGA
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);
  // disable Ethernet chip in order to enable the SD card
  pinMode(10, OUTPUT); 
  digitalWrite(10, HIGH); 
    
  MiniSerial.begin(57600);       // for debugging
  
  //start of sd module
  if (!sd.begin(SD_CHIP_SELECT, SPI_HALF_SPEED)) sd.initErrorHalt();
  MiniSerial.println("SUCCESS - SD card initialized.");
    
  Ethernet.begin(mac, ip);  // initialize Ethernet device
  server.begin();           // start to listen for clients on port:80 (HTTP Requests)
  doorserver.begin();       // start to listen for clients on port:3333 (door/Wifi Requests)
    
}


void loop(){
  EthernetClient client = server.available();  // try to get HTTP client
  EthernetClient doorclient = doorserver.available(); //try to get client from IP Reader

  //if a deletation of a user has occured, access here
  //here we delete joined records from access
  //we are also send to every IP Reader delete message of user
  if(wificonnect==1&&delete_user>0){
    webFile.open("dbase4.db",O_RDWR);
    db.open(0,TABLE_SIZE_ACCESS,sizeof(access));
    int dbcount=db.count();
    MiniSerial.print("AccessdbCount: ");
    MiniSerial.println(dbcount);
    for (int recno = 1; recno <=dbcount; recno++){
        db.readRec(recno, EDB_REC access);
        int del=0;
        if(access.cardHolder_id==delete_user){
           del=recno;
        }
        if(del>0){    //delete record from access table
            recno--;
            db.deleteRec(del);
	    webFile.close();
            delay(10);
            
            //use the door_id of the record that was deleted,
            //and send the message to the correct IP Reader
            //get IP address from "door" table
            webFile.open("dbase1.db");
            db.open(0,TABLE_SIZE_DOOR,sizeof(door));
            dbcount=db.count();
            MiniSerial.print("dbCount: ");
            MiniSerial.println(dbcount);
            for (int recno = 1; recno <=dbcount; recno++){
              db.readRec(recno, EDB_REC door);
              if(door.id==access.door_id){
                fourth_octet=door.ip;
                recno=dbcount+1;
              }
            }
            webFile.close();
            MiniSerial.println("Entering the Wifi communication");
            MiniSerial.println(wifiClient.status());
            delay(10);
            int j=wifiClient.connect(doorIP,1000);
            MiniSerial.print("Connection Number: ");
            MiniSerial.println(j);
            if(j==1){
              MiniSerial.println("doorIP Connected...");
              delay(100);
              wifiClient.println(tag_to_send);
            }else{
              MiniSerial.println("doorIP DID NOT Connected...");
            }
            delay(100);
            
            //read response message from IP Reader and print to Serial
            while(wifiClient.available()) {
              char c = wifiClient.read();
              MiniSerial.print(c);
            }
            wifiClient.stop();
            webFile.open("dbase4.db",O_RDWR);
            db.open(0,TABLE_SIZE_ACCESS,sizeof(access));
            dbcount=db.count();
          }
        }
	delete_user=0;
        wificonnect=0;
        webFile.close();
      }
      //if an addition or a join of a user to a door occurs
      //get here and sent to IP Reader
      else if(wificonnect==1){
        int recno=0;
        //the last "device_count records" of access
	for(int i=device_count;i>0;i--){ 
          webFile.open("dbase4.db");
          db.open(0,TABLE_SIZE_ACCESS,sizeof(access));
          int dbcount=db.count();
          db.readRec(dbcount-recno, EDB_REC access);
          recno++;
          webFile.close();
          delay(10);
          
          webFile.open("dbase1.db");
          db.open(0,TABLE_SIZE_DOOR,sizeof(door));
          dbcount=db.count();
          MiniSerial.print("dbCount: ");
          MiniSerial.println(dbcount);
          for (int recno = 1; recno <=dbcount; recno++){
            db.readRec(recno, EDB_REC door);
            if(door.id==access.door_id){
              fourth_octet=door.ip;
              recno=dbcount+1;
            }
          }
          webFile.close();
          MiniSerial.println("doorIP");
          MiniSerial.println(door.ip);
          
          MiniSerial.println("Entering the Wifi communication");
          MiniSerial.println(wifiClient.status());
          delay(10);
          int j=wifiClient.connect(doorIP,1000);
          MiniSerial.print("Connection Number: ");
          MiniSerial.println(j);
          if(j==1){
            MiniSerial.println("doorIP Connected...");
            delay(100);
            wifiClient.println(tag_to_send);
          }else{
            MiniSerial.println("doorIP DID NOT Connected...");
          }
          delay(100);
          
          while(wifiClient.available()) {
            char c = wifiClient.read();
            MiniSerial.print(c);
          }
          wifiClient.stop();
        }
        wificonnect=0;
      }

 //if an IP Reader send log message a tcp message with port 3333 arives
 //here we store log message into DoorLogEvent table
cnt=0;
if(doorclient) {
  MiniSerial.print("The bytes from IP Reader are: ");
  while (doorclient.connected()) {
    if (doorclient.available()) {
      tag[cnt] = doorclient.read();
      MiniSerial.print(tag[cnt]);
      cnt++;
    }
  }
  // Here we have to enter the LOG information.
  // appendRec on dbfile3 with struct log.
  MiniSerial.println();
  
  for(i=0;i<8;i++){
    DoorLogEvent.card[i]=tag[i];   //Set DoorLogEvent CARD
  }
  MiniSerial.println(DoorLogEvent.card);
  webFile.open("dbase2.db");
  db.open(0,TABLE_SIZE_CH,sizeof(cardHolder));
  int dbcount=db.count();
  MiniSerial.print("dbCount: ");
  MiniSerial.println(dbcount);
  int n=1;
  for (int recno = 1; recno <=dbcount; recno++){
    db.readRec(recno, EDB_REC cardHolder);
    n=memcmp(cardHolder.card,DoorLogEvent.card,9);
    MiniSerial.print("N:");
    MiniSerial.println(n);
    if(n==0){                      //Set DoorLogEvent NAME
      memcpy(DoorLogEvent.name, cardHolder.name, 25);
      recno=dbcount+1; //terminate "for loop".
    }
  }
  if(n!=0){
    memcpy(DoorLogEvent.name, noname,sizeof(noname));
  }
  MiniSerial.print("Name: ");
  MiniSerial.println(DoorLogEvent.name);
  webFile.close();
  StrClear(cardHolder.name, 25);
  StrClear(cardHolder.card, 9);
  uint8_t ipp=0;
  int count2=8;
  for (int k=cnt-9;k>0; k--){
    int g=1;
    for( int z=k-1; z>0; z--){
      g= g*10;
    }
    ipp = ((tag[count2]-'0')*g)+ipp;    //IP to compare
    count2++;
  }
  MiniSerial.print("IP: ");
  MiniSerial.println(ipp);
  
  
  webFile.open("dbase1.db");
  db.open(0,TABLE_SIZE_DOOR,sizeof(door));
  dbcount=db.count();
  for (int recno = 1; recno <=dbcount; recno++){
    db.readRec(recno, EDB_REC door);
    if(door.ip==ipp){            //Set DoorLogEvent DOORCODING
      memcpy(DoorLogEvent.doorcoding, door.coding, 5);
      recno=dbcount+1;
    }
  }
  MiniSerial.print("Door Coding: ");
  MiniSerial.println(DoorLogEvent.doorcoding);
  
  webFile.close();
  StrClear(door.name, 20);
  StrClear(door.coding, 5);
  
  DoorLogEvent.access=tag[11]-'0';  //Set DoorLogEvent ACCESS
  MiniSerial.print("Access: ");
  MiniSerial.println(DoorLogEvent.access);
                                   //Set DoorLogEvent TIME & DATE
  DoorLogEvent.hour=RTC.get(DS1307_HR,true);
  DoorLogEvent.minute=RTC.get(DS1307_MIN,false);
  DoorLogEvent.day=RTC.get(DS1307_DATE,false);
  DoorLogEvent.month=RTC.get(DS1307_MTH,false);
  DoorLogEvent.year=RTC.get(DS1307_YR,false);
  MiniSerial.print("Time: ");
  MiniSerial.print(DoorLogEvent.hour);
  MiniSerial.print(":");
  MiniSerial.println(DoorLogEvent.minute);
  MiniSerial.print("Date: ");
  MiniSerial.print(DoorLogEvent.day);
  MiniSerial.print("/");
  MiniSerial.print(DoorLogEvent.month);
  MiniSerial.print("/");
  MiniSerial.println(DoorLogEvent.year);
  
  webFile.open("dbase3.db",O_RDWR);
  db.open(0,TABLE_SIZE_DLE,sizeof(DoorLogEvent));
  db.appendRec(EDB_REC DoorLogEvent);
  
  //if logEvent DataBase reach the limit Delete the 100 older records.
  if(db.count()>998){
    for (int recno = 1; recno <=100; recno++){
      db.deleteRec(recno);
    }
  }
  webFile.close();
  
  StrClear(DoorLogEvent.name, 25);
  StrClear(DoorLogEvent.card, 9);
  StrClear(DoorLogEvent.doorcoding,5);
  
  MiniSerial.println("----End of read----");
}
  
  
  
  if (client) {  // got HTTP client ?
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {   // client data available to read
        char c = client.read();   // read 1 byte (character) from client
        count++;
               
        if(len_index==48){ //we must not exceed the buffer's size
          len_index=0;
        }
        
        if (c!='\n'){     //store to buffer
          contLbuffer[len_index]=c;
          len_index++;
	}else{         //if end of line check for Content-Length
                       //reset buffer
          if (StrContains(contLbuffer, "Content-Length")){
            int a = contLbuffer[16]-'0';
            int b = contLbuffer[17]-'0';
            d =a*10+b;
          }
          StrClear(contLbuffer, 50);
          len_index=0;
        }
		
        if (req_index < (REQ_BUF_SZ - 1)) {
          HTTP_req[req_index] = c;          // save HTTP request character
          req_index++;
        }
        
        MiniSerial.print(c);
                // print HTTP request character to MiniSerial monitor
                
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
        if (c == '\n' && currentLineIsBlank) {
          if (StrContains(HTTP_req, "POST / ")||StrContains(HTTP_req, "POST /login.htm")){
            while (client.connected()) {
              if (client.available()) {   // client data available to read
                                          // Read POST Data
                MiniSerial.print("Content Length: ");
                MiniSerial.println(d);
                for (int o=0;o<d;o++){
                  char c = client.read();
                  postcredentials[o]=c;
                }
                MiniSerial.print(postcredentials);
                //if login credentials are correct, give access to application
                //by sending index.htm
                if (StrContains(postcredentials, "username=admin&password=12345")){
	          StrClear(postcredentials,100 );
                  webFile.open("index.htm");
                }
                //If POST message contains the key-word "card"
                //this is for adding a new user
                else if(StrContains(postcredentials, "card=")){
                  device_count=0;
                  int cntpost=0;
                  int ii=3;
                  while (postcredentials[ii]!='&'){
                    cntpost++;
                    ii++;
                  }
                  int aa=0;
                  for (int k=ii-4;k>=0; k--){
                    int x=1;
                    for( int z=k; z>0; z--){
                      x= x*10;
                    }
                    aa = ((postcredentials[ii-cntpost]-'0')*x)+aa;
                    cntpost--;
                  }
                  //-----here we store User ID-------------
                  cardHolder.id=aa; 
                  access.cardHolder_id=aa;
                  //MiniSerial.println(cardHolder.id);
                  ii=ii+6;
                  cntpost=0;
                  //-----here we store User's Name-------------
                  while (postcredentials[ii]!='&'){
                    cardHolder.name[cntpost]=postcredentials[ii];
                    ii++;
                    cntpost++;
                  }
                  //MiniSerial.println(cardHolder.name);
                  
                  //-----here we store User Card Code----------
                  ii=ii+6;
                  cntpost=0;
                  while (postcredentials[ii]!='&'){
                    cardHolder.card[cntpost]=postcredentials[ii];
                    ii++;
                    cntpost++;
                  }
                  //MiniSerial.println(cardHolder.card);
                  
                  //-----here we store the devices
                  //     that user take access ----------
                  ii=ii+3;
                  webFile.open("dbase4.db",O_RDWR);
                  db.open(0,TABLE_SIZE_ACCESS,sizeof(access));
                  while(ii<d){
                    int iii=ii;
                    cntpost=0;
                    while (postcredentials[iii]!='&' && iii<d){
                      cntpost++;
                      iii++;
                    }
                    int bb=0;
                    int cntpost1=0;
                    for (int k=cntpost;k>0; k--){
                      int g=1;
                      for( int z=k-1; z>0; z--){
                        g= g*10;
                      }
                      bb = ((postcredentials[ii+cntpost1]-'0')*g)+bb;
                      cntpost1++;
                    }
                    access.door_id=bb;
                    MiniSerial.println(access.door_id);
                    access.id=db.count()+1;
                    db.appendRec(EDB_REC access);
                    ii=iii+3;
                    device_count++;
                  }
                  MiniSerial.print("device_count: ");
                  MiniSerial.println(device_count);
                  webFile.close();
                  delay(10);
                  webFile.open("dbase2.db",O_RDWR);
                  db.open(0,TABLE_SIZE_DOOR,sizeof(cardHolder));
                  db.appendRec(EDB_REC cardHolder);
                  delay(10);
                  webFile.close();
                  StrClear(postcredentials, 100);
                  StrClear(cardHolder.name, 25);
                  webFile.open("index.htm");
                  //formation of the array to sent to IP Readers
                  //this is for adding a new user
                  tag_to_send[0]='a';
                  for(i=1;i<9;i++){
                    tag_to_send[i]=cardHolder.card[i-1];
                  }
                  //tag_to_send[9]='\n';
                  StrClear(cardHolder.card, 9);
                  wificonnect=1;
                }
                
                //If POST message contains the key-word "ip4"
                //this is for adding a new IP Reader                
                else if(StrContains(postcredentials, "ip4=")){
                  int cntpost=0;
                  int ii=3;
                  while (postcredentials[ii]!='&'){
                    cntpost++;
                    ii++;
                  }
                  int aa=0;
                  for (int k=ii-4;k>=0; k--){
                    int x=1;
                    for( int z=k; z>0; z--){
                      x= x*10;
                    }
                    aa = ((postcredentials[ii-cntpost]-'0')*x)+aa;
                    cntpost--;
                  }
                  door.id=aa;
                  //MiniSerial.println(door.id);
                  ii=ii+6;
                  cntpost=0;
                  while (postcredentials[ii]!='&'){
                    door.name[cntpost]=postcredentials[ii];
                    ii++;
                    cntpost++;
                  }
                  //MiniSerial.println(door.name);
                  ii=ii+6;
                  cntpost=0;
                  while (postcredentials[ii]!='&'){
                    door.coding[cntpost]=postcredentials[ii];
                    ii++;
                    cntpost++;
                  }
                  //MiniSerial.println(door.coding);
                  ii=ii+27;
                  cntpost=0;
                  uint8_t bb=0;
                  for (int k=d-ii;k>0; k--){
                    int g=1;
                    for( int z=k-1; z>0; z--){
                      g= g*10;
                    }
                    bb = ((postcredentials[ii+cntpost]-'0')*g)+bb;
                    cntpost++;
                  }
                  door.ip=bb;
                  //MiniSerial.println(door.ip);
                  webFile.open("dbase1.db",O_RDWR);
                  db.open(0,TABLE_SIZE_DOOR,sizeof(door));
                  db.appendRec(EDB_REC door);
                  delay(10);
                  webFile.close();
                  StrClear(postcredentials, 70);
                  StrClear(door.name, 20);
                  StrClear(door.coding, 5);
                  webFile.open("index.htm");
                }
                //If POST message contains something else
                //we send a "404 not found" message
                else{
                  MiniSerial.println("Else case");
                  StrClear(postcredentials, 100);
                  webFile.open("404.htm");
                }
                break;
              }
            }
          }
          //Delete Request for IP Readers. Here we check the ID number
          //that comes after the last "/"
          else if (StrContains(HTTP_req, "DELETE /arduino/")){
            int m=0;
            for(int g=17;g<21;g++){
              if(HTTP_req[g]==' '){
                g=21;
              }
              m++;
            }
            int id=0;   //convert number from character to integer
            int cntpost=0;
            for (int k=m;k>0; k--){
              int g=1;
              for( int z=k-1; z>0; z--){
                g= g*10;
              }
              id = ((HTTP_req[16+cntpost]-'0')*g)+id;
              cntpost++;
            }
            int b=0;
            //MiniSerial.println(id);
            int dbcount;
            
            //delete record from database of IP Readers
            webFile.open("dbase1.db",O_RDWR);
            db.open(0,TABLE_SIZE_DOOR,sizeof(door));
            dbcount=db.count();
            //MiniSerial.print("dbCount: ");
            //MiniSerial.println(dbcount);
            for (int recno = 1; recno <=dbcount; recno++){
              db.readRec(recno, EDB_REC door);
              if(door.id==id){
                b=recno;
                MiniSerial.println(b);
              }
            }
            if(b>0){
              db.deleteRec(b);
            }
            webFile.close();
            delay(10);
            
            //Delete records from access table
            webFile.open("dbase4.db",O_RDWR);
            db.open(0,TABLE_SIZE_ACCESS,sizeof(access));
            dbcount=db.count();
            //MiniSerial.print("dbCount: ");
            //MiniSerial.println(dbcount);
            for (int recno = 1; recno <=dbcount; recno++){
              delay(10);
              db.readRec(recno, EDB_REC access);
              int f=0;
              if(access.door_id==id){
                f=recno;
              }
              if(f>0){
                db.deleteRec(f);
              }
            }
            webFile.close();
            delay(10);
            client.println("{}");
            client.println(); 
            
            delay(10);
            req_index = 0;
            StrClear(HTTP_req, REQ_BUF_SZ);
            client.stop();
            break;
          }
          
          //Delete Request for IP Readers. Here we are also check 
          //the ID number that comes after the last "/"
          else if (StrContains(HTTP_req, "DELETE /users/")){ 
            int m=0;
            for(int g=15;g<19;g++){
              if(HTTP_req[g]==' '){
                g=18;
              }
              m++;
            }
            int id=0;
            int cntpost=0;
            for (int k=m;k>0; k--){
              int g=1;
              for( int z=k-1; z>0; z--){
                g= g*10;
              }
              id = ((HTTP_req[14+cntpost]-'0')*g)+id;
              cntpost++;
            }
            int b=0;
            MiniSerial.print("id to be deleted: ");
            MiniSerial.println(id);
            
            webFile.open("dbase2.db",O_RDWR);
            db.open(0,TABLE_SIZE_CH,sizeof(cardHolder));
            int dbcount=db.count();
            MiniSerial.print("dbCount: ");
            MiniSerial.println(dbcount);
            for (int recno = 1; recno <=dbcount; recno++){
              db.readRec(recno, EDB_REC cardHolder);
              MiniSerial.print("cardHolder ID: ");
              MiniSerial.println(cardHolder.id);
              if(cardHolder.id==id){
                b=recno;
                MiniSerial.print("I thesi apo tin opoia tha ginei i diagrafi: ");
                MiniSerial.println(b);
              }
            }
            if(b>0){
              db.deleteRec(b);
            }
            webFile.close();
            delay(10);
            
            client.println("{}");
            client.println(); 
            
            delay(10);
            req_index = 0;
            StrClear(HTTP_req, REQ_BUF_SZ);
            client.stop();
            // This message is for deleting user
            // from IP Readers memory
            tag_to_send[0]='b';
            for(i=1;i<9;i++){
              tag_to_send[i]=cardHolder.card[i-1];
            }

			
            delete_user=id;      
            wificonnect=1;
            break;
          }
          
          //Get Request messages for Web Site / Web application
          else if (StrContains(HTTP_req, "GET / ")
          || StrContains(HTTP_req, "GET /login.htm")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/html");
            client.println();
            delay(10);
            webFile.open("login.htm");        // open web page file
          }
          
          else if (StrContains(HTTP_req, "GET /main.css")) {
            client.println("HTTP/1.1 200 OK");   //response message
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/css");
            client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
            client.println();
            delay(10);
            webFile.open("main.css");        // open web page file
          }
          
          else if (StrContains(HTTP_req, "GET /main.js")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/javascript");
            client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
            client.println();
            delay(10);
            webFile.open("main.js");
          }
          
          else if (StrContains(HTTP_req, "GET /bootstrap.min.js")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/javascript");
            client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
            client.println();
            delay(10);
            webFile.open("bt.js");
          }
          
          else if (StrContains(HTTP_req, "GET /bootstrap.min.css")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/css");
            client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
            client.println();
            delay(10);
            webFile.open("bt.css");
          }
          
          else if (StrContains(HTTP_req, "GET /fonts/glyphicons-halflings-regular.woff2")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/css");
            client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
            client.println();
            delay(10);
            webFile.open("regular.off");
          }
          
          else if (StrContains(HTTP_req, "GET /fonts/glyphicons-halflings-regular.woff")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/css");
            client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
            client.println();
            delay(10);
            webFile.open("regular.wff");
          }
          
          else if (StrContains(HTTP_req, "GET /fonts/glyphicons-halflings-regular.ttf")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/css");
            client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
            client.println();
            delay(10);
            webFile.open("regular.ttf");
          }
          
          else if (StrContains(HTTP_req, "GET /jquery")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: text/javascript");
            client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
            client.println();
            delay(10);
            webFile.open("jq.js");
          }
          
          //Here we send the json file for printing the IP Reader devices
          //that are stored in door table
          //if a GET /arduino message arive
          else if (StrContains(HTTP_req, "GET /arduino")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: application/json;charset=UTF-8");
            client.println();
            webFile.open("dbase1.db");
            db.open(0,TABLE_SIZE_DOOR,sizeof(door));
            client.print("[");
            int dbcount=db.count();
            for (int recno = 1; recno <=dbcount; recno++){
              db.readRec(recno, EDB_REC door);
              client.print("{\"id\" : ");client.print(door.id);
              client.print(",\"name\" : \"");client.print(door.name);
              client.print("\",\"code\" : \"");client.print(door.coding);
              client.print("\",\"ip\" : \"192.168.1.");client.print(door.ip);
              client.print("\",\"ip4\" : ");client.print(door.ip);
              client.print("}");
              if(recno!=dbcount){
                client.print(",");
              }
              StrClear(door.name, 20);
              StrClear(door.coding, 5);
            }
            client.print("]");
            client.println();
            webFile.close();
            delay(10);
            req_index = 0;
            StrClear(HTTP_req, REQ_BUF_SZ);
            client.stop();
            break;
          }
          
          //Here we send the json file for printing the Users
          //that are stored in CardHolder table 
          //if a GET /users message arive         
          else if (StrContains(HTTP_req, "GET /users")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Connection: keep-alive");
            client.println("Content-Type: application/json;charset=UTF-8");
            client.println();
            StrClear(cardHolder.name, 25);
            StrClear(cardHolder.card, 9);
            webFile.open("dbase2.db");
            db.open(0,TABLE_SIZE_CH,sizeof(cardHolder));
            client.print("[");
            int dbcount=db.count();
            for (int recno = 1; recno <=dbcount; recno++){
              db.readRec(recno, EDB_REC cardHolder);
              client.print("{\"id\" : ");client.print(cardHolder.id);
              client.print(",\"name\" : \"");client.print(cardHolder.name);
              client.print("\",\"card\" : \"");client.print(cardHolder.card);
              client.print("\"");
              webFile.close();
              delay(10);
              webFile.open("dbase4.db");
              db.open(0,TABLE_SIZE_ACCESS,sizeof(access));
              int dbcount1=db.count();
              int first_time=1;
              for (int recno = 1; recno <=dbcount1; recno++){
                db.readRec(recno, EDB_REC access);
                if(cardHolder.id==access.cardHolder_id){
                  if(first_time==1){
                    client.print(",\"devices\" :\"");
                    first_time=0;
                  }
                  client.print(access.door_id);
                  client.print(",");
                }
              }
              if(first_time==0){
                client.print("\"");
              }
            client.print("}");
            if(recno!=dbcount){
              client.print(",");
            }
            webFile.close();
            delay(10);
            webFile.open("dbase2.db");
            db.open(0,TABLE_SIZE_CH,sizeof(cardHolder));
            StrClear(cardHolder.name, 25);
            StrClear(cardHolder.card, 9);
          }
          client.print("]");
          client.println();
          webFile.close();
          delay(10);
          req_index = 0;
          StrClear(HTTP_req, REQ_BUF_SZ);
          client.stop();
          break;
        }
          //Here we send the json file for printing Log
          //that is stored in DoorLogEvent table 
          //if a GET /log message arive
          
        else if (StrContains(HTTP_req, "GET /log")) {
          client.println("HTTP/1.1 200 OK");
          client.println("Connection: keep-alive");
          client.println("Content-Type: application/json;charset=UTF-8");
          client.println();
          webFile.open("dbase3.db");
          db.open(0,TABLE_SIZE_DLE,sizeof(DoorLogEvent));
          client.print("[");
          int dbcount=db.count();
          int auksonArithmos=1;
          MiniSerial.print("dbCount: ");
          MiniSerial.println(dbcount);
          for (int recno = dbcount; recno >0; recno--){
            db.readRec(recno, EDB_REC DoorLogEvent);
            client.print("{\"id\" : ");client.print(auksonArithmos);
            client.print(",\"name\" : \"");client.print(DoorLogEvent.name);
            client.print("\",\"card\" : \"");client.print(DoorLogEvent.card);
            client.print("\",\"door\" : \"");client.print(DoorLogEvent.doorcoding);
            client.print("\",\"time\" : \"");client.print(DoorLogEvent.hour);
            client.print(":");client.print(DoorLogEvent.minute);
            client.print("\",\"date\" : \"");client.print(DoorLogEvent.day);
            client.print("/");client.print(DoorLogEvent.month);
            client.print("/");client.print(DoorLogEvent.year);
            client.print("\",\"access\" : \"");
            if(DoorLogEvent.access==0){
              client.print("NO");
            }
            else{
              client.print("YES");
            }
          client.print("\"}");
          if(recno!=1){
            client.print(",");
          }
          auksonArithmos++;
          StrClear(DoorLogEvent.name, 25);
          StrClear(DoorLogEvent.card, 9);
          StrClear(DoorLogEvent.doorcoding, 5);
        }
        client.print("]");
        client.println();
        webFile.close();
        delay(10);
        req_index = 0;
        StrClear(HTTP_req, REQ_BUF_SZ);
        client.stop();
        break;
      }
      //send favicon
      else if (StrContains(HTTP_req, ".ico")){
        if (StrContains(HTTP_req, "favicon.ico")){
          webFile.open("favicon.ico");
        }
        if (webFile.available()) {
          client.println("HTTP/1.1 200 OK");
          client.println("Connection: keep-alive");
          client.println("Content-Type: image/x-icon");
          client.println("Expires: Fri, 19 Aug 2016 23:36:12 GMT");
          client.println();
          delay(10);
        }
      }
      //if something else comes here, close connection
      else{
        req_index = 0;
        StrClear(HTTP_req, REQ_BUF_SZ);
        client.stop();
        MiniSerial.println("nonHttp:");
        break
      }
      //Here we send every file
      //open from SD -> send to client
      //we use a buffer for faster transmition
      clientCount = 0;
      int bytecnt = 0;
      if (webFile.available()) {
        while(webFile.available()) {
          clientBuf[clientCount] = webFile.read();
          clientCount++;
          if(clientCount > 127) {
            client.write(clientBuf,128);
            bytecnt = bytecnt + clientCount;
            clientCount = 0;
            delay(1);
          }
        }
        if(clientCount > 0){
          client.write(clientBuf,clientCount);
          bytecnt = bytecnt + clientCount;
        }
        webFile.close();
        MiniSerial.print(bytecnt);
        MiniSerial.println(" bytes sent.");
      }
      
      // reset HTTP_req buffer index and all buffer elements to 0
      req_index = 0;
      StrClear(HTTP_req, REQ_BUF_SZ);
      break;
    }
    
    // every line of text received from the client ends with \r\n
    if (c == '\n') {
      // last character on line of received text
      // starting new line with next character read
      currentLineIsBlank = true;
    }
    else if (c != '\r') {
      // a text character was received from client
      currentLineIsBlank = false;
    }
  } // end if (client.available())
} // end while (client.connected())
delay(1);      // give the web browser time to receive the data
client.stop(); // close the connection
} // end if (client)
}



    // sets every element of str to 0 (clears array)
    void StrClear(char *str, char length){
      for (int i = 0; i < length; i++) {
        str[i] = 0;
      }
    }

    // searches for the string sfind in the string str
    // returns 1 if string found
    // returns 0 if string not found
    char StrContains(char *str, char *sfind)
    {
      char found = 0;
      char index = 0;
      char len;
      len = strlen(str);
      if (strlen(sfind) > len) {
        return 0;
      }
      while (index < len) {
        if (str[index] == sfind[found]) {
          found++;
          if (strlen(sfind) == found) {
            return 1;
          }
        }
        else {
          found = 0;
        }
        index++;
      }
      return 0;
    }
