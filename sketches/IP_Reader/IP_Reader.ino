
#include <WiShield.h>
#include <EEPROMex.h>
#include <SoftwareSerial.h>


SoftwareSerial rfidSerial(4, 5);  // For Rfid communication

extern "C" {
    #include "socketapp.h"
    #include "uip.h"
    #include <string.h>
}


char tobesent[14]={0};      //tag to be sent if a card is used.
char invalidtag[]={"invalid tag"};
char existTag[]={"Tag already exist"};
char NOTexistTag[]={"Tag doesn't exist"};
char noEmpty[]={"No empty space at eeprom"};
int iTAG[8];  //Here we store the conversion of chars of the card
int TAG[4];  //Here we store Tag Number which is send by Controller/Server
int emptyTag[4]={255,255,255,255};  //represents an empty space at EEPROM
int i=0;
int len=0;
int len1=0;
int len2=0;
int invalidcard=1;
int Send_card=0;
int Send_card1=0;
int change_connection=0; 
uip_ipaddr_t srvaddr; //here we store ip address of Controller/Server
int TAG1[4]; //Here we store Tag Number when rfid is been used

//what's included should be deleted
//int CMD[64]; // Here the Max command length is set as 64 bytes. For example, Command “AB 02 01” is 3 bytes
//int comlen =0;

//what's included should be deleted


// Wireless configuration parameters ----------------------------------------

#define WIRELESS_MODE_INFRA	1
#define WIRELESS_MODE_ADHOC	2

unsigned char local_ip[]     = {192,168,1,100};	// IP address of WiShield
unsigned char gateway_ip[]   = {192,168,1,1};	// router or gateway IP address
unsigned char subnet_mask[]  = {255,255,255,0};	// subnet mask for the local network
char ssid[]                  = {"pcg01"};		// max 32 bytes
unsigned char security_type = 2;	// 0 - open; 1 - WEP; 2 - WPA; 3 - WPA2

// WPA/WPA2 passphrase
const prog_char security_passphrase[] PROGMEM = {"password"};	// max 64 characters

// WEP 128-bit keys
// sample HEX keys
prog_uchar wep_keys[] PROGMEM = {	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,	// Key 0
									0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00,	// Key 1
									0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00,	// Key 2
									0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00	// Key 3
								};

// setup the wireless mode
// infrastructure - connect to AP
// adhoc - connect to another WiFi device
unsigned char wireless_mode = WIRELESS_MODE_INFRA;

unsigned char ssid_len;
unsigned char security_passphrase_len;
//---------------------------------------------------------------------------

void setup()
{   
    Serial.begin(57600); 
    rfidSerial.listen();
    rfidSerial.begin(9600); //Init of Rfid Serial Communication
    rfidSerial.write(0x02); //Send the command to read RFID tag, please refer to the manual for more details.
    WiFi.init();  //Init of Wishield. zg_init() -> stack_init() ==> socket_app_init()
    
    Serial.println("Here we start...");
    tobesent[8]='1';
    tobesent[9]='0';
    tobesent[10]='0';  // Write the IP Address we use.
}

void loop()
{

        while (rfidSerial.available()){ //read the bytes from rfid and
          TAG1[len1]=rfidSerial.read(); //store them to TAG1 array        
          int m=TAG1[len1];
          int n=0;
              while(m>16){      //Here we convert Tag into characters
                m=m-16;
                n++;
              }
              if(m==16){
                tobesent[len2]=char(n+49);
              }else if(n>9){
                tobesent[len2]=char(n+55);
              }else{
                tobesent[len2]=char(n+48);
              }
              
              len2++;
              
              if(m==16){
                tobesent[len2]=char(48);
              }else if(m>9){
                tobesent[len2]=char(m+55);
              }else{
              tobesent[len2]=char(m+48);
              }
                                /*End of conversion.                                             
                                  The Characters of tag are being stored
                                  in the first 8bytes of "tobesent" array
                                  */
                                  
              Serial.print(" From RFID we took: ");    //Debuging perpose only
              Serial.println(TAG1[len1],HEX);
              Serial.print("we change it it to: ");
              Serial.print(tobesent[len2-1]);
              Serial.println(tobesent[len2]);
              
              len2++;
              len1++;
          delay(10);
         Serial.print("lengt1: ");
         Serial.println(len1); 
         if (len1==4){      //if we read 4 bytes, we check the card to give or not access
           Send_card=1;     // We send the card to Server Log.
           int k = EEPROMcheck(TAG1);            
           if (k>=0){                  
             /*digitalWrite(6, HIGH);
             delay(1000);  //pinMode(6,OUTPUT);
             digitalWrite(6, LOW);  // DoorLock Relay driver Pin.
             */
             Serial.print(" We Gave Acess to: ");
             tobesent[11]='1';      //This code refer to the card access.
           }else{
             Serial.print(" We DIDN'T Gave Acess to: ");
             tobesent[11]='0';
           }
           Serial.print(tobesent);
           len1=0;
           len2=0;
           break;
         }
       }
        
        if(Send_card==1){    //Here we are making the connection to the Server
          Send_card1=1;
          Send_card=0;
          uip_ipaddr(&srvaddr, 192,168,1,101);
          uip_connect(&srvaddr, HTONS(3333));
        }
        
        if(change_connection==1){  //if card data has been sent, 
          socket_app_init();       //we have to initialize socket again
          change_connection=0;
          Send_card1=0;
        }
        
        WiFi.run();      // The function that runs the Wishield
}


