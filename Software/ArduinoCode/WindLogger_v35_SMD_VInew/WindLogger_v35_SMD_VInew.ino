/********************************************************
/****** Wind Logger based upon the:  ********************
/****** DataDuino - Arduino DAQ UNIT ********************
/****** by Matt Little **********************************
/****** Date: 29/1/2014 *********************************
/****** info@re-innovation.co.uk ************************
/****** www.re-innovation.co.uk *************************
/********************************************************

  See www.re-innovation.co.uk for information and construction details
  
  This is sample code for the DataDuino.

/*************Details of Code*****************************

  The DataDuino is an Arduino based SD card datalogger.
  A PCF8563 Real Time Clock is used to timestamp the data.
  
  The pin assisgnments are as follows:
  D0 - Rx Serial Data
  D1 - Tx Serial Data
  D2 - RTC interrupt (1 second pulses)
  D3 - Anemometer 1 (pulse interrupt) (& Power to External Unit - eg GSM)
  D4 - LED DATA output
  D5 - Anemometer 2 (pulse interrupt)
  D6 - Calibrate Switch (pull LOW to set)
  D7 - Rx_GSM
  D8 - Tx_GSM
  D9 - Card Detect (SD)
  D10 - Chip Select - SD card
  D11 - MOSI SD card
  D12 - MISO SD card
  D13 - SPI CLock SD card
  
  A0 - Wind Vane measurement with 10k pull-down
  A1 - Internal battery voltage measurement (from potential divider)
  A2 - External voltage measurement (with potential divider) 0-40V DC
  A3 - Current measurement
  A4 - SDA - I2C connection to RTC
  A5 - SCK - I2C connection to RTC
  
  Counts pulses from a sensor (such as a anemometer or flow sensor)
  These are pulses are averaged into a wind speed.
  We also keep a record of the maximum in the averaging period.
  For turbulence measurement 
  
  Each logger has a reference (user adjustable).
  
  Data is written to a .csv file created on an SD card.
  A new file is created each day. If file alreay present then data is appended.
  The file name is created from the reference number and the date in the format:
  DXXXXXX.csv, where DXXXXXX is the date in the format DDMMYY. 
  
  Data is stored with human readable headers:

  // This will be added to a seperate 'calibrate' mode
  When in Calibrate mode:
  You can adjust the parameters of the device using serial commands. These parameters are stored in EEPROM.
  These are:
  "T??????E"
  This will change the time to HHMMSS
  "??????E"
  This will change the date to DDMMYY
  "S?????E"
  This will change the sample period to ????? seconds. Set to 00001 for 1 second data, set to 03600 for 1 hour data.
  The minimum is 1 second data. The maximum is 99999 seconds
  "R??E"
  This will change the reference to ??. 
  "OE"
  This will take the current reading and write it to the current offset.
  "V1???E"
  "V2???E"
  These select the two resistor values from the resistor potential divider.
  Values are in k Ohm.
  R1 is the top resistor, R2 is the lower resistor.
  "I???E"
  This sets the current gain value, in mV/A
 
  
  // Addedd Interrupt code from here:
  // PinChangeIntExample, version 1.1 Sun Jan 15 06:24:19 CST 2012
  // See the Wiki at http://code.google.com/p/arduino-pinchangeint/wiki 
  // for more information.
 
  Updates: 
  30/10/12 Code written - Matt Little
  31/10/12 writing code description and sorting out new sample timings - Matt Little
  19/9/13  Getting code to work on Arduino 1.0.5 - Matt Little
  5/2/14   Adding wind logging sections - Matt Little
  5/2/14   Sorting out sleep modes - Matt Little
  5/2/14   Adding HIH humidity/temperature sensor - Matt Little
  5/2/14   Sorting out Card Detect when card removed - Matt Little
  10/2/14  Sorting out card re-enter issue - Matt Little
  17/6/14  Adding pull-up enable for calibrate pin.- Matt Little
  17/6/14  Removing humidity sensor routines. Not needed -  Matt Little
  17/6/14  Adding debounce timer for anemometer pulses - Matt Little
  17/6/14  Adding Direction vane input - Matt Little
  10/7/14  Adding additional Anemometer input - Matt Little
  10/7/14  Needs additional interrupt pin added - Matt Little
  13/8/14  Added wind direction data - Matt Little
  15/8/14  Added 'I'm Alive' LED flash every 5 seconds - Matt Little
  11/3/15  Added string output when NO SD and in Calibrate Mode
  18/3/15  Changed code for V and I SMD design - Matt Little
  20/3/15  More work on V and I code. SMD design - Matt Little
  15/4/15  Re-doing code for new hardware - Matt Little
  21/4/15  Adding voltage and current calibration factors in serial - Matt Little
  
  TO DO
  Sort out Voltage conversion (via serial) - implemented - TEST
  Sort out Current conversion (via serial) - implemented - TEST
  Sort out RPM sensor (want RPM value, so convert correctly)
  Sort out maximum wind speed in time period
 
 //*********SD CARD DETAILS***************************	
 The SD card circuit:
 SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 10
 ** Card detect - pin 9
 
 //************ Real Time Clock code*******************
 A PCF8563 RTC is attached to pins:
 ** A4 - SDA (serial data)
 ** A5 - SDC (serial clock)
 ** D2 - Clock out - This gives a 1 second pulse to record the data
 
 RTC PCF8563 code details:
 By Joe Robertson, jmr
 orbitalair@bellsouth.net
 
**********************************************************************************************************/


