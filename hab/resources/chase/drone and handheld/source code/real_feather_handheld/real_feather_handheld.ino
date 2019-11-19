// -*- mode: C++ -*-
// Example sketch showing how to create a simple messaging client (receiver)
// with the RH_RF95 class. RH_RF95 class does not provide for addressing or
// reliability, so you should only use RH_RF95 if you do not need the higher
// level messaging abilities
// It is designed to work with the other example Feather9x_TX

#include <SPI.h>
#include <RH_RF95.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <LSM303.h>
#include <TinyGPS++.h>
#include "math.h"

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

TinyGPSPlus gps;
static const uint32_t GPSBaud = 9600;

LSM303 compass;
//float Magoffset = -(-5.6333333);//offset to the compass magnetic heading to get true heading.
float Magoffset=0;

//Radio stuff
// for feather m0 RFM9x
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Blinky on receipt
#define LED 13

//interrupts and buttons
const int interruptPin = 5;
const int interruptPin2 = 6;
const int interruptPin3 =9;
const int interruptPin4 = 10;
const int interruptPin5 =11

;

int buttonnumber=0;
char currentoption='a';
long debouncing_time = 70; //Debouncing Time in Milliseconds
volatile unsigned long last_micros;

char lcdbuf0[21]="                    ";
char lcdbuf1[21]="                    ";
char lcdbuf2[21]="                    ";
char lcdbuf3[21]="                    ";

////structs to hold nav and time data
struct MessageForm
{
  char messgetime[9]="******";
  int Altitude=0;
  char St_Altitude[7];
  char lattitude[10]="000000000";
  char longitude[10]="000000000";
  float Lattitude=0;
  float Longitude=0;   
};

struct CurrentPosForm
{
  String gpstime="*****";
  String gpsdate="*****";
  int Altitude;
  float Lattitude;
  float Longitude; 
  char LAttitude[10];
  char LOngitude[10];   
};

MessageForm MssgForm;
CurrentPosForm CurForm;


void setup()
{
  
  ScreenInit();
  //delay(3000);
  pinMode(LED, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  /*Serial.begin(115200);
  while (!Serial) {
    delay(1);
  }
  delay(100);*/

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    //Serial.println("LoRa radio init failed");
    while (1);
  }
  //Serial.println("LoRa radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    //Serial.println("setFrequency failed");
    while (1);
  }
  //Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);

  //
  Serial1.begin(GPSBaud);

  
  compass.init();
  compass.enableDefault();
  compass.m_min = (LSM303::vector<int16_t>){ -2400,  -2823,  -2081};
  compass.m_max = (LSM303::vector<int16_t>){ +2974,  +2795,  +3516};

  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), Button1, LOW);
  
  pinMode(interruptPin2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin2), Button2, LOW);
  
  pinMode(interruptPin3, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin3), Button3, LOW); 

  pinMode(interruptPin4, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin4), Button4, LOW);
  
  pinMode(interruptPin5, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin5), Button5, LOW); 
}

char good[5] = "good";
char bad[4] = "bad";
int MainMode = 0;
char usingcoordinates[36]="xxxxxxxxxxxxxxxx";
int laststatusrssi;

