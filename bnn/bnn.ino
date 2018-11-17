/*
*
* Birdy Num Num: Temperature-calibrated, time-stampled, weight logging application
*
* Continuously monitors the (100g) Load Cell for significant (as defined by a
* given threshold value) change in weight of the feeder, and then writes the
* new weight, along with a timestamp and temperature reading, to a file on the
* uSD card.
* At each powerup, the uSD card's (FAT32) file system is read and if a config
* file exists, the contents are used to overwrite the default values for the
* application's variables.
* After this initialization is done, this application creates a new log file
* and gives it a name that includes the startup time.
* An initial reading of the sensors is made and a log is written to the file.
* This application will continue reading the Load Cell in a loop and compare
* the weight against the last recorded value.  When the difference between
* them exceeds a given amount, then a new log record is written and the last
* weight value is updated.
* Log records are of the form:
*  <timestamp>, <temperature>, <loadCell value>
* The timestamp is in ISO format.  The temperature is a float in degrees C.
* The Load Cell value is the raw (uncompensated) value read from the HX711.
* Log records contain raw values and all compensation and conversion (e.g.,
* Load Cell temperature compensation) is done in post-processing.
* The uSD file is flushed after each log record is written.  This way the
* file is kept in a consistent state so the power can be removed at any time
* without compromising the data log.
*
* TODO figure out how to close the file properly
* TODO figure out how to have the RTC and uSD card share the SPI bus properly
* TODO decide if ISO format is the right thing for timestamps or if it can be raw as well
* TODO decide if raw temp readings are possible
* TODO figure out if this file can be refactored into multiple files
*/


/*
* SparkFun microSD shield SparkFun Arduino Pro
* --------------------------------------------
*  MOSI - 11
*  MISO - 12
*  CLK  - 13
*  CS   -  8
*
* This pin assignment is given by the PCB, so must work around it.
*
* HX711 Load Cell Amp
* -------------------
* Calibration factor can be very positive or very negative. It all depends on
* the setup of your scale system and the direction the sensors deflect from
* zero state.  Calibration factor will be significantly different but it will
* be linearly related to lbs (1 lbs = 0.453592 kg).
*
*  CLK  - 2
*  DOUT - 3
*  VCC  - 5V
*  GND  - GND
*
* Most any pin on the Arduino Uno will be compatible with DOUT/CLK.
* The HX711 board can be powered from 2.7V to 5V so the Arduino 5V power
* should be fine.
*
* DS3234 Real-Time Clock
* ----------------------
* SparkFun DeadOn RTC Breakout - DS3234
*
*  SS   - 10 (library default: 10)
*  MOSI - 11 (library default: 11)
*  MISO - 12 (library default: 12)
*  CLK  - 13 (library default: 13)
*  SQW  - N/C (library default: 2) optional
*  VCC  - 5V
*  GND  - GND
*
* One Wire Digital Temperature Sensor - DS18B20
* ---------------------------------------------
* Looking at flat side, leads down, from left to right: GND, Data, VCC
* Data needs to be be pulled up to VCC with 4.7K ohm resistor
*
*  GND  - GND
*  Data - 4 (library default: 2)
*  VCC  - 5V
*
* I/O Pin Map:
*  A0: N/C (Analog In)
*  A1: N/C (Analog In)
*  A2: N/C (Analog In)
*  A3: N/C (Analog In)
*  A4: N/C (Analog In)
*  A5: N/C (Analog In)
*  D0: TX
*  D1: RX
*  D2: LCA Clk
*  ~D3: LCA DataOut
*  D4: DTS Data
*  ~D5: N/C
*  ~D6: N/C
*  D7: N/C
*  D8: USDS CS (SPI)
*  D9: N/C
*  ~D10: RTC SS (SPI)
*  ~D11: SPI MOSI -- USDS, RTC
*  D12: SPI MISO -- USDS, RTC
*  D13: SPI CLK -- USDS, RTC
*/

//// TODO
////  * make auto-calibration mode -- select reference weight and then optimize
////    - make a separate setup program that sets the clock and calibrates the scale
////  * log/display only single digit after the decimal point of scale
////  * average samples and emit/log only when different from last one -- make that an option
////    - don't do averaging unless cycle time is too slow for the full loop
////  * make logging with timestamp -- or use openlog connected to Arduino
////  * figure out how to get TOD for timestamps -- add TOD board
////  * add SD Card board to log data
////    - clear and reset when card if first inserted
////    - look for and read config file (copy OpenLog format)
////  * power things with LiPo pack -- make removable and build charging station
////  * make all *setup() functions return a boolean indicating success/failure
////  * figure out what to do if init fails
////  * make special case for dealing with scale calibrate and RTC set
////    - maybe do it through the conf file, or use the host's time -- only should have to do once
////  * figure out how to do temp calibration for load cell
////  * write start time at start of logfile
////  * have to make sure that RTC and USDC can share the SPI bus together