/************ External Libraries*****************************/
#include <stdlib.h>
#include <Wire.h>          // Required for RTC
#include <Rtc_Pcf8563.h>   // RTC library
#include <SdFat.h>            // SD card library
#include <avr/pgmspace.h>  // Library for putting data into program memory
#include <EEPROM.h>        // For writing values to the EEPROM
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <PinChangeInt.h>  // For additional interrupts

/************ Application Libraries*****************************/
#include "battery.h"

/************User variables and hardware allocation**********************************************/

/******* SD CARD*************/
#define chipSelect 10 // The SD card Chip Select pin 10
#define cardDetect 9  // The SD card detect is on pin 6
// The other SD card pins (D11,D12,D13) are all set within SD.h
int cardDetectOld = LOW;  // This is the flag for the old reading of the card detect

// SD file system object
SdFat sd;
//Log file
SdFile datafile;  

//File datafile;   // The logging file
String dataString;    // This is the holder for the data as a string. Start as blank
int counter = 0;   // Clue is in the name - its a counter.
long dataCounter = 0;  // This holds the number of seconds since the last data store

/*************Real Time Clock*******/
Rtc_Pcf8563 rtc;
#define I2C_RTC 0x51 // 7 bit address (without last bit - look at the datasheet)
#define RTCinterrupt 0  // RTC interrupt - This is pin 2 of ardunio - which is INT0

/************* Pulse counter *******/
#define pulseInterrupt 1  // Pulse Counter Interrupt - This is pin 3 of arduino - which is INT1
#define ANEMOMETER1 3  //   This is digital pin the pulse is attached to
#define ANEMOMETER2 5  //   This is digital pin the pulse is attached to

/********* I/O Pins *************/
#define LEDred 4      // The output led is on pin 4
#define calibrate 6   // This controls if we are in serial calibrate mode or not

#define vanePin A0      // The wind vane with a 10k pull DOWN

#define voltagePin A2  // The external voltage with a potential divider (680k // 46k)
#define current1Pin A3  // Current from a hall effect sensor

/********** Wind Direction Storage *************/
String WindDirection = " ";  // Empty to start with
int windDirectionArray[] = {0,0,0,0,0,0,0,0};  //Holds the frequency of the wind direction

///********** Thermistor Data Storage ************/
//#define thermistor A0  // This is the analog pin for the thermistor
//float TempC = 0;  // This holds the converted value of temperature
//char TempCStr[6];  // A string buffer to hold the converted string
//// enumarating 3 major temperature scales
//enum {
//  T_KELVIN=0,
//  T_CELSIUS,
//  T_FAHRENHEIT
//};
//// Manufacturer data for episco k164 10k thermistor
//// simply delete this if you don't need it
//// or use this idea to define your own thermistors
////#define EPISCO_K164_10k 4300.0f,298.15f,10000.0f  // B,T0,R0
//#define GT_Thermistor_10k 4126.0f,298.15f,10000.0f  // B,T0,R0
////#define Vishay_Thermistor_47k 4090.0f,298.15f,47000.0f  // B,T0,R0
/////********* Thermistor Temperature sensor****************/
//float temp;        // Temporary store for float

// Lets use a dallas 1-wire sensotr, to save an analog value.

///********* External Voltage ****************/
int  r1,r2;  // The potential divider values  
float externalVoltage;        // Temporary store for float
char ExternalVoltStr[6];      // Hold the battery voltage as a string

///********* Current 1 ****************/
long int currentData1;      // Temp holder for value
float current1;        // Temporary store for float
float currentOffset;  // Holds the offset current
int currentOffsetInt;  // Holds the current offset as an in for EEPROM storing
char Current1Str[7];      // Hold the current as a string
int iGain;    // Holds the current conversion factor in mV/A

// ****** Serial Data Read***********
// Variables for the serial data read
char inByte;         // incoming serial char
String str_buffer = "";  // This is the holder for the string which we will display

//********Variables for the Filename*******************

char filename[] = "DXXXXXX.csv";  // This is a holder for the full file name
char deviceID[3]; // A buffer to hold the device ID

long sampleTime = 2;  // This is the time between samples for the DAQ
                      // Sample time is stored in EEPROM in locations 2 & 3

// Variables for the Pulse Counter
volatile long pulseCounter1 = 0;  // This counts pulses from the flow sensor  - Needs to be long to hold number
volatile long pulseCounter1Old = 0;  // This is storage for the old flow sensor - Needs to be long to hold number

volatile long pulseCounter2 = 0;  // This counts pulses from the flow sensor  - Needs to be long to hold number
volatile long pulseCounter2Old = 0;  // This is storage for the old flow sensor - Needs to be long to hold number

