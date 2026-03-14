#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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
#define BUZZER_PIANO_MODE 0
#define BUZZER_SYNTH_MODE 1
#define RESOLUTION 8

/// @brief LCD object configured using I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

volatile unsigned long previousClap;
volatile unsigned int currentBPM;
volatile unsigned int MODE = KEY_MODE;
volatile bool clapDetected = false;

volatile unsigned int buzzerMode = BUZZER_PIANO_MODE;
volatile unsigned int volume = 20;

const TickType_t distanceFrequency = pdMS_TO_TICKS(20);

// Queue handling to send data between tasks
QueueHandle_t distanceQueue;
QueueHandle_t bpmQueue;
QueueHandle_t frequencyQueue;
QueueHandle_t volumeQueue;

// C4, D4, E4, F4, G4, A4, B4, C5
const int pianoScale[] = {262, 294, 330, 349, 392, 440, 494, 523};
const char* keyScale[] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};

void getDistance(void* arg) {
  TickType_t lastLoop = xTaskGetTickCount();

  for(;;) {
    if (MODE == KEY_MODE || MODE == VOLUME_MODE) {
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

    // We wait until 10 ms passsed since it last started
    vTaskDelayUntil(&lastLoop, distanceFrequency);
  }
}

void IRAM_ATTR handleClap() {
  if (MODE != CLAP_MODE) return;

  unsigned long currentClap = micros();
  
  unsigned long millisDifference = (currentClap - previousClap) / 1000ULL;

  if (millisDifference < 300) return;

  if (millisDifference <= 2000) {
    currentBPM = 60000 / millisDifference;
    clapDetected = true;
  }
  previousClap = currentClap;

  xQueueSendFromISR(bpmQueue, (void*) &currentBPM, NULL);
}

void updateLCD(void* arg) {
  int tempFreq, tempBPM, tempVolume;
  int receivedFreq;
  int receivedBPM;
  int receivedVolume;

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

    while (xQueueReceive(volumeQueue, &tempVolume, 0) == pdPASS) {
      receivedVolume = tempVolume;
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
        s1 = String("Key mode");
        s2 = String("Key : ") + keyScale[index];
      } else if (MODE == VOLUME_MODE) {
        s1 = String("Volume mode");
        s2 = String("Volume : ") + String(receivedVolume);
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

}

// buzzer code 
void buzz(void* arg){
  int frequency = 400;
  float receivedDistance;
  int oldFrequency = 0;
  int oldVolume = 0;
  ledcAttach(BUZZER_PIN, frequency, RESOLUTION);

  for(;;) {
    // Wait until you receive the next distance measurement
    if (xQueueReceive(distanceQueue, &receivedDistance, portMAX_DELAY)) {
      if (MODE == KEY_MODE) {
        if (receivedDistance > 4 && receivedDistance < 40) {
          if (buzzerMode == BUZZER_PIANO_MODE) {
            frequency = pianoScale[map(receivedDistance, 4, 40, 0, 7)];
          } else {
            frequency = map(receivedDistance, 4, 40, 262, 523);
          }

          ledcChangeFrequency(BUZZER_PIN, frequency, RESOLUTION);
          ledcWrite(BUZZER_PIN, volume);

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

  distanceQueue = xQueueCreate(5, sizeof(float));
  bpmQueue = xQueueCreate(5, sizeof(int));
  frequencyQueue = xQueueCreate(5, sizeof(int));
  volumeQueue = xQueueCreate(5, sizeof(int));

  lcd.setBacklight(1);

  // We pin the distance task to core 0 for high speed
  xTaskCreatePinnedToCore(getDistance, "DistanceTask", 2048, NULL, 2, NULL, 0);

  // Pin to core 1 because it is not as sensitive as the distance measurement
  xTaskCreatePinnedToCore(buzz, "BuzzTask", 2048, NULL, 1, NULL, 1);  

  // Pin to core 1 because it is not as sensitive as the distance measurement
  xTaskCreatePinnedToCore(updateLCD, "UpdateLCD", 2048, NULL, 1, NULL, 1);
  
  // Attach interrupt LAST after everything is initialized
  attachInterrupt(digitalPinToInterrupt(MICROPHONE_OUT_PIN), handleClap, FALLING);
}

void loop() {

}