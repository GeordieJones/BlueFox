//ESP CAM code 

#include <WiFi.h>
#include "esp_camera.h"
#include <vector>

// pin defs
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

#define LORA_MAX_PACKET 240
#define PACKET_SIZE     230
#define MAX_RETRIES     3
#define ACK_TIMEOUT     2000

const char* ssid = "LoRa_AP";       // AP SSID
const char* host = "192.168.4.1";   // IP of AP (LoRa board)
const int port = 1234;

//packets that image will be broken into
std::vector<std::vector<uint8_t>> allPackets;

void createPacketsDirect(camera_fb_t *fb){
  //removes anything in the array
  allPackets.clear();
  size_t totalBytes = fb->len;
  size_t count = 0;
  uint8_t startSignature[] = {0xAA, 0xBB, 0xCC, 0xDD};
  uint8_t endSignature[]   = {0xEE, 0xFF, 0xAB, 0xCD};

  while(count<totalBytes){
    size_t chunkSize = std::min((size_t)PACKET_SIZE, totalBytes - count);
    std::vector<uint8_t> packet;
    //adding start sig then the data then end sig 
    packet.insert(packet.end(), std::begin(startSignature), std::end(startSignature));
    packet.insert(packet.end(), fb->buf + count, fb->buf + count + chunkSize);
    packet.insert(packet.end(), std::begin(endSignature), std::end(endSignature));
    //packet.push_back((uint8_t)chunkSize));
    uint8_t checksum = 0;
    for (auto b : packet) checksum ^= b;
    packet.push_back(checksum);
    //add to the vector
    allPackets.push_back(packet);
    count += chunkSize;
  }
  //for debugging
  Serial.printf("created %d packets, and the size is %d", allPackets.size(), totalBytes);
}

void sendPacket(WiFiClient &client, int packetIndex){
  //check the index is correct
  if (packetIndex >= allPackets.size()) return;
  //send over the packet
  client.write(allPackets[packetIndex].data(), allPackets[packetIndex].size());
  //send the expected length for debugging
  Serial.printf("sent packet %d\n", packetIndex);
  //need to add in the loop calling the function an ack ping


}

#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for microseconds to seconds
#define TIME_TO_SLEEP  1800        // Sleep time in seconds (30 minutes)

void setup(){
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid);  // No password
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  // Camera init
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if(err != ESP_OK){
    Serial.printf("Camera init failed: 0x%x\n", err);
    while (true);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    while (true);
  }
  Serial.printf("Captured image: %d bytes\n", fb->len);
  createPacketsDirect(fb);
  Serial.printf("created all packets %d \n",allPackets.size());
  WiFiClient client;
  if (!client.connect(host, port)) {
    Serial.println("Connection to server failed");
    esp_camera_fb_return(fb);
    return;
  }
 
  for(int i = 0; i < allPackets.size(); i++){
    sendPacket(client, i);
    Serial.printf("Packet %d sent, waiting for ACK", i);
    Serial.println();
    unsigned long startTime = millis();
    int maxretiries = 4;
    int retry =0;
    bool ackRecived = false;

    if(retry < maxretiries){
    while (millis() - startTime < 20000){ //20 second watchdog timer
      if(client.available()){
        String response = client.readStringUntil('\n');
        //checks the ACK number so delayed acks dont cause it to fail
        if(response.startsWith("ACK") && response.substring(3).toInt() == i){
          ackRecived = true; 
          Serial.printf("got ACK\n");
          break;
        }else{
          Serial.printf("Unexpected response: %s\n", response.c_str());
          
        }
      }
    }
    // if 20 seconds are up and no response reset
    if(!ackRecived){
      Serial.printf("No Ack Recived for packet %d retrying\n", i);
      i--;
      retry++;
      Serial.printf("Retry %d for packet %d\n", retry, i);
      delay(100);
    }
    }else{
      client.stop();
      esp_camera_fb_return(fb);
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }

    delay(2000);
  }

  client.stop();
  Serial.printf("All Packets have been sent: %d", allPackets.size());
  Serial.println();
  //serial print the full file to check image on cams end

  for (size_t i = 0; i < allPackets.size(); i++) {
    //Serial.printf("\033[1;33mPacket %03d:\033[0m ", (int)i);
    for (uint8_t b : allPackets[i]) {
      Serial.printf("%02X ", b);
    }
  }
  Serial.println();
  client.stop();
  esp_camera_fb_return(fb);
  Serial.println("Going to sleep now for 30 minutes...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void loop(){
  //nothing just wait for reset
}
