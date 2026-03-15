/**Final-project.ino
 * @file   Final-project.ino
 * @author    Connor Roane, Louis Bernard
 * @date      15-March-2026
 * @brief   electronic musical instrument
 *   
 * Electronic Musical Instrument, takes distance input as well as sound input to control a buzzer
 */

/// @brief library includes
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

/// @brief pin defines
#define IR_RECEIVER_PIN 3
#define TRIG_PIN 4
#define ECHO_PIN 5
#define MICROPHONE_OUT_PIN 38
#define SOUND_SPEED 343
#define BUZZER_PIN 19

/// @brief GPIO pin connected to the LED.
#define LED_PIN 20
/// @brief I2C data pin
#define SDA_PIN 10
/// @brief I2C clock pin
#define SCL_PIN 11
/// @brief I2C address of the LCD module
#define LCD_ADDR 0x27

/// @brief Macros for bitwise operations
/// @brief Bit mask to enable data transmission of the LCD
#define ENABLE 0x04
/// @brief Bit mask to enable backlight of the LCD
#define BACKLIGHT 0x08
/// @brief Bit mask to send data
#define DATA_MODE 0x01
/// @brief Bit mask to send a command
#define CMD_MODE 0x00

/// @brief mode value defines
#define CLAP_MODE 1
#define KEY_MODE 2
#define RESET_MODE 0
#define BUZZER_PIANO_MODE 0
#define BUZZER_SYNTH_MODE 1
#define RESOLUTION 8

/// @brief ir remote initialization
IRrecv irRemote(IR_RECEIVER_PIN);
decode_results results;

/// @brief LCD object configured using I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

/// @brief handClap variable initialization
volatile unsigned long previousClap;
volatile unsigned int currentBPM;

/// @brief setting base modes
volatile unsigned int MODE = CLAP_MODE;
volatile unsigned int buzzerMode = BUZZER_PIANO_MODE;

/// @brief Queue handling to send data between tasks
QueueHandle_t distanceQueue;
QueueHandle_t bpmQueue;
QueueHandle_t frequencyQueue;

/// @brief piano notes C4, D4, E4, F4, G4, A4, B4, C5
const int pianoScale[] = {262, 294, 330, 349, 392, 440, 494, 523};
const char* keyScale[] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};

/// @brief interrupt variables
volatile uint32_t lastInterruptTime = 0;
volatile bool clapDetected = false;

/// @brief beatTimer 
hw_timer_t *beatTimer = NULL;
volatile bool beatFlag = false;

/// @brief timer1 (for getdistance function)
hw_timer_t *timer1 = NULL;
TaskHandle_t distanceTaskHandle = NULL;