//
// Includes
//

#include <SPI.h>
#include <SD.h>
#include <HX711.h>
#include <SparkFunDS3234RTC.h>
#include <OneWire.h>
#include <DallasTemperature.h>


//
// Defines
//

////#define ENABLE_USDS
////#define ENABLE_LCA
////#define ENABLE_RTC
////#define ENABLE_DTS

#define USDS_CS     8    // The Sparkfun microSD shield uses pin 8 for CS

#define HX711_DOUT  3
#define HX711_CLK   2

#define CALIBRATION_MODE  0
#define WEIGH_MODE        1

#define RTC_CS  10    // share the SPI bus with the uSD card

#define ONE_WIRE_BUS  4

#define THERM_DEV_NUM  0


//
// Macros
//

////#define abs(x)  ( (x) > 0 ? (x) : -(x) )


//
// Typedefs and Structs
//

typedef struct BNN {
  String time;  // time of date of when load cell was sampled
  int16_t temp;  // temperature of load cell at time of sample
  double weight;  // current raw weight sample
  unsigned int waitPeriod;  // number of msec to wait between cycles
} bnn_t;


//
// Initialization
//

Sd2Card card;
SdVolume volume;
SdFile root;

#ifdef ENABLE_LCA
HX711 scale(HX711_DOUT, HX711_CLK);
#endif

float calibrationFactor = 13160.00;

#ifdef ENABLE_RTC
????
#endif

#ifdef ENABLE_DTS
// setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// pass oneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);
#endif

DeviceAddress thermDevAddr;

double lastWeight = 0;    //// FIXME select a better initialization value

// these define the default values, they are overwitten by values in the config file
bnn_t params = {
  ?, // time of weight sample
  0, // temp of load cell
  0.0, // weight sample
  0 // msec wait time
};

bool spiReady = false;


//
// Setup
//

// Setup the SPI Bus
void spiSetup() {
  if (spiReady == true) return;

  // Note that even if it's not used as the CS pin, the hardware SS pin
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output
  // or the SD library functions will not work.
  pinMode(10, OUTPUT);

  // ????
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE1);
  ????

  spiReady = true;
}