volatile boolean writedataflag = HIGH;  // A flag to tell the code when to write data

long windMaximum = 0;  // This holds the maximum wind speed over the  

// Varibales for writing to EEPROM
int hiByte;      // These are used to store longer variables into EEPROM
int loByte;

// Varibles for 'I'm alive' flash
int aliveFlashCounter = 0;  // This is used to count to give flash every 10 seconds

// These next ints are for the filename conversion
int day_int =0;      // To find the day from the Date for the filename
int day_int1 =0;
int day_int2 =0;
int month_int = 0;
int month_int1 = 0;
int month_int2 = 0;
int year_int = 0;  // Year
int hour_int = 0;
int min_int = 0;
int sec_int = 0;

boolean calibrateFlag = HIGH;  // This flag is lowered if we are in calibrate mode (switch ON)
boolean debugFlag = LOW;    // Set this if you want to be in debugging mode.

//**********STRINGS TO USE****************************
String comma = ",";
String date;        // The stored date from filename creation
String newdate;     // The new date, read every time 

// These are Char Strings - they are stored in program memory to save space in data memory
// These are a mixutre of error messages and serial printed information
const char headers[] PROGMEM = "Ref, Date, Time, RPM, Wind, Direction, Batt V, Ext V, Current";  // Headers for the top of the file
const char headersOK[] PROGMEM = "Headers OK";
const char erroropen[] PROGMEM = "Error open";
const char error[] PROGMEM = "ERROR";
const char initialisesd[] PROGMEM = "Init SD";
const char dateerror[] PROGMEM = "Date ERR";
const char reference[] PROGMEM = "The ref is:";
const char noSD[] PROGMEM = "No SD card";

#define MAX_STRING 130      // Sets the maximum length of string probably could be lower
char stringBuffer[MAX_STRING];  // A buffer to hold the string when pulled from program memory

/***************************************************
 *  Name:        pulse1
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: Count pulses from Anemometer 1
 *
 ***************************************************/
void pulse1(void)
{
  // If the anemometer has spun around
  // Increment the pulse counter
  pulseCounter1++;
  // ***TO DO**** Might need to debounce this
}

/***************************************************
 *  Name:        pulse2
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: Count pulses from Anemometer 2
 *
 ***************************************************/
void pulse2(void)
{
  // If the anemometer has spun around
  // Increment the pulse counter
  pulseCounter2++;
  // ***TO DO**** Might need to debounce this
}

/***************************************************
 *  Name:        RTC
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: I use the CLK_OUT from the RTC to give me exact 1Hz signal
 *               To do this I changed the initialise the RTC with the CLKOUT at 1Hz
 *
 ***************************************************/
void RTC()
{ 
  detachInterrupt(RTCinterrupt);
  dataCounter++;
  aliveFlashCounter++;
  
  if(writedataflag==LOW&&dataCounter>=sampleTime)  // This stops us loosing data if a second is missed
  { 
    // If this interrupt has happened then we want to write data to SD card:
    // Save the pulsecounter value (this will be stored to write to SD card
    pulseCounter1Old = pulseCounter1;
    pulseCounter2Old = pulseCounter2;
   // Reset the pulse counter
    pulseCounter1 = 0;
    pulseCounter2 = 0;
    // Reset the DataCounter
    dataCounter = 0;  
    // Set the writedataflag HIGH
    writedataflag=HIGH;
  }
}

/***************************************************
 *  Name:        enterSleep
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: Enters the arduino into sleep mode.
 *
 ***************************************************/
void enterSleep(void)
{

  attachInterrupt(RTCinterrupt, RTC, RISING);
  
  sleep_enable();
   
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  
  
  byte old_ADCSRA = ADCSRA;  // Store the old value to re-enable 
  // disable ADC
  ADCSRA = 0;

  byte old_PRR = PRR;  // Store previous version on PRR
  // turn off various modules
  PRR = 0b11111111;
  
  sleep_cpu();
  /* The program will continue from here. */
  /************* ASLEEP *******************/
  
  // ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ //
  
  /************** WOKEN UP ***************/
  /* First thing to do is disable sleep. */
  sleep_disable();
  
  // turn ON various modules USART and ADC
  PRR = old_PRR;  
  
  // enable ADC
  ADCSRA = old_ADCSRA;  
}


/***************************************************
 *  Name:        setup
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: Setup for the Arduino.           
 *
 ***************************************************/
