// ====================================================================
// PROGRAM TITLE: FarmGuard-AIoT: Level 1 Bring-Up
// PURPOSE: Establishes device identity and implements non-blocking controls.
// DESCRIPTION: This firmware initializes an IoT node with a structured JSON 
//              boot signature. It manages an LED blinking routine using 
//              hardware timers instead of delay functions, allowing the 
//              processor to concurrently monitor physical hardware inputs.
// ====================================================================

// --------------------------------------------------------------------
// HARDWARE CONFIGURATION & PIN MAPPING
// --------------------------------------------------------------------
// Assign descriptive names to microcontroller pins to improve code readability.
// Using 'const' ensures these pin assignments cannot be accidentally changed during runtime.
const int ledPin = D1;     // Digital pin D1 is mapped to control the indicator LED.
const int buttonPin = D2;  // Digital pin D2 is mapped to read the physical user button.

// --------------------------------------------------------------------
// NON-BLOCKING TIMING VARIABLES (MILLIS ENGINE)
// --------------------------------------------------------------------
// Microcontroller time tracks in milliseconds since bootup, stored in an unsigned long.
// These variables calculate elapsed time without halting processor execution.
unsigned long previousMillis = 0; // Stores the exact timestamp when the LED last toggled state.
const long interval = 1000;       // The target duration (in milliseconds) to wait between LED blinks.
bool ledState = LOW;              // Tracks current physical state of the LED (LOW = Off, HIGH = On).

// --------------------------------------------------------------------
// DEVICE STATE VARIABLES
// --------------------------------------------------------------------
// Global flags tracking the operational status and physical inputs of the system.
bool systemActive = false;        // Master system toggle switch. Tracks if the device is active or idle.
bool lastButtonState = HIGH;      // Remembers the previous loop's button state to detect changes (edges).

// ====================================================================
// SETUP FUNCTION: RUNS ONCE AT SYSTEM STARTUP
// ====================================================================
void setup() {
  // Initialize serial communication channel at a fast baud rate of 115200 bits per second.
  // This allows the microcontroller to transmit text data back to a connected computer.
  Serial.begin(115200);
  
  // Configure the electrical behavior of the mapped physical hardware pins.
  pinMode(ledPin, OUTPUT);         // Set the LED pin as an OUTPUT so we can send voltage to it.
  
  // INPUT_PULLUP activates an internal resistor that pulls the signal to a default HIGH (VCC).
  // When the physical button is pressed, it bridges the pin to GND, forcing the signal to LOW.
  pinMode(buttonPin, INPUT_PULLUP); 

  // Hard delay halting execution for 2 seconds. This creates a safe timing window for 
  // the computer's Serial Monitor software to successfully establish a connection after boot.
  delay(2000); 

  // Level 1 Requirement: Structured JSON Boot Log
  // Transmits device metadata over the serial interface. The structured JSON format 
  // allows parsing software on edge gateways or cloud servers to register the device automatically.
  Serial.println("========================================");
  Serial.println("Initiating FarmGuard Boot Sequence...");
  Serial.println("{");
  Serial.println("  \"device_id\": \"FG-AIOT-NODE-01\",");  // Unique alphanumeric identifier for this specific hardware asset.
  Serial.println("  \"firmware_version\": \"1.0.0\",");   // Tracks software release versioning for maintenance updates.
  Serial.println("  \"location\": \"JKUAT-Testbench\","); // Pinpoints geographic or laboratory placement context.
  Serial.println("  \"mode\": \"bring_up\",");            // Identifies execution mode (Bring-up/Testing vs Production).
  Serial.println("  \"status\": \"READY\"");               // Confirms hardware configuration completed successfully.
  Serial.println("}");
  Serial.println("========================================");
}

// ====================================================================
// MAIN LOOP FUNCTION: RUNS CONTINUOUSLY IN AN INFINITE LOOP
// ====================================================================
void loop() {
  // Capture current uptime runtime of the microcontroller in milliseconds.
  // This updates on every single loop iteration to provide an accurate baseline timestamp.
  unsigned long currentMillis = millis();

  // ------------------------------------------------------------------
  // 1. NON-BLOCKING LED BLINK ENGINE
  // ------------------------------------------------------------------
  // Math operation evaluating if the difference between current time and the last
  // action timestamp exceeds our 1000ms threshold. This runs asynchronously without pauses.
  if (currentMillis - previousMillis >= interval) {
    // Save the exact current timestamp to use as the new baseline reference for the next cycle.
    previousMillis = currentMillis;
    
    // Toggle the boolean value using the logical NOT operator (!). 
    // If ledState was HIGH, it becomes LOW. If it was LOW, it becomes HIGH.
    ledState = !ledState;
    
    // Nested Conditional: The blinking behavior depends entirely on the master system state.
    if(systemActive) {
      // If system state is active, output the toggling state directly to the physical hardware pin.
      digitalWrite(ledPin, ledState);
    } else {
      // Safety Override: Force physical LED off if the master system state is set to inactive.
      digitalWrite(ledPin, LOW);
    }
  }

  // ------------------------------------------------------------------
  // 2. USER INTERACTION: READ & PROCESS BUTTON STATE
  // ------------------------------------------------------------------
  // Read the immediate voltage present on the button pin (Will return HIGH if open, LOW if pressed).
  bool currentButtonState = digitalRead(buttonPin);
  
  // Edge Detection Logic: Triggers action *only* at the precise moment the button transitions.
  // Checks if current state is pressed (LOW) AND its previous recorded state was unpressed (HIGH).
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    // Invert the master operational system state flag (Toggles between True and False).
    systemActive = !systemActive; 
    
    // Stream status feedback logs to the terminal for debugging purposes.
    Serial.print("System Active State Changed: ");
    // Inline conditional operator prints text representation based on the state variable.
    Serial.println(systemActive ? "TRUE" : "FALSE");
    
    // Software Debounce: Pauses execution briefly to let physical electrical signals 
    // settle inside the button switch, preventing false multiple-triggering from metal contact bounce.
    delay(50); 
  }
  
  // State Retention: Store current hardware reading as the "last state" reference 
  // for comparison when the loop cycles back around to execute again.
  lastButtonState = currentButtonState;
}
