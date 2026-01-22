// Filename: lab2part2.ino
// Author: Connor Roane
// Date: 1/22/2026
// Description: code for an LED circuit. LED blinks, half second on, half second off

// ================= INCLUDES ====================
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_periph.h"
#include "soc/timer_group_reg.h"


// ================= MACROS ======================
// Define GPIO pin number
#define GPIO_PIN 20 // choose pin 20

// Define toggle interval in timer ticks (e.g., 1 second)
#define LED_TOGGLE_INTERVAL 500000 // half a second is toggle interval


void setup() {
  pinMode(20, OUTPUT); 
  digitalWrite(20, LOW);

  // Configure timer
  uint32_t timer_config = 0; // store config setting in a variable

  // apply a clock divider
  timer_config &= ~(0xFFFF << 13); // clearing bits 13-28 to 0
  timer_config |= (1<<17); // bit 4 equal to 1 (in the divider)
  timer_config |= (1<<19); // bit 6 equal to 1 (in the divider) 
  // bits 4 and 6 equal to 1 is binary 80, 

  // Set increment mode and enable timer
  timer_config |= ((1<<30)); // count up
  timer_config |= ((1<<31)); // Timer turns on



  REG_WRITE(TIMG_T0CONFIG_REG(0), timer_config); // load timer settings

  REG_WRITE(TIMG_T0UPDATE_REG(0), 1); // set update register to 1, connects timer to lo/hi reg so you can read time


}

void loop() {
  // Track last toggle time
  static uint32_t last_toggle_time = 0; // stoe last timer value when led last toggled

  // Read current timer value
  uint32_t current_time = 0; 
  current_time = *((volatile uint32_t *)TIMG_T0LO_REG(0)); // reads current timer value from timer group 0, timer 0

  //  Check if toggle interval has passed, if so perform led toggle
  if ((current_time - last_toggle_time) >= LED_TOGGLE_INTERVAL) {
    // Read current GPIO output state
    uint32_t gpio_out = 0;
    gpio_out = *((volatile uint32_t *)GPIO_OUT_REG); // get the current state of the output pin and put into variable

    // Toggle GPIO_PIN using XOR
    *((volatile uint32_t *)GPIO_OUT_REG) = gpio_out ^ (1<<GPIO_PIN); // toggle the pin using variable just stored

    // Update last_toggle_time
    last_toggle_time = current_time; 
  }

  //  Refresh timer counter value
  *((volatile uint32_t *)TIMG_T0UPDATE_REG(0)) = 1; 
}
