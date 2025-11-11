// ESP32-S3 - Single Mic to Wit.ai + Send to Python
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "AudioRecorder.h"
#include "config.h"
#include "WitAiProcess.h"

volatile int writeIndex_wit = 0;
volatile bool bufferReady_wit = false;
volatile bool shouldRecord_wit = false;
volatile bool dataReadyToConsume_wit = false;  

WiFiClientSecure* wifiClient = nullptr;

void startRecording_wit();
void testConnection_wit();
void sendBufferToPython_wit();
void sendToWitAi();
void parseWitAiResponse();

bool WIT_loop() {
  // Check for commands from Python or Serial Monitor
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'R' || command == 'r') {
      startRecording_wit();
    } else if (command == 'T' || command == 't') {
      testConnection_wit();
    }
  }
  
  // Recording loop
  if (shouldRecord_wit) {
    unsigned long startTime = micros();
    
    for (int i = 0; i < 100; i++) {
      if (writeIndex_wit >= BUFFER_SIZE_MIC1) {
        bufferReady_wit = true;
        shouldRecord_wit = false;
        writeIndex_wit = 0;
        break;
      }
      
      // Read microphone with DC offset removal and amplification
      ringBuffer1[writeIndex_wit] = (int16_t)((analogRead(micPin1) - 2048) * 16);
      writeIndex_wit++;
      
      // Timing for 16kHz sampling
      while (micros() - startTime < (i + 1) * 62.5) {}
    }
    
    // When buffer is full, send to both Wit.ai AND Python
    if (bufferReady_wit) {
      Serial.println("RECORDING COMPLETE");
      
      // First send to Python for saving
      sendBufferToPython_wit();
      
      // Then send to Wit.ai for recognition
      sendToWitAi();
      
      bufferReady_wit = false;
      dataReadyToConsume_wit=true;
    }
  }
  return dataReadyToConsume_wit;
}


void WIT_acknowledgeData() {
  // Call this after you've used the pitch buffer data
  dataReadyToConsume_wit = false;
}


void startRecording_wit() {
  writeIndex_wit = 0;
  bufferReady_wit = false;
  shouldRecord_wit = true;
  Serial.println("RECORDING STARTED - Filling 3 second buffer...");
}

void testConnection_wit() {
  Serial.println("\n[Test] Checking Wit.ai connection...");
  
  WiFiClientSecure* testClient = new WiFiClientSecure();
  testClient->setInsecure();
  
  if (testClient->connect("api.wit.ai", 443)) {
    Serial.println("[Test] ✓ Connection successful!");
    testClient->stop();
  } else {
    Serial.println("[Test] ✗ Connection failed!");
  }
  
  delete testClient;
}

void sendBufferToPython_wit() {
  Serial.println("\n[Python] Sending audio data to Python...");
  
  // Send binary header
  Serial.write(0xFF);
  Serial.write(0xAA);
  Serial.flush();
  
  // Send audio buffer
  Serial.write((uint8_t*)ringBuffer1, BUFFER_SIZE_MIC1 * 2);
  
  Serial.println("BUFFER_SENT");
  Serial.println("[Python] Audio data sent to Python for saving\n");
}

void sendToWitAi() {
  Serial.println("[Wit.ai] Connecting to api.wit.ai...");
  
  // Create secure client
  wifiClient = new WiFiClientSecure();
  wifiClient->setInsecure();
  
  if (!wifiClient->connect("api.wit.ai", 443)) {
    Serial.println("[Wit.ai] ✗ Connection failed!");
    delete wifiClient;
    wifiClient = nullptr;
    return;
  }
  
  Serial.println("[Wit.ai] ✓ Connected! Uploading audio...");
  
  // Send HTTP headers
  char authHeader[150];
  snprintf(authHeader, 150, "authorization: Bearer %s", WIT_AI_TOKEN);
  
  wifiClient->println("POST /speech?v=20200927 HTTP/1.1");
  wifiClient->println("host: api.wit.ai");
  wifiClient->println(authHeader);
  wifiClient->println("content-type: audio/raw; encoding=signed-integer; bits=16; rate=16000; endian=little");
  wifiClient->println("transfer-encoding: chunked");
  wifiClient->println();
  
  // Send audio data in chunks
  const int CHUNK_SIZE = 1000; // samples per chunk
  int samplesRemaining = BUFFER_SIZE_MIC1;
  int currentIndex = 0;
  int progressStep = BUFFER_SIZE_MIC1 / 5; // 20% increments
  
  while (samplesRemaining > 0) {
    int chunkSamples = min(CHUNK_SIZE, samplesRemaining);
    int chunkBytes = chunkSamples * 2;
    
    // Send chunk size in hex
    wifiClient->printf("%X\r\n", chunkBytes);
    
    // Send chunk data
    wifiClient->write((const uint8_t*)(ringBuffer1 + currentIndex), chunkBytes);
    wifiClient->print("\r\n");
    
    currentIndex += chunkSamples;
    samplesRemaining -= chunkSamples;
    
    // Show progress every 20%
    int totalProcessed = BUFFER_SIZE_MIC1 - samplesRemaining;
    if (totalProcessed % progressStep < CHUNK_SIZE) {
      int progress = (totalProcessed * 100) / BUFFER_SIZE_MIC1;
      Serial.printf("[Wit.ai] Upload progress: %d%%\n", progress);
    }
  }
  
  // Finish chunked encoding
  wifiClient->print("0\r\n\r\n");
  
  Serial.println("[Wit.ai] Upload complete! Waiting for response...");
  
  // Get response
  parseWitAiResponse();
  
  // Cleanup
  wifiClient->stop();
  delete wifiClient;
  wifiClient = nullptr;
}

