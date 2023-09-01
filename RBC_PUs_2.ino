/*********************
  Measure PUs during pasteurization
**********************/

// include the library code:

#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>  // RGB LCD Shield communications
#include <RTClib.h>                 // DS1307 clock communications
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SdFat.h>

SdFat SD;

const int chipSelect = 10; // SD card

// DS18B20 sensor(s) on digital line 2, 10-bit precision
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 11

// These defines make it easy to set the backlight color
#define OFF 0x0
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const byte sensorCount = 5;
const DeviceAddress sensorAddress[sensorCount] = {  // Previously determined
  {0x28, 0x9C, 0x63, 0x45, 0x92, 0x0F, 0x02, 0x2F},
  {0x28, 0x87, 0x78, 0x45, 0x92, 0x0F, 0x02, 0x80},
  {0x28, 0x1E, 0xF8, 0x45, 0x92, 0x0D, 0x02, 0xFD},
  {0x28, 0xB7, 0x33, 0x45, 0x92, 0x17, 0x02, 0x6E},
  {0x28, 0xBF, 0xF6, 0x45, 0x92, 0x04, 0x02, 0x86}};
  
const char* sensorNames[sensorCount] = { "w", "b1", "b2", "b3", "b4"};
const byte bottleMinIndex = 1, bottleMaxIndex = 4;
float lastTemp[sensorCount] = {999, 999, 999, 999, 999};
float currTemp[sensorCount] = {999, 999, 999, 999, 999};
float totalPU[sensorCount] = {0, 0, 0, 0, 0}; // totalPu[0] is not used, but simplifies things


// Set up initial values for temperature sensor
float Celsius = 0;
float PU = 0;
float maxPU = 60;
float maxTemp = 80;
int stopper = 0;
int readCount = 0;

// The shield uses the I2C SCL and SDA pins.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

RTC_PCF8523 RTC;      // Establish clock object
DateTime Clock;      // Holds current clock time

File dataFile;
char logFileName[] = "yyyymmdd-hhmmss.csv"; //yymmdd-hhmmss.csv
char dataString[69];
char str_Cel[6];
char str_PU[6];

int8_t offset = 0;   // Hour offset set by user with buttons
uint8_t backlight = GREEN;  // Backlight state

void setup() {
  lcd.begin(16, 2);         // initialize display colums and rows
  RTC.begin();              // Initialize clock
  lcd.setBacklight(backlight);  // Set to OFF if you do not want backlight on boot
  Serial.begin(9600);
  delay(1000);
  sensors.begin();

  for (int i = 0;  i < sensorCount;  i++) {
    sensors.setResolution(sensorAddress[i], TEMPERATURE_PRECISION);
  }


  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println(F("Card failed, or not present"));
    // don't do anything more:
    while (1);
  }
  Serial.println(F("card initialized."));

  Clock = RTC.now();
  sprintf(logFileName, "%02d%02d%02d-%02d%02d%02d.csv", Clock.year(), Clock.month(), Clock.day(), Clock.hour(), Clock.minute(), Clock.second());
  Serial.println(logFileName);

  sprintf(dataString, "Hr,Min,Sec");
  for (int i = 0;  i < sensorCount;  i++) {
    strcat(dataString, ",");
    strcat(dataString, sensorNames[i]);
    if (i > 0) {
     strcat(dataString, ",PU");
    }
  }

  // open the file. 
  dataFile = SD.open(logFileName, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println(F("error opening datalog.txt"));
  }
  
}