void setup()
{
  Serial.begin(115200);
  Wire.begin();
  
  //******Real Time Clock Set - up********
  // A4 and A5 are used as I2C interface.
  // D2 is connected to CLK OUT from RTC. This triggers an interrupt to take data
  // We need to enable pull up resistors
  pinMode(A4, INPUT);           // set pin to input
  digitalWrite(A4, HIGH);       // turn on pullup resistors
  pinMode(A5, INPUT);           // set pin to input
  digitalWrite(A5, HIGH);       // turn on pullup resistors
  pinMode(2,INPUT);    // Set D2 to be an input for the RTC CLK-OUT   
  //initialise the real time clock
  Rtc_Pcf8563 rtc; 

  //initialisetemp();  // Initialise the temperature sensors
  pinMode(LEDred,OUTPUT);    // Set D4 to be an output LED
  pinMode(cardDetect,INPUT);  // D9 is the SD card detect on pin 9.
 
  //Set up digital data lines
  pinMode(calibrate,INPUT_PULLUP);
  
  analogReference(EXTERNAL);  // This should be default, but just to be sure

  // Analog lines
  pinMode(vanePin,INPUT);
  pinMode(voltagePin,INPUT);
  pinMode(current1Pin,INPUT); 

  // Put unused pins to INPUT to try and save power...      
  
  setupRTC();  // Initialise the real time clock  
  
  initialiseSD();    // Inisitalise the SD card   
  createfilename();  // Create the corrct filename (from date)

  // Read the reference number from the EEROM
  deviceID[0] = char(EEPROM.read(0));
  deviceID[1] = char(EEPROM.read(1));
  
  // Read in the sample time from EEPROM
  hiByte = EEPROM.read(2);
  loByte = EEPROM.read(3);
  sampleTime = (hiByte << 8)+loByte;  // Get the sensor calibrate value 
  
  // Read the Current Voltage Offset from the EEROM
  hiByte = EEPROM.read(4);
  loByte = EEPROM.read(5);
  currentOffsetInt = (hiByte << 8)+loByte;  // Get the sensor calibrate value
  currentOffset = float(currentOffsetInt)*3.3f/1023.0f;   // Convert the current offset to a voltage
  
  // read the potential divider resistors.
  hiByte = EEPROM.read(6);
  loByte = EEPROM.read(7);
  r1 = (hiByte << 8)+loByte;  // Get the sensor calibrate value

  hiByte = EEPROM.read(8);
  loByte = EEPROM.read(9);
  r2 = (hiByte << 8)+loByte;  // Get the sensor calibrate value
 
  // read the current gain value
  hiByte = EEPROM.read(10);
  loByte = EEPROM.read(11);
  iGain = (hiByte << 8)+loByte;  // Get the sensor calibrate value
  
  // Interrupt for the 1Hz signal from the RTC
  attachInterrupt(RTCinterrupt, RTC, RISING); 
  // Attach interrupts for the pulse counting
  pinMode(ANEMOMETER1, INPUT); 
  digitalWrite(ANEMOMETER1, HIGH);
  PCintPort::attachInterrupt(ANEMOMETER1, &pulse1, FALLING);  // add more attachInterrupt code as required
  pinMode(ANEMOMETER2, INPUT); 
  digitalWrite(ANEMOMETER2, HIGH);
  PCintPort::attachInterrupt(ANEMOMETER2, &pulse2, FALLING); 

}

/***************************************************
 *  Name:        main loop
 *
 *  Returns:     Nothing.
 *
 *  Parameters:  None.
 *
 *  Description: Main application loop.
 *
 ***************************************************/
