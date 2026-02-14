#include <WiFi.h> 
#include <WiFiClientSecure.h> 
#include <UniversalTelegramBot.h> 
#include <ModbusMaster.h> 
const char* ssid   = "CMF2Pro";       
const char* password   = "12345678";   
// Telegram Bot Credentials 
#define BOTtoken "Your_Telegram_Bot_Token"   
#define CHAT_ID "Your_Chat_ID"                                        
const float VOLTAGE_HIGH_LIMIT = 250.0;  
const float CURRENT_HIGH_LIMIT = 20.0;   
#define MASTER_DE_RE_PIN 18   
#define SLAVE_DE_RE_PIN  19  
// ESP32 Serial2 Pins (Standard) 
#define RX_PIN 16 
#define TX_PIN 17 
#define ALERT_LED_PIN 25     
// PZEM Settings 
#define PZEM_SLAVE_ID 0xF8  // Default ID is usually 1 (or 0xF8 for broadcast) 
// Telegram Sending Interval 
// Let's send every 15 seconds. 
const unsigned long sendInterval = 15000;  
unsigned long lastSendTime = 0; 
  
ModbusMaster node; 
WiFiClientSecure client; 
UniversalTelegramBot bot(BOTtoken, client); 
 
 
// 1. Before Sending: Master SHOUTS, Slave LISTENS 
void preTransmission() { 
  digitalWrite(MASTER_DE_RE_PIN, HIGH); // Master -> TX 
  digitalWrite(SLAVE_DE_RE_PIN, LOW);   // Slave  -> RX 
  delay(10);  
} 
 
// 2. After Sending: Master LISTENS, Slave SHOUTS (to reply) 
void postTransmission() { 
  digitalWrite(MASTER_DE_RE_PIN, LOW);  // Master -> RX 
  digitalWrite(SLAVE_DE_RE_PIN, HIGH);  // Slave  -> TX 
  delay(10);  
} 
 
 
void setup() { 
  Serial.begin(115200); // Monitor for PC 
  Serial.println("\nStarting System..."); 
 
  // 1. Configure Pins 
  pinMode(MASTER_DE_RE_PIN, OUTPUT); 
  pinMode(SLAVE_DE_RE_PIN, OUTPUT); 
  pinMode(ALERT_LED_PIN, OUTPUT); 
   
  // Start in IDLE State (Both Listening, Bus Quiet, LED OFF) 
  digitalWrite(MASTER_DE_RE_PIN, LOW); 
  digitalWrite(SLAVE_DE_RE_PIN, LOW); 
  digitalWrite(ALERT_LED_PIN, LOW); 
 
  // 2. Connect to WiFi 
  Serial.print("Connecting to WiFi: "); 
  Serial.println(ssid); 
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password); 
  while (WiFi.status() != WL_CONNECTED) { 
    Serial.print("."); 
    delay(500); 
  } 
  Serial.println("\nWiFi connected!"); 
  Serial.print("IP address: "); 
  Serial.println(WiFi.localIP()); 
 
  // Secure client setup for Telegram (allow self-signed certs if necessary) 
  client.setInsecure(); 
 
  // 3. Start RS485 and Modbus 
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); 
  node.begin(PZEM_SLAVE_ID, Serial2); 
  // Attach the "See-Saw" callbacks 
  node.preTransmission(preTransmission); 
  node.postTransmission(postTransmission); 
 
  Serial.println("RS485 System Initialized. Waiting for first reading interval..."); 
  bot.sendMessage(CHAT_ID, "    System Online: Industrial Energy Monitor Started.", ""); 
  delay(1000); 
} 
 