void loop() {
  uint8_t buttons;                       // button read value
  uint8_t hourval, minuteval, secondval; // holds the time

  char* colon = ":";                 // static characters save a bit
  char* slash = "/";                 //   of memory

  // make a string for assembling the data to log:
  // String dataString = "";

  sensors.requestTemperatures();
  
  for (int i = 0;  i < sensorCount;  i++) {
    lastTemp[i] = currTemp[i];
    currTemp[i] = sensors.getTempC(sensorAddress[i]);

    // set lastTemp to initital temp
    if (lastTemp[i] == 999) {
      if ((currTemp[i] > 0) && (currTemp[i] < maxTemp)){
        lastTemp[i] = currTemp[i];
      }
    }
    
    // check for read errors
    if ((currTemp[i] < 0) || (currTemp[i] > maxTemp)){
      if (lastTemp[i] != 999){
        currTemp[i] = lastTemp[i];
      } else {
        do {
          delay(100);
          currTemp[i] = sensors.getTempC(sensorAddress[i]);
          Serial.println("Error reading sensor.");
        } while((currTemp[i] < 0) || (currTemp[i] > maxTemp));
      }
    }

    if (i > 0) {
      if (currTemp[i] > 50) {
        totalPU[i] = totalPU[i] + (pow(10, ((currTemp[i] - 60) / 6.94))) / 12;
      }
      if (totalPU[i] > maxPU) {
        lcd.setBacklight(RED);  // set the new backlight state
      }
    }
  }

  Clock = RTC.now();                 // get the RTC time

  hourval = Clock.hour();     // calculate hour to display
  if (hourval > 23) hourval -= 24;   // adjust for over 23 hour
  else if (hourval < 0) hourval += 24; //   or under 0 hours

  minuteval = Clock.minute();        // This block prints the time
  secondval = Clock.second();        //  to the LCD Shield
  lcd.clear();
  lcd.setCursor(0, 0);
  if (hourval < 10) printzero();     // print function does not print
  lcd.print(hourval);                //   leading zeros so this will
  lcd.print(colon);
  if (minuteval < 10) printzero();
  lcd.print(minuteval);
  lcd.print(colon);
  if (secondval < 10) printzero();
  lcd.print(secondval);

  buttons = lcd.readButtons();  // read the buttons on the shield

  if (buttons != 0) {                  // if a button was pressed
    if (buttons & BUTTON_LEFT) {       // if left pressed, break loop
      // Exit the loop 
      exit(0);  //The 0 is required to prevent compile error.
    }
    if (buttons & BUTTON_SELECT) {   // if select button pressed
      if (backlight)                // if the backlight is on
        backlight = OFF;           //   set it to off
      else                          // else turn on the backlight if off
        backlight = WHITE;         //   (you can select any color)
      lcd.setBacklight(backlight);  // set the new backlight state
    }
  }
  lcd.setCursor(11, 0);                 // This block prints the temp and PUs
  lcd.print(sensorNames[readCount % sensorCount]);
  lcd.print(" : ");
  lcd.setCursor(0, 1);                 // This block prints the temp and PUs
  dtostrf(currTemp[readCount % sensorCount], 3, 1, str_Cel);
  lcd.print(str_Cel);
  lcd.print(" C ");
  if (readCount % sensorCount > 0) {
    dtostrf(totalPU[readCount % sensorCount], 3, 1, str_PU);
    lcd.print(str_PU);
    lcd.print(" PUs");
  }

  sprintf(dataString, "%02d,%02d,%02d", Clock.hour(), Clock.minute(), Clock.second());
  for (int i = 0;  i < sensorCount;  i++) {
    strcat(dataString, ",");
    dtostrf(currTemp[i], 3, 1, str_Cel);
    strcat(dataString, str_Cel);
    if (i > 0) {
     strcat(dataString, ",");
     dtostrf(totalPU[i], 3, 1, str_PU);
     strcat(dataString, str_PU);
    }
  }

  // open the file. 
  dataFile = SD.open(logFileName, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println(F("error opening datalog.txt"));
  }
  delay(5000);  // wait 5 seconds
  readCount = readCount + 1;
  
}

void printzero() {  // prints a zero to the LCD for leading zeros
  lcd.print("0");   // a function saves multiple calls to the print function
}