void loop()
{

  // *********** WIND DIRECTION **************************************  
  // Want to measure the wind direction every second to give good direction analysis
  // This can be checked every second and an average used
   convertWindDirection(analogRead(vanePin));    // Run this every second. It increments the windDirectionArray 
 
  if(aliveFlashCounter>=10)
  {
    // Flash the LED every 10 seconds to show alive
    pinMode(LEDred,OUTPUT);    // Set LED to be an output LED 
    digitalWrite(LEDred, HIGH);   // set the LED ON
    delay(5);
    digitalWrite(LEDred, LOW);   // set the LED OFF 
    aliveFlashCounter=0;  // Reset the counter 
  }
  
  if(writedataflag==HIGH)
  {  
    pinMode(LEDred,OUTPUT);    // Set LED to be an output LED 
    digitalWrite(LEDred, HIGH);   // set the LED ON

    // *********** WIND SPEED ******************************************
    // Want to get the number of pulses and average into the sample time
    // This gives us the average wind speed
    // pulsecounterold holds the value of pulses.
    // This can be converted into the wind speed using the time and 
    // the pulse-wind speed characterisitic of the anemometer.
    // Do this as post processing - pulse count is most important.
    
    // *********** WIND DIRECTION **************************************
    // This can be checked every second and an average used
    WindDirection = analyseWindDirection();
    for(int i=0;i<8;i++)
    {
      //Resets the wind direction array
      windDirectionArray[i]=0;
    }

//    // *********** TEMPERATURE *****************************************
//    // Two versions of this - either with thermistor or I2C sensor (if connected)
//    // Thermistor version
//    // Get the temperature readings and store to variables
//    TempC = Temperature(thermistor,T_CELSIUS,GT_Thermistor_10k,1000.0f);
//    dtostrf(TempC,2,2,TempCStr);  // Convert the temperature value (double) into a string
//    
////    if(debugFlag==HIGH)
////    {
////      Serial.print("Therm: ");
////      Serial.println(TempCStr);  
////    }   

    updateBatteryVoltage(analogRead(battVoltagePin));
    
    // *********** EXTERNAL VOLTAGE ***************************************
    // From Vcc-680k--46k-GND potential divider
    // This is to test in case battery voltage has dropped too low - alert?
    externalVoltage = float(analogRead(voltagePin))*(3.3f/1023.0f)*((float(r1)+float(r2))/float(r2));        // Temporary store for float
    dtostrf(externalVoltage,2,2,ExternalVoltStr);     // Hold the battery voltage as a string
    
    // ********** EXTERNAL CURRENTS **************************************
    // Measured using a hall effect current sensor
    // Either using a ACS*** or a LEM HTFS 200-P
    // Comment out whichever you are not using
 
    currentData1 = 0;  // Reset the value
    // Lets average the data here over 20 samples.
    for(int i = 0;i<=19;i++)
    {  
      currentData1 += analogRead(current1Pin);
      delay(2);
    }
    current1 = float(currentData1)/20.0f;  
    current1 = (current1*3.3f/1023.0f) - currentOffset;
    // Current 1 holds the incoming voltage.
     
    // ********** LEM HTFS 200-P SENSOR *********************************
    // Voutput is Vref +/- 1.25 * Ip/Ipn 
    // Vref = Vsupply/2 +/1 0.025V (Would be best to remove this with analog stage)
    //current1 = (current1*200.0f)/1.25f;
    current1 = current1*float(iGain);  
  
//    // ************* ACS*** Hall Effect **********************
//    // Output is Input Voltage - offset / mV per Amp sensitivity
//    // Datasheet says 60mV/A     

    
    // Convert the current to a string.
    dtostrf(current1,2,2,Current1Str);     // Hold the battery voltage as a string
  
    // ******** put this data into a file ********************************
    // ****** Check filename *********************************************
    // Each day we want to write a new file.
    // Compare date with previous stored date, every second
    newdate = String(rtc.formatDate(RTCC_DATE_WORLD));  
    if(newdate != date)
    {
       // If date has changed then create a new file
       createfilename();  // Create the corrct filename (from date)
    }    

    // ********* Create string of data **************************
    dataString =  String(deviceID[0]); 
    dataString += deviceID[1];  // Reference
    dataString += comma;
    dataString += newdate;  // Date
    dataString += comma;
    dataString += String(rtc.formatTime()); // Time
    dataString += comma;
    dataString += String(pulseCounter1Old); // Wind pulses 1
    dataString += comma;
    dataString += String(pulseCounter2Old); // Wind pulses 2
    dataString += comma;
    dataString += WindDirection; // Wind direction
//    dataString += comma;
//    dataString += TempCStr; // Temperature
    dataString += comma;
    dataString += getBatteryVoltageStr();  // Battery voltage  
    dataString += comma;
    dataString += ExternalVoltStr;  // Current 1 reading
    dataString += comma;
    dataString += Current1Str;  // Current 2 reading
 
    
    // ************** Write it to the SD card *************
    // This depends upon the card detect.
    // If card is there then write to the file
    // If card has recently been inserted then initialise the card/filenames
    // If card is not there then flash LEDs

    if(digitalRead(cardDetect)==LOW&&cardDetectOld==HIGH)
    {
      delay(100);  // Wait for switch to settle down.
      // There was no card previously so re-initialise and re-check the filename
      initialiseSD();
      createfilename();
    }
    if(digitalRead(cardDetect)==LOW&&cardDetectOld==LOW)
    {
      //Ensure that there is a card present)
      // We then write the data to the SD card here:
      writetoSD();
    }
    else
    {
       // print to the serial port too:
       Serial.println(dataString);
       Serial.println(getString(noSD));
    }   
    cardDetectOld = digitalRead(cardDetect);  // Store the old value of the card detect
    
    // Finish up write routine here:    
    digitalWrite(LEDred, LOW);   // set the LED OFF 
    pinMode(LEDred,INPUT);    // Set LED to be an INPUT - saves power   
    writedataflag=LOW;
    Serial.flush();    // Force out the end of the serial data
  }
  
  // Want to check the SD card every second
  if(digitalRead(cardDetect)==HIGH)
  {
    pinMode(LEDred,OUTPUT);    // Set LED to be an output LED 
    // This meands there is no card present so flash the LED every second
    for(int x=0;x<=5;x++)
    {
      digitalWrite(LEDred, HIGH);   // set the LED ON
      delay(5);
      digitalWrite(LEDred, LOW);   // set the LED ON
      delay(50);     
    }
  } 
    
  if(debugFlag==HIGH)
  {
    // DEBUGGING ONLY........
    Serial.print("Anemometer1: ");
    Serial.println(pulseCounter1, DEC);
    Serial.print("Anemometer2: ");
    Serial.println(pulseCounter2, DEC);
  }
  
  // A Switch on D7 will set if the unit is in serial adjust mode or not  
  //calibrateFlag = digitalRead(calibrate);  
  
  if(digitalRead(calibrate)== HIGH)
  {    
    // We ARE in calibrate mode
    Serial.println("Calibrate");    
    getData();
    delay(500);  // Some time to read data
    Serial.flush();    // Force out the end of the serial data 

    writedataflag=HIGH;  // Set the write data flag high
  }
  else
  {     
    attachInterrupt(RTCinterrupt, RTC, RISING); 
    enterSleep();     
  }
  
}

