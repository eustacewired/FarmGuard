/* 
 * ==============================================================================
 * FARMGUARD-AIoT LEVEL 5: NETWORK-INTEGRATED CONCURRENT SYSTEM ARCHITECTURE
 * ==============================================================================
 * This production firmware executes an asynchronous, dual-core, thread-safe network
 * client. Core 0 isolates network lag (Wi-Fi/MQTT) so it does not block the real-time
 * safety mechanisms, local display loops, and environmental sensing tasks on Core 1.
 */

#include <WiFi.h>           // ESP32 Layer-2 Wi-Fi Stack: Controls radio hardware, scans, and DHCP negotiations
#include <PubSubClient.h>   // MQTT Client Engine: Non-blocking state machine for Broker packet formatting
#include "DHT.h"            // Adafruit DHT Driver: Handles microsecond-accurate 1-wire bit-stream reading

// --- Network Access Control Credentials ---
const char* ssid = "Mugzi";                           // Target Wi-Fi Access Point (SSID) router string
const char* password = "2444666668888888";                   // Security pass-phrase required to authenticate WPA2
const char* mqtt_server = "broker.hivemq.com";                 // Global public sandbox MQTT broker URL address
const char* telemetry_topic = "farmguard/jkuat_node_01/telemetry"; // Cloud outbound channel for publishing telemetry
const char* command_topic = "farmguard/jkuat_node_01/command";   // Cloud inbound channel for incoming control packets

WiFiClient espClient;         // Creates the base network socket client wrapper for handling raw TCP connections
PubSubClient client(espClient); // Wraps the raw TCP socket in the MQTT protocol processor engine

// --- Hardware GPIO Allocations ---
const int dhtPin = 4;        // Digital pin matching the 1-Wire physical data line from the DHT11 sensor
const int soilPin = 34;      // Analog input pin bound to Internal ADC Channel 1 to measure moisture voltage
const int relayPin = 5;      // Digital output pin driving the transistor switch for the water pump relay
#define DHTTYPE DHT11        // Set the library parsing mode to handle 8-bit integer frames from the DHT11
DHT dht(dhtPin, DHTTYPE);    // Initialize the DHT software driver instance with pin and type mappings

// --- The Shared "Thread-Safe" Data Vault ---
// Shared structure in RAM where independent tasks securely read or write variables.
struct GlobalData {
  float temperature;               // Current air temperature in degrees Celsius (updated by SensorTask)
  float humidity;                  // Current relative air humidity percentage (updated by SensorTask)
  int soilMoisture;                // Filtered soil moisture level mapped from 0% to 100% (updated by SensorTask)
  bool pumpIsOn;                   // Flags the current real-time state of the physical relay pin
  unsigned long lastSensorUpdate;  // Millisecond timestamp used by the Watchdog to check for code lockups
  
  // Remote Control Automation Flags
  bool remoteCommandActive;        // State flag raised when a remote cloud trigger overrides local loop rules
  unsigned long remoteCommandStartTime; // Tracks when a remote manual pump burst started to prevent overwatering

  String edgeState; // Stores "NORMAL", "DRY_STRESS", "SEVERE_HEAT_STRESS", etc.
};
GlobalData systemData;       // Allocate physical memory space inside global RAM for our state struct
SemaphoreHandle_t dataMutex; // Mutex token pointer: used to lock/unlock access to systemData

// --- OS Task Management Handles ---
// Array of system headers tracking memory locations and task run states across both CPU cores.
TaskHandle_t SensorTaskHandle;   // Tracking handle for the background sensor reading loop
TaskHandle_t ControlTaskHandle;  // Tracking handle for the closed-loop decision matrix loop
TaskHandle_t DisplayTaskHandle;  // Tracking handle for the serial print telemetry stream loop
TaskHandle_t AlertTaskHandle;    // Tracking handle for the emergency threshold scanner loop
TaskHandle_t NetworkTaskHandle;  // Tracking handle for the network stack client loop running on Core 0
TaskHandle_t WatchdogTaskHandle; // Tracking handle for the high-priority software supervisor loop

// =========================================================================================================
// MQTT ASYNCHRONOUS CALLBACK HANDLER (Triggered automatically by client.loop() on packet receipt)
// =========================================================================================================
// Warning: This function executes inside the NetworkTask context on Core 0. Do not use blocking delays here!
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";             // Create a temporary local dynamic string buffer to reconstruct the payload
  for (int i = 0; i < length; i++) { // Step through each index of the incoming raw byte array
    message += (char)payload[i];   // Cast the raw byte data to a character and append it to the string buffer
  }
  
  Serial.println("[NETWORK] Command Received: " + message); // Print the string to the terminal for debugging

  // PARSE PACKET: Search for the specific control substring inside the incoming network string wrapper
  if (message.indexOf("\"PUMP_ON\"") > 0) {
    // MUTEX ACQUISITION: Request the memory token to safely change global operational state variables.
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      systemData.remoteCommandActive = true;       // Set flag true to command the control loop to turn the pump on
      systemData.remoteCommandStartTime = millis(); // Save the current timestamp to start the safety cutoff clock
      xSemaphoreGive(dataMutex);                   // Release the key so the local control loop can process this command
    }
  }
}

