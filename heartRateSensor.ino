
// requires 3.3V for IR and RED LEDs
// language is "Wiring" - a combination of C++ and Java

#include <Wire.h> // to communicate with I2C/TWI peripheral devices like the board
#include "MAX30102.h" // package for the sensor to obtain IR samples
#include "heartRate.h"
#define PRINT_DELAY 10 // heartbeat print intervals
#define AVG_SIZE 10 // size for averaging: Increase to improve accuracy > Reduces output frequency
#define MIN_HEART_RATE 50 // minimum heart rate - avoid printing false positives

MAX30102 heartRateSensor;

// Store the last few heartheartRates to take an average
static byte heartRates[AVG_SIZE];
static byte rateSpot = 0;

// Time at which last heartbeat was detected
static long lastBeat_t = 0;
static float beatsPerMinute;

// Average heartBeat
static int heartBeatAvg;

// Printed "PleasePlaceFinger"?
static bool isFingerPlaced = false;

// Print Delay
static volatile int printDelayCnt = 0;

void setup() {
  heartRateSensor.begin(Wire, I2C_SPEED_FAST); //connects to external device
  heartRateSensor.setup(); // first thing to be called, runs first when sketch loaded, initialize input and output pins
  heartRateSensor.setPulseAmplitudeRed(0x0A); //trigger for setting pulse amplitude (strength of light) of red LED

  Serial.begin(9600); // starts serial communication so it can send out commands thru USB connection where 9600 is baud rate, rate at which data to be sent
  Serial.println("Setting Timer Interrupts");

  // Setting Timer1 for 1sec TimerInterrupt
  
  // Initialize Counter values to 0
  TCCR1A = 0; // timer compare capture
  TCCR1B = 0;

  // Clear Counter Value
  TCNT1 = 0;

  // Compare & Match at 1Hz increments, 16MHz clock, 62.5ns a pulse
  // [(16 Mhz) / (1Hz * 64)] - 1
  OCR1A = 15624;

  // Set CTC Mode - Clear Timer on Compare
  TCCR1B |= (1 << WGM12);

  // Set prescaler - internal divider
  TCCR1B |= (1 << CS12) | (1 << CS10);

  // Enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  // Enable the interrupt
  sei();

  // Set Pin13 to output for visual check of detection
  pinMode(13, OUTPUT);

  // End of Setup
  Serial.println("EOS");
}

void loop() {
  // Check IR Value to detect finger presence
  long irValue = heartRateSensor.getIR();
  if (irValue > 7000) {
    isFingerPlaced = true;
    if (checkForBeat(irValue) == true) {
      digitalWrite(13, HIGH);
      Serial.println("-");
      delay(100);
      long delta = millis()  - lastBeat_t;
      lastBeat_t  = millis();

      // Calculate Beats Per Minute
      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        heartRates[rateSpot++] = (byte) beatsPerMinute;
        rateSpot %= AVG_SIZE;

        //Take  average of readings
        heartBeatAvg = 0;

        for (byte x = 0; x < AVG_SIZE; x++) {
          heartBeatAvg += heartRates[x];
        }

        // No Float Point Logical Unit - Division takes a long time
        heartBeatAvg /= AVG_SIZE;
      }

      digitalWrite(13, LOW);
    }
  }

  // Finger not placed
  if (irValue < 7000 && isFingerPlaced) {
    heartBeatAvg = 0;
    Serial.println("Please place your finger on the sensor.");
    isFingerPlaced = false;
  }
}

/**
 * Timer1A Interrupt Service Request for Printing every 10 seconds
 * Gets called every one second
 **/
ISR(TIMER1_COMPA_vect) {
  printDelayCnt ++;
  if ((printDelayCnt > PRINT_DELAY) && (heartBeatAvg > MIN_HEART_RATE)) {
    Serial.print(heartBeatAvg + 40);
    Serial.println(" BPM");
    printDelayCnt = 0;
  }

  // Clear Counter Value
  TCNT1 = 0;
}
