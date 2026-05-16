// ====================================================================
// PROGRAM TITLE: FarmGuard-AIoT: Level 3 - IoT Connectivity & Telemetry
// PURPOSE: Establishes Wi-Fi networking and MQTT cloud data pipelines.
// DESCRIPTION: This firmware upgrades the local irrigation system into an 
//              active IoT edge node. It connects to an external local network,
//              maintains an active link with a public cloud MQTT broker, 
//              packages environmental telemetry into standard JSON strings, 
//              and publishes live updates on an active 5-second interval.
// ====================================================================

// --- External Network & Hardware Libraries ---
#include <WiFi.h>          // Core ESP32 networking library enabling 802.11 Wi-Fi station connectivity.
#include <PubSubClient.h>  // MQTT abstraction client library enabling publish/subscribe cloud messaging patterns.
#include "DHT.h"           // Native driver suite providing mathematical extraction layers for the DHT11 sensor.

// --------------------------------------------------------------------
// NETWORK & CLOUD MQTT BROKER CONFIGURATION
// --------------------------------------------------------------------
// Provide local local area network routing credentials for standard station registration.
const char* ssid = "Molleene";         // Input variable storing target 2.4GHz network service identifier.
const char* password = "19molleene58"; // Input variable storing security keys for local area network access.

// Cloud communication gateway references. Using HiveMQ's public testing sandbox infrastructure.
const char* mqtt_server = "broker.hivemq.com"; // Dynamic Domain Name System (DNS) string target for the cloud broker.
const int mqtt_port = 1883;                    // Standard TCP port allocated globally for unencrypted MQTT routing traffic.

// Unique topic namespace layout to segregate messages across public brokers.
// Structure mirrors standard RESTful pathways: root/project_node/telemetry_payload.
const char* telemetry_topic = "farmguard/jkuat_node_01/telemetry";

// --------------------------------------------------------------------
// INSTANTIATED INFRASTRUCTURE CLIENT CONSTRUCTORS
// --------------------------------------------------------------------
WiFiClient espClient;           // Allocates TCP internal networking stack handlers within ESP32 execution layers.
PubSubClient client(espClient); // Links the MQTT operational layer directly to the physical underlying network socket.

// --------------------------------------------------------------------
// HARDWARE PERIPHERAL PIN ALLOCATIONS
// --------------------------------------------------------------------
const int ledPin = 21;       // Pin 21 controls physical hardware data line driving the visual status indicator LED.
const int dhtPin = 4;        // Pin 4 implements single-wire time-critical serial protocols linked to the DHT11.
const int soilPin = 34;      // Pin 34 processes incoming analog metrics through the ESP32's built-in 12-bit ADC interface.
const int relayPin = 5;      // Pin 5 handles manual or automatic physical trigger gates on the irrigation water pump.

// --------------------------------------------------------------------
// TELEMETRY CALIBRATION & MANAGEMENT VARIABLES
// --------------------------------------------------------------------
#define DHTTYPE DHT11        // Setup parameter declaring physical model version limits of the DHT architecture.
DHT dht(dhtPin, DHTTYPE);    // Direct definition binding pin configurations into the driver object model instance.

const int AirValue = 2654;   // Fixed ADC calibration maximum corresponding to bone-dry structural soil conditions.
const int WaterValue = 1200; // Fixed ADC calibration minimum corresponding to completely saturated soil profiles.
const int MOISTURE_THRESHOLD = 35; // Logic parameter: If calculated moisture falls below 35%, mechanical loops engage.

// --------------------------------------------------------------------
// MILLIS MULTI-TASKING TIMING MATRIX
// --------------------------------------------------------------------
unsigned long previousMillisSensors = 0; // Tracks the absolute system uptime millisecond telemetry was last gathered.
const long sensorInterval = 5000;       // Interval parameter scaled up to 5000ms to comply with cloud ingestion traffic limits.

// --------------------------------------------------------------------
// SYSTEM OPERATION STRINGS
// --------------------------------------------------------------------
String pumpState = "OFF";    // Global data buffer storing string tracking elements representing immediate physical relay status.

// ====================================================================
// SETUP FUNCTION: RUNS ONCE ON POWER DELIVERY / BOOT SEQUENCE
// ====================================================================
void setup() {
  // Initialize communication registers for physical hardware interface logging at 115200 bps.
  Serial.begin(115200);
  
  // Designate directions for input pins and output transistors.
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  
  // Power up protection: High electrical state stops active LOW module switches from pulsing out water at boot.
  digitalWrite(relayPin, HIGH); 
  
  // Enable baseline clock line initialization cycles inside digital sensor data arrays.
  dht.begin();
  
  // Inform the MQTT driver abstraction layer where to point messaging payloads across network grids.
  client.setServer(mqtt_server, mqtt_port);

  // Task execution call: Boots routing operations to cross active local network infrastructure.
  setup_wifi();
}