// Set Up RTC routine
void setupRTC()
{
    // This section configures the RTC to have a 1Hz output.
  // Its a bit strange as first we read the data from the RTC
  // Then we load it back again but including the correct second flag  
  rtc.formatDate(RTCC_DATE_WORLD);
  rtc.formatTime();
  
  year_int = rtc.getYear();
  day_int = rtc.getDay();
  month_int = rtc.getMonth();  
  hour_int = rtc.getHour();
  min_int = rtc.getMinute();
  sec_int = rtc.getSecond(); 
  
  Wire.begin(); // Initiate the Wire library and join the I2C bus as a master
  Wire.beginTransmission(I2C_RTC); // Select RTC
  Wire.write(0);        // Start address
  Wire.write(0);     // Control and status 1
  Wire.write(0);     // Control and status 2
  Wire.write(DecToBcd(sec_int));     // Second
  Wire.write(DecToBcd(min_int));    // Minute
  Wire.write(DecToBcd(hour_int));    // Hour
  Wire.write(DecToBcd(day_int));    // Day
  Wire.write(DecToBcd(2));    // Weekday
  Wire.write(DecToBcd(month_int));     // Month (with century bit = 0)
  Wire.write(DecToBcd(year_int));    // Year
  Wire.write(0b10000000);    // Minute alarm (and alarm disabled)
  Wire.write(0b10000000);    // Hour alarm (and alarm disabled)
  Wire.write(0b10000000);    // Day alarm (and alarm disabled)
  Wire.write(0b10000000);    // Weekday alarm (and alarm disabled)
  Wire.write(0b10000011);     // Output clock frequency enabled (1 Hz) ***THIS IS THE IMPORTANT LINE**
  Wire.write(0);     // Timer (countdown) disabled
  Wire.write(0);     // Timer value
  Wire.endTransmission();
}

// Converts a decimal to BCD (binary coded decimal)
byte DecToBcd(byte value){
  return (value / 10 * 16 + value % 10);
}

//*********** FUNCTION TO INITIALISE THE SD CARD***************
void initialiseSD()
{
  // Initialize the SD card at SPI_HALF_SPEED to avoid bus errors 
  // We use SPI_HALF_SPEED here as I am using resistor level shifters.
  //if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
  
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(chipSelect, OUTPUT);
  
  // see if the card is present and can be initialized:
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) {
//    if(debugFlag==HIGH)
//    {
//      Serial.println("FAIL");
//    }
    // don't do anything more:
    // Want to turn on an ERROR LED here
    return;
  }
  else
  {
    if(debugFlag==HIGH)
    {
      Serial.println(getString(initialisesd));
    }
  }
}

// *********FUNCTION TO SORT OUT THE FILENAME**************
void createfilename()
{
  // Check there is a file created with the date in the title
  // If it does then create a new one with the new name
  // The name is created from:
  // DMMDDYY.CSV, where YY is the year MM is the month, DD is the day
  // You must add on the '0' to convert to ASCII
  
  date = String(rtc.formatDate());
  day_int = rtc.getDay();  // Get the actual day from the RTC
  month_int = rtc.getMonth();  // Get the month
  day_int1 = day_int/10;    // Find the first part of the integer
  day_int2 = day_int%10;    // Find the second part of the integer
  month_int1 = month_int/10;    // Find the first part of the integer
  month_int2 = month_int%10;    // Find the second part of the integer
  filename[1]=(year_int/10) + '0';  // Convert from int to ascii
  filename[2]=(year_int%10) + '0';  // Convert from int to ascii 
  filename[3]= month_int1 + '0';  // Convert from int to ascii
  filename[4]= month_int2 + '0';  // Convert from int to ascii   
  filename[5]= day_int1 + '0';  // Convert from int to ascii
  filename[6]= day_int2 + '0';  // Convert from int to ascii 
  
  if(debugFlag==HIGH)
  {
    Serial.println(filename);
  }
  
  if(!sd.exists(filename))
  {
    // open the file for write at end like the Native SD library
    if (!datafile.open(filename, O_RDWR | O_CREAT | O_AT_END)) 
    {
//      if(debugFlag==HIGH)
//      {
//        Serial.println(getString(erroropen));
//      }
    }
    // if the file opened okay, write to it:
    datafile.println(getString(headers));
    // close the file:
    datafile.sync();
//    if(debugFlag==HIGH)
//    {
//      Serial.println(getString(headersOK));
//    }
  } 
  else
  {
//    if(debugFlag==HIGH)
//    {
//      Serial.println("Filename exists");
//    }
  }
  
}

// This routine writes the dataString to the SD card
void writetoSD()
{
  datafile.open(filename, O_RDWR | O_CREAT | O_AT_END);    // Open the correct file
  // if the file is available, write to it:
  if (sd.exists(filename)) {
    datafile.println(dataString);
    datafile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }  
  // if the file isn't open, pop up an error:
  else {
//    if(debugFlag==HIGH)
//    {
//      Serial.println(getString(erroropen));
//    }
  }
}