void loop()
{
  //handle button presses
  currentoption=Buttonhandler(buttonnumber,currentoption);
  buttonnumber=0;
  

  
   if(currentoption=='a')//Radio Mode default
   {
    String line = "Connecting         ";
    line.toCharArray(lcdbuf0,21);
    //lcd.clear();
    printscreen(0, lcdbuf0);

      
    switch(StatusChecker()) ///read status packet from drone
    {                       // select option based on that
  
     case 'U'  ://transmit send message, then receive main packet
        printscreen(1, "Ready and Receiving"); 
        SendSend();
        if(GetCoordinates())
        {
          printscreen(2, "     Success   ");
          delay(500);
          lcd.setCursor(0,3);
          printcharscreen(3, usingcoordinates, 0, 16, 20);
          MessageParse();
          delay(5000);
          printscreen(3,"                        "); 
        }
        else
        {
          printscreen(2, "     Failed      ");
          delay(200);    
        }
        break; /* optional */
    
     case 'N'  ://do nothing,not ready    
        printscreen(1, "    No New Data     "); 
        delay(250); 
        break; /* optional */
        
     case 'R'  :// problem,reception error 
        printscreen(1, " Reception Error    ");     
        break; /* optional */
    
     case 'O' :// do nothing, out of range 
      printscreen(1, "     No Signal    ");
      delay(500);      
        break; /* optional */     
        
     default:
        break;
    }
  }
  else if(currentoption=='b')//radio resend mode
  {
     
  }
  else if(currentoption=='c')//gps one mode
  { 
    ///compass stuff
    float heading = CompassRead();// output is xxx.xxx
    float heading1=heading+Magoffset;
    
    //gps read
    gpsread();//encode character by character whatever is in SERIAL1 buffer

    //GPS Parse
    //time and date
    
    if(gps.location.isValid())
    {
      CurForm.Lattitude=gps.location.lat();//float char
      gcvt(gps.location.lat(),8,(CurForm.LAttitude));
      
      CurForm.Longitude=gps.location.lng();
      gcvt(gps.location.lng(),8,(CurForm.LOngitude));
    }
    else
    {
      CurForm.Lattitude=0;
      for(int i=0; i< 9; i++)
      {
        CurForm.LAttitude[i]='*';
      }

      CurForm.Longitude=0;
      for(int i=0; i< 9; i++)
      {
        CurForm.LOngitude[i]='*';
      }     
    }
    
    if(gps.location.isValid() && MssgForm.messgetime[3]!='*')
    {
      
      float DIST = (float)TinyGPSPlus::distanceBetween(CurForm.Lattitude, CurForm.Longitude,MssgForm.Lattitude ,MssgForm.Longitude) / 1000;    //in KM
      
      sprintf(lcdbuf1,"Dis:%-2.3f km    ",DIST);
   
      float B = bearing(CurForm.Lattitude, CurForm.Longitude, MssgForm.Lattitude ,MssgForm.Longitude);   
      sprintf(lcdbuf2,"Dir:%-3.0f  Head:%-3.0f",B, heading1);
      //lcdbuf2[20]='\n';
    }
    else
    {
      String title="No Valid GPS Data  ";
      title.toCharArray(lcdbuf1,21);
      String title1="                    ";
      title1.toCharArray(lcdbuf3,21);
      title1.toCharArray(lcdbuf2,21);
   
    }
    printscreen(0,lcdbuf0);
    printscreen(1,lcdbuf1);    
    printscreen(2,lcdbuf2);
    printscreen(3,lcdbuf3);      
    
  }
  else if(currentoption=='d')//compass mode
  { 
    float heading = CompassRead();// output is xxx.xxx
    heading=heading+Magoffset;
 
    sprintf(lcdbuf1,"Mag Head: %-3d",(int)heading);//int to char array
    
    int pp = CompassPitch();
    int rolll=CompassRoll();
    sprintf(lcdbuf2,"Pitc: %-4d Rol: %-4d",pp, rolll);
    
    sprintf(lcdbuf3,"Mag Offset: %-3d",(int)Magoffset);
    
    printscreen(0, lcdbuf0);
    printscreen(1, lcdbuf1);
    printscreen(2, lcdbuf2);
    printscreen(3, lcdbuf3);
    delay(400);
    
  }
  else if(currentoption=='e')//compasss calibration mode
  {
    printscreen(0,lcdbuf0);
    CompassCalibrate();    
    buttonnumber=4;    
  }
  else if(currentoption=='f')//show message
  {    
    ShowMessage(MssgForm);
    printscreen(0,lcdbuf0);
    printscreen(1,lcdbuf1);
    printscreen(2,lcdbuf2);
    printscreen(3,lcdbuf3);
  }
  else if(currentoption=='g')//show gps data
  {
    gpsread();
    
    if(gps.location.isValid())
    {
      CurForm.Longitude=gps.location.lng();

      CurForm.Lattitude=gps.location.lat();//float char

      if(CurForm.Lattitude<0)
      {
        sprintf(lcdbuf1,"S:%-3.5f",-1*CurForm.Lattitude);//int to char array 
      }
      else
      {
        sprintf(lcdbuf1,"N:%-3.5f",CurForm.Lattitude);//int to char array
      }
    
      if(CurForm.Longitude<0)
      {
        
        sprintf(lcdbuf2,"W:%-3.5f",-1*CurForm.Longitude);//int to char array 
      }
      else
      {
        sprintf(lcdbuf2,"E:%-3.5f",CurForm.Longitude);//int to char array
      }        
    }

    else
    {
      CurForm.Longitude=0;
      CurForm.Lattitude=0;
      String title="No Valid GPS Data  ";
      title.toCharArray(lcdbuf1,21);
      title="                    ";
      title.toCharArray(lcdbuf3,21);
      title.toCharArray(lcdbuf2,21);
      
              
    }
    
    TinyGPSTime t=gps.time;
    
      
      if (t.isValid())
      {
        sprintf(lcdbuf3, "%02d:%02d:%02d     Sat:%02d", t.hour(), t.minute(), t.second(),gps.satellites.value());
      }      
    
    else{
      String title="      No time       ";
      title.toCharArray(lcdbuf3,21);
    }
      printscreen(0,lcdbuf0);
      printscreen(1,lcdbuf1);    
      printscreen(2,lcdbuf2);
      printscreen(3,lcdbuf3);      
  }
}
////////////////////////////////////////////////
void Button1() { 
    if((long)(micros() - last_micros) >= debouncing_time * 1000) 
    {
     // buttonpushed=true;
      buttonnumber=1;
    }
    last_micros = micros();
}
///////////////////////////////////////////////
void Button2() {
    if((long)(micros() - last_micros) >= debouncing_time * 1000) 
    {
      //buttonpushed=true;
      buttonnumber=2;
    }
    last_micros = micros();
}
///////////////////////////////////////////////
void Button3() {
    if((long)(micros() - last_micros) >= debouncing_time * 1000) 
    {
      //buttonpushed=true;
      buttonnumber=3;
    }
    last_micros = micros();
}
///////////////////////////////////////////////
void Button4() {
    if((long)(micros() - last_micros) >= debouncing_time * 1000) 
    {
      //buttonpushed=true;
      buttonnumber=4;
    }
    last_micros = micros();
}
///////////////////////////////////////////////
void Button5() {
    if((long)(micros() - last_micros) >= debouncing_time * 1000) 
    {
      //buttonpushed=true;
      buttonnumber=5;
    }
    last_micros = micros();
}
///////////////////////////////////////////////