// =========================================================================================================
// TASK 1: ENVIRONMENTAL SENSOR ACQUISITION THREAD (Allocated to CPU Core 1) - Frequency: 2000ms
// =========================================================================================================
void SensorTask(void *pvParameters) {
  for (;;) { // Infinite process loop
    float t = dht.readTemperature();   // Query DHT11 over its 1-wire bus to parse air temperature data
    float h = dht.readHumidity();      // Query DHT11 over its 1-wire bus to parse relative humidity data
    int rawSoil = analogRead(soilPin); // Sample the 12-bit ADC channel (returns a raw value between 0 and 4095)
    
    // Scale and limit the raw ADC voltage into an agricultural percentage range (0% = dry, 100% = wet)
    int mappedSoil = constrain(map(rawSoil, 2654, 1200, 0, 100), 0, 100);

    // LEVEL 6: Rule-Based Anomaly Detection Engine
    String currentInference = "NORMAL";

    if (isnan(t) || isnan(h) || t > 50.0 || t < -10.0 || h < 5.0 || h > 100.0) {
      currentInference = "SENSOR_FAULT";
    } else if (mappedSoil < 20 && t > 35.0 && h < 35.0) {
      currentInference = "SEVERE_HEAT_STRESS";
    } else if (mappedSoil < 35) {
      currentInference = "DRY_STRESS";
    } else if (t > 38.0) {
      currentInference = "CRITICAL_HIGH_TEMP";
    }

    // MUTEX LOCK SEQUENCE: Request exclusive write permissions for the shared global variables.
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      systemData.temperature = t;              // Store the newly parsed temperature value in the data vault
      systemData.humidity = h;                 // Store the newly parsed humidity value in the data vault
      systemData.soilMoisture = mappedSoil;     // Store the scaled soil moisture percentage in the data vault
      systemData.lastSensorUpdate = millis();  // Reset the watchdog timeout timer to prove the task is healthy
      xSemaphoreGive(dataMutex);               // Release the data lock so other tasks can use these values
    }
    vTaskDelay(pdMS_TO_TICKS(2000));           // Put this task to sleep for 2 seconds to free up Core 1 execution time
  }
}

// =========================================================================================================
// TASK 2: CLOSE-LOOP AUTOMATED ACTUATOR CONTROL THREAD (Allocated to CPU Core 1) - Frequency: 2000ms
// =========================================================================================================
// =========================================================================================================
// TASK 2: CLOSE-LOOP AUTOMATED ACTUATOR CONTROL THREAD (Allocated to CPU Core 1) - Frequency: 2000ms
// =========================================================================================================
void ControlTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { // Lock the memory block before evaluating conditions
      bool shouldPumpRun = false;                    // Local flag storing the resolved decision state for the pump

      // LEVEL 6 CRITICAL OVERRIDE: If a sensor fault is inferred, block all activation logic entirely
      if (systemData.edgeState == "SENSOR_FAULT") {
        Serial.println("[CONTROL] EDGE INTELLIGENCE OVERRIDE: Sensor fault detected. Actuator isolated.");
        shouldPumpRun = false; 
      } 
      else {
        // SUB-LOGIC 1: Evaluate local hardware thresholds (Auto-irrigate if soil moisture drops below 35%)
        if (systemData.soilMoisture < 35) {
          shouldPumpRun = true;                       // Raise the flag to turn on the pump
        }

        // SUB-LOGIC 2: Evaluate remote cloud overrides and enforce the 5-second failsafe window
        if (systemData.remoteCommandActive) {
          if (millis() - systemData.remoteCommandStartTime <= 5000) {
            shouldPumpRun = true;                     // Keep the pump turned on for the duration of the cloud command burst
          } else {
            Serial.println("[CONTROL] SAFETY TRIGGER: Remote command timed out.");
            systemData.remoteCommandActive = false;   // Clear the flag to hand control back to local thresholds
          }
        }
      }

      // SUB-LOGIC 3: Apply the resolved state directly to the physical relay hardware pin using High-Z
      if (shouldPumpRun) {
        pinMode(relayPin, OUTPUT);
        digitalWrite(relayPin, LOW);  // Output 0V to pull the relay circuit low and turn the pump ON
        systemData.pumpIsOn = true;   // Log the active hardware state in the global variables
      } else {
        pinMode(relayPin, INPUT);     // Disconnect the pin (High-Z) to turn off the relay circuit cleanly
        systemData.pumpIsOn = false;  // Log the idle hardware state in the global variables
      }
      
      xSemaphoreGive(dataMutex);      // Release the memory lock so other tasks can inspect the pump status
    }
    vTaskDelay(pdMS_TO_TICKS(2000));  // Pause execution for 2 seconds before running the next control evaluation
  }
}

