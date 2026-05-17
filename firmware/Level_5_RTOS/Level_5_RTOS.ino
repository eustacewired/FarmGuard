/* * ==============================================================================
 * FARMGUARD-AIoT LEVEL 6: INTELLIGENT & CONCURRENT SYSTEM ARCHITECTURE
 * ==============================================================================
 * This production-grade firmware executes an asynchronous, dual-core, thread-safe 
 * IoT network client. Core 0 isolates network latency (Wi-Fi/MQTT) so it never 
 * blocks real-time safety mechanisms, local display loops, or environmental sensing 
 * tasks executing concurrently on Core 1.
 * * Built-In Reliability: Watchdog Supervisor, Local Rule-Based Anomaly Detection,
 * Active-LOW High-Impedance Actuator Isolation, and Asynchronous Network Keep-Alives.
 * ==============================================================================
 */

#include <WiFi.h>           // ESP32 Layer-2 Wi-Fi Stack: Controls radio hardware and DHCP negotiations
#include <PubSubClient.h>   // MQTT Client Engine: Non-blocking state machine for packet formatting
#include "DHT.h"            // Adafruit DHT Driver: Handles microsecond-accurate 1-wire bit-streams
#include "model.h"

// Instantiate the TinyML Decision Tree globally
Eloquent::ML::Port::TinyMLBrain mlBrain;

// --- Network Access Control Credentials ---
const char* ssid = "Mugzi";                           // Target Wi-Fi Access Point (SSID) router string
const char* password = "2444666668888888";            // Security pass-phrase required to authenticate WPA2
const char* mqtt_server = "broker.hivemq.com";        // Target MQTT broker address (Currently testing sandbox)
const char* telemetry_topic = "farmguard/jkuat_node_01/telemetry"; // Cloud outbound channel for telemetry [cite: 387]
const char* command_topic = "farmguard/jkuat_node_01/command";     // Cloud inbound channel for manual control

WiFiClient espClient;         // Creates the base network socket client wrapper
PubSubClient client(espClient); // Wraps the raw TCP socket in the MQTT protocol engine

// --- Hardware GPIO Allocations ---
const int dhtPin = D1;       // XIAO Digital Pin 1
const int soilPin = A0;      // XIAO Analog Pin 0 (ADC)
const int relayPin = D2;     // XIAO Digital Pin 2      // Digital pin driving the transistor switch for the water pump relay
#define DHTTYPE DHT11        // Set library parsing mode to handle 8-bit integer frames from the DHT11
DHT dht(dhtPin, DHTTYPE);    // Initialize DHT software driver instance

// --- The Shared "Thread-Safe" Data Vault ---
// Shared structure in RAM where independent tasks securely read or write variables[cite: 230].
struct GlobalData {
  float temperature;               // Current air temperature in degrees Celsius (updated by SensorTask)
  float humidity;                  // Current relative air humidity percentage (updated by SensorTask)
  int soilMoisture;                // Filtered soil moisture level mapped from 0% to 100% (updated by SensorTask)
  bool pumpIsOn;                   // Flags the current real-time state of the physical relay pin
  unsigned long lastSensorUpdate;  // Millisecond timestamp used by the Watchdog to check for task lockups [cite: 214]
  
  // Remote Control Automation Flags
  bool remoteCommandActive;        // State flag raised when a remote cloud trigger overrides local loop rules
  unsigned long remoteCommandStartTime; // Tracks when a remote manual pump burst started to prevent flooding

  // Level 6 Intelligence Tracker
  String edgeState;                // Stores inferred state: "NORMAL", "DRY_STRESS", "SEVERE_HEAT_STRESS", "SENSOR_FAULT" [cite: 365]
};
GlobalData systemData;       // Allocate physical memory space inside global RAM for our state struct
SemaphoreHandle_t dataMutex; // Mutex token pointer: used to lock/unlock access to systemData [cite: 230]

// --- OS Task Management Handles ---
TaskHandle_t SensorTaskHandle;   // Tracking handle for background sensor reading loop
TaskHandle_t ControlTaskHandle;  // Tracking handle for closed-loop decision matrix loop
TaskHandle_t DisplayTaskHandle;  // Tracking handle for serial print telemetry stream loop
TaskHandle_t AlertTaskHandle;    // Tracking handle for emergency threshold scanner loop
TaskHandle_t NetworkTaskHandle;  // Tracking handle for network stack client loop running on Core 0
TaskHandle_t WatchdogTaskHandle; // Tracking handle for high-priority software supervisor loop

