#define DEBUG

#define NB_DETECT  1
#define SETUP_ROUND 100
#define DETECT_ROUND 10
#define CHIP_SELECT 53    // pin where the CS of the SD card module is connected
#define LASER_PIN 40       // pin controlling the lasers alimentation
#define RUN_PIN 10         // pin controlling the motor
#define START_BUTTON_PIN 42
#define STOP_BUTTON_PIN 43
#define LOGFILE "logs.txt"

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RTClib.h>
#include <Time.h>
#include <LiquidCrystal.h>


/*
 * led: digital output pin used to signal if the detector tripped
 * photo: analog input to read on to determine if the detector tripped
 * base_av: base value expected to be read on photo
 * last_read: last value read on photo
 * sum: sum of the DETECT_ROUND values, averaged to test if the detector tripped
 */
typedef struct s_detector
{
  byte photo;
  int base_av;
  int last_read;
  long sum;
} t_detector;

// take note that the pins 50 to 52 are reserved for SPI communication with the SD card module, and must not be used here
t_detector detects[NB_DETECT] =
{
  {0, 0, 0, 0},
/*  {1, 0, 0, 0},
  {2, 0, 0, 0},
  {3, 0, 0, 0},
  {4, 0, 0, 0},
  {5, 0, 0, 0},
  {6, 0, 0, 0},
  {7, 0, 0, 0},
  {8, 0, 0, 0},
  {9, 0, 0, 0} */
};

File logfile;
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
RTC_DS1307 RTC;
bool run = false;

void setup() {
  long  sum;

  // Serial only used while debugging
  #ifdef DEBUG
    Serial.begin(9600);
  #endif

  pinMode(STOP_BUTTON_PIN, INPUT);
  pinMode(START_BUTTON_PIN, INPUT);
  // the motor should not start until the start button is pressed
  pinMode(RUN_PIN, OUTPUT);
  digitalWrite(RUN_PIN, LOW);
  // Switch the lasers ON
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, HIGH);
  delay(100); // ensure the relay has time to switch ON before reading values from photodiodes

  /*
  * get the average value which will be used as the base value for each photodiode
  */
  for (int i = 0; i < NB_DETECT; ++i)
  {

    sum = 0;
    for (int j = 0; j < SETUP_ROUND; ++j)
    {
      detects[i].last_read = analogRead(detects[i].photo);
      sum += detects[i].last_read;
    }
    detects[i].base_av = sum / SETUP_ROUND;
    #ifdef DEBUG
      Serial.print("detector number ");
      Serial.print(i);
      Serial.print(": ");
      display_detect_info(detects + i);
    #endif
  }
  init_log();
  init_clock();
  lcd.begin(16, 2);
}

/* 
* initialize the SD card where the logs will be stored
* open LOGFILE with write access
*/
void init_log()
{
  #ifdef DEBUG
    Serial.println("Initializing SD card");
  #endif
  if (!SD.begin(CHIP_SELECT))
  {
    #ifdef DEBUG
      Serial.println("failed SD initialization");
    #endif
  }
  #ifdef DEBUG
    Serial.println("SD initialization OK");
    Serial.print("Opening logfile \"");
    Serial.print(LOGFILE);
    Serial.println("\"");
  #endif
  if (!(logfile = SD.open(LOGFILE, FILE_WRITE)))
  {
    #ifdef DEBUG
      Serial.println("failed to open logfile");
    #endif
  }
  #ifdef DEBUG
    Serial.println("logfile ready");
  #endif
}

void init_clock()
{
  Wire.begin();
  RTC.begin();
  if (!RTC.isrunning())
  {
    #ifdef DEBUG
      Serial.println("clock not running");
    #endif
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  #ifdef DEBUG
    Serial.print("Clock starting time: ");
    Serial.println(RTC.now().unixtime());
  #endif
}

DateTime t;

void loop() {
  int error_count = 0;

  if (digitalRead(STOP_BUTTON_PIN) == HIGH && run == true)
  {
    digitalWrite(RUN_PIN, LOW);
    run = false;
    #ifdef DEBUG
      Serial.println("Stopped");
    #endif
  }
  for (int i = 0; i < NB_DETECT; ++i)
    detects[i].sum = 0;
  for (int i = 0; i < DETECT_ROUND; ++i)
  {
    for (int j = 0; j < NB_DETECT; ++j)
    {
      detects[j].last_read = analogRead(detects[j].photo);
      detects[j].sum += detects[j].last_read;
    }
  }
  for (int i = 0; i < NB_DETECT; ++i)
  {
    #ifdef DEBUG
//      Serial.print("\t");
//      Serial.print(detects[i].sum / DETECT_ROUND);
    #endif
    if (detects[i].sum / DETECT_ROUND > detects[i].base_av + 10 )
    {
      t = RTC.now();
      log_error(i, detects[i].sum / DETECT_ROUND);
      display_error(i, detects[i].sum / DETECT_ROUND);
      digitalWrite(RUN_PIN, LOW);
      run = false;
      ++error_count;
      Serial.println("Stopped 2");
    }
  }
//  Serial.print("\t");
//  Serial.print(error_count);
  #ifdef DEBUG
//    Serial.println("");
  #endif
  if (run == false && error_count == 0 && digitalRead(START_BUTTON_PIN) == HIGH)
//    && digitalRead(STOP_BUTTON_PIN) == LOW)
  {
    #ifdef DEBUG
      Serial.println("Starting motor");
    #endif
    run = true;
    digitalWrite(RUN_PIN, HIGH);
  }
}

void display_error(int id, int av)
{
  static int line;

  String error = "";

  error += String(id / 10);
  error += String(id % 10);
  error += " ";
  error += String(av);
  error += " ";
  error += t.hour();
  error += ":";
  error += t.minute();
  lcd_print_line(error);
}

void lcd_print_line(String str)
{
  static int line;

  lcd.setCursor(0, line);
  lcd.print(str);
  line = (line ? 0 : 1);
}

void log_error(int id, int av)
{
  String error = "";

  error += "Id: ";
  error += String(id);
  error += "\tav: ";
  error += String(av);
  error += "\t";
  error += String(t.unixtime());
  send_log(error);
}

void send_log(String error)
{
  if (!logfile)
    init_log();
  if (!logfile)
    lcd_print_line("Check SD card");
  else
    logfile.println(error);
}

void display_detect_info(struct s_detector *det)
{
  Serial.print("photo=");
  Serial.print(det->photo);
  Serial.print(" | base_av=");
  Serial.println(det->base_av);
}