// timer1 interrupt to run get distance
void IRAM_ATTR onTimer1() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(distanceTaskHandle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

void getDistance(void* arg) {

  for(;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (MODE == KEY_MODE) {
      // We send the trigger
      digitalWrite(TRIG_PIN, LOW);
      delayMicroseconds(2);
      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(TRIG_PIN, LOW);

      // Measure echo
      long duration = pulseIn(ECHO_PIN, HIGH, 15000);

      // Convert to distance in centimeters and store
      float distance = (duration * SOUND_SPEED / 2.0) * 0.0001;

      if (duration == 0) {
        // If no pulse detected we say that the object is very far way
        distance = 999.0;
      }

      xQueueSend(distanceQueue, &distance, 0);
    }
  }
}

void IRAM_ATTR handleClap() {
  if (MODE != CLAP_MODE) return;

  uint32_t now = millis();

  if (now - lastInterruptTime < 150) return;
  lastInterruptTime = now;
  
  clapDetected = true;
}

void IRAM_ATTR onBeatTimer() {
  beatFlag = true;
}

void updateLCD(void* arg) {
  int tempFreq, tempBPM;
  int receivedFreq = 262;
  int receivedBPM = 0;

  for (;;) {
    bool update = false;

    while (xQueueReceive(frequencyQueue, &tempFreq, 0) == pdPASS) {
      receivedFreq = tempFreq;
      update = true;
    }

    while (xQueueReceive(bpmQueue, &tempBPM, 0) == pdPASS) {
      receivedBPM = tempBPM;
      update = true;
    }

    if (update) {
      String s1 = "";
      String s2 = "";

      if (MODE == CLAP_MODE) {
        s1 = String("BPM mode");
        s2 = String("Current BPM: ") + String(receivedBPM);
      } else if (MODE == KEY_MODE) {
        int index = map(receivedFreq, 262, 523, 0, 7);
        index = constrain(index, 0, 7);
        s1 = String("Key mode");
        s2 = String("Key : ") + keyScale[index];
      }
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(s1);
      lcd.setCursor(0, 1);
      lcd.print(s2);
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

//ir receiver code
void irReciever(void* arg){
  while(1){
    if (irRemote.decode(&results)) {

      if(results.value == 0xFFA25D){ // if 1 pressed then enter clap mode
        MODE = CLAP_MODE;
      } else if(results.value == 0xFF9867){ // if 0 pressed then enter reset mode
        MODE = RESET_MODE;
      } else if(results.value == 0xFF629D){ //if 2 pressed then enter Key mode
        MODE = KEY_MODE;
      }

      if(MODE == KEY_MODE && results.value == 0xFF10EF){ //if in key mode and press left arrow then enter piano mode
        buzzerMode = BUZZER_PIANO_MODE;
      } else if(MODE == KEY_MODE && results.value == 0xFF5AA5){ // if in key mode and press right arrow then enter synth mode
        buzzerMode = BUZZER_SYNTH_MODE;
      }

      irRemote.resume();

    }
  vTaskDelay(pdMS_TO_TICKS(20));
  }
}
//key for ir receiver values
// 1 - 0xFFA25D
// 2 - 0xFF629D
// 3 - 0xFFE21D
// 4 - 0xFF22DD
// 5 - 0xFF02FD
// 6 - 0xFFC23D
// 7 - 0xFFE01F
// 8 - 0xFFA857
// 9 - 0xFF906F
// 0 - 0xFF9867
// * - 0xFF6897
// # - 0xFFB04F
// up arrorw - 0xFF18E7
// down - 0xFF4AB5
// left - 0xFF10EF
// right - 0xFF5AA5
// ok - 0xFF38C7

// buzzer code 
void buzz(void* arg){
  uint8_t clapEvent;
  uint32_t previousClapTime = 0;
  int frequency = 400;
  float receivedDistance;
  int oldFrequency = 0;
  TickType_t lastBeatTime = xTaskGetTickCount();

  ledcAttach(BUZZER_PIN, frequency, 6);

  for(;;) {
    if (MODE == CLAP_MODE) {
      if (clapDetected) {
        clapDetected = false;
        uint32_t clapTime = millis();
        uint32_t diff = clapTime - previousClapTime;

        Serial.print("Test");
        Serial.print("Diff : ");
        Serial.println(diff);

        if (diff > 300 && diff < 2000) {
          currentBPM = 60000 / diff;
        }

        previousClapTime = clapTime;

        if (currentBPM > 0) {
          uint32_t interval_us = 60000000 / currentBPM;

          timerAlarmWrite(beatTimer, interval_us, true);
          timerAlarmEnable(beatTimer);
        }

        int bpm = currentBPM;
        xQueueSend(bpmQueue, &bpm, 0);
      } 
      if (beatFlag) {
        beatFlag = false;

        ledcChangeFrequency(BUZZER_PIN, 150, 6);
        ledcWrite(BUZZER_PIN, 127);
        vTaskDelay(pdMS_TO_TICKS(50));
        ledcWrite(BUZZER_PIN, 0);
      }
    } else if (MODE == KEY_MODE) {
      // Wait until you receive the next distance measurement
      if (xQueueReceive(distanceQueue, &receivedDistance, pdMS_TO_TICKS(20))) {
        if (receivedDistance > 4 && receivedDistance < 40) {
          if (buzzerMode == BUZZER_PIANO_MODE) {
            frequency = pianoScale[map(receivedDistance, 4, 40, 0, 7)];
          } else {
            frequency = map(receivedDistance, 4, 40, 262, 523);
          }

          ledcChangeFrequency(BUZZER_PIN, frequency, RESOLUTION);
          ledcWrite(BUZZER_PIN, 127);

          // If the frequency changes enough, we update the  lcd
          if (abs(oldFrequency - frequency) > 10) {
            xQueueSend(frequencyQueue, &frequency, 0);
          }

          oldFrequency = frequency;
        } else if (receivedDistance > 0 && receivedDistance <= 4) {
          // Stop the sound
          ledcWrite(BUZZER_PIN, 0);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}


void setup() {
  Serial.begin(115200);

  // enable ir remote
  irRemote.enableIRIn();
  
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
  pinMode(IR_RECEIVER_PIN, INPUT);

  distanceQueue = xQueueCreate(5, sizeof(float));
  bpmQueue = xQueueCreate(5, sizeof(int));
  frequencyQueue = xQueueCreate(5, sizeof(int));

  lcd.setBacklight(1);

  // We pin the distance task to core 0 for high speed
  xTaskCreatePinnedToCore(getDistance, "DistanceTask", 2048, NULL, 2, &distanceTaskHandle, 0);

  // Pin to core 1 because it is not as sensitive as the distance measurement
  xTaskCreatePinnedToCore(buzz, "BuzzTask", 4096, NULL, 1, NULL, 1);  

  // Pin to core 1 because it is not as sensitive as the distance measurement
  xTaskCreatePinnedToCore(updateLCD, "UpdateLCD", 4096, NULL, 1, NULL, 1);

  // pin ir reciever task to core 1 because it is not as sensitive as the distance measurement
  xTaskCreatePinnedToCore(irReciever, "IRReceiver", 2048, NULL, 1, NULL, 1);

  beatTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(beatTimer, &onBeatTimer, true);

  // turn on + configure timer 1
  timer1 = timerBegin(50000);              // 50 kHz timer = 20 us per tick
  timerAttachInterrupt(timer1, &onTimer1);
  timerAlarm(timer1, 1000, true, 0);      // 1000 ticks * 20 us = 20 ms
  
  // Attach interrupt LAST after everything is initialized
  attachInterrupt(digitalPinToInterrupt(MICROPHONE_OUT_PIN), handleClap, FALLING);
}

void loop() {

}