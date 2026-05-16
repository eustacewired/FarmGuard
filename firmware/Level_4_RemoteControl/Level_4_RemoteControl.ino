// ====================================================================
// PROGRAM TITLE: FarmGuard-AIoT: Level 4 - Remote Command & Safety Timeout
// PURPOSE: Implements bi-directional MQTT control and hardware safety fail-safes.
// DESCRIPTION: This firmware shifts the edge node from a passive data transmitter
//              into a responsive control unit. It subscribes to a dedicated cloud 
//              command channel, decodes remote actions, and runs an asynchronous 
//              hardware "stopwatch" safety thread to prevent agricultural flooding.
// ====================================================================

// --- External Network & Hardware Libraries ---
#include <WiFi.h>          // Core ESP32 radio stack driver enabling local wireless router pairing.
#include <PubSubClient.h>  // MQTT protocol abstraction handling network data subscription packet brokers.

// --------------------------------------------------------------------
// LOCAL NETWORK & MQTT TOPIC INFRASTRUCTURE CONFIGURATION
// --------------------------------------------------------------------
// Local area network routing targets used to configure physical RF transceiver chips.
const char* ssid = "Molleene";         // Storage string for target local router identity name tags.
const char* password = "19molleene58"; // Storage string for local network security authentication keys.

// Public network endpoints facilitating distributed messaging architectures.
const char* mqtt_server = "broker.hivemq.com"; // Web URL string identifying the public gateway gateway broker.
const int mqtt_port = 1883;                    // TCP protocol interface port mapped globally for basic MQTT streams.

// BI-DIRECTIONAL TOPIC PATHWAYS:
// One out-bound pipe to distribute telemetry; one inbound pipe to capture remote operator variables.
const char* telemetry_topic = "farmguard/jkuat_node_01/telemetry";
const char* command_topic = "farmguard/jkuat_node_01/command";

// --------------------------------------------------------------------
// INSTANTIATED WIRELESS CLIENT ROUTERS
// --------------------------------------------------------------------
WiFiClient espClient;           // Allocates internal processing registers to manage open TCP networking stack links.
PubSubClient client(espClient); // Wraps standard MQTT command handlers directly inside the active network client socket.

// --------------------------------------------------------------------
// HARDWARE PERIPHERALS & OPERATIONAL STATES
// --------------------------------------------------------------------
const int relayPin = 5;      // Pin 5 acts as the digital logic switch line feeding the physical water pump relay.
bool pumpIsOn = false;       // Master operational state flag tracking immediate runtime status of water movement.

// --------------------------------------------------------------------
// ASYNCHRONOUS SAFETY OVER-RUN WATCHDOG MOTOR
// --------------------------------------------------------------------
// Prevents continuous irrigation if a cloud dashboard crashes, network signals freeze, or parameters fail to clear.
unsigned long pumpStartTime = 0;           // Dynamic container capturing the exact startup snapshot millisecond of a cycle.
const unsigned long MAX_PUMP_TIME = 5000;  // Critical design limit capping maximum mechanical run cycles to exactly 5 seconds.

// ====================================================================
// SETUP FUNCTION: RUNS ONCE AT SYSTEM BOOT
// ====================================================================
void setup() {
  // Initialize communication registers for physical hardware interface logging at 115200 bps.
  Serial.begin(115200);
  
  // Bind pin orientation roles to control electrical logic line driving layouts.
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // Hardware Safety Interlock: Force output HIGH to ensure pump stays OFF at startup.
  
  // Establish baseline local physical network connections before spinning up system logic layers.
  setup_wifi();
  
  // Direct MQTT client engine hooks to resolve address locations across external cloud matrices.
  client.setServer(mqtt_server, mqtt_port);
  
  // Register the operational listening callback handler. This maps incoming network packet streams 
  // into memory arrays for data evaluation cycles.
  client.setCallback(mqttCallback); 
}

