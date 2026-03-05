#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define TRIG_PIN 4
#define ECHO_PIN 5
#define MICROPHONE_OUT_PIN 38
#define SOUND_SPEED 343

/// @brief GPIO pin connected to the LED.
#define LED_PIN 20
/// @brief I2C data pin
#define SDA_PIN 1
/// @brief I2C clock pin
#define SCL_PIN 2
/// @brief I2C address of the LCD module
#define LCD_ADDR 0x27

// Macros for bitwise operations
/// @brief Bit mask to enable data transmission of the LCD
#define ENABLE 0x04
/// @brief Bit mask to enable backlight of the LCD
#define BACKLIGHT 0x08
/// @brief Bit mask to send data
#define DATA_MODE 0x01
/// @brief Bit mask to send a command
#define CMD_MODE 0x00

#define CLAP_MODE 1
#define KEY_MODE 2
#define VOLUME_MODE 3
#define RESET_MODE 0

/// @brief LCD object configured using I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);
/// @brief Distance from ultrasonic sensor to object 
volatile float uDistance = 20.0;
volatile unsigned long previousClap;
volatile unsigned int currentBPM;
volatile unsigned int MODE = CLAP_MODE;
volatile bool clapDetected = false;

/**
* @brief Sends a 4 bit data packet to the LCD
* 
* The function sends a data packet to the LCD. It sets enable 
* HIGH to signal that data or a command is incoming, then LOW 
* for the LCD device to process it.
*
* @param data the 4 bit data to send
*/
void lcdTransmission(uint8_t data) {
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(data | ENABLE);
  Wire.endTransmission();
  // Delay to ensure correct transmission
  delay(10);
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(data & ~ENABLE);
  Wire.endTransmission();
}

/**
* @brief Splits a character in two data packets, then sends them
* 
* @param c the character to send
*/
void sendChar(char c) {
  // Add backlight and put in data mode
  uint8_t highChar = (c & 0xF0) | BACKLIGHT | DATA_MODE;
  uint8_t lowChar = ((c << 4) & 0xF0) | BACKLIGHT | DATA_MODE;
  lcdTransmission(highChar);
  lcdTransmission(lowChar);
}

/**
* @brief Splits a command in two data packets, then sends them
* 
* @param c the command to send
*/
void sendCmd(uint8_t c) {
  // Add backlight and put in command mode
  uint8_t highCmd = (c & 0xF0) | BACKLIGHT | CMD_MODE;
  uint8_t lowCmd = ((c << 4) & 0xF0) | BACKLIGHT | CMD_MODE;
  lcdTransmission(highCmd);
  lcdTransmission(lowCmd);
}

/**
* @brief Splits a String into characters, then sends each character
* 
* @param str the string to send
*/
void sendString(String str) {
  // Send each character one by one
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    sendChar(c);
  }
}

void displayLCD(String s1, String s2) {
  lcd.setCursor(0, 0);
  sendString(s1);
  lcd.setCursor(0, 1);
  sendString(s2);
}


void getDistance(void* arg) {
  // We send the trigger
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure echo
  long duration = pulseIn(ECHO_PIN, HIGH);

  // Convert to distance in centimeters and store
  uDistance = (duration * SOUND_SPEED / 2.0) * 0.0001;
}

void IRAM_ATTR handleClap() {
  if (MODE != CLAP_MODE) return;

  unsigned long currentClap = micros();
  // Using 1000ULL (Unsigned Long Long) to force integer math
  unsigned long millisDifference = (currentClap - previousClap) / 1000ULL;

  if (millisDifference < 300) {
    return; // Debounce: ignore too-fast triggers
  }

  if (millisDifference <= 2000) {
    currentBPM = 60000 / millisDifference;
    clapDetected = true;
  }
  
  // Always update previousClap so the next difference is calculated from THIS clap
  previousClap = currentClap;
}

void updateLCD(void* arg) {
  String s1;
  String s2;
  if (MODE == CLAP_MODE) {
    s1 = "BPM mode";
    s2 = "Current BPM: " + currentBPM;
  } 

}

void setup() {
  Serial.begin(115200);
  
  // Initialize timing variable
  previousClap = micros();

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(MICROPHONE_OUT_PIN, INPUT);

  // Attach interrupt LAST after everything is initialized
  attachInterrupt(digitalPinToInterrupt(MICROPHONE_OUT_PIN), handleClap, FALLING);
}

void loop() {
  getDistance(NULL);

  Serial.print("Distance: ");
  Serial.print(uDistance);
  Serial.println(" cm");

  if (clapDetected) {
    Serial.print("BPM : ");
    Serial.println(currentBPM);
    clapDetected = false;
  }

  delay(500);
}