// ====================================================================
// FUNCTION: ESTABLISH STATION-MODE LOCAL ROUTER LINKAGE
// ====================================================================
void setup_wifi() {
  delay(10); // Short internal hardware settlement period before issuing RF radio triggers.
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  // Trigger base level connection sequence using provided network authentication variables.
  WiFi.begin(ssid, password);

  // Blocking validation state engine: Halts primary setup loops until local handshake phases close out.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // Wait 500ms between validation checks to avoid flooding standard access points.
    Serial.print(".");
  }

  // Network path closed: Print dynamic local addresses to verify physical connection parameters.
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ====================================================================
// FUNCTION: BLOCKING RETRY SUBSYSTEM MAINTAINING CLOUD MQTT RELATIONS
// ====================================================================
void reconnect() {
  // Check loop constraints: Retries connection until the handshake resolves into a clean validation state.
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create an ephemeral identification string. Public brokers disconnect overlapping client signatures immediately.
    // Using a random hex suffix prevents this local processor from bumping duplicate nodes offline.
    String clientId = "FarmGuardNode-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt standard authentication connection procedures using the clean dynamic tracking identifier.
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected to HiveMQ Broker!");
    } else {
      // Diagnostic branch: Extract structural client states to decipher network failures (e.g., -2 = No network link).
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      
      // Delay framework pacing retry frequencies to prevent denial-of-service flags on open gateways.
      delay(5000); 
    }
  }
}

// ====================================================================
// MAIN APPLICATION LOOP: RUNS REPETITIVELY WITHOUT INTERRUPTION
// ====================================================================
void loop() {
  // Structural Guardrail: Check active connection states before issuing any transactional payloads.
  if (!client.connected()) {
    reconnect(); // Force immediate path recovery loops if link states fall over during execution.
  }
  
  // Internal MQTT engine tick handler: Reviews active network buffers for incoming subscribe messages 
  // and dispatches outbound keep-alive pings to prevent the broker from dropping the line.
  client.loop(); 

  // Capture current uptime runtime of the microcontroller in milliseconds.
  unsigned long currentMillis = millis();

  // ------------------------------------------------------------------
  // TASK: TELEMETRY EXTRACTION, LOCAL PROCESSING, & CLOUD INGESTION
  // ------------------------------------------------------------------
  // Asynchronous evaluation verifying if execution has scaled past the 5-second cloud transmission window.
  if (currentMillis - previousMillisSensors >= sensorInterval) {
    previousMillisSensors = currentMillis; // Re-align reference line to track the next interval target.

    // 1. Physical Extraction: Digital and Analog Sensor Layer Reads
    float humidity = dht.readHumidity();      // Pull calculated relative atmospheric humidity values.
    float temperature = dht.readTemperature(); // Pull calculated atmospheric Celsius temperature profiles.
    int rawSoilValue = analogRead(soilPin);   // Check direct hardware ADC counts passing from the root zone.
    
    // 2. Data Standardizing: Percentage Curve Mapping & Clamping
    // Normalize raw physical data arrays across a standardized, human-readable 0% - 100% scale.
    int soilMoisturePercent = map(rawSoilValue, AirValue, WaterValue, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100); // Protect database boundaries from extreme spikes.

    // 3. Automation Layer: Closed-Loop Threshold Evaluation
    if (soilMoisturePercent < MOISTURE_THRESHOLD) {
      digitalWrite(relayPin, LOW); // Pull Active LOW circuit line down to close magnetic coil coils (Pump ON).
      pumpState = "ON";            // Update internal state variable to generate clean reporting logs.
    } else {
      digitalWrite(relayPin, HIGH); // Drive logic line up to break system circuit flows (Pump OFF).
      pumpState = "OFF";            // Update internal state variable to generate clean reporting logs.
    }

    // 4. Serialization Matrix: Structural JSON String Compilation
    // Concatenate character data and integer primitives into a standard JSON payload format string.
    // Explicit escape sequencing (\") formats clean quotation metrics compliant with cloud databases.
    String payload = "{";
    payload += "\"device_id\":\"FG-AIOT-NODE-01\",";
    payload += "\"temperature_c\":" + String(temperature) + ",";
    payload += "\"humidity_percent\":" + String(humidity) + ",";
    payload += "\"soil_moisture_percent\":" + String(soilMoisturePercent) + ",";
    payload += "\"pump_status\":\"" + pumpState + "\",";
    payload += "\"firmware_version\":\"1.0.0\"";
    payload += "}";

    // 5. Cloud Transmission: Outbound Network Ingestion
    Serial.print("Publishing to topic: ");
    Serial.println(telemetry_topic);
    Serial.println(payload); // Copy structured output string directly out through local diagnostic terminals.
    
    // Transmit character array conversion directly into the established network socket pipe.
    client.publish(telemetry_topic, payload.c_str());
    
    // Visual Heartbeat Diagnostic: Flash indicator LED line briefly to verify successful cloud packet transmissions.
    digitalWrite(ledPin, HIGH); // Illuminate indicating network packet transaction processed.
    delay(100);                 // Short inline execution delay to make the flash human-visible.
    digitalWrite(ledPin, LOW);  // Extinguish indicator line to return back to quiet processing loops.
  }
}