// uSD Card Setup
void usdsSetup(bnn_t *parmsPtr) {
  Serial.print("\nInitializing SD card...");

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working
  if (!card.init(SPI_HALF_SPEED, USDS_CS)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card is inserted?");
    Serial.println("* Is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    return;
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  // print the type of card
  Serial.print("\nCard type: ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  // try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    return;
  }

  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("\nVolume type is FAT");
  Serial.println(volume.fatType(), DEC);
  Serial.println();

  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize *= 512;                         // SD card blocks are always 512 bytes
  Serial.print("Volume size (bytes): ");
  Serial.println(volumesize);
  Serial.print("Volume size (Kbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Mbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);

  Serial.println("\nFiles found on the card (name, date and size in bytes): ");
  root.openRoot(volume);

  // list all files in the card with date and size
  root.ls(LS_R | LS_DATE | LS_SIZE);
}

// Load-Cell Amplifier Setup
void lcaSetup(double *weightPtr) {
  Serial.println("\nInitializing load-cell amplifier...");

  //// TODO make initialization optional
  Serial.println("Remove all weight from scale");
  Serial.println("After readings begin, place known weight on scale");
  Serial.println("Press + or a to increase calibration factor");
  Serial.println("Press - or z to decrease calibration factor");

  // reset the scale to 0
  scale.set_scale();
  scale.tare(100);

  // get a baseline reading
  long zeroFactor = scale.read_average();
  Serial.print("Zero factor: ");
  Serial.println(zeroFactor);

  // calibrate the load-cell
  if (0) {    //// FIXME
    scale.set_scale(calibrationFactor);

    Serial.print("Reading: ");
    Serial.print(scale.get_units(), 2);
    Serial.print(" grams");
    Serial.print(" calibrationFactor: ");
    Serial.print(calibrationFactor);
    Serial.println();

    if(Serial.available()) {
      char temp = Serial.read();
      if (temp == '+' || temp == 'a')
        calibrationFactor += 10;
      else if(temp == '-' || temp == 'z')
        calibrationFactor -= 10;
    }
  }
}

// Set time and date on the RTC
void SetTimeDate(int d, int mo, int y, int h, int mi, int s) {
  int TimeDate [7]={s, mi, h, 0, d, mo, y};
  for (int i = 0; (i <= 6); i++) {
    if (i == 3) i++;  // skip the null field

    // build the next timedate value
    int b = (TimeDate[i] / 10);
    int a = (TimeDate[i] - (b * 10));
    if (i == 2) {
      if (b == 2) b = B00000010;
      else if (b == 1) b = B00000001;
    }
    TimeDate[i] = (a + (b << 4));
    
    // write the next timedate value
    digitalWrite(RTC_CS, LOW);
    SPI.transfer(i + 0x80);
    SPI.transfer(TimeDate[i]);
    digitalWrite(RTC_CS, HIGH);
  }
}

// Read time and date from the RTC
String ReadTimeDate() {
  String temp;
  int TimeDate [7];  // second, minute, hour, null, day, month, year    
  for (int i = 0; (i <= 6); i++) {
    if (i == 3) i++;  // skip the null field

    // ????
    digitalWrite(RTC_CS, LOW);
    SPI.transfer(i + 0x00); 
    unsigned int n = SPI.transfer(0x00);        
    digitalWrite(RTC_CS, HIGH);

    // ????
    int a = (n & B00001111);
    if (i == 2) {
      int b = ((n & B00110000) >> 4);  // 24 hour mode
      if (b == B00000010) b = 20;
      else if (b == B00000001) b = 10;
      TimeDate[i] = (a + b);
    } else if (i == 4) {
      int b = ((n & B00110000) >> 4);
      TimeDate[i] = (a + (b * 10));
    } else if (i == 5) {
      int b = ((n & B00010000) >> 4);
      TimeDate[i] = (a + (b * 10));
    } else if (i == 6) {
      int b = ((n & B11110000) >> 4);
      TimeDate[i] = (a + (b * 10));
    } else {
      int b=(n & B01110000)>>4;
      TimeDate[i]=a+b*10; 
      }
  }

  // ????
  temp.concat(TimeDate[4]);
  temp.concat("/");
  temp.concat(TimeDate[5]);
  temp.concat("/");
  temp.concat(TimeDate[6]);
  temp.concat("     ");
  temp.concat(TimeDate[2]);
  temp.concat(":");
  temp.concat(TimeDate[1]);
  temp.concat(":");
  temp.concat(TimeDate[0]);
  return(temp);
}

// Real-Time Clock Setup
void rtcSetup() {
  pinMode(RTC_CS, OUTPUT);
  digitalWrite(RTC_CS, LOW);
  SPI.transfer(????);  // 0x8E == ????
  SPI.transfer(????);  // 0x60 == disable Osc and Batter SQ wave @ 1Hz, temp compensation, alarms disabled
  digitalWrite(RTC_CS, HIGH);
  delay(3);

  if (true) {
    //// use network to set time and date
  }
}

// Dallas Temperature (OneWire) Sensor Setup
void dtsSetup() {
  Serial.println("\nInitializing temperature IC...")

  sensors.begin();

  if (!sensors.getAddress(thermDevAddr, THERM_DEV_NUM)) {
    Serial.println("Unable to find address of thermometer device");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Birdy Num Num: Hummingbird Feeder Application");

#ifdef ENABLE_USDS
  usdsSetup(*params);
#endif

#ifdef ENABLE_LCA
  lcaSetup(&lastWeight);
#endif

#ifdef ENABLE_RTC
  rtcSetup(*params);
#endif

#ifdef ENABLE_DTS
  dtsSetup();
#endif
}


//
// Main Loop
//

void loop() {
  // sample the weight
#ifdef ENABLE_LCA
  //// FIXME sample the weight here
#else
  params.weight = 0;
#endif

  // determine if the change is significant
  if (abs(weight - lastWeight) > weightThreshold) {
    // got significant change, so get time
#ifdef ENABLE_RTC
    //// FIXME
#else
    params.time = 0;
#endif

    // get temperature
#ifdef ENABLE_DTS
    sensors.requestTemperatures();
    params.temp = sensors.getTemp(thermDevAddr);
#else
    params.temp = 0;
#endif

    // write log to file
#ifdef ENABLE_LCA
#else
    Serial.print("Time=%d, Temp=%d, Weight=%d", time, temp, weight);
    //// TODO
#endif

    lastWeight = params.weight;
  }

  // delay the defined amount before the next cycle
  delay(waitPeriod);
}