char Buttonhandler(int buttonnum, char currentopt)
{
  String b1="                    ";
  
  if(buttonnum>0)
  {
    switch (buttonnum) 
    {

    case 1://default radio mode
     // delay(5000);
     lcd.clear();
     FirstShowing("Connecting        ",b1,b1,b1);
      return 'a';
      break;
      
    case 2://radio request resend if radio deafult is selected
      if(currentopt=='a')
      {
        lcd.clear();
        FirstShowing("Connecting        ",b1,b1,b1);
        return 'b';  
      }
      else
      {
        lcd.clear();
        FirstShowing("Connecting        ",b1,b1,b1);
        return currentopt;  
      }
      break;
      
    case 3://gps one mode
      lcd.clear();
      if(currentopt=='c')
      {
        
        FirstShowing("      GPS MODE        ",b1,b1,b1);
        return 'g';  
      }
      else
      {
        //String t1=" Main Nav Mode   ";     
        FirstShowing(" Main Nav Mode   ",b1,b1,b1);
        //GPSoneModeFirst();
        //Serial1.flush();
        return 'c';
      }
      break;
      
    case 4://compass centered mode, then calibration mode, then calibration exit 
      
      if(currentopt=='d')
      {
        FirstShowing("Compass Calibration",b1,b1,b1);
        lcd.clear();
        return 'e';//opton already selected go into calibration mode
      }
      else
      {
        FirstShowing("   Compass Mode      ","Mag Head:           ","Pitc:     Rol:      ",b1);
        lcd.clear();
        return 'd';//option is not already selected, choose first part
      }
            
      break;
      
    case 5://view full received message
      FirstShowing("  Received Message  ",b1,b1,b1);
      lcd.clear();
      return 'f';
      break;
      
    default:
      // statements
      break;    
    }
  }
  else
  {
    return currentopt;
  }  
}
/////////////////////////////////////////////////
bool arraychecker(uint8_t buf2[], uint8_t good[])
{
  for(int i=0;i<5;i++)
  {
    if(buf2[i]==good[i])
    {
    }
    else
    {
      return false;
    }
  }
  return true;
}

