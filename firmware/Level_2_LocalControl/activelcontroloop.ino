// ====================================================================
// PROGRAM TITLE: FarmGuard-AIoT: Level 2 (Step B) - Sensor Calibration & Actuation
// PURPOSE: Implements sensor data mapping, threshold monitoring, and automatic irrigation control.
// DESCRIPTION: This firmware maps raw analog data from a soil moisture probe into a standard percentage scale.
//              It introduces a digital relay output to actuate a water pump based on real-time soil conditions.
//              The system features a fail-safe shutdown mechanism that deactivates the pump if manual override is engaged.
// ====================================================================

// --- External Libraries ---
#include "DHT.h" // Hardware driver handling complex communications with the digital DHT temperature/humidity sensor.

// --------------------------------------------------------------------
// HARDWARE CONFIGURATION & PIN MAPPING
// --------------------------------------------------------------------
// Map descriptive variable names to concrete microcontroller GPIO pins to make maintenance easy.
const int ledPin = 21;       // Pin 21 runs the visual operation status indicator LED.
const int buttonPin = 22;    // Pin 22 handles user input from a push-button toggle interface.
const int dhtPin = 4;        // Pin 4 processes single-wire serial data streams from the DHT11 sensor.
const int soilPin = 34;      // Pin 34 connects to a built-in Analog-to-Digital Converter (ADC) for soil measurements.
const int relayPin = 5;      // NEW: Pin 5 serves as a digital trigger line to switch the water pump relay on or off.

// --------------------------------------------------------------------
// SENSOR CONFIGURATION SUITE
// --------------------------------------------------------------------
#define DHTTYPE DHT11        // Compiler macro identifying the specific sensor model protocol being used.
DHT dht(dhtPin, DHTTYPE);    // Creates an instance of the DHT driver class called 'dht' using your pin settings.

// --------------------------------------------------------------------
// CALIBRATION COEFFICIENTS (PHYSICAL TUNING VALUES)
// --------------------------------------------------------------------
// These reference points represent physical testing limits used to map raw voltage to actual moisture percentages.
// AirValue is the ADC integer read when dry. WaterValue is the ADC integer read when completely submerged.
const int AirValue = 2654;   // Hardware baseline reading recorded in bone-dry air conditions.
const int WaterValue = 1200; // Calibrated or estimated hardware baseline reading recorded in pure water.

// --------------------------------------------------------------------
// NON-BLOCKING TIMING ENGINE (MILLIS SCHEDULER)
// --------------------------------------------------------------------
// Time tracking structures keeping system operations executing concurrently without delays.
unsigned long previousMillisLED = 0;    // Records the absolute millisecond timestamp of the last indicator LED blink event.
const long ledInterval = 1000;          // Fixed time window setting the pace of the status indicator to a 1-second cycle.

unsigned long previousMillisSensors = 0; // Records the absolute millisecond timestamp of the last telemetry collection cycle.
const long sensorInterval = 2000;       // Fixed time window forcing data acquisition routines to run every 2 seconds.

// --------------------------------------------------------------------
// CORE STATE CONTROLLERS
// --------------------------------------------------------------------
// Global runtime variables tracking functional parameters and physical switch positions.
bool ledState = LOW;          // Stores the current electrical state of the indicator LED (LOW = Off, HIGH = On).
bool systemActive = true;     // Master automation flag allowing sensor logic to operate when set to true.
bool lastButtonState = HIGH;  // Holds onto previous digital loop readings to detect immediate button transition clicks.

// --------------------------------------------------------------------
// IRRIGATION MANAGEMENT LOGIC PARAMS
// --------------------------------------------------------------------
// Defines the operational boundaries that determine when automatic mechanical irrigation begins.
const int MOISTURE_THRESHOLD = 35; // Target percentage floor: any moisture level below 35% forces the pump to turn on.

// ====================================================================
// SETUP FUNCTION: EXECUTES ONCE AT COLD BOOT
// ====================================================================
void setup() {
  // Open serial data pipe at 115200 bits per second to send performance feedback logs to the terminal.
  Serial.begin(115200);
  
  // Set the structural data direction of your physical microcontroller pins.
  pinMode(ledPin, OUTPUT);         // Sets up the LED line to allow software to emit voltage triggers.
  pinMode(buttonPin, INPUT_PULLUP); // Enables internal pullup to hold line at high voltage (HIGH) until a button press grounds it (LOW).
  
  // Configure the relay. 
  // Note: Many common relay breakout modules use "Active LOW" electronics, meaning a LOW voltage signal activates the coil.
  // We initialize the line to a high electrical voltage to guarantee the pump stays off at boot.
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // Assumes Active LOW layout logic: Writing HIGH turns the pump relay completely OFF.
  
  // Initialize communication protocols inside the DHT driver engine.
  dht.begin(); 
  
  // Hold execution for 2 seconds to allow internal sensor circuits and terminal links to stabilize cleanly.
  delay(2000); 

  Serial.println("========================================");
  Serial.println("FarmGuard Node Initialized (Level 2b)");
  Serial.println("========================================");
}