// =========================================================================================================
// MQTT ASYNCHRONOUS CALLBACK HANDLER (Triggered automatically by client.loop() on packet receipt)
// =========================================================================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";             
  for (int i = 0; i < length; i++) { 
    message += (char)payload[i];   
  }
  
  Serial.println("[NETWORK] Command Received: " + message); 

  // PARSE PACKET: Search for the specific control substring inside the incoming payload [cite: 179]
  if (message.indexOf("\"PUMP_ON\"") > 0) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      systemData.remoteCommandActive = true;       // Raise override flag to request pump activation [cite: 176]
      systemData.remoteCommandStartTime = millis(); // Save timestamp to start safety cutoff clock [cite: 181]
      xSemaphoreGive(dataMutex);                   
    }
  }
}

// =========================================================================================================
// TASK 1: ENVIRONMENTAL SENSOR ACQUISITION THREAD (Allocated to CPU Core 1) - Frequency: 2000ms [cite: 214]
// =========================================================================================================
void SensorTask(void *pvParameters) {
  for (;;) { 
    float t = dht.readTemperature();   // Query DHT11 over 1-wire bus
    float h = dht.readHumidity();      // Query DHT11 over 1-wire bus
    int rawSoil = analogRead(soilPin); // Sample the 12-bit ADC channel (0 to 4095)
    
    // Scale and limit raw ADC voltage into calibrated percentage range (0% = dry, 100% = wet) [cite: 100]
    int mappedSoil = constrain(map(rawSoil, 2654, 1200, 0, 100), 0, 100);

    /*// LEVEL 6: Rule-Based Anomaly Detection Engine (The Edge Brain) 
    String currentInference = "NORMAL";

    if (isnan(t) || isnan(h) || t > 50.0 || t < -10.0 || h < 5.0 || h > 100.0) {
      currentInference = "SENSOR_FAULT"; // Catch broken wires, missing hardware, or impossible sensor metrics
    } else if (mappedSoil < 20 && t > 35.0 && h < 35.0) {
      currentInference = "SEVERE_HEAT_STRESS"; // Microclimate combinations that threaten immediate crop survival
    } else if (mappedSoil < 35) {
      currentInference = "DRY_STRESS"; // Standard soil moisture drop requiring baseline loop attention
    } else if (t > 38.0) {
      currentInference = "CRITICAL_HIGH_TEMP"; // Moisture is fine, but ambient thermal levels are dangerous
    }*/


    // LEVEL 6: TinyML Edge Inference Engine [Option C]
    // 1. Package the live readings into a float array [Temp, Hum, Soil]
    float input_features[3] = { t, h, (float)mappedSoil };

    // 2. Feed the array into the Decision Tree
    int prediction = mlBrain.predict(input_features);

    // 3. Map the ML output integers back to our system string states
    String currentInference = "NORMAL"; // Default fallback (Prediction 0)
    
    if (prediction == 1) {
      currentInference = "DRY_STRESS";
    } else if (prediction == 2) {
      currentInference = "SEVERE_HEAT_STRESS"; // Or "HEAT_STRESS" based on your Node-RED config
    } else if (prediction == 3) {
      currentInference = "SENSOR_FAULT";
    }

    // MUTEX LOCK SEQUENCE: Request exclusive write permissions for shared variables [cite: 230]
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      systemData.temperature = t;      
      systemData.humidity = h;         
      systemData.soilMoisture = mappedSoil; 
      systemData.edgeState = currentInference; // Log the evaluated intelligent state [cite: 266]
      systemData.lastSensorUpdate = millis();  // Reset watchdog timeout timer to prove thread health
      xSemaphoreGive(dataMutex);               
    }
    vTaskDelay(pdMS_TO_TICKS(2000));           // Yield CPU Core 1 execution for 2 seconds [cite: 214]
  }
}

