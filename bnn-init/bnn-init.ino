/*
*
* Initializer for Birdy Num Num Hardware
*
* Tool for setting the Realtime Clock, calibrating the Load Cell, and
* validating the uSD card's file system.
*
* TODO figure out how to write a decent CLI interface
* TODO see if this can be refactored to share code with the main application
*/

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