// ====================================================================
// MAIN SYSTEM LOOP: RUNS CONTINUOUSLY IN AN INFINITE LOOP
// ====================================================================
void loop() {
  // Capture current uptime runtime of the microcontroller in milliseconds.
  unsigned long currentMillis = millis();

  // ------------------------------------------------------------------
  // TASK 1: ASYNCHRONOUS VISUAL STATUS INDICATOR
  // ------------------------------------------------------------------
  // Evaluate if the timeline gap matches or surpasses the 1s LED threshold.
  if (currentMillis - previousMillisLED >= ledInterval) {
    previousMillisLED = currentMillis; // Advance timeline baseline reference to evaluate the next cycle.
    ledState = !ledState;              // Toggle logic state tracker flag using a logical NOT operation.
    
    // Ternary operator path: If system is active, apply the toggled state. If paused, lock pin LOW to stop blinking.
    digitalWrite(ledPin, systemActive ? ledState : LOW);
  }

  // ------------------------------------------------------------------
  // TASK 2: BUTTON DRIVEN SYSTEM OVERRIDE & SAFETY SHUTDOWN
  // ------------------------------------------------------------------
  // Fetch instantaneous real-time electrical state from user interface button.
  bool currentButtonState = digitalRead(buttonPin);
  
  // Physical edge evaluation: Looks for immediate shifts from an unpressed state (HIGH) to a compressed connection (LOW).
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    systemActive = !systemActive; // Toggle the master state flag to enable or disable automatic operations.
    
    // Hardwired Safety Interlocking Logic: Instantly cuts pump power if a human operator pauses operations.
    if (!systemActive) {
      digitalWrite(relayPin, HIGH); // Force relay line high to shut off pump power lines immediately.
      Serial.println("SYSTEM PAUSED: Pump safety shutdown.");
    } else {
      Serial.println("SYSTEM ACTIVE: Resuming auto-control.");
    }
    
    // Hardware debounce block: Suspends execution briefly to let noisy mechanical metal switch contacts settle out.
    delay(50); 
  }
  lastButtonState = currentButtonState; // Maintain current digital values for comparative validation next cycle.

  // ------------------------------------------------------------------
  // TASK 3: ENVIRONMENTAL TELEMETRY & AUTOMATED ACTUATION
  // ------------------------------------------------------------------
  // Asynchronous execution block: Pulls environmental measurements and runs controls exactly every 2000ms.
  if (currentMillis - previousMillisSensors >= sensorInterval) {
    previousMillisSensors = currentMillis; // Re-align reference point to evaluate next schedule block.

    // Run telemetry processing logic only if master authorization status is active.
    if(systemActive) {
      // 1. Digital Telemetry Extraction (DHT11)
      float humidity = dht.readHumidity();      // Pull calculated relative atmospheric humidity from digital data streams.
      float temperature = dht.readTemperature(); // Pull calculated atmospheric temperature data arrays.

      // 2. Analog Telemetry Extraction & Data Calibration (Soil Moisture)
      int rawSoilValue = analogRead(soilPin); // Sample instant electrical voltage value from the 12-bit ADC line (0 - 4095).
      
      // Proportional Data Mapping: Translates raw integer ranges into a standard human-readable scale.
      // AirValue maps to 0% moisture (dry soil). WaterValue maps to 100% moisture (saturated soil).
      int soilMoisturePercent = map(rawSoilValue, AirValue, WaterValue, 0, 100);
      
      // Clamping Guardrail: Clamps output values firmly to extreme endpoints. 
      // Prevents impossible negative or over-range percentages if real conditions stray outside calibration references.
      soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);

      // 3. AUTOMATION STATE ENGINE: IRRIGATION CONTROL LOGIC
      String pumpState = "OFF"; // String placeholder used to generate human-readable diagnostic text.
      
      // Threshold check: Evaluate if the mapped moisture content drops below the critical 35% target floor.
      if (soilMoisturePercent < MOISTURE_THRESHOLD) {
        digitalWrite(relayPin, LOW); // Pull Active LOW relay line down to ground to open the water valve.
        pumpState = "ON";            // Update diagnostic text string for status readouts.
      } else {
        digitalWrite(relayPin, HIGH); // Push relay pin high to cut power and stop the water pump.
        pumpState = "OFF";            // Update diagnostic text string for status readouts.
      }

      // 4. PRINT DIAGNOSTICS DASHBOARD TO SERIAL CONSOLE
      Serial.println("--- FarmGuard Live Data ---");
      Serial.print("Temp:     "); Serial.print(temperature); Serial.println(" °C");
      Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
      Serial.print("Moisture: "); Serial.print(soilMoisturePercent); Serial.println(" %");
      Serial.print("Pump:     "); Serial.println(pumpState); // Confirms clear correlation between soil readings and pump activation.
      Serial.println("---------------------------");
    }
  }
}