// =========================================================================================================
// TASK 2: CLOSE-LOOP AUTOMATED ACTUATOR CONTROL THREAD (Allocated to CPU Core 1) - Frequency: 2000ms [cite: 214]
// =========================================================================================================
void ControlTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { 
      bool shouldPumpRun = false;                    

      // LEVEL 6 CRITICAL OVERRIDE: If a hardware or sensor fault is inferred, bypass all activation logic [cite: 216]
      if (systemData.edgeState == "SENSOR_FAULT") {
        Serial.println("[CONTROL] EDGE OVERRIDE: Sensor fault detected! Actuator forcefully isolated.");
        shouldPumpRun = false; 
      } 
      else {
        // SUB-LOGIC 1: Evaluate local automated thresholds (Auto-irrigate if soil moisture drops below 35%) [cite: 103]
        if (systemData.soilMoisture < 35) {
          shouldPumpRun = true;                       
        }

        // SUB-LOGIC 2: Evaluate cloud commands and enforce strict 5-second failsafe window [cite: 181]
        if (systemData.remoteCommandActive) {
          if (millis() - systemData.remoteCommandStartTime <= 5000) {
            shouldPumpRun = true;                     // Maintain cloud burst runtime [cite: 180]
          } else {
            Serial.println("[CONTROL] SAFETY TRIGGER: Remote manual command timeout achieved.");
            systemData.remoteCommandActive = false;   // Clear flag to restore local sensor control loop authority
          }
        }
      }

      // SUB-LOGIC 3: Apply resolved logic state directly to relay pin using High-Impedance Fix
      if (shouldPumpRun) {
        pinMode(relayPin, OUTPUT);    // Reconnect pin to output network
        digitalWrite(relayPin, LOW);  // Output 0V to pull optocoupled relay circuit low and turn pump ON
        systemData.pumpIsOn = true;   
      } else {
        pinMode(relayPin, INPUT);     // DISCONNECT THE PIN (High Impedance). Relay pull-up resistor forces 5.0V OFF state.
        systemData.pumpIsOn = false;  
      }
      
      xSemaphoreGive(dataMutex);      
    }
    vTaskDelay(pdMS_TO_TICKS(2000));  // Yield execution control for 2 seconds [cite: 214]
  }
}

// =========================================================================================================
// TASK 3: TELEMETRY AND SERIAL DISPLAY THREAD (Allocated to CPU Core 1) - Frequency: 2000ms [cite: 214]
// =========================================================================================================
void DisplayTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { 
      Serial.printf("[DISPLAY] Temp: %.1fC | Hum: %.1f%% | Soil: %d%% | Edge Brain: %s | Pump: %s\n", 
                    systemData.temperature, systemData.humidity, systemData.soilMoisture,
                    systemData.edgeState.c_str(), systemData.pumpIsOn ? "ON" : "OFF");
      xSemaphoreGive(dataMutex);                    
    }
    vTaskDelay(pdMS_TO_TICKS(2000));                
  }
}

// =========================================================================================================
// TASK 4: RISK ASSESSMENT AND THRESHOLD ALERT THREAD (Allocated to CPU Core 1) - Frequency: 5000ms [cite: 214]
// =========================================================================================================
void AlertTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { 
      if (systemData.soilMoisture < 20) {
        Serial.println("[ALERT] CRITICAL: Soil moisture dangerously low (<20%)!"); // Crop death boundary alert [cite: 171]
      }
      xSemaphoreGive(dataMutex);                    
    }
    vTaskDelay(pdMS_TO_TICKS(5000));                // Run safety validation scan every 5 seconds [cite: 214]
  }
}

// =========================================================================================================
// TASK 5: NETWORK STACK TELEMETRY ENGINE (Isolated to CPU Core 0) - Loop Speed: 50ms [cite: 214]
// =========================================================================================================
void NetworkTask(void *pvParameters) {
  Serial.println("\n[NETWORK] Core 0 Booting Wi-Fi Controller Stack..."); 
  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  unsigned long lastTelemetryTime = 0;

  for (;;) { 
    
    // LAYER-2 WI-FI KEEPALIVE ENGINE [cite: 216]
    if (WiFi.status() != WL_CONNECTED) { 
      Serial.print(".");                 // Diagnostic trace: Prints tracking indicators while connecting
      vTaskDelay(pdMS_TO_TICKS(1000));   
      continue;                          
    }

    // LAYER-7 MQTT BROKER RECONNECTION ENGINE [cite: 216]
    if (!client.connected()) {           
      Serial.println("\n[NETWORK] Local Wi-Fi Associated! Synchronizing MQTT Broker handshake..."); 
      
      String clientId = "ESP32Client-" + String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) {  
        Serial.println("[NETWORK] MQTT Broker Session Established!"); 
        client.subscribe(command_topic);       
      } else {
        Serial.println("[NETWORK] MQTT Handshake Failed. Retrying in 5000ms..."); 
        vTaskDelay(pdMS_TO_TICKS(5000));       
        continue;                              
      }
    }

    client.loop(); // Background socket processing loop for active TCP parsing

    // TELEMETRY TRANSMISSION ENGINE (Schedules outbound reports every 10 seconds) [cite: 214]
    if (millis() - lastTelemetryTime >= 10000) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { 
        
        // LEVEL 6 JSON PAYLOAD STRUCTURE: Formatted strictly to Appendix A specifications 
        String payload = "{";
        payload += "\"device_id\": \"FG-AIOT-NODE-01\",";
        payload += "\"schema_version\": \"1.0\",";
        payload += "\"temperature_c\": " + String(systemData.temperature, 1) + ",";
        payload += "\"humidity_percent\": " + String(systemData.humidity, 1) + ",";
        payload += "\"soil_moisture_percent\": " + String(systemData.soilMoisture) + ",";
        payload += "\"edge_state\": \"" + systemData.edgeState + "\","; // Injected local edge inference state [cite: 365]
        payload += "\"pump_status\": \"" + String(systemData.pumpIsOn ? "ON" : "OFF") + "\",";
        payload += "\"firmware_version\": \"1.1.0\"";
        payload += "}";

        client.publish(telemetry_topic, payload.c_str()); // Broadcast payload out to cloud channel broker [cite: 136]
        xSemaphoreGive(dataMutex);                        
      }
      lastTelemetryTime = millis(); 
    }
    
    // CORE 0 IDLE WATCHDOG PROTECTION
    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}

