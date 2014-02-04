/*  RGB Pong Clock - Andrew Holmes @pongclock
**  Inspired by, and shamelessly derived from 
**      Nick's LED Projects
**  https://123led.wordpress.com/about/
**  
**  Videos of the clock in action:
**  https://vine.co/v/hwML6OJrBPw
**  https://vine.co/v/hgKWh1KzEU0
**  https://vine.co/v/hgKz5V0jrFn
**  I run this on a Mega 2560, your milage on other chips may vary,
**  Can definately free up some memory if the bitmaps are shrunk down to size.
**  Uses an Adafruit 16x32 RGB matrix availble from here:
**  http://www.phenoptix.com/collections/leds/products/16x32-rgb-led-matrix-panel-by-adafruit
**  This microphone:
**  http://www.phenoptix.com/collections/adafruit/products/electret-microphone-amplifier-max4466-with-adjustable-gain-by-adafruit-1063
**  a DS1307 RTC chip (not sure where I got that from - was a spare)
**  and an Ethernet Shield
**  http://hobbycomponents.com/index.php/dvbd/dvbd-ardu/ardu-shields/2012-ethernet-w5100-network-shield-for-arduino-uno-mega-2560-1280-328.html
** 
*/

#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <Adafruit_GFX.h>   // Core graphics library
#include <RGBmatrixPanel.h> // Hardware-specific library
#include <Time.h>
#include <Wire.h>  
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include <Button.h>
#include <ffft.h>
#include <math.h>
#include <EEPROM.h>
#include "blinky.h"
#include "font3x5.h"
#include "font5x5.h"


#define CLK 12  //Used to be 50!!!  // MUST be on PORTB! //used to work on 11
#define LAT A3
#define OE  8  //used to be 51!!
#define A   A4
#define B   A1
#define C   A2

#define ADC_CHANNEL 0

#define BAT1_X 2                         // Pong left bat x pos (this is where the ball collision occurs, the bat is drawn 1 behind these coords)
#define BAT2_X 28        

#define SHOWCLOCK 5000  

#define MAX_CLOCK_MODE 6                 // Number of clock modes

#define X_MAX 31                         // Matrix X max LED coordinate (for 2 displays placed next to each other)
#define Y_MAX 15                         

//Setup networking
//Had trouble with DHCP, so sticking with static
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip( 192,168,0,210 );
IPAddress _dns( 192,168,0,2 );
IPAddress gateway( 192,168,0,1 );
IPAddress subnet( 255,255,255,0 );

char buffer[512];

//Weather Stuff
IPAddress server(192,168,0,73);
char weatherInput[200];
int stringPos;
boolean startRead=false;
EthernetClient client;
boolean weatherGood=false;
char w_temp[8][7];
char w_id[8][4];
char w_desc[8][10];
boolean wasWeatherShownLast= true;
long lastWeatherTime =0;
//int rtc[7];

// Last parameter = 'true' enables double-buffering, for flicker-free,
// buttery smooth animation.  Note that NOTHING WILL SHOW ON THE DISPLAY
// until the first call to swapBuffers().  This is normal.
RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, true);

Button buttonA = Button(3,BUTTON_PULLUP_INTERNAL);       // Setup button A (using button library)
Button buttonB = Button(2,BUTTON_PULLUP_INTERNAL);       // Setup button B (using button library)

int mode_changed = 0;                    // Flag if mode changed.
int clock_mode = 0;                      // Default clock mode (1 = pong)

int random_mode = 1;
int mode_time_up;         

int powerPillEaten = 0;

char   str[]      = "Adafruit 16x32 RGB LED Matrix";
char   textTime[] = "HH:MM:SS";
int    textX   = matrix.width(),
textMin = sizeof(textTime) * -12;
long   hue     = 0;


//Spectrum Analyser
int16_t       capture[FFT_N];    // Audio capture buffer
complex_t     bfly_buff[FFT_N];  // FFT "butterfly" buffer
uint16_t      spectrum[FFT_N/2]; // Spectrum output buffer
//uint16_t spectrum[FHT_N];
volatile byte samplePos = 0;     // Buffer position counter

byte
peak[32],      // Peak level of each column; used for falling dots
dotCount = 0, // Frame counter for delaying dot-falling speed
colCount = 0; // Frame counter for storing past column data
int
col[32][10],   // Column levels for the prior 10 frames
minLvlAvg[32], // For dynamic adjustment of low & high ends of graph,
maxLvlAvg[32], // pseudo rolling averages for the prior few frames.
colDiv[32];    // Used when filtering FFT output to 8 columns

PROGMEM uint8_t
// This is low-level noise that's subtracted from each FFT output column:
noise[64]={ 
  8,6,6,5,3,4,4,4,3,4,4,3,2,3,3,4,
  2,1,2,1,3,2,3,2,1,2,3,1,2,3,4,4,
  3,2,2,2,2,2,2,1,3,2,2,2,2,2,2,2,
  2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,4 }
,
// These are scaling quotients for each FFT output column, sort of a
// graphic EQ in reverse.  Most music is pretty heavy at the bass end.
eq[64]={
  255, 175,218,225,220,198,147, 99, 68, 47, 33, 22, 14,  8,  4,  2,
  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 }
,

col0data[] = {  
  2,  1,  // # of spectrum bins to merge, index of first
  111,   8 }
,           // Weights for each bin
col1data[] = {  
  4,  1,  // 4 bins, starting at index 1
  19, 186,  38,   2 }
, // Weights for 4 bins.  Got it now?
col2data[] = {  
  5,  2,
  11, 156, 118,  16,   1 }
,
col3data[] = {  
  8,  3,
  5,  55, 165, 164,  71,  18,   4,   1 }
,
col4data[] = { 
  11,  5,
  3,  24,  89, 169, 178, 118,  54,  20,   6,   2,   1 }
,
col5data[] = { 
  17,  7,
  2,   9,  29,  70, 125, 172, 185, 162, 118, 74,
  41,  21,  10,   5,   2,   1,   1 }
,
col6data[] = { 
  25, 11,
  1,   4,  11,  25,  49,  83, 121, 156, 180, 185,
  174, 149, 118,  87,  60,  40,  25,  16,  10,   6,
  4,   2,   1,   1,   1 }
,
col7data[] = { 
  37, 16,
  1,   2,   5,  10,  18,  30,  46,  67,  92, 118,
  143, 164, 179, 185, 184, 174, 158, 139, 118,  97,
  77,  60,  45,  34,  25,  18,  13,   9,   7,   5,
  3,   2,   2,   1,   1,   1,   1 }
,
// And then this points to the start of the data for each of the columns:
*colData[] = {
  col0data, col1data, col2data, col3data,
  col4data, col5data, col6data, col7data };
//Spectrum Analyser End


void setup() {
  pinMode(A10,OUTPUT);
  digitalWrite(A10,HIGH);

  matrix.begin();
  matrix.setTextWrap(false); // Allow text to run off right edge
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(210, 210, 210));

  randomSeed(analogRead(8));
  //setTime(10,12,0,13,9,2013);

  //  delay(1000);

  Serial.begin(9600);

  delay(500);

  while(!Serial);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if(timeStatus()!= timeSet) 
    Serial.println("Unable to sync with the RTC");
  else
    Serial.println("RTC has set the system time");      

  mode_time_up = hour();

  //spectrum analyser
  uint8_t i, j, nBins, binNum, *data;

  memset(peak, 0, sizeof(peak));
  memset(col , 0, sizeof(col));

  for(i=0; i<32; i++) {
    minLvlAvg[i] = 0;
    maxLvlAvg[i] = 512;
  }



  analogReference(EXTERNAL);
  // Init ADC free-run mode; f = ( 16MHz/prescaler ) / 13 cycles/conversion 
  ADMUX  = ADC_CHANNEL; // Channel sel, right-adj, use AREF pin
  ADCSRA = _BV(ADEN)  | // ADC enable
  _BV(ADSC)  | // ADC start
  _BV(ADATE) | // Auto trigger
  _BV(ADIE)  | // Interrupt enable
  _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 128:1 / 13 = 9615 Hz
  ADCSRB = 0;                // Free run mode, no high MUX bit
  DIDR0  = 1 << ADC_CHANNEL; // Turn off digital input for ADC pin
  TIMSK0 = 1;                // Timer0 off

  sei(); // Enable interrupts
  //Spectrum Analyser End
  pacMan();
  setup_ethernet();
}

void setup_ethernet()
{
  Serial.begin(9600);

  // disable w5100 while setting up SD
  pinMode(10,OUTPUT);
  digitalWrite(10,HIGH);

  Serial.print("Starting SD..");
  if(!SD.begin(4)) Serial.println("failed");
  else Serial.println("ok");

  Ethernet.begin(mac, ip, _dns, gateway, subnet);
  digitalWrite(10,HIGH);

  delay(2000);

  Serial.println("Ready");
  Serial.println(Ethernet.localIP());

  delay(500);

  quickWeather();
}

void loop(){
  //pong();
  //normal_clock();
  if (random_mode){  
    //gettime();
    //set counter to change clock type every 3 or 4 hours
    if (mode_time_up == hour()) {
      mode_time_up = hour() + random (2,4);   //set next time to change - add 2 to 3 hours
      if (mode_time_up >= 24) { 
        mode_time_up = random (1,2); 
      }   //if time is over 24, set to 0 + random
      clock_mode = random(0,MAX_CLOCK_MODE - 1);     //pick new random mode
    }
  }

  //reset clock type clock_mode
  switch (clock_mode){
  case 0: 
    normal_clock(); 
    break; 
  case 1: 
    pong(); 
    break;
  case 2: 
    word_clock(); 
    break;
  case 3: 
    jumble(); 
    break; 
  case 4: 
    spectrumDisplay();
    break;
  case 6: 
    set_time(); 
    break;
    //    case 4: binary1(); break;
    //    case 5: binary2(); break;
  }

  //if the mode hasn't changed, show the date
  pacClear();
  if (mode_changed == 0) { 
    //fade_down();
    display_date(); 
    pacClear();
    //fade_down();
  } 
  else {
    //the mode has changed, so don't bother showing the date, just go to the new mode.
    mode_changed = 0; //reset mdoe flag.
  }



}


//*****************Weather Stuff*********************

void quickWeather(){
  getWeather();
  if(weatherGood){
    showWeather();
  }
  else{
    cls();
    matrix.drawPixel(0,0,matrix.Color333(1,0,0));
    matrix.swapBuffers(true);
    delay(1000);
  }
}

void getWeather(){
  Serial.println("in getWeather");
  if(client.connect(server,1337)){
    Serial.println("connected");
    client.println("WEATHER");
    weatherGood=readWeather();
  }
  else{
    Serial.println("didnt connect");
    weatherGood = false;
  }
  if(weatherGood){
    lastWeatherTime = millis();
    processWeather(); 
  }
}

boolean readWeather(){
  Serial.println("in readWeather");
  memset(&weatherInput,0,200);
  stringPos =0;
  long startTime = millis();
  while(millis()<(startTime+30000)){
    if(client.available()){
      char c = client.read();
      if (c == '<' ) { //'<' is our begining character
        startRead = true; //Ready to start reading the part 
      }
      else if(startRead){

        if(c != '>'){ //'>' is our ending character
          Serial.print(c);
          weatherInput[stringPos] = c;
          stringPos ++;
        }
        else{
          //got what we need here! We can disconnect now
          startRead = false;
          client.stop();
          client.flush();
          Serial.println("disconnecting.");
          return true;
        }
      }
    }
  }
  return false;
}