extern "C" {

  /*
 * Declaration of the protosocket function that handles the connection
 * (defined at the end of the code).
 */
static int handle_connection(struct socket_app_state *s);


/*
 * if a new card occurs, we send the the data with send_connection function
 */
static void send_connection(void);
/*---------------------------------------------------------------------------*/
/*
 * The initialization function. It is been called, some time after uip_init() is
 * called. First call on Wifi.init();
 */
void socket_app_init(void)
{
  uip_listen(HTONS(1000));  // We start to listen for connections on TCP port 1000.
}
/*---------------------------------------------------------------------------*/
/*
 * In socketapp.h we have defined the UIP_APPCALL macro to
 * socket_app_appcall so that this function is uIP's application
 * function. This function is called whenever an uIP event occurs
 * (e.g. when a new connection is established, new data arrives, sent
 * data is acknowledged, data needs to be retransmitted, etc.).
 */
void socket_app_appcall(void)
{
  /*
   * The uip_conn structure has a field called "appstate" that holds
   * the application state of the connection. We make a pointer to
   * this to access it easier.
   */
  struct socket_app_state *s = &(uip_conn->appstate);
  /*
   * If a new connection was just established, we should initialize
   * the protosocket in our applications' state structure.
   */
  if(uip_connected()&&Send_card==0) {
    PSOCK_INIT(&s->p, s->inputbuffer, sizeof(s->inputbuffer));
  }


  /*
   * Finally, we run the protosocket function or the funtion that makes 
   * the connection between this device and and the Server. For handle_connection
   * function we have to pass a pointer to the application state
   * of the current connection.
   */
  if(Send_card1==1){
    send_connection();
  }else{
    handle_connection(s);
  }
}
/*---------------------------------------------------------------------------*/
/*
 * This is the function were we are sending data to Server.
 * Here we have to check the connection status and change the flags state.
 */
 
static void send_connection(void){
      if(uip_closed() || uip_timedout()) {
         Serial.println("SA: closed / timedout");
         uip_close();
         change_connection=1;
         return;
      }
      if(uip_connected()) {
         Serial.println("SA: connected / send");
         uip_send(tobesent,strlen(tobesent)); 
         Send_card=0;    
      }
      if(uip_poll()) {
         Serial.println("SA: poll");
      }
      if(uip_aborted()) {
         Serial.println("SA: aborted");
         return;
      }
      if(uip_acked()) {
         Serial.println("SA: acked");
         uip_close();
      }
      if(uip_newdata()) {
         Serial.println("SA: newdata");
      }
      if(uip_rexmit()) {
         Serial.println("SA: rexmit");
         uip_send(tobesent,strlen(tobesent));
      }
      
}

/*---------------------------------------------------------------------------*/
/*
 * This is the protosocket function that handles the communication. A
 * protosocket function must always return an int, but must never
 * explicitly return - all return statements are hidden in the PSOCK
 * macros.
 * Here we are also handling by case the card number that is been send from server 
 * to add to or delete from the EEPROM. 
 * Case a stands for adding a card to EEPROM
 * Case b stands for deleting a card from EEPROM
 */
static int handle_connection(struct socket_app_state *s)
{
  PSOCK_BEGIN(&s->p);      //protosocket initialization
  PSOCK_READTO(&s->p,'\n');
     // check at inputbuffer for the upcoming data. Check if valid and seperate them.
     // reads until "\n" character is been read.
  switch (s->inputbuffer[0]){ //Switch case that depends on first byte of buffer
     case 'a':
      Serial.print('\n');
      len=0;
      invalidcard=0;
      Serial.println(s->inputbuffer);
      for(int i=0; i<8; i++){
        iTAG[i]=SerialReadHexDigit(s->inputbuffer[i+1]);
        if(iTAG[i]<0){  //Invalid character if SerialReadHexDigit returns -1
          memset(s->inputbuffer, 0x00, sizeof(s->inputbuffer)); 
          memcpy(s->inputbuffer, invalidtag, sizeof(invalidtag) );
          invalidcard=1;
          Serial.println(invalidtag);
          break;
        }
        if(i==1||i==3||i==5||i==7){
          int k= 16*iTAG[i-1];
          int l=iTAG[i];
          TAG[len]=k+l;
          len++;
         }
       }  //convet the number of card from char to integer 
        
        
        if (invalidcard == 0){
          int f = EEPROMcheck(TAG);  //check if card already exists in EEPROM
          if(f>=0){
            memset(s->inputbuffer, 0x00, sizeof(s->inputbuffer)); //fil inputbuffer with zeros
            memcpy(s->inputbuffer, existTag, sizeof(existTag));   //copy the debug message
            Serial.print('\n');
            Serial.print(existTag);
            Serial.print('\n');
          }else{
            int g=EEPROMcheck(emptyTag); //Find empty space at EEPROM
            if (g>=0){                   //returns the position
              EEPROM.writeByte(g,TAG[0]);
              EEPROM.writeByte(g+1,TAG[1]);
              EEPROM.writeByte(g+2,TAG[2]);
              EEPROM.writeByte(g+3,TAG[3]);                              
              Serial.print(" The bytes that was writen in adress: ");
              Serial.print(g);
              Serial.print(EEPROM.readByte(g));
              Serial.print(EEPROM.readByte(g+1));
              Serial.print(EEPROM.readByte(g+2));
              Serial.print(EEPROM.readByte(g+3));
              Serial.print('\n');
            }else{
              memset(s->inputbuffer, 0x00, sizeof(s->inputbuffer));
              memcpy(s->inputbuffer, noEmpty, sizeof(noEmpty));
              Serial.print('\n');
              Serial.print("There is no empty space in EEPROM");
              Serial.print('\n');
            }
          }
          Serial.print(" The tag that was given by IP: ");
          Serial.print('\n');
          Serial.print(TAG[0],HEX);
          Serial.print(TAG[1],HEX);
          Serial.print(TAG[2],HEX);
          Serial.print(TAG[3],HEX);
          Serial.print('\n');
        }
      break;
      case 'b':
      Serial.print('\n');
      len=0;
      invalidcard=0;
      for(int i=0; i<8; i++){
        iTAG[i]=SerialReadHexDigit(s->inputbuffer[i+1]);
        if(iTAG[i]<0){
          memset(s->inputbuffer, 0x00, sizeof(s->inputbuffer)); 
          memcpy(s->inputbuffer, invalidtag, sizeof(invalidtag) );
          invalidcard=1;
          Serial.print('\n');
          Serial.print(invalidtag);
          Serial.print('\n');
          break;
        }
        if(i==1||i==3||i==5||i==7){
          int k= 16*iTAG[i-1];
          int l=iTAG[i];
          TAG[len]=k+l;
          len++;
         }}
        if (invalidcard == 0){
         int f = EEPROMcheck(TAG); //find the position of the card that is going to be deleted
          if(f<0){
            memset(s->inputbuffer, 0x00, sizeof(s->inputbuffer)); 
            memcpy(s->inputbuffer, NOTexistTag, sizeof(NOTexistTag));
            Serial.print('\n');
            Serial.print(NOTexistTag);
            Serial.print('\n');
          }else{                  //deleting the card number from EEPROM
            EEPROM.writeByte(f,emptyTag[0]);
            EEPROM.writeByte(f+1,emptyTag[1]);
            EEPROM.writeByte(f+2,emptyTag[2]);
            EEPROM.writeByte(f+3,emptyTag[3]);                            
            Serial.print(" The bytes that was writen in adress: ");
            Serial.print(f);
            Serial.print(EEPROM.readByte(f));
            Serial.print(EEPROM.readByte(f+1));
            Serial.print(EEPROM.readByte(f+2));
            Serial.print(EEPROM.readByte(f+3));
            Serial.print('\n');
          }
          Serial.print(" The tag that was given by IP: ");
          Serial.print('\n');
          Serial.print(TAG[0],HEX);
          Serial.print(TAG[1],HEX);
          Serial.print(TAG[2],HEX);
          Serial.print(TAG[3],HEX);
          Serial.print('\n');
        }
      break;
     default: 
       memset(tobesent, 0x00, sizeof(tobesent));
       memset(s->inputbuffer, 0x00, sizeof(s->inputbuffer));
       break;
  }
  
  PSOCK_SEND_STR(&s->p, s->inputbuffer); // debuging message to be sent to server
  memset(s->inputbuffer, 0x00, sizeof(s->inputbuffer));

  PSOCK_CLOSE(&s->p);
  PSOCK_END(&s->p);
}
}


int SerialReadHexDigit(char a)
{
  byte c = byte(a);
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else {
    return -1; // getting here is bad: it means the character was invalid
  }
}


  int EEPROMcheck(int array[4]){
    int b=-1;
    for (int c=0; c<1024; c=c+4){
      if (array[0] == EEPROM.readByte(c) ){
        if (array[1] == EEPROM.readByte(c+1)){
          if (array[2] == EEPROM.readByte(c+2)){
            if (array[3] == EEPROM.readByte(c+3)){
              b=c;
              break;
            }}}}}
    return b;
  }
  
  
  int SerialReadHexDigit1()
{
byte c = (byte) Serial.read();
if (c >= '0' && c <= '9') {
return c - '0';
} else if (c >= 'a' && c <= 'f') {
return c - 'a' + 10;
} else if (c >= 'A' && c <= 'F') {
return c - 'A' + 10;
} else {
return -1; // getting here is bad: it means the character was invalid
}
}