void parseWitAiResponse() {
  // Read HTTP status and headers
  int status = -1;
  int contentLength = 0;
  
  unsigned long timeout = millis();
  while (wifiClient->connected() && (millis() - timeout < 10000)) {
    if (wifiClient->available()) {
      String line = wifiClient->readStringUntil('\n');
      
      // Blank line = end of headers
      if (line == "\r") {
        break;
      }
      
      if (line.startsWith("HTTP/1.1")) {
        status = line.substring(9, 12).toInt();
      } else if (line.startsWith("Content-Length:")) {
        contentLength = line.substring(16).toInt();
      }
    }
  }
  
  Serial.printf("[Wit.ai] HTTP Status: %d\n", status);
  
  if (status == 200) {
    // Read the entire JSON response
    String jsonResponse = "";
    unsigned long readTimeout = millis();
    
    while (wifiClient->connected() && (millis() - readTimeout < 5000)) {
      if (wifiClient->available()) {
        char c = wifiClient->read();
        jsonResponse += c;
        readTimeout = millis(); // Reset timeout on data
      }
    }
    
    Serial.println("\n========== RAW JSON RESPONSE ==========");
    Serial.println(jsonResponse);
    Serial.println("=======================================\n");
    
    // Parse JSON response
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, jsonResponse);
    
    if (error) {
      Serial.printf("[Wit.ai] ✗ JSON parsing failed: %s\n", error.c_str());
      return;
    }
    
    // Extract and display results
    const char* text = doc["text"];
    
    Serial.println("========== PARSED RESULTS ==========");
    Serial.printf("Text: %s\n", text ? text : "(none)");
    
    // Intents
    if (doc["intents"].size() > 0) {
      Serial.println("\nIntents:");
      for (JsonVariant intent : doc["intents"].as<JsonArray>()) {
        const char* name = intent["name"];
        float confidence = intent["confidence"];
        Serial.printf("  - %s: %.4f confidence\n", name ? name : "(none)", confidence);
      }
    }
    
    // Entities
    JsonObject entities = doc["entities"];
    if (!entities.isNull() && entities.size() > 0) {
      Serial.println("\nEntities:");
      for (JsonPair kv : entities) {
        Serial.printf("  %s:\n", kv.key().c_str());
        if (kv.value().is<JsonArray>()) {
          for (JsonVariant entity : kv.value().as<JsonArray>()) {
            const char* value = entity["value"];
            float confidence = entity["confidence"];
            Serial.printf("    - value: %s (%.4f)\n", value ? value : "(none)", confidence);
          }
        }
      }
    }
    
    // Traits
    JsonObject traits = doc["traits"];
    if (!traits.isNull() && traits.size() > 0) {
      Serial.println("\nTraits:");
      for (JsonPair kv : traits) {
        Serial.printf("  %s:\n", kv.key().c_str());
        if (kv.value().is<JsonArray>()) {
          for (JsonVariant trait : kv.value().as<JsonArray>()) {
            const char* value = trait["value"];
            float confidence = trait["confidence"];
            Serial.printf("    - value: %s (%.4f)\n", value ? value : "(none)", confidence);
          }
        }
      }
    }
    
    Serial.println("====================================\n");
    
  } else {
    Serial.printf("[Wit.ai] ✗ Error: HTTP %d\n", status);
    
    // Try to read error response
    if (wifiClient->available()) {
      Serial.println("\nError Response:");
      while (wifiClient->available()) {
        Serial.write(wifiClient->read());
      }
      Serial.println();
    }
  }
}