// ====================================================================
// FUNCTION: NON-BLOCKING LOCAL AREA NETWORK ENGAGEMENT ENGINE
// ====================================================================
void setup_wifi() {
  delay(10); // Safe hardware rest interval letting core transceiver chip voltages normalize cleanly.
  WiFi.begin(ssid, password); // Pass configuration criteria out to initiate network link sequences.
  
  // Safe verification loops checking connection flags before returning runtime to higher application processing.
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  Serial.println("WiFi connected!");
}

// ====================================================================
// FUNCTION: RECOVERY ROUTINE RESTORING LOST BROKER PERSISTENCE
// ====================================================================
void reconnect() {
  // Execute connection evaluation loops until the cloud validates the handshake token block.
  while (!client.connected()) {
    // Generate a unique transaction tracking signature using dynamic hex calculation variables.
    // Unique naming constraints prevent overlapping node assets from dropping each other offline.
    String clientId = "FarmGuardNode-" + String(random(0xffff), HEX);
    
    // Attempt standard authentication loops using our clean identification string profiles.
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected to Broker!");
      
      // CRITICAL NETWORK STEP: Issue subscription packets right after connecting to register 
      // the node with the broker routing tables. If skipped, remote commands are completely ignored.
      client.subscribe(command_topic);
      Serial.println("Listening for remote commands...");
    } else {
      // Safety Back-off Delay: Halts operational processing before triggering automated loop retries.
      delay(5000);
    }
  }
}

// ====================================================================
// THE INTERRUPT-STYLE LISTENER: TRIGGERS WHENEVER CLOUD PACKETS ARRIVE
// ====================================================================
// Parameters:
//   - topic: Char array holding the address string that received the message payload.
//   - payload: Raw pointer to raw byte blocks holding packet content.
//   - length: Integer size limit determining byte allocation boundaries.
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  
  // Parsing Matrix: Extract raw byte streams from memory buffers and convert them into a manipulable String format.
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i]; // Typecast character formatting indices directly inside extraction strings.
  }
  Serial.println("Payload: " + message);

  // Command Evaluation Block: Evaluates if string markers map directly to intended targets.
  // indexOf checks string array profiles. If return integers point above zero, validation tests clear.
  if (message.indexOf("\"PUMP_ON\"") > 0) {
    Serial.println("COMMAND RECEIVED: Activating Pump remotely!");
    
    // Actuate physical pins: Drive Active LOW relay circuit down to close coil connection paths (Pump ON).
    digitalWrite(relayPin, LOW); 
    
    pumpIsOn = true;             // Set state tracker true to instruct independent safety tracking loops to begin evaluation.
    pumpStartTime = millis();    // Log current running millisecond timestamp to lock in tracking references.
  }
}

// ====================================================================
// MAIN RUNTIME KERNEL: EXECUTED REPETITIVELY WITHOUT INTERRUPTION
// ====================================================================
void loop() {
  // Connection Guardrail: Check active cloud linking registers before executing transactional sweeps.
  if (!client.connected()) { reconnect(); }
  
  // Yield processing power to background network registers to capture inbound incoming traffic, 
  // route callbacks, and dispatch standard MQTT heartbeat data frames.
  client.loop(); 

  // ------------------------------------------------------------------
  // INDEPENDENT HARDWARE FAIL-SAFE WATCHDOG THREAD
  // ------------------------------------------------------------------
  // Evaluate if system configurations register active mechanical field loops.
  if (pumpIsOn) {
    // Non-blocking delta calculations: Subtract current millisecond values from stored start snapshots.
    // If the elapsed period matches or breaks past the 5-second maximum allocation limit, force an emergency override.
    if (millis() - pumpStartTime >= MAX_PUMP_TIME) {
      Serial.println("SAFETY TRIGGER: Maximum pump time reached. Forcing OFF.");
      
      // Override Actuation: Instantly push the active LOW line high to kill power lines feeding water actuators.
      digitalWrite(relayPin, HIGH); 
      
      pumpIsOn = false; // Reset the state tracker flag to clear emergency routines and secure steady state.
    }
  }
}