void processWeather(){
  Serial.println("in process weather");
  memset(&w_temp,0,8*7);
  memset(&w_id,0,8*4);
  memset(&w_desc,0,8*10);
  int dayCounter =0;
  int itemCounter = 0;
  char tempString[10];
  int tempStringLoc=0;
  boolean dropChar = false;
  for (int i=0; i<stringPos; i++){
    if(weatherInput[i]=='~'){
      itemCounter++;
      tempStringLoc = 0;
      dropChar = false;
      if(itemCounter>2){
        dayCounter++;
        itemCounter=0;
      }
    }
    else if(weatherInput[i]=='.'){
      //if we get a . we want to drop all characters until the next ~
      dropChar=true;
    }
    else{
      if(!dropChar){
        switch(itemCounter){
        case 0:
          w_temp[dayCounter][tempStringLoc++] = weatherInput[i];
          break;
        case 1:
          w_id[dayCounter][tempStringLoc++] = weatherInput[i];
          break;
        case 2:
          w_desc[dayCounter][tempStringLoc++] = weatherInput[i];
          break;
        }
      }
    }
  }
}

void showWeather(){
  byte dow = weekday()-1;
  char daynames[7][4]={
    "Sun", "Mon","Tue", "Wed", "Thu", "Fri", "Sat"
  };

  for(int i = 0 ; i<8; i++){

    int numTemp = atoi(w_temp[i]);
    //fix within range to generate colour value
    if (numTemp<-14) numTemp=-10;
    if (numTemp>34) numTemp =30;
    //add 14 so it falls between 0 and 48
    numTemp = numTemp +14;
    //divide by 3 so value between 0 and 16
    numTemp = numTemp / 3;

    int tempColor;
    if(numTemp<8){
      tempColor = matrix.Color444(0,tempColor/2,7);
    }
    else{
      tempColor = matrix.Color444(7,(7-numTemp/2) ,0); 
    } 

    cls();

    //Display the day on the top line.
    if(i==0){
      drawString(2,2,"Now",51,matrix.Color444(1,1,1));
    }
    else{
      drawString(2,2,daynames[(dow+i-1) % 7],51,matrix.Color444(0,1,0));
      Serial.println(daynames[(dow+i-1)%7]);
    }

    //put the temp underneath
    boolean positive = !(w_temp[i][0]=='-');
    for(int t=0; t<7; t++){
      if(w_temp[i][t]=='-'){
        //matrix.drawLine(3,10,3,0,matrix.Color333(1,1,1));
        matrix.drawLine(3,10,4,10,tempColor);
      }
      else if(!(w_temp[i][t]==0)){
        //vectorNumber(w_temp[i][t]-'0',t*4+2+(positive*2),8,matrix.Color333(1,1,1),1,1);
        vectorNumber(w_temp[i][t]-'0',t*4+2+(positive*2),8,tempColor,1,1);
      }
    }

    matrix.swapBuffers(true);
    drawWeatherIcon(16,0,atoi(w_id[i]));  

  }
}

void drawWeatherIcon(uint8_t x, uint8_t y, int id){
  long start = millis();
  static int rain[12];
  for(int r=0; r<13; r++){
    rain[r]=random(9,18);
  }
  int rainColor = matrix.Color333(0,0,1);
  byte intensity=id-(id/10)*10 + 1;
  
  int deep =0;
  boolean raining = false;
  Serial.println(id);
  Serial.println(freeRam());
  Serial.println(intensity);

  while(millis()<start+5000){
    
    //Serial.println(freeRam());
    switch(id/100){
    case 2:
      //Thunder
      matrix.fillRect(x,y,16,16,matrix.Color333(0,0,0));
      matrix.drawBitmap(x,y,cloud_outline,16,16,matrix.Color333(1,1,1));
      if(random(0,10)==3){
        int pos = random(-5,5);
        matrix.drawBitmap(pos+x,y,lightning,16,16,matrix.Color333(1,1,1));
      }
      //rainColor = matrix.Color333(0,1,1);
      raining = true;
      break;
    case 3:  
      //drizzle
      matrix.fillRect(x,y,16,16,matrix.Color333(0,0,0));
      matrix.drawBitmap(x,y,cloud,16,16,matrix.Color333(1,1,1));
      raining=true;
      break;
    case 5:
      //rain was 5

      matrix.fillRect(x,y,16,16,matrix.Color333(0,0,0));
      
      if(intensity<3){
        
        matrix.drawBitmap(x,y,cloud,16,16,matrix.Color333(1,1,1));
      }
      else{
        matrix.drawBitmap(x,y,cloud_outline,16,16,matrix.Color333(1,1,1));
      }
      raining = true;
      //Serial.println(intensity);
      break;
    case 6:
      //snow was 6
      rainColor = matrix.Color333(4,4,4);
      matrix.fillRect(x,y,16,16,matrix.Color333(0,0,0));
      
      deep = (millis()-start)/500;
      if(deep>6) deep=6;
      //Serial.println("Deep");
      //Serial.println(deep);
      if(intensity<3){
        matrix.drawBitmap(x,y,cloud,16,16,matrix.Color333(1,1,1));
        matrix.fillRect(x,y+16-deep/2,16,deep/2,rainColor);
      }
      else{
        matrix.drawBitmap(x,y,cloud_outline,16,16,matrix.Color333(1,1,1));
        matrix.fillRect(x,y+16-(deep),16,deep,rainColor);
      }
      raining = true;
      //Serial.println("at break");
      break;  
    case 7:
      //athmosphere
      matrix.drawRect(x,y,16,16,matrix.Color333(1,0,0));
      drawString(x+2,y+6,"FOG",51,matrix.Color333(1,1,1));
      //matrix.drawBitmap(x,y,cloud,16,16,matrix.Color333(1,1,1));

      break;
    case 8:
      //cloud
      matrix.fillRect(x,y,16,16,matrix.Color333(0,0,1));
      if(id==800){
        matrix.drawBitmap(x,y,big_sun,16,16,matrix.Color333(2,2,0));
      }
      else{
        if(id==801){
          matrix.drawBitmap(x,y,big_sun,16,16,matrix.Color333(2,2,0));
          matrix.drawBitmap(x,y,cloud,16,16,matrix.Color333(1,1,1));
        }
        else{
          if(id==802 || id ==803){
            matrix.drawBitmap(x,y,small_sun,16,16,matrix.Color333(1,1,0));
          }
          matrix.drawBitmap(x,y,cloud,16,16,matrix.Color333(1,1,1));
          matrix.drawBitmap(x,y,cloud_outline,16,16,matrix.Color333(0,0,0));
        }
      }
      //matrix.drawBitmap(x,y,cloud_outline,16,16,matrix.Color333(7,7,7));
      break;
    case 9:
      //extreme
      matrix.fillRect(x,y,16,16,matrix.Color333(0,0,0));
      matrix.drawRect(x,y,16,16,matrix.Color333(7,0,0));
      if(id==906){
        raining =true; 
        intensity=3;
        matrix.drawBitmap(x,y,cloud,16,16,matrix.Color333(1,1,1));
      };
      break;
    default:
      matrix.fillRect(x,y,16,16,matrix.Color333(0,1,1));
      matrix.drawBitmap(x,y,big_sun,16,16,matrix.Color333(2,2,0));
      break;    
    }
    if(raining){
      for(int r = 0; r<13; r++){

        matrix.drawPixel(x+(r)+2, rain[r]++, rainColor);
        if(rain[r]==20)rain[r]=9;

      }
    } 
    matrix.swapBuffers(false);
    delay(( 50 -( intensity * 10 )) < 0 ? 0: 50-intensity*10);
  }
}
//*****************End Weather Stuff*********************

void scrollBigMessage(char *m){
  for(int i = 32; i>-24; i--){
    cls();
    matrix.setCursor(i,1);
    matrix.setTextSize(1);
    matrix.setTextColor(matrix.Color444(1,1,1));
    matrix.print(m);
    matrix.setCursor(1,8);
    matrix.print(i);
    matrix.swapBuffers(true);
    delay(50); 
  }

}

void scrollMessage(char* top, char* bottom ,uint8_t top_font_size,uint8_t bottom_font_size, uint16_t top_color, uint16_t bottom_color){

  for(int i=32; i>(strlen(top)>strlen(bottom)?strlen(top):strlen(bottom))*-1; i--){
    cls();
    drawString(i,1,top,top_font_size, top_color);
    drawString(i,8,bottom, bottom_font_size, bottom_color);
    matrix.swapBuffers(true);
    delay(100);
  }

}

//Runs pacman or other animation, refreshes weather data
void pacClear(){

  //refresh weather if we havent had it for 20 mins
  //or the last time we had it, it was bad, 
  //or weve never had it before.
  if((millis()>lastWeatherTime+1200000) || lastWeatherTime==0 || !weatherGood) getWeather();

  if(!wasWeatherShownLast && weatherGood){
    showWeather();
    wasWeatherShownLast = true;
  }
  else{  
    wasWeatherShownLast = false;

    //Yuk - hard coded special days - should have done this differently
    if(month()==12 && day()==13){
      birthday(0);
    }
    else if(month()==3 && day()==3){
      birthday(1);
    }
    else if(month()==5 && day()==27){
      birthday(2);
    }
    else if(month()==2 && day()==25){
      birthday(3);
    }
    else if(month()==10 && day()>27){
      halloween();
    }
    else if(month()==12 && day()>5){
      christmas();
    }
    else if(month()==1 && day()<5){
      christmas();
    }
    else{
      pacMan();
    }
  }
}  

void halloween(){
  long starttime = millis();
  if(random(0,4)==2){
    while(millis()<starttime+5000){
      cls();
      if(random(0,30)==10)matrix.drawBitmap(0,0,boo,32,16,matrix.Color444(7,7,7));
      matrix.swapBuffers(true);
    }
  }
  else{
    while(millis()<starttime+5000){
      cls();
      matrix.drawBitmap(0,0,pumpkin_body,32,16,matrix.Color444(7,3,0));
      matrix.drawBitmap(0,0,pumpkin_top,32,16,matrix.Color444(0,7,0));
      //if(1==0)matrix.drawBitmap(0,0,pumpkin_ribs,32,16,matrix.Color444(0,0,0));
      matrix.drawBitmap(0,0,pumpkin_face,32,16,matrix.Color444(0,0,0));
      if(random(0,10)==3) matrix.drawBitmap(0,0,pumpkin_face,32,16,matrix.Color444(7,7,random(0,5)));
      matrix.swapBuffers(true);    
    }
  }
}

