#include "LoRaWan_APP.h"
#include <vector>

// LoRa Configuration
#define RF_FREQUENCY 915000000
#define TX_OUTPUT_POWER 14
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

// Repeater State
std::vector<uint8_t> receivedImageData;
static RadioEvents_t RadioEvents;
bool lora_idle = true;
bool forwarding = false;
uint8_t txBuffer[250];
unsigned long lastRxTime = 0;
const unsigned long timeout = 3000;

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    
    // Initialize radio events
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    
    // Configure both RX and TX
    Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
                    
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
    
    Serial.println("LoRa Repeater Ready");
    Radio.Rx(0); // Start listening immediately
}

void loop() {
    // Handle timeout for incomplete images
    if(forwarding && millis() - lastRxTime > timeout) {
        Serial.println("Forwarding timeout - sending partial image");
        forwardData();
    }
    
    Radio.IrqProcess();
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    lastRxTime = millis();
    
    // Minimum viable packet size (2B header + 4B footer + 1B checksum)
    if(size < 7) {
        Serial.println("Packet too small, ignoring");
        Radio.Rx(0);
        return;
    }

    // Strip header bytes (CC DD)
    uint8_t* packetData = payload + 2;
    uint16_t dataSize = size - 2;

    // Search for JPEG end marker (FF D9)
    uint16_t jpegEnd = 0;
    bool foundJpegEnd = false;
    for(uint16_t i = 0; i < dataSize - 1; i++) {
        if(packetData[i] == 0xFF && packetData[i+1] == 0xD9) {
            jpegEnd = i + 2;
            foundJpegEnd = true;
            break;
        }
    }

    // Search for footer pattern (EE FF AB CD)
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
    uint16_t bytesToStore = foundJpegEnd ? jpegEnd : (foundFooter ? footerPos : dataSize);
    
    // Store the image data
    for(uint16_t i = 0; i < bytesToStore; i++) {
        receivedImageData.push_back(packetData[i]);
    }

    // Immediately forward the packet
    if(bytesToStore > 0) {
        memcpy(txBuffer, payload, size); // Forward the original packet
        forwarding = true;
        Radio.Send(txBuffer, size);
    } else {
        Radio.Rx(0);
    }

    Serial.printf("Received %d bytes | Forwarding %d bytes [RSSI: %d, SNR: %d]%s%s\n",
                 bytesToStore, size, rssi, snr,
                 foundFooter ? " [Footer]" : "",
                 foundJpegEnd ? " [JPEG End]" : "");
}

void forwardData() {
    if(receivedImageData.empty()) {
        Serial.println("No data to forward");
        forwarding = false;
        Radio.Rx(0);
        return;
    }

    // Add your forwarding logic here if you want to process the complete image
    // Otherwise it's already forwarding packets immediately in OnRxDone
    
    receivedImageData.clear();
    forwarding = false;
    Radio.Rx(0);
}

void OnTxDone(void) {
    Serial.println("Forwarding complete");
    lora_idle = true;
    forwarding = false;
    Radio.Rx(0); // Go back to receive mode
}

void OnTxTimeout(void) {
    Serial.println("Forwarding timeout");
    lora_idle = true;
    forwarding = false;
    Radio.Rx(0);
}