// =========================================================================================================
// TASK 6: SYSTEM WATCHDOG SUPERVISOR THREAD (Allocated to CPU Core 1) - Frequency: 1000ms (High Priority) [cite: 214]
// =========================================================================================================
void WatchdogTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) { 
      
      // CRITICAL TIMING AUDIT: Identify if the background sensing thread has frozen or encountered runtime lockup [cite: 217]
      if (millis() - systemData.lastSensorUpdate > 10000) {
        Serial.println("[WATCHDOG] FATAL EXCEPTION DETECTED: Core 1 Sensing Grid Frozen!");
        
        // EMERGENCY HARDWARE CUTOFF OVERRIDE [cite: 216]
        pinMode(relayPin, INPUT); // Break the output line connection immediately to isolate relay and save crops
      }
      xSemaphoreGive(dataMutex); 
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Audit runtime health variables exactly once per second [cite: 214]
  }
}

// =========================================================================================================
// INTERRUPT SYSTEM ENTRY, PERIPHERAL INITIALIZATION, & OS BOOT SEQUENCER
// =========================================================================================================
void setup() {
  Serial.begin(115200);   
  delay(3000);               
  
  // INITIALIZE SAFE FALLBACK HARDWARE MODE
  pinMode(relayPin, INPUT);              // Force pump relay to boot safely disconnected from output logic paths
  dht.begin();                           // Initialize 1-wire communication registers on DHT11 IC
  
  // Instantiate Binary Mutex token flags prior to FreeRTOS task distribution scheduler allocation [cite: 230]
  dataMutex = xSemaphoreCreateMutex();

  // =======================================================================================================
  // FREERTOS DISTRIBUTED MULTI-CORE SCHEDULER INITIALIZATION [cite: 212]
  // Syntax: xTaskCreatePinnedToCore( Function_Pointer, Debug_Name, Stack_Bytes, Params, Priority, &Handle, Core_ID );
  // =======================================================================================================
  
  // SPAWN NETWORK SUB-ROUTINES ON CPU CORE 0 (Complete layer isolation from microclimatic processing controls)
  xTaskCreatePinnedToCore(NetworkTask, "NetTask", 8192, NULL, 1, &NetworkTaskHandle, 0); 
  
  // SPAWN APPLICATION ARCHITECTURE INFRASTRUCTURE ON CPU CORE 1 (Silicon isolated execution)
  xTaskCreatePinnedToCore(SensorTask, "SensTask", 4096, NULL, 1, &SensorTaskHandle, 1);   // Priority 1: Reads peripheral inputs
  xTaskCreatePinnedToCore(ControlTask, "CtrlTask", 2048, NULL, 2, &ControlTaskHandle, 1); // Priority 2: Higher priority ensures immediate local safety actuation
  xTaskCreatePinnedToCore(DisplayTask, "DispTask", 4096, NULL, 1, &DisplayTaskHandle, 1); // Priority 1: Handles local diagnostics
  xTaskCreatePinnedToCore(AlertTask, "AlrtTask", 2048, NULL, 1, &AlertTaskHandle, 1);     // Priority 1: Audits emergency state vectors
  
  // SPAWN OPERATING SYSTEM WATCHDOG SUPERVISOR ON CPU CORE 1 (Maximum priority preemptive task scheduling) [cite: 214]
  xTaskCreatePinnedToCore(WatchdogTask, "WdogTask", 2048, NULL, 3, &WatchdogTaskHandle, 1); 
}

// =========================================================================================================
// ARDUINO CYCLIC TASK STACK: DELETED
// =========================================================================================================
void loop() {
  // FreeRTOS structures bypass sequential loops. The loop task handle is explicitly targeted for memory destruction
  // here to reclaim active hardware heap storage space and allocate 100% of execution cycles to our tasks.
  vTaskDelete(NULL); 
}