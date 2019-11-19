#include <SPI.h>
#include <RH_RF95.h>

// for feather m0  
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);


void setup() {
  // put your setup code here, to run once:
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(115200);
  
  while (!Serial) {
    delay(1);
  }

  delay(100);
  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    //Serial.println("LoRa radio init failed");
    while (1);
  }

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) 
  {
    //Serial.println("setFrequency failed");
    while (1);
  }
  
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);



//while not received any serial send not found

}
char lastgoodsentmessage[36]="xxxxxxh,3249.61N,08338.97W,A=000000";//35 bytes
char lastunsentmessage[36]="xx99xxh,32XX.61N,08338.97W,A=000XXX";//35 bytes, if sent set to zero
//char messagetosend[35]="xx99xxh,3249.61N,08338.97W,A=000000";//35 bytes

bool isupdate=0;
void loop()
  {
  delay(50);
  
  if(Serial.available()>34)                     //get new or same coordinates
  {
  
    //char message1[]="162716h,3249.61N,08338.97W,A=000560";//35 bytes
    //char message1[]="162716h,3233.65N,08334.95W,A=000560";//35 bytes
    char newmessage[36];
    (Serial.readString()).toCharArray(newmessage, 36);//get serial message
    
    //check if new message and message to send are same
    if(!arraychecker((uint8_t *) newmessage,(uint8_t *) lastunsentmessage, 36))//if true, new message; if false, not new message, discard
    {
      strcpy( (char*)lastunsentmessage, (char *)newmessage );//copy new message to last unsent message
      
      if(!arraychecker((uint8_t *)lastunsentmessage,(uint8_t *)lastgoodsentmessage, 36))// if message to send and last sent one not same, set update flag.
      {
        isupdate=1;
      }
      
      else 
      {
        isupdate=0;  
      }
            
    }
    else//discard new message
    {
      
    }
    
    


  }
  else//nothing is available from serial, Send status of nothing yet or update available
  {
    SendStatus(isupdate);
    
    if(listenforrequest())
    {
      if(Sendgoodcoordinates((char *)lastunsentmessage))
      {
        ///send succeeded, copy lasstunsent to lastgood
        strcpy( lastgoodsentmessage, lastunsentmessage );
        char nowold[36]="xx99xxh,32XX.61N,08338.97W,A=000XXX";
        strcpy( lastunsentmessage, nowold );
        isupdate=0;
      }
      else//send failed, still have update
      {
        isupdate=1;
      }

      
    }
    else//did not get request
    {
        
    }
   }
  
  }
/////////////////////////////////////////////////////////////////
bool arraychecker(uint8_t buf[], uint8_t radiopacket[], int arrsize)
{
  for(int i=0;i<arrsize;i++)
  {
    if(buf[i]==radiopacket[i])
    {
    }
    else
    {
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////
bool Sendgoodcoordinates(char radiopacket[36])
{
  char good[5]="good";
  char bad[4] = "bad";
  //delay(1000); // Wait 1 second between transmits, could also 'sleep' here!
  //char radiopacket[35] ="162716h,3249.61N,08338.97W,A=000560";
  delay(10);
  
  rf95.send((uint8_t *)radiopacket, 36);
  delay(10);
  rf95.waitPacketSent();
  
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  
  // Now wait for a reply
  if (rf95.waitAvailableTimeout(1000))
  { 
    // Should be a reply message for us now   
    if (rf95.recv(buf, &len) && arraychecker(buf, (uint8_t *)radiopacket, 36))
    {
    
    rf95.send((uint8_t *)good, 5);
    rf95.waitPacketSent();
    return true;
 
    }
    else//should never get called
    {
      //Serial.println("Receive failed");
      rf95.send((uint8_t *)bad, 4);
      rf95.waitPacketSent();
      return false;
    }
  }
  else//no reply sent back, check failed
  {
    return false;
  }
}
////////////////////////////////////////////////////////////////
void SendStatus(bool updatte)
{
  if(updatte) //send update ready message
  {
   char updatemessage[10]="111111111";
   rf95.send((uint8_t *)updatemessage, 10);
   delay(10);
   rf95.waitPacketSent(); 
  }
  else //send nothing yet message
  {
    char nothingmessage[10]="000000000";
    rf95.send((uint8_t *)nothingmessage, 10);
    delay(10);
    rf95.waitPacketSent(); 
  }
}
//////////////////////////////////////////////////////////////////
bool listenforrequest()
{
  if (rf95.waitAvailableTimeout(500))
  {
    char sendd[9]="sendsend";
    uint8_t buf2[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf2);
    if(rf95.recv(buf2, &len) && arraychecker(buf2, (uint8_t *)sendd,9))
    {
      return true;  
    }
  }
  else //did not get request
  {
    return false;
  }
}
  