/////////////////////////////////////////////
char StatusChecker()
{
  char updates;
    delay(100);
    //printscreen(3,"hh");
   if (rf95.waitAvailableTimeout(1000))
   {
      uint8_t superbuf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(superbuf);
  
      
      if (rf95.recv(superbuf, &len))
      {
 
        if(superbuf[1]=='1')
        {
          updates='U';
          lcd.setCursor(12,0);
          lcd.print("RX:");
          lcd.setCursor(16,0);
          int rssi=(int)rf95.lastRssi();
          lcd.print((String)rssi);
          //"UpdateReady";
          return updates;
        }
        else
        {
          lcd.setCursor(12,0);
          lcd.print("RX:");
          updates='N';
          lcd.setCursor(16,0);
          int rssi=(int)rf95.lastRssi();
          lcd.print((String)rssi);
          //"NotReady", no new updates
          return updates;
        }
      }
      else
      {
        updates='R';
        //"ReceptionError";        
        return updates;
      }
   }
   else
   {
    updates='O';
    //"NothingHeard";
    return updates;
   }
}

/////////////////////////////////////////////////
void SendSend()
{
  char sendd[9]="sendsend";
  //buf1[] = buf;
  rf95.send((uint8_t *)sendd, 9);
  rf95.waitPacketSent();
  //Serial.println("Sent a send request");
}
////////////////////////////////////////////////
bool GetCoordinates()
{
  if (rf95.waitAvailableTimeout(900))
  {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t buf1[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t buf2[5];
    uint8_t len = sizeof(buf);
    uint8_t len1 = sizeof(buf1);
    uint8_t len2 = sizeof(buf2);

    if (rf95.recv(buf, &len))
    {
      // Send back same thing
      for(int i = 0; i < 35; i++)
      {
        buf1[i]=buf[i];
      }
      //buf1[] = buf;
      rf95.send(buf1, sizeof(buf1));
      rf95.waitPacketSent();
      //Serial.println("Sent a check");
      
      if (rf95.waitAvailableTimeout(1000)) // wait to see if check is good or bad
      {
      
        //receive good or bad
        if(rf95.recv(buf2, &len2) && arraychecker(buf2, (uint8_t *)good))// check is good, buf is valid packet 
        {  
            for(int c = 0; c<36; c++ )//copy buf into global coordinates array
            {
              usingcoordinates[c] = buf[c];
            }
            return true;
         }
         else//bad check received
         {
            //Serial.println("did not get a good check back");
            return false;
         }
      }
      
      else//check not received
      {
        //Serial.println("Check Receive failed");
      }
      digitalWrite(LED, LOW);
      return false;
    }
    else
    {
      //Serial.println("Nothing Received");
      return false;
    }
  }

  
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void ScreenInit()
{
  lcd.init();  //initialize the lcd
  lcd.backlight();  //open the backlight 
   
  
  lcd.setCursor ( 0, 0 );            // go to the top left corner
  lcd.print("    HARP NAV SYS    "); // write this string on the top row
  lcd.setCursor ( 0, 1 );            // go to the 2nd row
  lcd.print("      Ver 0.9      "); // pad string with spaces for centering
  lcd.setCursor ( 0, 2 );            // go to the third row
  lcd.print(" By: Michael Gentry "); // pad with spaces for centering
  lcd.setCursor ( 0, 3 );            // go to the fourth row
  lcd.print("                ");
  delay(3000);
  lcd.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
float CompassRead()
{
  compass.read();
  float heading = compass.heading();
  return heading;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
char CompassCalibrate()//enters compass calibration mode nad exits when a button is pressed, returning to previous compass menu
{

  LSM303::vector<int16_t> running_min = {32767, 32767, 32767}, running_max = {-32768, -32768, -32768};
  
  
  while(buttonnumber==0)
  {
    compass.read();
    
    running_min.x = min(running_min.x, compass.m.x);
    running_min.y = min(running_min.y, compass.m.y);
    running_min.z = min(running_min.z, compass.m.z);
  
    running_max.x = max(running_max.x, compass.m.x);
    running_max.y = max(running_max.y, compass.m.y);
    running_max.z = max(running_max.z, compass.m.z);

    snprintf(lcdbuf1, sizeof(lcdbuf1), "X: {%-6d, %-6d}",
    running_min.x, running_max.x);
    printscreen(1,lcdbuf1);
    
    snprintf(lcdbuf2, sizeof(lcdbuf2), "Y: {%-6d, %-6d}",
    running_min.y, running_max.y);
    printscreen(2,lcdbuf2);
    
    snprintf(lcdbuf3, sizeof(lcdbuf3), "Z: {%-6d, %-6d}",
    running_min.z, running_max.z);  
    printscreen(3,lcdbuf3);              
  }
  compass.m_min = running_min;
  compass.m_max = running_max;
  //currentopt='d';
  return 'd';//return to compass option
}
////////////////////////////////////////////////////////////////////////////////////////////////
int CompassPitch()
{
  float pitchh=-(atan2(compass.a.x, sqrt(compass.a.y*compass.a.y + compass.a.z*compass.a.z))*180.0)/M_PI;
  
  return (int) pitchh;
}
////////////////////////////////////////////////////////////////////////////////////////////////
int CompassRoll()
{
  float rolll=(atan2(compass.a.y, compass.a.z)*180.0)/M_PI;
  
  return (int) rolll;
}
////////////////////////////////////////////////////////////////////////////////////////////////
void gpsread()
{
    while (Serial1.available() > 0)
    {
      if (gps.encode(Serial1.read()))//if true, a sentence has been correctly encoded into gps library structs
      {
        
      }
    }
  
}
//////////////////////////////////////////////////////////////////////////////////////////////
void MessageParse()
{
  //get time from usingcoordinates[c] xx99xxh,3249.61N,08338.97W,A=000000
  MssgForm.messgetime[0] = usingcoordinates[0];
  MssgForm.messgetime[1] = usingcoordinates[1];
  MssgForm.messgetime[3] = usingcoordinates[2];
  MssgForm.messgetime[4] = usingcoordinates[3];
  MssgForm.messgetime[6] = usingcoordinates[4];
  MssgForm.messgetime[7] = usingcoordinates[5];
  MssgForm.messgetime[2] = ':';
  MssgForm.messgetime[5] = ':';
  MssgForm.messgetime[8] = '\n';
  
  /// get altitude
  for(int i = 0;i<6 ; i++)
  {
    MssgForm.St_Altitude[i] = usingcoordinates[i+29];
  }
  sscanf(MssgForm.St_Altitude, "%d",&(MssgForm.Altitude));//put array in int

  //get lattitude 3249.61N
  //DD = d + (min/60) + (sec/3600)
  float degree =((float)(usingcoordinates[8]-'0'))*10+(float)(usingcoordinates[9]-'0');
  float minutes = ((float)(usingcoordinates[10]-48))*10+(float)(usingcoordinates[11]-48)+((float)(usingcoordinates[13]-48))/10+((float)(usingcoordinates[14]-48))/100;
  float latdec=degree+(minutes/60);
  
  if(usingcoordinates[15]=='S')
  {
    latdec=latdec*(-1);
  }
  
  MssgForm.Lattitude=latdec;
  gcvt(latdec,8,(MssgForm.lattitude));

    //get longtitude 08338.97W, format is dd+mm.mm
  //DD = d + (min/60) + (sec/3600)
  degree =((float)(usingcoordinates[17]-'0')*100+(float)(usingcoordinates[18]-'0'))*10+(float)(usingcoordinates[19]-'0');
  minutes = ((float)(usingcoordinates[20]-48))*10+(float)(usingcoordinates[21]-48)+((float)(usingcoordinates[23]-48))/10+((float)(usingcoordinates[24]-48))/100;
  latdec=degree+(minutes/60);
  
  if(usingcoordinates[25]=='W')
  {
    latdec=latdec*(-1);
  }
  
  MssgForm.Longitude=latdec;
  gcvt(latdec,8,(MssgForm.longitude));
  
}
/////////////////////////////////////////////////////////////////////////////////
void printscreen(int i, String message)// go to row i and print string message
{
  lcd.setCursor ( 0, i );            // go to the top left corner
  lcd.print(message); // write this string on the top row
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void printcharscreen(int linerow, char message[], int cursorstart, int charstart, int charlength)// go to row i and print string message
{
  for(int x = cursorstart; x < ( cursorstart + charlength ) ; x++)
  {
    
    lcd.setCursor ( x , linerow );
    lcd.print((String)message[charstart]);
    charstart++;   
  }
  //lcd.print(message); // write this string on the top row
}
///////////////////////////////////////////////////////////////////
/*void RadiomodeFirst()
{
  String title="      Radio Mode      ";
  title.toCharArray(lcdbuf0, 21); 
}*/
////////////////////////////////////////////////////////////////////
void FirstShowing(String line0, String line1, String line2, String line3)
{
  line0.toCharArray(lcdbuf0,21);
  line1.toCharArray(lcdbuf1,21);
  line2.toCharArray(lcdbuf2,21);
  line3.toCharArray(lcdbuf3,21);  
}
///////////////////////////////////////////////////////////////////////
void ShowMessage(MessageForm MssgForm1)
{
  
  String title="  Received Message  ";
  title.toCharArray(lcdbuf0 ,21);
  
  title="                    ";
  title.toCharArray(lcdbuf1 ,21); //clear out buffer
  
  char alt[7]; //A:xxxx    
  sprintf(alt,"A:%-4d",MssgForm1.Altitude);//int to char array
  for(int i=0;i<6;i++)
  {
    lcdbuf1[i]=alt[i];
  }
    for(int i=0;i<8;i++)
  {
    lcdbuf1[i+8]=MssgForm1.messgetime[i];
  }

  if(MssgForm1.Lattitude<0)
  {
    sprintf(lcdbuf2,"S:%-3.5f",-1*MssgForm1.Lattitude);//int to char array 
  }
  else
  {
    sprintf(lcdbuf2,"N:%-3.5f",MssgForm1.Lattitude);//int to char array
  }

  if(MssgForm1.Longitude<0)
  {
    
    sprintf(lcdbuf3,"W:%-3.5f",-1*MssgForm1.Longitude);//int to char array 
  }
  else
  {
    sprintf(lcdbuf3,"E:%-3.5f",MssgForm1.Longitude);//int to char array
  }  

  
}
/////////////////////////////////////////////////////////////////////////
float bearing(float lat,float lon,float lat2,float lon2){

    float teta1 = radians(lat);
    float teta2 = radians(lat2);
    float delta1 = radians(lat2-lat);
    float delta2 = radians(lon2-lon);

    //==================Heading Formula Calculation================//

    float y = sin(delta2) * cos(teta2);
    float x = cos(teta1)*sin(teta2) - sin(teta1)*cos(teta2)*cos(delta2);
    float brng = atan2(y,x);
    brng = degrees(brng);// radians to degrees
    brng = ( ((int)brng + 360) % 360 ); 

    //Serial.print("Heading GPS: ");
    //Serial.println(brng);

    return brng;
}
////////////////////////////////////////////////////////////////////////
