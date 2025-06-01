#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <vector>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Wire.h>
#include "pins_arduino.h"


#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             5        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define BUFFER_SIZE                                 250 // Increased buffer size for image chunks
#define WIFI_SSID "LoRa_AP"
#define SERVER_PORT 1234

char txBuffer[BUFFER_SIZE];
uint8_t buffer[BUFFER_SIZE];
bool lora_idle = true;
static RadioEvents_t RadioEvents;
std::vector<std::vector<uint8_t>> allPackets;
WiFiServer server(SERVER_PORT);
bool gotIMG = false;
bool allrec = false;

void OnTxDone(void);
void OnTxTimeout(void);

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for microseconds to seconds
#define TIME_TO_SLEEP  1800  

void setup() {
    Serial.begin(115200);

    Serial.println("Starting Wifi");
    WiFi.softAP(WIFI_SSID);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP: "); Serial.println(IP);
    server.begin();

    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
}

void loop() {
    WiFiClient client = server.available();
    
    // Receive image packets from camera
    if(!gotIMG && client) {
        client.setTimeout(5000);
        int count = 0;
        uint32_t received = 0;
        allPackets.clear();
        
        while(client.connected()) {
            if(client.available()) {
                int bytesRead = client.read(buffer, sizeof(buffer));
                if(bytesRead <= 0) {
                    break;
                }
                
                // Verify checksum (last byte)
                uint8_t checksum = buffer[bytesRead-1];
                uint8_t calc_checksum = 0;
                for(int i = 0; i < bytesRead - 1; i++) {
                    calc_checksum ^= buffer[i];
                }

                if(checksum == calc_checksum) {
                    // Store the packet (without checksum)
                    std::vector<uint8_t> packet;
                    packet.insert(packet.end(), buffer, buffer+bytesRead-1);
                    allPackets.push_back(packet);
                    received += bytesRead;
                    
                    // Send ACK to camera
                    client.printf("ACK%d\n", count);
                    count++;
                }
            }
        }
        client.stop();
        gotIMG = true;
        Serial.printf("Received complete image in %d packets\n", allPackets.size());
    }

    // Send image data via LoRa
    if(gotIMG && lora_idle) {
        static size_t packetIndex = 0;
        static size_t byteIndex = 2; // Skip first 2 bytes (header) of each packet
        
        if(packetIndex < allPackets.size()) {
            // Get current packet
            std::vector<uint8_t>& currentPacket = allPackets[packetIndex];
            
            // Determine how much data we can send in this chunk
            size_t remainingInPacket = currentPacket.size() - byteIndex;
            size_t chunkSize = min(remainingInPacket, (size_t)BUFFER_SIZE);
            
            if(chunkSize > 0) {
                // Copy the raw image data (skip headers)
                memcpy(txBuffer, &currentPacket[byteIndex], chunkSize);
                
                Serial.printf("Sending packet %d, chunk starting at byte %d, size %d\n", 
                             packetIndex, byteIndex, chunkSize);
                
                // Send the raw image data
                Radio.Send((uint8_t *)txBuffer, chunkSize);
                lora_idle = false;
                
                byteIndex += chunkSize;
            }
            
            // Move to next packet if we've sent all data from current packet
            if(byteIndex >= currentPacket.size()) {
                packetIndex++;
                byteIndex = 2; // Reset byte index for next packet (skip header)
            }
        } else {
            // All packets sent
            Serial.println("Finished sending all image data");
            gotIMG = false;
            allPackets.clear();
            delay(2000); // Wait before checking for new images
            esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
            esp_deep_sleep_start();
        }
    }
    
    Radio.IrqProcess();
}

void OnTxDone(void) {
    Serial.println("TX done");
    lora_idle = true;
}

void OnTxTimeout(void) {
    Radio.Sleep();
    Serial.println("TX Timeout");
    lora_idle = true;
}