// =========================================================================================================
// TASK 3: TELEMETRY AND SERIAL DISPLAY THREAD (Allocated to CPU Core 1) - Frequency: 2000ms
// =========================================================================================================
void DisplayTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { // Lock the data vault to print steady, uncorrupted telemetry
      // Print a clean, formatted telemetry summary string out to the serial monitor connection
      Serial.printf("[DISPLAY] Temp: %.1fC | Hum: %.1f%% | Soil: %d%% | Pump: %s\n", 
                    systemData.temperature, systemData.humidity, 
                    systemData.soilMoisture, systemData.pumpIsOn ? "ON" : "OFF");
      xSemaphoreGive(dataMutex);                    // Unlock the memory vault immediately after printing
    }
    vTaskDelay(pdMS_TO_TICKS(2000));                // Wait 2 seconds before displaying the next data frame
  }
}

// =========================================================================================================
// TASK 4: RISK ASSESSMENT AND THRESHOLD ALERT THREAD (Allocated to CPU Core 1) - Frequency: 5000ms
// =========================================================================================================
void AlertTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { // Lock down the variables to perform an emergency system scan
      // CRITICAL CHECK: Look for dangerously low soil moisture levels (under 20%) that can kill crops
      if (systemData.soilMoisture < 20) {
        Serial.println("[ALERT] CRITICAL: Soil moisture dangerously low!"); // Print emergency warning to console
      }
      xSemaphoreGive(dataMutex);                    // Release the lock so other tasks are not delayed
    }
    vTaskDelay(pdMS_TO_TICKS(5000));                // Run this safety scan once every 5 seconds to minimize CPU load
  }
}

// =========================================================================================================
// TASK 5: CORES-ISOLATED NETWORK STACK TELEMETRY THREAD (Allocated to CPU Core 0) - Loop Speed: 50ms (Asynchronous)
// =========================================================================================================
// =========================================================================================================
// TASK 5: CORES-ISOLATED NETWORK STACK TELEMETRY THREAD (Allocated to CPU Core 0)
// =========================================================================================================
void NetworkTask(void *pvParameters) {
  Serial.println("\n[NETWORK] Core 0 Booting Wi-Fi..."); // <--- Added Boot Message
  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  unsigned long lastTelemetryTime = 0;

  for (;;) { 
    
    // LAYER-2 WI-FI KEEPALIVE ENGINE
    if (WiFi.status() != WL_CONNECTED) { 
      Serial.print(".");                 // <--- Added: Prints dots while trying to connect
      vTaskDelay(pdMS_TO_TICKS(1000));   
      continue;                          
    }

    // LAYER-7 MQTT BROKER RECONNECTION ENGINE
    if (!client.connected()) {           
      Serial.println("\n[NETWORK] Wi-Fi Connected! Attempting MQTT connection..."); // <--- Added
      
      String clientId = "ESP32Client-" + String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) {  
        Serial.println("[NETWORK] MQTT Broker Connected!"); // <--- Added
        client.subscribe(command_topic);       
      } else {
        Serial.println("[NETWORK] MQTT Failed. Retrying in 5s..."); // <--- Added
        vTaskDelay(pdMS_TO_TICKS(5000));       
        continue;                              
      }
    }

    client.loop(); 

    // TELEMETRY TRANSMISSION ENGINE... (Keep the rest of your payload code exactly the same) // Process incoming TCP data frames and keep the cloud connection alive

    // TELEMETRY TRANSMISSION ENGINE (Schedules outbound reports every 10 seconds)
    if (millis() - lastTelemetryTime >= 10000) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { // Lock the memory block to safely convert data to string text
        
        // Construct a compliant JSON string payload using the current values inside the data vault
        String payload = "{";
        payload += "\"device_id\": \"FG-AIOT-NODE-01\",";
        payload += "\"temperature_c\": " + String(systemData.temperature, 1) + ",";
        payload += "\"humidity_percent\": " + String(systemData.humidity, 1) + ",";
        payload += "\"soil_moisture_percent\": " + String(systemData.soilMoisture) + ",";
        payload += "\"edge_state\": \"" + systemData.edgeState + "\","; // <--- INJECTED LOCAL INFERENCE
        payload += "\"pump_status\": \"" + String(systemData.pumpIsOn ? "ON" : "OFF") + "\",";
        payload += "\"firmware_version\": \"1.0.0\"";
        payload += "}";

        client.publish(telemetry_topic, payload.c_str()); // Transmit the JSON payload to the cloud broker
        xSemaphoreGive(dataMutex);                        // Unlock the shared variables immediately
      }
      lastTelemetryTime = millis(); // Reset the telemetry transmission schedule clock to the current system time
    }
    
    // IDLE CORESET SAFETY PROTECTION:
    // vTaskDelay(50) yields execution control for 50 milliseconds to feed the internal ESP32 Hardware Idle Task.
    // This resets the Core 0 Watchdog timer (IDLE-WDT) and prevents a hardware crash reset loop.
    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}