// Get a string from program memory
// This routine pulls the string stored in program memory so we can use it
// It is temporaily stored in the stringBuffer
char* getString(const char* str) {
	strcpy_P(stringBuffer, (char*)str);
	return stringBuffer;
}

//// Temperature function outputs float , the actual
//// temperature
//// Temperature function inputs
//// 1.AnalogInputNumber - analog input to read from
//// 2.OuputUnit - output in celsius, kelvin or fahrenheit
//// 3.Thermistor B parameter - found in datasheet
//// 4.Manufacturer T0 parameter - found in datasheet (kelvin)
//// 5. Manufacturer R0 parameter - found in datasheet (ohms)
//// 6. Your balance resistor resistance in ohms  
//
//float Temperature(int AnalogInputNumber,int OutputUnit,float B,float T0,float R0,float R_Balance)
//{
//  float R,T,data;
//
//  //R=1024.0f*R_Balance/float(analogRead(AnalogInputNumber))-R_Balance;
//  
//  // Changes as using thermistor to ground:
//  data = float(analogRead(AnalogInputNumber));
//  R=(data*R_Balance)/(1024.0f-data);
//  
//  T=1.0f/(1.0f/T0+(1.0f/B)*log(R/R0));
//
//  switch(OutputUnit) {
//    case T_CELSIUS :
//      T-=273.15f;
//    break;
//    case T_FAHRENHEIT :
//      T=9.0f*(T-273.15f)/5.0f+32.0f;
//    break;
//    default:
//    break;
//  };
//
//  return T;
//}


// **********************GET DATA SUBROUTINE*****************************************
// This sub-routine picks up and serial string sent to the device and sorts out a power string if there is one
// All values are global, hence nothing is sent/returned

void getData()
{
    // **********GET DATA*******************************************
  // We want to find the bit of interesting data in the serial data stream
  // If we write H then house number then the code will update the house number in EEPROM
  // **** aslo need to write code to update RTC
  
  for(int i = 0; i<10;i++)  // This helps us just take a 'chunk' of data so does not fill up serial buffer
  {
    // get incoming bytes:
    if (Serial.available() > 0) 
    {
     inByte = Serial.read(); 
     str_buffer+=inByte;
    
     if(inByte=='E')    // We read everything up to the byte 'E' which stands for END
     {
       int buffer_length = str_buffer.length();  // We also find the length of the string so we know how many char to display 
       // Depending upon what came before we update different values
       // To change the reference number we enter R00E, where 00 can be any number up to 99 

        for(int i = buffer_length; i>=0; i--)  // Check the buffer from the end of the data, working backwards
        {
          if(str_buffer[i]=='R')
          {
              // In this case we have changed the house number, so UPDATE and store in EEPROM
              deviceID[0]=str_buffer[i+1];
              deviceID[1]=str_buffer[i+2];
              Serial.print(getString(reference));
              Serial.print(deviceID[0]);
              Serial.println(deviceID[1]);
              EEPROM.write(0,deviceID[0]);
              EEPROM.write(1,deviceID[1]);
              initialiseSD();
              createfilename();
          }          
          if(str_buffer[i]=='T')
          {
              // In this case we have changed the TIME, so UPDATE and store to RTC
              // The time is in the format  HHMMSS
              
              String hourstr = str_buffer.substring(i+1,i+3);
              int hour = atoi(&hourstr[0]);
              String minutestr = str_buffer.substring(i+3,i+5);
              int minute = atoi(&minutestr[0]);
              String secondstr = str_buffer.substring(i+5,i+7);
              int second = atoi(&secondstr[0]);
              //hr, min, sec into Real Time Clock
              rtc.setTime(hour, minute, second);

              initialiseSD();
              createfilename();
              
              Serial.println(String(rtc.formatTime())); // Time
          }
          if(str_buffer[i]=='D')
          {
              // In this case we have changed the DATE, so UPDATE and store to RTC
              // The time is in the format  DDMMYY
              
              String daystr = str_buffer.substring(i+1,i+3);
              int day = atoi(&daystr[0]);
              String monthstr = str_buffer.substring(i+3,i+5);
              int month = atoi(&monthstr[0]);          
              String yearstr = str_buffer.substring(i+5,i+7);
              int year = atoi(&yearstr[0]);          
           
              //day, weekday, month, century(1=1900, 0=2000), year(0-99)
              rtc.setDate(day, 3, month, 0, year);
              
              initialiseSD();
              createfilename();
              
              Serial.println(String(rtc.formatDate(RTCC_DATE_WORLD)));
          }           
          if(str_buffer[i]=='S')
          {          
              // In this case we have changed the sample time, so UPDATE and store to EEPROM
              // Data will be in the form of 5 x chars, signifying XXXXX, a value from 00001 to 99999 seconds
              
              sampleTime = atol(&str_buffer[i+1]);  // Convert the string to a long int
              
              EEPROM.write(2, sampleTime >> 8);    // Do this seperately
              EEPROM.write(3, sampleTime & 0xff);
              Serial.print("Sample Time:");
              Serial.println(sampleTime);
              
              dataCounter=0;  // Reset the data counter to start counting again.
          }
          
          if(str_buffer[i]=='O')
          {    
            // ******** Recalibrate the offset *******************
            // We want to measure the offset for the current sensor here. 
            // This will be changed to a calibrate function.
            // When switched on the unit should not have any current through the sensor or this
            // will read incorrectly.
            currentData1 = 0;  // Reset this holder
            for(int i = 0;i<=19;i++)
            {  
              currentData1 += analogRead(current1Pin);
              delay(20);
            }           
            currentOffsetInt = currentData1/20;
            
            currentOffset = float(currentOffsetInt)*3.3f/1023.0f;
            Serial.print("Ioffset:");
            Serial.print(currentOffset);
            Serial.println("V");               // Write this info to EEPROM   
            EEPROM.write(4, currentOffsetInt >> 8);    // Do this seperately
            EEPROM.write(5, currentOffsetInt & 0xff);  
          }   
          if(str_buffer[i]=='V'&&str_buffer[i+1]=='1')
          {    
            // ******** Change resistor r1 ******************
            String Rstr = str_buffer.substring(i+2,i+5);
            int value = Rstr.toInt();
            r1 = value;  // Use this new value
            Serial.print("R1:");
            Serial.println(value);   
            // Write this info to EEPROM   
            EEPROM.write(6, value >> 8);    // Do this seperately
            EEPROM.write(7, value & 0xff);  
          }        
          if(str_buffer[i]=='V'&&str_buffer[i+1]=='2')
          {    
            // ******** Change resistor r1 ******************
            String Rstr = str_buffer.substring(i+2,i+5);
            int value = Rstr.toInt();
            r2 = value; // Use this new value
            Serial.print("R2:");
            Serial.println(value);   
            // Write this info to EEPROM   
            EEPROM.write(8, value >> 8);    // Do this seperately
            EEPROM.write(9, value & 0xff);  
          } 
          if(str_buffer[i]=='I')
          {    
            // ******** Change Current gain value (mV) ******************
            String Rstr = str_buffer.substring(i+1,i+4);
            int value = Rstr.toInt();
            iGain = value; // Use this new value
            Serial.print("I Gain:");
            Serial.println(value);   
            // Write this info to EEPROM   
            EEPROM.write(10, value >> 8);    // Do this seperately
            EEPROM.write(11, value & 0xff);  
          } 
          
        }
        str_buffer="";  // Reset the buffer to be filled again 
      }
    }
  }
}