void loop() { 
  // Use non-blocking timer to read sensor and send data every 'sendInterval' 
  if (millis() - lastSendTime > sendInterval) { 
     
    Serial.println("\nRequesting Data from PZEM..."); 
    uint8_t result; 
 
    // Request 10 Registers starting at 0x0000 (Voltage) 
    // This gets Voltage, Current, Power, Energy, Freq, PF all at once 
    result = node.readInputRegisters(0x0000, 10); 
 
    digitalWrite(SLAVE_DE_RE_PIN, LOW); 
 
    if (result == node.ku8MBSuccess) { 
      Serial.println("Read Success. Processing..."); 
       
      // 1. Voltage (Register 0x0000) - Unit: 0.1V 
      float voltage = node.getResponseBuffer(0) / 10.0f; 
 
      // 2. Current (Registers 0x0001 - 0x0002) - Unit: 0.001A 
      uint32_t current_raw = ((uint32_t)node.getResponseBuffer(2) << 16) |node.getResponseBuffer(1); 
      float current = current_raw / 1000.0f; 
 
      // 3. Power (Registers 0x0003 - 0x0004) - Unit: 0.1W 
      uint32_t power_raw = ((uint32_t)node.getResponseBuffer(4) << 16) | node.getResponseBuffer(3); 
      float power = power_raw / 10.0f; 
 
      // 4. Energy (Registers 0x0005 - 0x0006) - Unit: 1Wh 
      uint32_t energy_raw = ((uint32_t)node.getResponseBuffer(6) << 16) | node.getResponseBuffer(5); 
      float energy = energy_raw / 1000.0f; // Convert to kWh 
 
      // 5. Frequency (Register 0x0007) - Unit: 0.1Hz 
      float freq = node.getResponseBuffer(7) / 10.0f; 
 
      // 6. Power Factor (Register 0x0008) - Unit: 0.01 
      float pf = node.getResponseBuffer(8) / 100.0f; 
 
      bool alertActive = false; 
      String alertMsg = ""; 
 
      if (voltage > VOLTAGE_HIGH_LIMIT) { 
        alertActive = true; 
        alertMsg += "⚠ HIGH VOLTAGE ALERT! "; 
      } 
      if (current > CURRENT_HIGH_LIMIT) { 
        alertActive = true; 
        alertMsg += "⚠ HIGH CURRENT ALERT! "; 
      } 
 
      if (alertActive) { 
        digitalWrite(ALERT_LED_PIN, HIGH); // Turn ON Alert LED 
        Serial.println("ALERT CONDITION DETECTED!"); 
      } else { 
        digitalWrite(ALERT_LED_PIN, LOW);  // Turn OFF Alert LED 
      } 
 
       
      Serial.println("---- PZEM Data ----"); 
      Serial.print("Voltage: "); Serial.print(voltage, 1); Serial.println(" V"); 
      Serial.print("Current: "); Serial.print(current, 3); Serial.println(" A"); 
      Serial.print("Power:   "); Serial.print(power, 1);   Serial.println(" W"); 
      Serial.println("-------------------"); 
 
      // --- SEND TO TELEGRAM --- 
      if (WiFi.status() == WL_CONNECTED) { 
        String msg = "              Energy Monitor Report\n\n"; 
         
       
        if (alertActive) { 
           msg += alertMsg + "\n\n"; 
        } 
         
        msg += "V: " + String(voltage, 1) + " V\n"; 
        msg += "I:  " + String(current, 3) + " A\n"; 
        msg += "P:  " + String(power, 1) + " W\n"; 
        msg += "E:  " + String(energy, 3) + " kWh\n"; 
        msg += "F:  " + String(freq, 1) + " Hz\n"; 
        msg += "PF: " + String(pf, 2); 
 
        Serial.println("Sending Telegram message..."); 
 
        if(bot.sendMessage(CHAT_ID, msg, "Markdown")) { 
           Serial.println("Telegram sent successfully."); 
        } else { 
           Serial.println("Telegram send failed."); 
        } 
      } else { 
        Serial.println("WiFi Disconnected. Cannot send Telegram."); 
        
      } 
 
    } else { 
      // Error Handling 
      Serial.print("Error Reading Modbus: 0x"); 
      Serial.println(result, HEX); 
      Serial.println("Check Connections: A-A, B-B, and Control Wires."); 
      if (WiFi.status() == WL_CONNECTED) { 
          bot.sendMessage(CHAT_ID, "⚠ Error: Could not read data from PZEM sensor.", ""); 
      } 
    } 
 
    // Update the last send time 
    lastSendTime = millis(); 
  } 
   
 
  delay(10); 
}
