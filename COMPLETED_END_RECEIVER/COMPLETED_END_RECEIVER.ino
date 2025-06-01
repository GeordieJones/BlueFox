#include "LoRaWan_APP.h"
#include <vector>

// LoRa Configuration
#define RF_FREQUENCY 915000000
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

// Receiver state
std::vector<uint8_t> receivedImageData;
unsigned long lastPacketTime = 0;
const unsigned long timeout = 3000; // 3 second timeout
bool imageStarted = false;
bool lora_idle = true;
static RadioEvents_t RadioEvents;

void dumpCompleteImage();
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnTxDone(void);
void OnTxTimeout(void);

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for microseconds to seconds
#define TIME_TO_SLEEP  1800  

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                     LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                     LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                     0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
    
    Serial.println("LoRa Receiver Ready");
}

void loop() {
    if(lora_idle) {
        lora_idle = false;
        Radio.Rx(0);
    }
    
    // Handle timeout for incomplete images
    if(imageStarted && millis() - lastPacketTime > timeout) {
        Serial.println("\nTIMEOUT - INCOMPLETE IMAGE RECEIVED");
        Serial.printf("Received %d bytes without end marker\n", receivedImageData.size());
        dumpCompleteImage();
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
    }
    
    Radio.IrqProcess();
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    lastPacketTime = millis();

    // Minimum viable packet size
    if(size < 7) { // 2B header + 4B footer + 1B checksum
        Serial.println("Packet too small, ignoring");
        lora_idle = true;
        return;
    }

    // Strip header bytes (CC DD)
    uint8_t* packetData = payload + 2;
    uint16_t dataSize = size - 2;

    // Search for JPEG end marker (FF D9) first
    uint16_t jpegEnd = 0;
    bool foundJpegEnd = false;
    for(uint16_t i = 0; i < dataSize - 1; i++) {
        if(packetData[i] == 0xFF && packetData[i+1] == 0xD9) {
            jpegEnd = i + 2; // Include the FF D9 bytes
            foundJpegEnd = true;
            break;
        }
    }

    // Now search for footer pattern (EE FF AB CD)
    uint16_t footerPos = 0;
    bool foundFooter = false;
    for(uint16_t i = 0; i < dataSize - 3; i++) {
        if(packetData[i] == 0xEE && 
           packetData[i+1] == 0xFF && 
           packetData[i+2] == 0xAB && 
           packetData[i+3] == 0xCD) {
            footerPos = i;
            foundFooter = true;
            break;
        }
    }

    // Determine what to store
    uint16_t bytesToStore = dataSize;
    if(foundJpegEnd) {
        bytesToStore = jpegEnd; // Stop at FF D9
    } 
    else if(foundFooter) {
        bytesToStore = footerPos; // Stop before footer if no FF D9 found
    }

    // Store the image data
    for(uint16_t i = 0; i < bytesToStore; i++) {
        receivedImageData.push_back(packetData[i]);
    }

    // Check for complete image
    if(foundJpegEnd) {
        Serial.println("JPEG END DETECTED");
        dumpCompleteImage();
    } 
    else {
        Serial.printf("Received %d bytes%s%s\n", 
                    bytesToStore,
                    foundFooter ? " [Footer found]" : "",
                    foundJpegEnd ? " [JPEG End found]" : "");
    }

    Radio.Sleep();
    lora_idle = true;
}

void dumpCompleteImage() {
    if(receivedImageData.empty()) {
        Serial.println("No image data to dump");
        return;
    }

    // Verify we have proper start/end markers
    bool validStart = (receivedImageData.size() >= 2) && 
                     (receivedImageData[0] == 0xFF) && 
                     (receivedImageData[1] == 0xD8);
    
    bool validEnd = (receivedImageData.size() >= 2) &&
                   (receivedImageData[receivedImageData.size()-2] == 0xFF) && 
                   (receivedImageData[receivedImageData.size()-1] == 0xD9);

    Serial.println("\n==== IMAGE DATA ====");
    Serial.printf("Total size: %d bytes\n", receivedImageData.size());
    Serial.printf("Start marker: %s\n", validStart ? "FF D8 (Valid)" : "Invalid");
    Serial.printf("End marker: %s\n", validEnd ? "FF D9 (Valid)" : "Invalid");

    // Only dump if markers are valid
    if(validStart && validEnd) {
        Serial.println("\nFirst 16 bytes:");
        for(int i = 0; i < min(16, (int)receivedImageData.size()); i++) {
            Serial.printf("%02X ", receivedImageData[i]);
        }
        
        Serial.println("\n\nLast 16 bytes:");
        int start = max(0, (int)receivedImageData.size() - 16);
        for(int i = start; i < receivedImageData.size(); i++) {
            Serial.printf("%02X ", receivedImageData[i]);
        }


        // COMPLETE HEX DUMP ADDITION
        Serial.println("\n\nCOMPLETE HEX DUMP:");
        for(size_t i = 0; i < receivedImageData.size(); i++) {
            Serial.printf("%02X ", receivedImageData[i]);
            //if((i + 1) % 16 == 0) Serial.println(); // New line every 16 bytes
        }


    } else {
        Serial.println("Invalid JPEG data - not dumping contents");
    }

    // Clear for next image
    receivedImageData.clear();
    imageStarted = false;
    Serial.println("\nREADY FOR NEXT TRANSMISSION");
}

void OnTxDone(void) {
    Serial.println("TX done");
    lora_idle = true;
}

void OnTxTimeout(void) {
    Serial.println("TX timeout");
    lora_idle = true;
}