// ******** CALC DIRECTION *********
// This routine takes in an analog read value and converts it into a wind direction
// The Wind vane uses a series of resistors to show what direction the wind comes from
// The different values are (with a 10k to Ground):
//    R1 = 33k  => 238 N
//    R2 = 8.2k => 562 NE
//    R3 = 1k => 930 E
//    R4 = 2.2k => 839 SE
//    R5 = 3.9k => 736 S
//    R6 = 16k => 394 SW
//    R7 = 120k => 79 W
//    R8 = 64.9k => 137 NW

// The different values are (with a 10k to Vbattery):
// The value will be 1024 - vane integer reading

// This means we can 'band' the data into 8 bands

void convertWindDirection(int reading)
{
  
  // The reading has come from the ADC
  if(reading>0&&reading<100)
  {
    windDirectionArray[6]++;
  }
  else if(reading>100&&reading<200)
  {
    windDirectionArray[7]++;
  }
  else if(reading>200&&reading<350)
  {
    windDirectionArray[0]++; 
  }
  else if(reading>350&&reading<450)
  {
    windDirectionArray[5]++;
  }  
  else if(reading>450&&reading<650)
  {
    windDirectionArray[1]++;
  }  
  else if(reading>650&&reading<800)
  {
    windDirectionArray[4]++;
  }
  else if(reading>800&&reading<900)
  {
    windDirectionArray[3]++;
  }
  else if(reading>900&&reading<1024)
  {
    windDirectionArray[2]++;
  }
  else
  {
      // This is an error reading
  }
}

String analyseWindDirection()
{
  // When a data sample period is over we need to see the most frequent wind direction.
  // This needs to be converted back to a direction and stored on SD
  
  int data1 = windDirectionArray[0];
  int maxIndex = 0;
  // First need to find the maximum integer in the array
  for(int i=1;i<8;i++)
  {
    if(data1<windDirectionArray[i])
    {
      data1=windDirectionArray[i];
      maxIndex = i;
    }
  }
  // Serial.println(maxIndex);  Testing
    
  
  // Then convert that into the direction
  switch(maxIndex)
  {
    case 0:
      return("N");
    break;
    case 1:
      return("NE");
    break;    
    case 2:
      return("E");
    break;  
    case 3:
      return("SE");
    break;
    case 4:
      return("S");
    break;  
    case 5:
      return("SW");
    break;
    case 6:
      return("W");
    break;
    case 7:
      return("NW");
    break;
  }
}