void christmas(){
  int xpos[11] = {6, 8, 8, 5, 13, 10, 9, 6, 7, 11,2};
  int ypos[11] = {1, 2, 5, 6, 8,   8, 9, 9, 11,12,13};
  long starttime = millis();
  int mode = random(0,3);
  //int mode =1;
  if(mode==0){
  	//Draw a tree
    int pos=-16;
    while(millis()<starttime+10000){
      cls();
      matrix.drawBitmap(0+pos,0, tree ,16,16,matrix.Color444(0,1,0));
      matrix.fillRect(7+pos,14,2,2,matrix.Color444(7,3,0));
      int lightColor = matrix.Color444(0,0,7);
      if(millis()/1000 %2) lightColor = matrix.Color444(7,0,2);
      for(int a=0;a<11;a++){
        matrix.drawPixel(xpos[a]+pos,ypos[a],lightColor);
      }
      matrix.swapBuffers(true);
      pos++;
      delay(200);
    }
  }
  else if(mode==1){
  	//Ho Ho Ho
    while(millis()<starttime+10000){
       int xpos=random(0,31);
       int ypos=random(0,15);
       int size=random(0,2);
       matrix.fillRect(xpos-1,ypos-1,(size==0?9:13),7,matrix.Color444(0,0,0));
       drawString(xpos,ypos,"HO",(size==0?51:53),matrix.Color444(random(0,7),random(0,7),random(0,7)));    
   
       matrix.swapBuffers(true);
       delay(100);
    }
  }
  else{
  	//scrolling present
    int pos=-16;
    while(millis()<starttime+10000){
      cls();
      matrix.fillRect(2+pos,5,12,10,matrix.Color444(1,0,3));
      matrix.drawBitmap(0+pos,0, bow ,16,16,matrix.Color444(4,4,0));
      matrix.swapBuffers(true);
      pos++;
      delay(200);
    }
  }
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void pacMan(){

  if(powerPillEaten>0){
    for(int i =32+(powerPillEaten*17); i>-17; i--){
      long nowish = millis();
      cls();

      drawPac(i,0,-1);
      if(powerPillEaten>0) drawScaredGhost(i-17,0);
      if(powerPillEaten>1) drawScaredGhost(i-34,0);
      if(powerPillEaten>2) drawScaredGhost(i-51,0);
      if(powerPillEaten>3) drawScaredGhost(i-68,0);

      matrix.swapBuffers(false);    
      while(millis()-nowish<50);  
    }
    powerPillEaten = 0;
  }
  else{  

    int hasEaten = 0;

    int powerPill = random(0,5);
    int numGhosts=random(0,4);
    if(powerPill ==0){
      if(numGhosts==0) numGhosts++;
      powerPillEaten = numGhosts;

    }

    for(int i=-17; i<32+(numGhosts*17); i++){
      cls();
      long nowish = millis();
      for(int j = 0; j<6;j++){

        if( j*5> i){

          if(powerPill==0 && j==4){
            matrix.fillCircle(j*5,8,2,matrix.Color333(7,3,0));

          }
          else{
            matrix.fillRect(j*5,8,2,2,matrix.Color333(7,3,0));
          }
        }

      }

      if(i==19 && powerPill == 0) hasEaten=1;

      drawPac(i,0,1);
      if(hasEaten == 0){
        if(numGhosts>0) drawGhost(i-17,0,matrix.Color333(3,0,3));
        if(numGhosts>1) drawGhost(i-34,0,matrix.Color333(3,0,0));
        if(numGhosts>2) drawGhost(i-51,0,matrix.Color333(0,3,3));
        if(numGhosts>3) drawGhost(i-68,0,matrix.Color333(7,3,0));
      }
      else{
        if(numGhosts>0) drawScaredGhost(i-17-(i-19)*2,0);
        if(numGhosts>1) drawScaredGhost(i-34-(i-19)*2,0);
        if(numGhosts>2) drawScaredGhost(i-51-(i-19)*2,0);
        if(numGhosts>3) drawScaredGhost(i-68-(i-19)*2,0);

      }

      matrix.swapBuffers(false);
      while(millis()-nowish<50);
    }
  }
}

void drawPac(int x, int y, int z){
  int c = matrix.Color333(3,3,0);
  if(x>-16 && x<32){
    if(abs(x)%4==0){
      matrix.drawBitmap(x,y,(z>0?pac:pac_left),16,16,c);
    }
    else if(abs(x)%4==1 || abs(x)%4==3){
      matrix.drawBitmap(x,y,(z>0?pac2:pac_left2),16,16,c);
    }
    else{
      matrix.drawBitmap(x,y,(z>0?pac3:pac_left3),16,16,c);
    }
  }
}



void drawGhost( int x, int y, int color){
  if(x>-16 && x<32){
    if(abs(x)%8>3){
      matrix.drawBitmap(x,y,blinky,16,16,color);
    }
    else{
      matrix.drawBitmap(x,y,blinky2,16,16,color);
    }
    matrix.drawBitmap(x,y,eyes1,16,16,matrix.Color333(3,3,3));
    matrix.drawBitmap(x,y,eyes2,16,16,matrix.Color333(0,0,7));
  }
}  

void drawScaredGhost( int x, int y){
  if(x>-16 && x<32){
    if(abs(x)%8>3){
      matrix.drawBitmap(x,y,blinky,16,16,matrix.Color333(0,0,7));
    }
    else{
      matrix.drawBitmap(x,y,blinky2,16,16,matrix.Color333(0,0,7));
    }
    matrix.drawBitmap(x,y,scared,16,16,matrix.Color333(7,3,2));
  }
}  


void birthday(int who){

  String person="";     
  switch(who){
  case 0:
    person = "FILOMENA";
    break;
  case 1:
    person = "ANDREW";
    break;
  case 2:
    person = "CHLOE";
    break;
  case 3        :
    person = "EMILY";
    break;
  }

  //Slide Happy Birthday in from right
  for(int i = 0;i<32; i++){
    cls();
    drawString(32-(i<16?i*2:31),1,"HAPPY",53,matrix.Color444(15-(i/2),1,(i/2)));
    drawString(32-i,8,"BIRTHDAY",51,matrix.Color444((i/2),1,15-(i/2))); 
    matrix.swapBuffers(false);
    delay(20);  
  }
  //Change Colours 
  for(int a = 0;a<16; a++){
    cls();
    drawString(1,1,"HAPPY",53,matrix.Color444((a>7?7:a),1,(15-a<7?7:15-a)));
    drawString(1,8,"BIRTHDAY",51,matrix.Color444( ( (15-a) < 7 ? 7: (15-a) ) ,1, (a>7 ? 7 :a) ) ); 
    matrix.swapBuffers(false);
    delay(20);  
  }
  delay(1000);
  //Slide Happy Birthday down off screen
  delay(1000);
  for(int a = 1;a<17; a++){
    cls();
    drawString(1,a,"HAPPY",53,matrix.Color444((a>7?7:a),1,(15-a<7?7:15-a)));
    drawString(1,7+a,"BIRTHDAY",51,matrix.Color444( ( (15-a) < 7 ? 7: (15-a) ) ,1, (a>7 ? 7 :a) ) ); 
    matrix.swapBuffers(false);
    delay(20);  
  };

  //Slide Cake up
  for(int i=17;i>-1;i--){
    cls();
    drawCake(0,i);
    matrix.swapBuffers(true);
  }


  //Background cake, scroll name

    for(int i=64; i>-200; i--){
    cls();
    drawCake(0,0);

    matrix.setCursor(i,1);
    matrix.setTextColor(matrix.Color444(0,0,15));
    matrix.setTextSize(2);
    matrix.setTextWrap(false);
    matrix.print(person);

    matrix.swapBuffers(true);

    delay(20);   
  }

  //Slide Cake up
  for(int i=0;i>-17;i--){
    cls();
    drawCake(0,i);
    matrix.swapBuffers(true);
  }


}

void drawCake(int x, int y){
  cls();
  matrix.drawBitmap(x,y,cake_main,32,16,matrix.Color444(4,1,0));  
  matrix.drawBitmap(x,y,cake_top,32,16,matrix.Color444(7,3,2));  
  matrix.drawBitmap(x,y,cake_cream,32,16,matrix.Color333(7,7,7));  
  matrix.drawBitmap(x,y,cake_cherries,32,16,matrix.Color333(7,0,0));  
  matrix.drawBitmap(x,y,cake_candle,32,16,matrix.Color333(0,2,2));  
  int r = random(0,5);
  int c = random(4,15);
  int col = matrix.Color444(c,c-3,0);
  switch(r){
  case 0:
    matrix.drawLine(16+x,y,16+x,2+y,col);
    break;
  case 1:      
    matrix.drawLine(15+x,y,16+x,2+y,col);
    break;
  case 2:      
    matrix.drawLine(17+x,y,16+x,2+y,col);
    break;
  case 3:      
    matrix.drawLine(15+x,y,16+x,2+y,col);
    break;
  case 4:      
    matrix.fillTriangle(15+x,y,16+x,2+y,15+x,2+y,col);
    break;
  case 5:      
    matrix.fillTriangle(17+x,y,16+x,2+y,17+x,2+y,col);
    break;
  }
}


void printTime(){

  char buffer[2];
  itoa(hour(),buffer,10);
  if(hour()<10){
    buffer[1]=buffer[0];
    buffer[0]='0';
  }
  textTime[0]=buffer[0];
  textTime[1]=buffer[1];

  itoa(minute(),buffer,10);
  if(minute()<10){
    buffer[1]=buffer[0];
    buffer[0]='0';
  }

  textTime[3]=buffer[0];
  textTime[4]=buffer[1];

  itoa(second(),buffer,10);
  if(second()<10){
    buffer[1]=buffer[0];
    buffer[0]='0';
  }


  textTime[6]=buffer[0];
  textTime[7]=buffer[1];  

  matrix.print(textTime);
}


void cls(){
  matrix.fillScreen(0);
}

void pong(){
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(2, 2, 2));

  float ballpos_x, ballpos_y;
  byte erase_x = 10;  //holds ball old pos so we can erase it, set to blank area of screen initially.
  byte erase_y = 10;
  float ballvel_x, ballvel_y;
  int bat1_y = 5;  //bat starting y positions
  int bat2_y = 5;  
  int bat1_target_y = 5;  //bat targets for bats to move to
  int bat2_target_y = 5;
  byte bat1_update = 1;  //flags - set to update bat position
  byte bat2_update = 1;
  byte bat1miss, bat2miss; //flags set on the minute or hour that trigger the bats to miss the ball, thus upping the score to match the time.
  byte restart = 1;   //game restart flag - set to 1 initially to setup 1st game


  cls();

  for(int i=0; i< SHOWCLOCK; i++) {
    cls();
    //draw pitch centre line
    int adjust = 0;
    if(second()%2==0)adjust=1;
    for (byte i = 0; i <16; i++) {

      if ( i % 2 == 0 ) { //plot point if an even number

        matrix.drawPixel(16,i+adjust,matrix.Color333(0,4,0));
      }
    } 

    //main pong game loop

    //flash led for seconds on arduino
    if ( (second() % 2) == 0) { 
      digitalWrite(13,HIGH);
    }
    else{ 
      digitalWrite(13,LOW); 
    }

    //check buttons  
    if(buttonA.uniquePress()){
      switch_mode();  
      //switch display mode.
      return;
    }
    if(buttonB.uniquePress()){
      display_date();
      quickWeather(); //fade_down();
      pong();
      return;
    }

    int ampm=0;
    //update score / time
    byte mins = minute();
    byte hours = hour();
    if (hours > 12) {
      hours = hours - ampm * 12;
    }
    if (hours < 1) {
      hours = hours + ampm * 12;
    }

    char buffer[3];

    itoa(hours,buffer,10);
    //fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ". 
    if (hours < 10) {
      buffer[1] = buffer[0];
      buffer[0] = '0';
    }
    //matrix.setCursor(4,1);
    //matrix.print(buffer);
    vectorNumber(buffer[0]-'0',8,1,matrix.Color333(1,1,1),1,1);
    vectorNumber(buffer[1]-'0',12,1,matrix.Color333(1,1,1),1,1);
    //      ht1632_puttinychar(14, 0, buffer[0] );
    //      ht1632_puttinychar(18, 0, buffer[1]);


    itoa(mins,buffer,10); 
    if (mins < 10) {
      buffer[1] = buffer[0];
      buffer[0] = '0';
    } 
    //      matrix.setCursor(18,1);
    //      matrix.print(buffer);
    vectorNumber(buffer[0]-'0',18,1,matrix.Color333(1,1,1),1,1);
    vectorNumber(buffer[1]-'0',22,1,matrix.Color333(1,1,1),1,1);
    //ht1632_puttinychar(28, 0, buffer[0]);
    //ht1632_puttinychar(32, 0, buffer[1]);  


    //if restart flag is 1, setup a new game
    if (restart) {

      //erase ball pos
      //matrix.drawCircle(erase_x, erase_y,1, 0);


      //set ball start pos
      ballpos_x = 16;
      ballpos_y = random (4,12);

      //pick random ball direction
      if (random(0,2) > 0) {
        ballvel_x = 1; 
      } 
      else {
        ballvel_x = -1;
      }
      if (random(0,2) > 0) {
        ballvel_y = 0.5; 
      } 
      else {
        ballvel_y = -0.5;
      }
      //draw bats in initial positions
      bat1miss = 0; 
      bat2miss = 0;
      //reset game restart flag
      restart = 0;

      //short wait for effect
      //delay(500);
    }

    //get the time from the rtc
    //gettime();

    //if coming up to the minute: secs = 59 and mins < 59, flag bat 2 (right side) to miss the return so we inc the minutes score
    //if (rtc[0] == 59 && rtc[1] < 59){
    if (second() == 59 && minute() < 59){
      bat1miss = 1;
    }
    // if coming up to the hour: secs = 59  and mins = 59, flag bat 1 (left side) to miss the return, so we inc the hours score.
    if (second() == 59 && minute() == 59){
      bat2miss = 1;
    }


    //AI - we run 2 sets of 'AI' for each bat to work out where to go to hit the ball back 

    //very basic AI...
    // For each bat, First just tell the bat to move to the height of the ball when we get to a random location.
    //for bat1
    if (ballpos_x == random(18,32)){// && ballvel_x < 0) {
      bat1_target_y = ballpos_y;
    }
    //for bat2
    if (ballpos_x == random(4,16)){//  && ballvel_x > 0) {
      bat2_target_y = ballpos_y;
    }

    //when the ball is closer to the left bat, run the ball maths to find out where the ball will land
    if (ballpos_x == 15 && ballvel_x < 0) {

      byte end_ball_y = pong_get_ball_endpoint(ballpos_x, ballpos_y, ballvel_x, ballvel_y);

      //if the miss flag is set,  then the bat needs to miss the ball when it gets to end_ball_y
      if (bat1miss == 1){
        bat1miss = 0;
        if ( end_ball_y > 8){
          bat1_target_y = random (0,3); 
        } 
        else {
          bat1_target_y = 8 + random (0,3);              
        }      
      } 
      //if the miss flag isn't set,  set bat target to ball end point with some randomness so its not always hitting top of bat
      else {
        bat1_target_y = end_ball_y - random (0, 6);        
        //check not less than 0
        if (bat1_target_y < 0){
          bat1_target_y = 0;
        }
        if (bat1_target_y > 10){
          bat1_target_y = 10;
        } 
      }
    }


    //right bat AI
    //if positive velocity then predict for right bat - first just match ball height

    //when the ball is closer to the right bat, run the ball maths to find out where it will land
    if (ballpos_x == 17 && ballvel_x > 0) {

      byte end_ball_y = pong_get_ball_endpoint(ballpos_x, ballpos_y, ballvel_x, ballvel_y);

      //if flag set to miss, move bat out way of ball
      if (bat2miss == 1){
        bat2miss = 0;
        //if ball end point above 8 then move bat down, else move it up- so either way it misses
        if (end_ball_y > 8){
          bat2_target_y = random (0,3); 
        } 
        else {
          bat2_target_y = 8 + random (0,3);
        }      
      } 
      else {
        //set bat target to ball end point with some randomness 
        bat2_target_y =  end_ball_y - random (0,6);
        //ensure target between 0 and 15
        if (bat2_target_y < 0){
          bat2_target_y = 0;
        } 
        if (bat2_target_y > 10){
          bat2_target_y = 10;
        } 
      }
    }


    //move bat 1 towards target    
    //if bat y greater than target y move down until hit 0 (dont go any further or bat will move off screen)
    if (bat1_y > bat1_target_y && bat1_y > 0 ) {
      bat1_y--;
      bat1_update = 1;
    }

    //if bat y less than target y move up until hit 10 (as bat is 6)
    if (bat1_y < bat1_target_y && bat1_y < 10) {
      bat1_y++;
      bat1_update = 1;
    }

    //draw bat 1
    if (bat1_update){
      matrix.fillRect(BAT1_X-1,bat1_y,2,6,matrix.Color333(0,0,4));
      //for (byte i = 0; i < 16; i++){
      //  if (i - bat1_y < 6 &&  i - bat1_y > -1){
      //plot(BAT1_X-1, i , 1);
      //plot(BAT1_X-2, i , 1);
      //    matrix.fillRect(BAT1_X-1,i,2,1,255);
      //  } 
      //   else {
      //          plot(BAT1_X-1, i , 0);
      //          plot(BAT1_X-2, i , 0);             
      //     matrix.fillRect(BAT1_X-1,i,2,1,0);
      //   }
      // } 
    }


    //move bat 2 towards target (dont go any further or bat will move off screen)

    //if bat y greater than target y move down until hit 0
    if (bat2_y > bat2_target_y && bat2_y > 0 ) {
      bat2_y--;
      bat2_update = 1;
    }

    //if bat y less than target y move up until hit max of 10 (as bat is 6)
    if (bat2_y < bat2_target_y && bat2_y < 10) {
      bat2_y++;
      bat2_update = 1;
    }

    //draw bat2
    if (bat2_update){
      matrix.fillRect(BAT2_X+1,bat2_y,2,6,matrix.Color333(0,0,4));
      //for (byte i = 0; i < 16; i++){
      //  if (  i - bat2_y < 6 && i - bat2_y > -1){
      //plot(BAT2_X+1, i , 1);
      //plot(BAT2_X+2, i , 1);
      //    matrix.fillRect(BAT2_X+1,i,2,1,255);  
      //} 
      //  else {
      //plot(BAT2_X+1, i , 0);
      //plot(BAT2_X+2, i , 0);
      //    matrix.fillRect(BAT2_X+1,i,2,1,0);
      //  }
      //} 
    }

    //update the ball position using the velocity
    ballpos_x =  ballpos_x + ballvel_x;
    ballpos_y =  ballpos_y + ballvel_y;

    //check ball collision with top and bottom of screen and reverse the y velocity if either is hit
    if (ballpos_y <= 0 ){
      ballvel_y = ballvel_y * -1;
      ballpos_y = 0; //make sure value goes no less that 0
    }

    if (ballpos_y >= 15){
      ballvel_y = ballvel_y * -1;
      ballpos_y = 15; //make sure value goes no more than 15
    }

    //check for ball collision with bat1. check ballx is same as batx
    //and also check if bally lies within width of bat i.e. baty to baty + 6. We can use the exp if(a < b && b < c) 
    if ((int)ballpos_x == BAT1_X+1 && (bat1_y <= (int)ballpos_y && (int)ballpos_y <= bat1_y + 5) ) { 

      //random if bat flicks ball to return it - and therefor changes ball velocity
      if(!random(0,3)) { //not true = no flick - just straight rebound and no change to ball y vel
        ballvel_x = ballvel_x * -1;
      } 
      else {
        bat1_update = 1;
        byte flick;  //0 = up, 1 = down.

        if (bat1_y > 1 || bat1_y < 8){
          flick = random(0,2);   //pick a random dir to flick - up or down
        }

        //if bat 1 or 2 away from top only flick down
        if (bat1_y <=1 ){
          flick = 0;   //move bat down 1 or 2 pixels 
        } 
        //if bat 1 or 2 away from bottom only flick up
        if (bat1_y >=  8 ){
          flick = 1;  //move bat up 1 or 2 pixels 
        }

        switch (flick) {
          //flick up
        case 0:
          bat1_target_y = bat1_target_y + random(1,3);
          ballvel_x = ballvel_x * -1;
          if (ballvel_y < 2) {
            ballvel_y = ballvel_y + 0.2;
          }
          break;

          //flick down
        case 1:   
          bat1_target_y = bat1_target_y - random(1,3);
          ballvel_x = ballvel_x * -1;
          if (ballvel_y > 0.2) {
            ballvel_y = ballvel_y - 0.2;
          }
          break;
        }
      }
    }


    //check for ball collision with bat2. check ballx is same as batx
    //and also check if bally lies within width of bat i.e. baty to baty + 6. We can use the exp if(a < b && b < c) 
    if ((int)ballpos_x == BAT2_X && (bat2_y <= (int)ballpos_y && (int)ballpos_y <= bat2_y + 5) ) { 

      //random if bat flicks ball to return it - and therefor changes ball velocity
      if(!random(0,3)) {
        ballvel_x = ballvel_x * -1;    //not true = no flick - just straight rebound and no change to ball y vel
      } 
      else {
        bat1_update = 1;
        byte flick;  //0 = up, 1 = down.

        if (bat2_y > 1 || bat2_y < 8){
          flick = random(0,2);   //pick a random dir to flick - up or down
        }
        //if bat 1 or 2 away from top only flick down
        if (bat2_y <= 1 ){
          flick = 0;  //move bat up 1 or 2 pixels 
        } 
        //if bat 1 or 2 away from bottom only flick up
        if (bat2_y >=  8 ){
          flick = 1;   //move bat down 1 or 2 pixels 
        }

        switch (flick) {
          //flick up
        case 0:
          bat2_target_y = bat2_target_y + random(1,3);
          ballvel_x = ballvel_x * -1;
          if (ballvel_y < 2) {
            ballvel_y = ballvel_y + 0.2;
          }
          break;

          //flick down
        case 1:   
          bat2_target_y = bat2_target_y - random(1,3);
          ballvel_x = ballvel_x * -1;
          if (ballvel_y > 0.2) {
            ballvel_y = ballvel_y - 0.2;
          }
          break;
        }
      }
    }

    //plot the ball on the screen
    byte plot_x = (int)(ballpos_x + 0.5f);
    byte plot_y = (int)(ballpos_y + 0.5f);

    //take a snapshot of all the led states
    //snapshot_shadowram();

    //if the led at the ball pos is on already, dont bother printing the ball.
    //if (get_shadowram(plot_x, plot_y)){
    //erase old point, but don't update the erase positions, so next loop the same point will be erased rather than this point which shuldn't be
    //  plot (erase_x, erase_y, 0);   
    //} else {
    //else plot the ball and erase the old position
    //  plot (plot_x, plot_y, 1);     
    //  plot (erase_x, erase_y, 0); 
    //reset erase to new pos
    //  erase_x = plot_x; 
    //  erase_y = plot_y;
    //}
    matrix.drawPixel(plot_x,plot_y,matrix.Color333(4, 0, 0));

    //check if a bat missed the ball. if it did, reset the game.
    if ((int)ballpos_x == 0 ||(int) ballpos_x == 32){
      restart = 1; 
    } 
    delay(40);
    matrix.swapBuffers(false);
  } 
  //fade_down();
}
byte pong_get_ball_endpoint(float tempballpos_x, float  tempballpos_y, float  tempballvel_x, float tempballvel_y) {

  //run prediction until ball hits bat
  while (tempballpos_x > BAT1_X && tempballpos_x < BAT2_X  ){     
    tempballpos_x = tempballpos_x + tempballvel_x;
    tempballpos_y = tempballpos_y + tempballvel_y;
    //check for collisions with top / bottom
    if (tempballpos_y <= 0 || tempballpos_y >= 15){
      tempballvel_y = tempballvel_y * -1;
    }    
  }  
  return tempballpos_y; 
}