// =========================================================================================================
// TASK 6: SYSTEM WATCHDOG SUPERVISOR THREAD (Allocated to CPU Core 1) - Frequency: 1000ms (High Priority)
// =========================================================================================================
void WatchdogTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { // Lock the data vault to safely read performance timestamps
      
      // DIAGNOSTIC CHECK: Calculate how long it has been since the sensor task refreshed the shared variables.
      // If the current uptime exceeds the last update by 10,000 milliseconds, the sensor task has crashed or frozen.
      if (millis() - systemData.lastSensorUpdate > 10000) {
        Serial.println("[WATCHDOG] FAULT DETECTED: Sensor Task Frozen!");
        
        // HARDWARE FAIL-SAFE OVERRIDE: Force the relay output pin HIGH immediately.
        // This cuts power to the pump directly through hardware, protecting the crops from catastrophic flooding.
        pinMode(relayPin, INPUT); 
      }
      xSemaphoreGive(dataMutex); // Release the data lock token
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wake up exactly once per second to audit system health variables
  }
}

// =========================================================================================================
// INTERRUPT SYSTEM ENTRY, PERIPHERAL INITIALIZATION, & OS BOOT SEQUENCER
// =========================================================================================================
void setup() {
  Serial.begin(115200);                  // Initialize the UART serial communications engine at 115,200 baud
  pinMode(relayPin, INPUT);         // Drive the relay pin HIGH immediately to ensure the pump boots safely in the OFF state
  dht.begin();                           // Initialize the DHT11 sensor internal integrated circuit interface

  // Mutex instantiation: Allocates RAM for the binary token flag. This must run before spawning any tasks.
  dataMutex = xSemaphoreCreateMutex();

  // =======================================================================================================
  // FREERTOS DISTRIBUTED TASK SCHEDULER INITIALIZATION
  // Syntax: xTaskCreatePinnedToCore( Function_Pointer, Debug_Name, Stack_Bytes, Params, Priority, &Handle, Core_ID );
  // =======================================================================================================
  
  // SPAWN NETWORK TELEMETRY ON CORE 0: Isolate heavy networking lags from timing-sensitive crop controls.
  xTaskCreatePinnedToCore(NetworkTask, "NetTask", 8192, NULL, 1, &NetworkTaskHandle, 0); 
  
  // SPAWN APPLICATION LAYER ON CORE 1: Execute concurrent routines safely on isolated silicon space.
  xTaskCreatePinnedToCore(SensorTask, "SensTask", 4096, NULL, 1, &SensorTaskHandle, 1);   // Priority 1: Reads data inputs
  xTaskCreatePinnedToCore(ControlTask, "CtrlTask", 2048, NULL, 2, &ControlTaskHandle, 1); // Priority 2: Higher priority ensures quick actuation response
  xTaskCreatePinnedToCore(DisplayTask, "DispTask", 2048, NULL, 1, &DisplayTaskHandle, 1); // Priority 1: Outputs standard logs
  xTaskCreatePinnedToCore(AlertTask, "AlrtTask", 2048, NULL, 1, &AlertTaskHandle, 1);     // Priority 1: Monitors thresholds
  
  // SPAWN SYSTEM WATCHDOG ON CORE 1: High priority allows this safety monitor to pause other tasks when it needs to run.
  xTaskCreatePinnedToCore(WatchdogTask, "WdogTask", 2048, NULL, 3, &WatchdogTaskHandle, 1); 
}

// =========================================================================================================
// STANDARD SEQUENTIAL LOOP FUNCTION: Erased
// =========================================================================================================
void loop() {
  // FreeRTOS ignores sequential architectures. The default background Arduino loop task is deleted here
  // to reclaim system heap memory and free up 100% of Core 1 execution cycles for our custom concurrent grid.
  vTaskDelete(NULL); 
}
