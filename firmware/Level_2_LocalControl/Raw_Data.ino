// ====================================================================
// PROGRAM TITLE: FarmGuard-AIoT: Level 2 (Step A) - Sensor Integration
// PURPOSE: Adds environmental telemetry to the non-blocking execution core.
// DESCRIPTION: This firmware integrates a DHT11 temperature/humidity sensor 
//              and an analog soil moisture probe. It uses separate hardware 
//              timers for the LED interface (1s) and the sensor acquisition 
//              engine (2s), preventing telemetry reads from bottlenecking the system.
// ====================================================================

// --- External Libraries ---
#include "DHT.h" // Driver library providing API functions for DHT series digital sensors.

// --------------------------------------------------------------------
// HARDWARE CONFIGURATION & PIN MAPPING
// --------------------------------------------------------------------
// Assign concrete physical GPIO pin numbers to system hardware peripherals.
// Const types block unintended runtime modifications to hardware geometry.
const int ledPin = 21;       // Pin 21 controls the physical status LED indicator.
const int buttonPin = 22;    // Pin 22 interfaces with the system control button.
const int dhtPin = 4;        // Pin 4 operates as the single-wire data bus for the DHT11.
const int soilPin = 34;      // Pin 34 utilizes an Analog-to-Digital Converter (ADC) for soil readings.

// --------------------------------------------------------------------
// SENSOR CONFIGURATION SUITE
// --------------------------------------------------------------------
#define DHTTYPE DHT11        // Macro defining the exact sensor protocol model (DHT11 variant).
DHT dht(dhtPin, DHTTYPE);    // Instantiates an instance of the driver class named 'dht' with our pin configurations.

// --------------------------------------------------------------------
// NON-BLOCKING TIMING ENGINE (MILLIS MULTI-TASKING)
// --------------------------------------------------------------------
// Dedicated timestamps and interval ceilings tracking independent asynchronous system schedules.
unsigned long previousMillisLED = 0;    // Tracks the exact system uptime millisecond the LED last switched state.
const long ledInterval = 1000;          // Schedule interval boundary forcing the LED to blink every 1000ms.

unsigned long previousMillisSensors = 0; // Tracks the exact system uptime millisecond telemetry was last gathered.
const long sensorInterval = 2000;       // Schedule interval boundary pulling telemetry sweeps every 2000ms.

// --------------------------------------------------------------------
// CORE STATE CONTROLLERS
// --------------------------------------------------------------------
// Conditional status markers driving background behaviors and internal state retention.
bool ledState = LOW;          // Stores physical power configuration state of the LED (LOW/HIGH).
bool systemActive = true;     // Master runtime flag allowing telemetry loop operations; defaults active for engineering tests.
bool lastButtonState = HIGH;  // Preserves button's electrical configuration from previous loop to detect edge transitions.

// ====================================================================
// SETUP FUNCTION: RUNS ONCE AT BOOTUP
// ====================================================================
void setup() {
  // Initialize serial interface channel at 115200 bits per second for peripheral debugging logs.
  Serial.begin(115200);
  
  // Define physical pins as software input signals or hardware output drivers.
  pinMode(ledPin, OUTPUT);         // Sets up the LED line to allow software to emit voltage triggers.
  pinMode(buttonPin, INPUT_PULLUP); // Holds input at default high voltage (VCC), grounding to zero (LOW) when pressed.
  
  // Execute internal software initialization protocols to prepare the single-wire DHT sensor line.
  dht.begin(); 

  // Hard delay forcing processor idle for 2 seconds to let the serial console interface establish link lines.
  delay(2000); 

  Serial.println("========================================");
  Serial.println("FarmGuard Node Initialized (Level 2a)");
  Serial.println("========================================");
}

// ====================================================================
// MAIN SYSTEM LOOP: REPEATS INDEFINITELY
// ====================================================================
void loop() {
  // Capture snapshot of processing lifespan in milliseconds to manage scheduling matrices.
  unsigned long currentMillis = millis();

  // ------------------------------------------------------------------
  // TASK 1: ASYNCHRONOUS LED CONTROLLER
  // ------------------------------------------------------------------
  // Asynchronous evaluation: Checks if the timeline gap matches or surpasses the 1s LED threshold.
  if (currentMillis - previousMillisLED >= ledInterval) {
    previousMillisLED = currentMillis; // Re-align reference point to evaluate next schedule block.
    ledState = !ledState;              // Flip logic configuration of indicator tracking byte.
    
    // Ternary operation constraint: Outputs the toggled state if active, otherwise clamps physical line to safe zero (LOW).
    digitalWrite(ledPin, systemActive ? ledState : LOW);
  }

  // ------------------------------------------------------------------
  // TASK 2: DEBOUNCED EXTERNAL INTERRUPT OVERRIDE
  // ------------------------------------------------------------------
  // Fetch instantaneous real-time electrical state from user interface button.
  bool currentButtonState = digitalRead(buttonPin);
  
  // Mechanical edge evaluation: Looks for immediate shifts from an unpressed state (HIGH) to a compressed connection (LOW).
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    systemActive = !systemActive; // Invert global flag toggling sensor acquisition engines.
    
    Serial.print("System Active State: ");
    Serial.println(systemActive ? "TRUE" : "FALSE"); // Output operational mode confirmation lines.
    
    // Strategic inline hardware block: Pauses code to filter bouncing metal plates in switch mechanisms.
    delay(50); 
  }
  lastButtonState = currentButtonState; // Maintain current digital values for comparative validation next cycle.

  // ------------------------------------------------------------------
  // TASK 3: MULTI-SENSOR ENVIRONMENTAL DATA ACQUISITION
  // ------------------------------------------------------------------
  // Asynchronous evaluation: Tracks if the timeline gap matches or surpasses the 2s sensor check threshold.
  if (currentMillis - previousMillisSensors >= sensorInterval) {
    previousMillisSensors = currentMillis; // Advance base tracker for next scheduled assessment cycle.

    // Telemetry processes operate if operational authorization status is active.
    if(systemActive) {
      // 1. Digital Sensor Extraction (DHT11)
      // Call driver algorithms pulling digital temperature and humidity data arrays via the single-wire bus.
      float humidity = dht.readHumidity();
      float temperature = dht.readTemperature();

      // Hardware Diagnostic: Validate data returns are clean floating numbers.
      // isnan() evaluates True if the return payload is corrupted, disconnected, or missing entirely.
      if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Error: Failed to read from DHT sensor!");
      }

      // 2. Analog Sensor Extraction (Soil Moisture Probe)
      // Sample immediate physical voltage using internal 12-bit ADC architecture (Maps 0-3.3V into integer steps 0-4095).
      int rawSoilValue = analogRead(soilPin);
      
      // 3. Diagnostics Serial Transmissions
      // Format parameters and print raw datasets out over the terminal configuration links.
      Serial.println("--- Sensor Readings ---");
      Serial.print("Temp: "); Serial.print(temperature); Serial.println(" °C");
      Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
      Serial.print("Raw Soil ADC: "); Serial.println(rawSoilValue); // Outputting uncalibrated readings to prepare for calibration steps.
      Serial.println("-----------------------");
    }
  }
}