void normal_clock()
{
  matrix.setTextWrap(false); // Allow text to run off right edge
  matrix.setTextSize(2);
  matrix.setTextColor(matrix.Color333(2, 3, 2));

  cls();
  byte hours = hour();
  byte mins = minute();

  int  msHourPosition = 0;
  int  lsHourPosition = 0;
  int  msMinPosition = 0;
  int  lsMinPosition = 0;      
  int  msLastHourPosition = 0;
  int  lsLastHourPosition = 0;
  int  msLastMinPosition = 0;
  int  lsLastMinPosition = 0;      

  //Start with all characters off screen
  int c1 = -17;
  int c2 = -17;
  int c3 = -17;
  int c4 = -17;

  float scale_x =2.5;
  float scale_y =3.0;


  char lastHourBuffer[3]="  ";
  char lastMinBuffer[3] ="  ";

  //loop to display the clock for a set duration of SHOWCLOCK

  for (int show = 0; show < SHOWCLOCK ; show++) {

    cls();

    //gettime(); //get the time from the clock chip

    //flash led for seconds on arduino
    if ( (second() % 2) == 0) { 
      digitalWrite(13,HIGH);
    }
    else{ 
      digitalWrite(13,LOW); 
    } 

    //check buttons  
    if(buttonA.uniquePress()){
      switch_mode();
      //pacClear();
      return;      
    }
    if(buttonB.uniquePress()){
      display_date();
      quickWeather();//pacClear();//fade_down();
      normal_clock();
      return;
    }

    //update the clock if this is the first run of the show clock loop, or if the time has changed from what we had stored in mins and hors vars.
    //if ( show >= 0 || (mins != rtc[1] ) ) {  

    //udate mins and hours with the new time
    mins = minute();
    hours = hour();

    char buffer[3];

    itoa(hours,buffer,10);
    //fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ". 
    if (hours < 10) {
      buffer[1] = buffer[0];
      buffer[0] = '0';
    }

    if(lastHourBuffer[0]!=buffer[0] && c1==0) c1= -17;
    if( c1 < 0 )c1++;
    msHourPosition = c1;
    msLastHourPosition = c1 + 17;

    if(lastHourBuffer[1]!=buffer[1] && c2==0) c2= -17;
    if( c2 < 0 )c2++;
    lsHourPosition = c2;
    lsLastHourPosition = c2 + 17;

    //update the display

    //shadows first
    vectorNumber((lastHourBuffer[0]-'0'), 2, 2+msLastHourPosition, matrix.Color444(0,0,1),scale_x,scale_y);
    vectorNumber((lastHourBuffer[1]-'0'), 9, 2+lsLastHourPosition, matrix.Color444(0,0,1),scale_x,scale_y);
    vectorNumber((buffer[0]-'0'), 2, 2+msHourPosition, matrix.Color444(0,0,1),scale_x,scale_y);
    vectorNumber((buffer[1]-'0'), 9, 2+lsHourPosition, matrix.Color444(0,0,1),scale_x,scale_y); 


    vectorNumber((lastHourBuffer[0]-'0'), 1, 1+msLastHourPosition, matrix.Color444(1,1,1),scale_x,scale_y);
    vectorNumber((lastHourBuffer[1]-'0'), 8, 1+lsLastHourPosition, matrix.Color444(1,1,1),scale_x,scale_y);
    vectorNumber((buffer[0]-'0'), 1, 1+msHourPosition, matrix.Color444(1,1,1),scale_x,scale_y);
    vectorNumber((buffer[1]-'0'), 8, 1+lsHourPosition, matrix.Color444(1,1,1),scale_x,scale_y);    


    if(c1==0) lastHourBuffer[0]=buffer[0];
    if(c2==0) lastHourBuffer[1]=buffer[1];

    matrix.fillRect(16,5,2,2,matrix.Color444(0,0,second()%2));
    matrix.fillRect(16,11,2,2,matrix.Color444(0,0,second()%2));

    matrix.fillRect(15,4,2,2,matrix.Color444(second()%2,second()%2,second()%2));
    matrix.fillRect(15,10,2,2,matrix.Color444(second()%2,second()%2,second()%2));

    itoa (mins, buffer, 10);
    if (mins < 10) {
      buffer[1] = buffer[0];
      buffer[0] = '0';
    }

    if(lastMinBuffer[0]!=buffer[0] && c3==0) c3= -17;
    if( c3 < 0 )c3++;
    msMinPosition = c3;
    msLastMinPosition= c3 + 17;

    if(lastMinBuffer[1]!=buffer[1] && c4==0) c4= -17;
    if( c4 < 0 )c4++;
    lsMinPosition = c4;
    lsLastMinPosition = c4 + 17;

    vectorNumber((buffer[0]-'0'), 19, 2+msMinPosition, matrix.Color444(0,0,1),scale_x,scale_y);
    vectorNumber((buffer[1]-'0'), 26, 2+lsMinPosition, matrix.Color444(0,0,1),scale_x,scale_y);
    vectorNumber((lastMinBuffer[0]-'0'), 19, 2+msLastMinPosition, matrix.Color444(0,0,1),scale_x,scale_y);
    vectorNumber((lastMinBuffer[1]-'0'), 26, 2+lsLastMinPosition, matrix.Color444(0,0,1),scale_x,scale_y);

    vectorNumber((buffer[0]-'0'), 18, 1+msMinPosition, matrix.Color444(1,1,1),scale_x,scale_y);
    vectorNumber((buffer[1]-'0'), 25, 1+lsMinPosition, matrix.Color444(1,1,1),scale_x,scale_y);
    vectorNumber((lastMinBuffer[0]-'0'), 18, 1+msLastMinPosition, matrix.Color444(1,1,1),scale_x,scale_y);
    vectorNumber((lastMinBuffer[1]-'0'), 25, 1+lsLastMinPosition, matrix.Color444(1,1,1),scale_x,scale_y);


    if(c3==0) lastMinBuffer[0]=buffer[0];
    if(c4==0) lastMinBuffer[1]=buffer[1];

    matrix.swapBuffers(false); 

  }

}

//Draw number n, with x,y as top left corner, in chosen color, scaled in x and y.
//when scale_x, scale_y = 1 then character is 3x5
void vectorNumber(int n, int x, int y, int color, float scale_x, float scale_y){

  switch (n){
  case 0:
    matrix.drawLine(x ,y , x , y+(4*scale_y) , color);
    matrix.drawLine(x , y+(4*scale_y) , x+(2*scale_x) , y+(4*scale_y), color);
    matrix.drawLine(x+(2*scale_x) , y , x+(2*scale_x) , y+(4*scale_y) , color);
    matrix.drawLine(x ,y , x+(2*scale_x) , y , color);
    break; 
  case 1: 
    matrix.drawLine( x+(1*scale_x), y, x+(1*scale_x),y+(4*scale_y), color);  
    matrix.drawLine(x , y+4*scale_y , x+2*scale_x , y+4*scale_y,color);
    matrix.drawLine(x,y+scale_y, x+scale_x, y,color);
    break;
  case 2:
    matrix.drawLine(x ,y , x+2*scale_x , y , color);
    matrix.drawLine(x+2*scale_x , y , x+2*scale_x , y+2*scale_y , color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
    matrix.drawLine(x , y+2*scale_y, x , y+4*scale_y,color);
    matrix.drawLine(x , y+4*scale_y , x+2*scale_x , y+4*scale_y,color);
    break; 
  case 3:
    matrix.drawLine(x ,y , x+2*scale_x , y , color);
    matrix.drawLine(x+2*scale_x , y , x+2*scale_x , y+4*scale_y , color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y , x+scale_x , y+2*scale_y, color);
    matrix.drawLine(x , y+4*scale_y , x+2*scale_x , y+4*scale_y,color);
    break;
  case 4:
    matrix.drawLine(x+2*scale_x , y , x+2*scale_x , y+4*scale_y , color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
    matrix.drawLine(x ,y , x , y+2*scale_y , color);
    break;
  case 5:
    matrix.drawLine(x ,y , x+2*scale_x , y , color);
    matrix.drawLine(x , y , x , y+2*scale_y , color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y, x+2*scale_x , y+4*scale_y,color);
    matrix.drawLine( x , y+4*scale_y , x+2*scale_x , y+4*scale_y,color);
    break; 
  case 6:
    matrix.drawLine(x ,y , x , y+(4*scale_y) , color);
    matrix.drawLine(x ,y , x+2*scale_x , y , color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y, x+2*scale_x , y+4*scale_y,color);
    matrix.drawLine(x+2*scale_x , y+4*scale_y , x, y+(4*scale_y) , color);
    break;
  case 7:
    matrix.drawLine(x ,y , x+2*scale_x , y , color);
    matrix.drawLine( x+2*scale_x, y, x+scale_x,y+(4*scale_y), color);
    break;
  case 8:
    matrix.drawLine(x ,y , x , y+(4*scale_y) , color);
    matrix.drawLine(x , y+(4*scale_y) , x+(2*scale_x) , y+(4*scale_y), color);
    matrix.drawLine(x+(2*scale_x) , y , x+(2*scale_x) , y+(4*scale_y) , color);
    matrix.drawLine(x ,y , x+(2*scale_x) , y , color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
    break;
  case 9:
    matrix.drawLine(x ,y , x , y+(2*scale_y) , color);
    matrix.drawLine(x , y+(4*scale_y) , x+(2*scale_x) , y+(4*scale_y), color);
    matrix.drawLine(x+(2*scale_x) , y , x+(2*scale_x) , y+(4*scale_y) , color);
    matrix.drawLine(x ,y , x+(2*scale_x) , y , color);
    matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
    break;    
  }
}

void fade_down(){
  pacClear();
}

//print a clock using words rather than numbers
void word_clock() {

  cls();

  char numbers[19][10]   = { 
    "one", "two", "three", "four","five","six","seven","eight","nine","ten",
    "eleven","twelve", "thirteen","fourteen","fifteen","sixteen","7teen","8teen","nineteen"                  };              
  char numberstens[5][7] = { 
    "ten","twenty","thirty","forty","fifty"                   };

  byte hours_y, mins_y; //hours and mins and positions for hours and mins lines  

  byte hours = hour();
  byte mins  = minute();

  //loop to display the clock for a set duration of SHOWCLOCK
  for (int show = 0; show < SHOWCLOCK ; show++) {

    //gettime(); //get the time from the clock chip

    if(buttonA.uniquePress()){
      switch_mode();
      return;
    }
    if(buttonB.uniquePress()){
      display_date();
      quickWeather();//fade_down();
      word_clock();
      return;
    }

    //flash led for seconds on arduino
    if ( (second() % 2) == 0) { 
      digitalWrite(13,HIGH);
    }
    else{ 
      digitalWrite(13,LOW); 
    } 

    //print the time if it has changed or if we have just come into the subroutine
    if ( show == 0 || mins != minute() ) {  

      //reset these for comparison next time
      mins = minute();   
      hours = hour();

      //make hours into 12 hour format
      if (hours > 12){ 
        hours = hours - 12; 
      }
      if (hours == 0){ 
        hours = 12; 
      } 

      //split mins value up into two separate digits 
      int minsdigit = mins % 10;
      byte minsdigitten = (mins / 10) % 10;

      char str_top[8];
      char str_bot[8];
      char str_mid[8];

      //if mins <= 10 , then top line has to read "minsdigti past" and bottom line reads hours
      if (mins < 10) {     
        strcpy (str_top,numbers[minsdigit - 1]);
        strcpy (str_mid,"PAST");
        strcpy (str_bot,numbers[hours - 1]);
      }
      //if mins = 10, cant use minsdigit as above, so soecial case to print 10 past /n hour.
      if (mins == 10) {     
        strcpy (str_top,numbers[9]);
        strcpy (str_mid,"PAST");
        strcpy (str_bot,numbers[hours - 1]);
      }

      //if time is not on the hour - i.e. both mins digits are not zero, 
      //then make top line read "hours" and bottom line ready "minstens mins" e.g. "three /n twenty one"
      else if (minsdigitten != 0 && minsdigit != 0  ) {

        strcpy (str_top,numbers[hours - 1]); 

        //if mins is in the teens, use teens from the numbers array for the bottom line, e.g. "three /n fifteen"
        if (mins >= 11 && mins <= 19) {
          strcpy (str_bot, numbers[mins - 1]);
          strcpy(str_mid," ");

          //else bottom line reads "minstens mins" e.g. "three \n twenty three"
        } 
        else {     
          strcpy (str_mid, numberstens[minsdigitten - 1]);
          //strcat (str_bot, " "); 
          //strcat (str_bot, numbers[minsdigit - 1]); 
          strcpy (str_bot, numbers[minsdigit -1]);
        }
      }
      // if mins digit is zero, don't print it. read read "hours" "minstens" e.g. "three /n twenty"
      else if (minsdigitten != 0 && minsdigit == 0  ) {
        strcpy (str_top, numbers[hours - 1]);     
        strcpy (str_bot, numberstens[minsdigitten - 1]);
        strcpy (str_mid, " " );
      }

      //if both mins are zero, i.e. it is on the hour, the top line reads "hours" and bottom line reads "o'clock"
      else if (minsdigitten == 0 && minsdigit == 0  ) {
        strcpy (str_top,numbers[hours - 1]);     
        strcpy (str_bot, "O'CLOCK");
        strcpy (str_mid, " ");
      }

      //work out offset to center top line on display. 
      byte lentop = 0;
      while(str_top[lentop]) { 
        lentop++; 
      }; //get length of message
      byte offset_top;
      if(lentop<6){
        offset_top = (X_MAX - ((lentop*6)-1)) / 2; //
      }
      else{
        offset_top = (X_MAX - ((lentop - 1)*4)) / 2; //
      }


      //work out offset to center bottom line on display. 
      byte lenbot = 0;
      while(str_bot[lenbot]) { 
        lenbot++; 
      }; //get length of message
      byte offset_bot;
      if(lenbot<6){
        offset_bot = (X_MAX - ((lenbot*6)-1)) / 2; //
      }
      else{
        offset_bot = (X_MAX - ((lenbot - 1)*4)) / 2; //
      }

      byte lenmid = 0;
      while(str_mid[lenmid]) { 
        lenmid++; 
      }; //get length of message
      byte offset_mid;
      if(lenmid<6){
        offset_mid = (X_MAX - ((lenmid*6)-1)) / 2; //
      }
      else{
        offset_mid = (X_MAX - ((lenmid - 1)*4)) / 2; //
      }

      //      fade_down();
      cls();
      drawString(offset_top,(lenmid>1?0:2),str_top,(lentop<6?53:51),matrix.Color333(0,1,5));
      if(lenmid>1){
        drawString(offset_mid,5,str_mid,(lenmid<6?53:51),matrix.Color333(1,1,5));
      }
      drawString(offset_bot,(lenmid>1?10:8),str_bot,(lenbot<6?53:51),matrix.Color333(0,5,1));    
      matrix.swapBuffers(false);
      //plot hours line
      //      byte i = 0;
      //      while(str_top[i]) {
      //        ht1632_puttinychar((i*4) + offset_top, 2, str_top[i]); 
      //        i++;
      //      }

      //      i = 0;
      //      while(str_bot[i]) {
      //        ht1632_puttinychar((i*4) + offset_bot, 9, str_bot[i]); 
      //        i++;
      //      }
    }
    delay (50); 
  }
  //fade_down();
}


//show time and date and use a random jumble of letters transition each time the time changes.
void jumble() {

  char days[7][4] = {
    "SUN","MON","TUE", "WED", "THU", "FRI", "SAT"                  }; //DS1307 outputs 1-7
  char allchars[37] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"                  };
  char endchar[16];
  byte counter[16];
  byte mins = minute();
  byte seq[16];

  cls();

  for (int show = 0; show < SHOWCLOCK ; show++) {

    //gettime();
    //flash led for seconds on arduino
    if ( (second() % 2) == 0) { 
      digitalWrite(13,HIGH); 
    }
    else{ 
      digitalWrite(13,LOW); 
    }

    //check buttons  
    if(buttonA.uniquePress()){
      switch_mode();
      return;      
    }
    if(buttonB.uniquePress()){
      display_date();
      quickWeather();//fade_down();
      jumble();
      return;
    }

    if ( show == 0 || mins != minute()  ) {  

      //fill an arry with 0-15 and randomize the order so we can plot letters in a jumbled pattern rather than sequentially
      for (int i=0; i<16; i++) {
        seq[i] = i;  // fill the array in order
      }
      //randomise array of numbers 
      for (int i=0; i<(16-1); i++) {
        int r = i + (rand() % (16-i)); // Random remaining position.
        int temp = seq[i]; 
        seq[i] = seq[r]; 
        seq[r] = temp;
      }


      //reset these for comparison next time
      mins = minute();
      byte hours = hour();   
      byte dow   = weekday() - 1; // the DS1307 outputs 1 - 7. 
      byte date  = day();

      byte alldone = 0;

      //set counters to 50
      for(byte c=0; c<16 ; c++) {
        counter[c] = 3 + random (0,20);
      }

      //set final characters
      char buffer[3];
      itoa(hours,buffer,10);

      //fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ". 
      if (hours < 10) {
        buffer[1] = buffer[0];
        buffer[0] = '0';
      }

      endchar[0] = buffer[0];
      endchar[1] = buffer[1];
      endchar[2] = ':';

      itoa (mins, buffer, 10);
      if (mins < 10) {
        buffer[1] = buffer[0];
        buffer[0] = '0';
      }

      endchar[3] = buffer[0];
      endchar[4] = buffer[1];

      itoa (date, buffer, 10);
      if (date < 10) {
        buffer[1] = buffer[0];
        buffer[0] = '0';
      }

      //then work out date 2 letter suffix - eg st, nd, rd, th etc
      char suffix[4][3]={
        "st", "nd", "rd", "th"                                                      };
      byte s = 3; 
      if(date == 1 || date == 21 || date == 31) {
        s = 0;
      } 
      else if (date == 2 || date == 22) {
        s = 1;
      } 
      else if (date == 3 || date == 23) {
        s = 2;
      }
      //set topline
      endchar[5] = ' ';
      endchar[6] = ' ';
      endchar[7] = ' ';

      //set bottom line
      endchar[8] = days[dow][0];
      endchar[9] = days[dow][1];
      endchar[10] = days[dow][2];
      endchar[11] = ' ';
      endchar[12] = buffer[0];
      endchar[13] = buffer[1];
      endchar[14] = suffix[s][0];
      endchar[15] = suffix[s][1];

      byte x = 0;
      byte y = 0;

      //until all counters are 0
      while (alldone < 16){

        //for each char    
        for(byte c=0; c<16 ; c++) {

          if (seq[c] < 8) { 
            x = 0;
            y = 0; 
          } 
          else {
            x = 8;
            y = 8;   
          }

          //if counter > 1 then put random char
          if (counter[ seq[c] ] > 1) {
            //ht1632_putchar( ( seq[c] -x) * 6, y, allchars[random(0,36)]); //random
            matrix.fillRect((seq[c]-x)*4,y,3,5,matrix.Color333(0,0,0));
            drawChar((seq[c] - x) *4, y, allchars[random(0,36)],51,matrix.Color444(1,0,0));
            counter[ seq[c] ]--;
            matrix.swapBuffers(true);
          }

          //if counter == 1 then put final char 
          if (counter[ seq[c] ] == 1) {
            //            ht1632_putchar( (seq[c]-x) * 6, y, endchar[seq[c]]); //final char
            matrix.fillRect((seq[c]-x)*4,y,3,5,matrix.Color444(0,0,0));
            drawChar((seq[c] - x) *4, y, endchar[seq[c]],51,matrix.Color444(0,0,1));
            counter[seq[c]] = 0;
            alldone++;
            matrix.swapBuffers(true);
          } 

          //if counter == 0 then just pause to keep update rate the same
          if (counter[seq[c]] == 0) {
            delay(4);
          }

          if(buttonA.uniquePress()){
            switch_mode();
            return;      
          }
        }
      }
    }
    delay(50);
  } //showclock
  //fade_down();
}


void display_date()
{

  uint16_t color = matrix.Color333(0,1,0);
  cls();
  matrix.swapBuffers(true);
  //read the date from the DS1307
  //it returns the month number, day number, and a number representing the day of week - 1 for Tue, 2 for Wed 3 for Thu etc.
  byte dow = weekday()-1; //rtc[3] - 1; //we  take one off the value the DS1307 generates, as our array of days is 0-6 and the DS1307 outputs  1-7.
  byte date = day();  //rtc[4];
  byte mont = month()-1; //rtc[5] - 1; 

  //array of day and month names to print on the display. Some are shortened as we only have 8 characters across to play with 
  char daynames[7][9]={
    "Sunday", "Monday","Tuesday", "Wed", "Thursday", "Friday", "Saturday"                  };
  char monthnames[12][9]={
    "January", "February", "March", "April", "May", "June", "July", "August", "Sept", "October", "November", "December"                  };

  //call the flashing cursor effect for one blink at x,y pos 0,0, height 5, width 7, repeats 1
  flashing_cursor(0,0,3,5,1);

  //print the day name
  int i = 0;
  while(daynames[dow][i])
  {
    flashing_cursor(i*4,0,3,5,0);
    //ht1632_putchar(i*6 , 0, daynames[dow][i]); 
    drawChar(i*4,0,daynames[dow][i],51,color);
    matrix.swapBuffers(true);
    i++;

    //check for button press and exit if there is one.
    if(buttonA.uniquePress() || buttonB.uniquePress()){
      return;
    }
  }

  //pause at the end of the line with a flashing cursor if there is space to print it.
  //if there is no space left, dont print the cursor, just wait.
  if (i*4 < 32){
    flashing_cursor(i*4,0,3,5,1);  
  } 
  else {
    delay(300);
  }

  //flash the cursor on the next line  
  flashing_cursor(0,8,3,5,0);

  //print the date on the next line: First convert the date number to chars so we can print it with ht1632_putchar
  char buffer[3];
  itoa(date,buffer,10);

  //then work out date 2 letter suffix - eg st, nd, rd, th etc
  char suffix[4][3]={
    "st", "nd", "rd", "th"                    };
  byte s = 3; 
  if(date == 1 || date == 21 || date == 31) {
    s = 0;
  } 
  else if (date == 2 || date == 22) {
    s = 1;
  } 
  else if (date == 3 || date == 23) {
    s = 2;
  } 

  //print the 1st date number
  //ht1632_putchar(0, 8, buffer[0]);
  drawChar(0,8,buffer[0],51,color);
  matrix.swapBuffers(true);

  //if date is under 10 - then we only have 1 digit so set positions of sufix etc one character nearer
  byte suffixposx = 4;

  //if date over 9 then print second number and set xpos of suffix to be 1 char further away
  if (date > 9){
    suffixposx = 8;
    flashing_cursor(4,8,3,5,0); 
    //ht1632_putchar(6, 8, buffer[1]);
    drawChar(4,8,buffer[1],51,color);
    matrix.swapBuffers(true);
  }

  //print the 2 suffix characters
  flashing_cursor(suffixposx, 8,3,5,0);
  //  ht1632_putchar(suffixposx, 8, suffix[s][0]); 
  drawChar(suffixposx,8,suffix[s][0],51,color);
  matrix.swapBuffers(true);
  //delay(70);

  flashing_cursor(suffixposx+4,8,3,5,0);
  //  ht1632_putchar(suffixposx+6, 8, suffix[s][1]); 
  drawChar(suffixposx+4,8,suffix[s][1],51,color);
  matrix.swapBuffers(true);
  //delay(70);

  //blink cursor after 
  flashing_cursor(suffixposx + 8,8,3,5,1);  

  //replace day name with date on top line - effectively scroll the bottom line up by 8 pixels
  for(int q = 8; q>=0; q--){
    cls();
    int w =0 ;
    while(daynames[dow][w])
    {
      drawChar(w*4,q-8,daynames[dow][w],51,color);

      w++;
    }


    matrix.swapBuffers(true);
    //date first digit
    drawChar(0,q,buffer[0],51,color);
    //date second digit - this may be blank and overwritten if the date is a single number
    drawChar(4,q,buffer[1],51,color);
    //date suffix
    drawChar(suffixposx,q,suffix[s][0],51,color);
    //date suffix
    drawChar(suffixposx+4,q,suffix[s][1],51,color);
    matrix.swapBuffers(true);
    delay(50);
  }
  //flash the cursor for a second for effect
  flashing_cursor(suffixposx + 8,0,3,5,0);  

  //print the month name on the bottom row
  i = 0;
  while(monthnames[mont][i])
  {  
    flashing_cursor(i*4,8,3,5,0);
    //ht1632_putchar(i*6, 8, monthnames[month][i]); 
    drawChar(i*4,8,monthnames[mont][i],51,color);
    matrix.swapBuffers(true);
    i++; 

    //check for button press and exit if there is one.
    if(buttonA.uniquePress() || buttonB.uniquePress()){
      return;
    }
  }

  //blink the cursor at end if enough space after the month name, otherwise juts wait a while
  if (i*4 < 32){
    flashing_cursor(i*4,8,3,5,2);  
  } 
  else {
    delay(1000);
  }

  for(int q = 8; q>=-8; q--){
    cls();
    int w =0 ;
    while(monthnames[mont][w])
    {
      drawChar(w*4,q,monthnames[mont][w],51,color);

      w++;
    }


    matrix.swapBuffers(true);
    //date first digit
    drawChar(0,q-8,buffer[0],51,color);
    //date second digit - this may be blank and overwritten if the date is a single number
    drawChar(4,q-8,buffer[1],51,color);
    //date suffix
    drawChar(suffixposx,q-8,suffix[s][0],51,color);
    //date suffix
    drawChar(suffixposx+4,q-8,suffix[s][1],51,color);
    matrix.swapBuffers(true);
    delay(50);
  }


}


/*
 * flashing_cursor
 * print a flashing_cursor at xpos, ypos and flash it repeats times 
 */
void flashing_cursor(byte xpos, byte ypos, byte cursor_width, byte cursor_height, byte repeats)
{
  for (byte r = 0; r <= repeats; r++) {
    matrix.fillRect(xpos,ypos,cursor_width, cursor_height, matrix.Color333(0,3,0));
    matrix.swapBuffers(true);

    if (repeats > 0) {
      delay(400);
    } 
    else {
      delay(70);
    }

    matrix.fillRect(xpos,ypos,cursor_width, cursor_height, matrix.Color333(0,0,0));
    matrix.swapBuffers(true);

    //if cursor set to repeat, wait a while
    if (repeats > 0) {
      delay(400); 
    }
  }
}


//print menu to change the mode
void switch_mode() {
  matrix.setTextWrap(false); // Allow text to run off right edge
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.Color333(2, 0, 2));
  uint16_t color = matrix.Color333(2, 0, 2);

  char* modes[] = {
    "Numbers", "Pong", "Words", "Jumble", "Sound", "Random", "Set Clk"                   };

  byte next_clock_mode;
  byte firstrun = 1;

  mode_changed = 1; //set this flag so we don't show the date when we get out the menu, we just go straigh to the new mode.

  //loop waiting for button (timeout after X loops to return to mode X)
  for(int count=0; count< 40 ; count++) {

    //if user hits button, change the clock_mode
    if(buttonA.uniquePress() || firstrun == 1){

      count = 0;
      cls();

      if (firstrun == 0) { 
        clock_mode++; 
      } 
      if (clock_mode > MAX_CLOCK_MODE) { 
        clock_mode = 0; 
      }

      //print arrown and current clock_mode name on line one and print next clock_mode name on line two
      char str_top[9];
      char str_bot[9];

      strcpy (str_top, " ");
      strcat (str_top, modes[clock_mode]);

      next_clock_mode = clock_mode + 1;
      if (next_clock_mode > MAX_CLOCK_MODE) { 
        next_clock_mode = 0; 
      }

      strcpy (str_bot, " ");
      strcat (str_bot, modes[next_clock_mode]);

      drawString(0,0,str_top,51,color);

      drawString(0,8,str_bot,51,color);

      matrix.fillTriangle(0,0,0,4,2,2, color);

      firstrun = 0;
      matrix.swapBuffers(false);
    }
    delay(50); 
  }
  //if random clock_mode set, set next hour to change clock type 
  if (clock_mode == MAX_CLOCK_MODE - 1 ){ 
    random_mode = 1;  
    //    mode_time_up = rtc[2]; 
    mode_time_up = hour();
    clock_mode = 0;
  } 
  else {
    random_mode = 0;
  } 
}

void drawString(int x, int y, char* c,uint8_t font_size, uint16_t color)
{
  // x & y are positions, c-> pointer to string to disp, update_s: false(write to mem), true: write to disp
  //font_size : 51(ascii value for 3), 53(5) and 56(8)
  for(char i=0; i< strlen(c); i++)
  {
    drawChar(x, y, c[i],font_size, color);
    x+=calc_font_displacement(font_size); // Width of each glyph
  }
}

int calc_font_displacement(uint8_t font_size)
{
  switch(font_size)
  {
  case 51:
    return 4;  //5x3 hence occupies 4 columns ( 3 + 1(space btw two characters))
    break;

  case 53:
    return 6;
    break;

    //case 56:
    //return 6;
    //break;

  default:
    return 6;
    break;
  }
}

void drawChar(int x, int y, char c, uint8_t font_size, uint16_t color)  // Display the data depending on the font size mentioned in the font_size variable
{

  uint8_t dots;
  if (c >= 'A' && c <= 'Z' ||
    (c >= 'a' && c <= 'z') ) {
    c &= 0x1F;   // A-Z maps to 1-26
  } 
  else if (c >= '0' && c <= '9') {
    c = (c - '0') + 27;
  } 
  else if (c == ' ') {
    c = 0; // space
  }
  else if (c == '#'){
    c=37;
  }
  else if (c=='/'){
    c=37;
  }

  switch(font_size)
  {
  case 51:  // font size 3x5  ascii value of 3: 51

      if(c==':'){
      matrix.drawPixel(x+1,y+1,color);
      matrix.drawPixel(x+1,y+3,color);
    }
    else if(c=='-'){
      matrix.drawLine(x,y+2,3,0,color);
    }
    else if(c=='.'){
      matrix.drawPixel(x+1,y+2,color);
    }
    else if(c==39 || c==44){
      matrix.drawLine(x+1,y,2,0,color);
      matrix.drawPixel(x+2,y+1,color);
    }
    else{
      for (char row=0; row< 5; row++) {
        dots = pgm_read_byte_near(&font3x5[c][row]);
        for (char col=0; col < 3; col++) {
          int x1=x;
          int y1=y;
          if (dots & (4>>col))
            matrix.drawPixel(x1+col, y1+row, color);
        }    
      }
    }
    break;

  case 53:  // font size 5x5   ascii value of 5: 53

      if(c==':'){
      matrix.drawPixel(x+2,y+1,color);
      matrix.drawPixel(x+2,y+3,color);
    }
    else if(c=='-'){
      matrix.drawLine(x+1,y+2,3,0,color);
    }
    else if(c=='.'){
      matrix.drawPixel(x+2,y+2,color);
    }
    else if(c==39 || c==44){
      matrix.drawLine(x+2,y,2,0,color);
      matrix.drawPixel(x+4,y+1,color);
    }
    else{
      for (char row=0; row< 5; row++) {
        dots = pgm_read_byte_near(&font5x5[c][row]);
        for (char col=0; col < 5; col++) {
          int x1=x;
          int y1=y;
          if (dots & (64>>col))  // For some wierd reason I have the 5x5 font in such a way that.. last two bits are zero.. 
            // dots &64(10000000) gives info regarding the first bit... 
            // dots &32(01000000) gives info regarding second bit and so on...
            matrix.drawPixel(x1+col, y1+row, color);        
        }
      }
    }          


    break;

  default:
    break;

  }
}



//set time and date routine
void set_time() {

  cls();

  //fill settings with current clock values read from clock
  //gettime();
  byte set_min   = minute();
  byte set_hr    = hour();
  byte set_dow   = weekday()-1;// rtc[3] -1; //the DS1307 outputs 1-7.
  byte set_date  = day();
  byte set_mnth  = month();
  byte set_yr    = year()-2000; //not sure about this   

  //Set function - we pass in: which 'set' message to show at top, current value, reset value, and rollover limit.


  set_min  = set_value(0, set_min, 0, 59);
  set_hr   = set_value(1, set_hr, 0, 23);
  set_dow  = set_value_dow(set_dow);
  set_date = set_value(4, set_date, 1, 31);
  set_mnth = set_value(3, set_mnth, 1, 12);
  set_yr   = set_value(2, set_yr, 10, 99);

  //write the changes to the clock chip
  //RTC.stop();

  tmElements_t t;
  t.Second = 0;
  t.Minute = set_min;
  t.Hour = set_hr;
  t.Wday = set_dow;
  t.Day = set_date;
  t.Month = set_mnth;
  t.Year = set_yr+30;

  RTC.set(makeTime(t));
  RTC.write(t);
  setTime(makeTime(t));

  //RTC.set(DS1307_SEC,0);
  //RTC.set(DS1307_MIN,set_min);
  //RTC.set(DS1307_HR,set_hr);
  // RTC.set(DS1307_DOW,(set_dow + 1)); //add one as DS1307 expects va1-7
  //RTC.set(DS1307_DATE,set_date);
  //RTC.set(DS1307_MTH,set_mnth);
  //RTC.set(DS1307_YR,set_yr);
  //RTC.start();

  //reset clock mode from 'set clock'
  cls();
  clock_mode = 0;  
}

byte set_value_dow(byte current_value){

  cls();
  char message[9] = {
    "DAY NAME"                  };
  char days[7][9] = {
    "SUNDAY  ", "MONDAY  ","TUESDAY ", "WED     ", "THURSDAY", "FRIDAY  ", "SATURDAY"                  };

  //Print "set xyz" top line
  drawString(0,0,message,51,matrix.Color333(1,1,1));

  //Print current day set
  drawString(0,8,days[current_value],51,matrix.Color333(4,1,1));
  matrix.swapBuffers(true);

  //wait for button input
  delay(300);
  while (!buttonA.uniquePress()) {
    while (buttonB.isPressed()) {

      if(current_value == 6) { 
        current_value = 0;
      } 
      else {
        current_value++;
      }
      //print the new value 
      matrix.fillRect(0,8,31,15,matrix.Color333(0,0,0));
      drawString(0,8,days[current_value],51,matrix.Color333(4,1,1));
      matrix.swapBuffers(true);
      delay(150);
    }
  }
  return current_value;
}



//used to set min, hr, date, month, year values. pass 
//message = which 'set' message to print, 
//current value = current value of property we are setting
//reset_value = what to reset value to if to rolls over. E.g. mins roll from 60 to 0, months from 12 to 1
//rollover limit = when value rolls over

byte set_value(byte message, byte current_value, byte reset_value, byte rollover_limit){

  cls();
  char messages[6][17]   = {
    "SET MINS", "SET HOUR", "SET YEAR", "SET MNTH", "SET DAY", "DAY NAME"                  };

  //Print "set xyz" top line
  byte i = 0;
  //while(messages[message][i])
  //{
  //  ht1632_putchar(i*6 , 0, messages[message][i]); 
  //  i++;
  //}
  drawString(0,0,messages[message],51,matrix.Color333(1,1,1));  
  matrix.swapBuffers(true);

  //print digits bottom line
  char buffer[3];
  itoa(current_value,buffer,10);
  //ht1632_putchar(0 , 8, buffer[0]); 
  //ht1632_putchar(6 , 8, buffer[1]); 
  matrix.fillRect(0,8,31,15,matrix.Color333(0,0,0));
  drawChar(0,8,buffer[0],53,matrix.Color333(3,1,1));
  drawChar(6,8,buffer[1],53,matrix.Color333(3,1,1));
  matrix.swapBuffers(true);

  delay(300);
  //wait for button input
  while (!buttonA.uniquePress()) {

    while (buttonB.isPressed()){

      if(current_value < rollover_limit) { 
        current_value++;
      } 
      else {
        current_value = reset_value;
      }
      //print the new value
      itoa(current_value, buffer ,10);
      //ht1632_putchar(0 , 8, buffer[0]); 
      //ht1632_putchar(6 , 8, buffer[1]);
      matrix.fillRect(0,8,31,15,matrix.Color333(0,0,0));
      drawChar(0,8,buffer[0],53,matrix.Color333(3,1,1));
      drawChar(6,8,buffer[1],53,matrix.Color333(3,1,1));
      matrix.swapBuffers(true);

      delay(150);
    }
  }
  return current_value;
}


//Spectrum Analyser stuff
void spectrumDisplay(){
  uint8_t  i,  L, *data, nBins, binNum, weighting, c;
  uint16_t x,minLvl, maxLvl;
  int      level, y, sum, red,green,yellow, off;


  red = matrix.Color333(2,0,0);
  green = matrix.Color333(0,2,0);
  yellow = matrix.Color333(1,1,0);
  off = 0;

  cls();
  for (int show = 0; show < SHOWCLOCK ; show++) {

    //gettime();
    //flash led for seconds on arduino
    if ( (second() % 2) == 0) { 
      digitalWrite(13,HIGH); 
    }
    else{ 
      digitalWrite(13,LOW); 
    }

    //check buttons  
    if(buttonA.uniquePress()){

      switch_mode();
      return;      
    }
    if(buttonB.uniquePress()){

      display_date();
      quickWeather();
      //fade_down();
      spectrumDisplay();
      return;
    }

    while(ADCSRA & _BV(ADIE)); // Wait for audio sampling to finish

    fft_input(capture, bfly_buff);   // Samples -> complex #s
    samplePos = 0;    // Reset sample counter


    ADCSRA |= _BV(ADIE);             // Resume sampling interrupt
    fft_execute(bfly_buff);          // Process complex data
    fft_output(bfly_buff, spectrum); // Complex -> spectrum



      // Remove noise and apply EQ levels
    for(x=0; x<FFT_N/2; x++) {
      L = pgm_read_byte(&noise[x]);
      spectrum[x] = (spectrum[x] <= L) ? 0 :
      (((spectrum[x] - L) * (256L - pgm_read_byte(&eq[x]))) >> 8);
    }


    for(int l=0; l<16;l++){
      int col = matrix.Color444(16-l,0,l);
      matrix.drawLine(0,l,31,l,col);
    }

    // Downsample spectrum output to 32 columns:
    for(x=0; x<32; x++) {
      if(x<22){
        col[x][colCount] = spectrum[(x+10)];
      }
      else{
        col[x][colCount]=(spectrum[(x+10)+(x-22)*2]+spectrum[(x+10)+(x-22)*2+1]+spectrum[(x+10)+(x-22)*2+2])/3;
      };
      minLvl = maxLvl = col[x][0];
      int colsum=col[x][0];
      for(i=1; i<10; i++) { // Get range of prior 10 frames
        if(i<10)colsum = colsum + col[x][i];
        if(col[x][i] < minLvl)      minLvl = col[x][i];
        else if(col[x][i] > maxLvl) maxLvl = col[x][i];
      }
      // minLvl and maxLvl indicate the extents of the FFT output, used
      // for vertically scaling the output graph (so it looks interesting
      // regardless of volume level).  If they're too close together though
      // (e.g. at very low volume levels) the graph becomes super coarse
      // and 'jumpy'...so keep some minimum distance between them (this
      // also lets the graph go to zero when no sound is playing):
      if((maxLvl - minLvl) < 16) maxLvl = minLvl + 8;
      minLvlAvg[x] = (minLvlAvg[x] * 7 + minLvl) >> 3; // Dampen min/max levels
      maxLvlAvg[x] = (maxLvlAvg[x] * 7 + maxLvl) >> 3; // (fake rolling average)

      // Second fixed-point scale based on dynamic min/max levels:
      //level = 10L * (col[x][colCount] - minLvlAvg[x]) / (long)(maxLvlAvg[x] - minLvlAvg[x]);
      level = col[x][colCount];
      // Clip output and convert to byte:
      if(level < 0L)      c = 0;
      else if(level > 18) c = 18; // Allow dot to go a couple pixels off top
      else                c = (uint8_t)level;

      if(c > peak[x]) peak[x] = c; // Keep dot on top

      if(peak[x] <= 0) { // Empty column?
        matrix.drawLine(x, 0, x, 15, off);
        continue;
      } 
      else if(c < 15) { // Partial column?
        matrix.drawLine(x, 0, x, 15 - c, off);
      }

      // The 'peak' dot color varies, but doesn't necessarily match
      // the three screen regions...yellow has a little extra influence.
      y = 16 - peak[x];
      //if(y < 4)      matrix.drawPixel(x, y, red);
      //else if(y < 12) matrix.drawPixel(x, y, yellow);
      //else           matrix.drawPixel(x, y, green);
      matrix.drawPixel(x,y,matrix.Color444(peak[x],0,16-peak[x]));
    }

    int mins = minute();
    int hours = hour();

    char buffer[3];

    itoa(hours,buffer,10);
    //fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ". 
    if (hours < 10) {
      buffer[1] = buffer[0];
      buffer[0] = '0';
    }
    vectorNumber(buffer[0]-'0',8,1,matrix.Color333(0,1,0),1,1);
    vectorNumber(buffer[1]-'0',12,1,matrix.Color333(0,1,0),1,1);

    itoa(mins,buffer,10);
    //fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ". 
    if (mins < 10) {
      buffer[1] = buffer[0];
      buffer[0] = '0';
    }
    vectorNumber(buffer[0]-'0',18,1,matrix.Color333(0,1,0),1,1);
    vectorNumber(buffer[1]-'0',22,1,matrix.Color333(0,1,0),1,1);

    matrix.drawPixel(16,2,matrix.Color333(0,1,0));
    matrix.drawPixel(16,4,matrix.Color333(0,1,0));

    matrix.swapBuffers(true);
    //delay(10);

    // Every third frame, make the peak pixels drop by 1:
    if(++dotCount >= 3) {
      dotCount = 0;
      for(x=0; x<32; x++) {
        if(peak[x] > 0) peak[x]--;
      }
    }

    if(++colCount >= 10) colCount = 0;
  }
}

ISR(ADC_vect) { // Audio-sampling interrupt
  static const int16_t noiseThreshold = 4;
  int16_t              sample         = ADC; // 0-1023

  capture[samplePos] =
    //fht_input[samplePos] = 
  ((sample > (512-noiseThreshold)) &&
    (sample < (512+noiseThreshold))) ? 0 :
  sample - 512; // Sign-convert for FFT; -512 to +511

  if(++samplePos >= FFT_N) ADCSRA &= ~_BV(ADIE); // Buffer full, interrupt off

}










