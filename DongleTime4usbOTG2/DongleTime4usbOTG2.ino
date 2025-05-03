#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h> // Include the graphics library

// WiFi Credentials
const char* ssid = "Green Base Mesh";
const char* password = "Welcome20!";

TFT_eSPI tft = TFT_eSPI();  // Create object "tft"
WiFiUDP udp;
unsigned int localPort = 12345;  // Local port to listen on

const unsigned long udpTimeout = 6000;  // 5 seconds timeout for UDP messages
const unsigned long serialTimeout = 1000;  // 1.5 seconds interval for sending '666'

// Array to store the valid numbers and their last receive times
const int validNumbers[] = {18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60};
const int numValidNumbers = sizeof(validNumbers) / sizeof(validNumbers[0]);
unsigned long lastReceiveTimes[numValidNumbers] = {0};  // Array to store the last receive times for each valid number

unsigned long lastSerialTime = 0;  // To store the last time a valid Serial byte was received
unsigned long lastSentTime = 0;    // To store the last time '666' was sent

void setup() {
  // Initialize serial communication at 9600 baud rate
  Serial.begin(9600);
  while (!Serial);

  // Connecting to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  // Start UDP
  udp.begin(localPort);
  delay(1000);

  tft.init(); // Initialize the display
  tft.setRotation(3); // Set screen rotation
  tft.fillScreen(TFT_BLACK);

  // Display "Laser Empire"
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1); // Reset text size to default for the title
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Laser Empire NU:Dongle", tft.width() / 2, 2); // Position at top center
  tft.drawString("IP:" + WiFi.localIP().toString(), tft.width() / 2, 19);
}

void loop() {
  // Listening for incoming UDP packets
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255]; // Buffer to hold incoming packet
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0; // Null-terminate the string
    }

    // Check if the packet starts with '666'
    if (strncmp(incomingPacket, "666", 3) == 0) {
      int receivedNumber = atoi(&incomingPacket[3]); // Convert the remaining part to an integer
      
      // Check if the received number is in the list of valid numbers
      for (int i = 0; i < numValidNumbers; i++) {
        if (receivedNumber == validNumbers[i]) {
          lastReceiveTimes[i] = millis(); // Update the receive time for this number
          break;
        }
      }
    }
  }

  // Check if data is available to read from the serial input
  if (Serial.available() > 0) {
    // Read the incoming byte
    char incomingByte1 = Serial.read(); // Read as a char
    int incomingByte = incomingByte1;   // Cast to int for clarity

    // Check if the byte is in the valid numbers list
    for (int i = 0; i < numValidNumbers; i++) {
      if (incomingByte == validNumbers[i]) {
        // Check if we received a matching byte after receiving '666<number>' in UDP
        if (millis() - lastReceiveTimes[i] <= udpTimeout) {
          int number = 160;
          char charValue = (char)number;

          Serial.println(charValue);
          break;
        }
      }
    }
  }

  // Send '666' via UDP every 1.5 seconds regardless of other conditions
  if (millis() - lastSentTime >= serialTimeout) {
    udp.beginPacket(WiFi.broadcastIP(), 1234);
    udp.write((const uint8_t*)"666", strlen("666"));
    udp.endPacket();
    lastSentTime = millis();
  
tft.fillRect(0, tft.height() / 2, tft.width(), tft.height() / 2, TFT_BLACK);  // Clear the bottom half
  // Display the valid numbers with active receive times on the TFT screen
  int xPos = 12;
  int yPos = 58;

  for (int i = 0; i < numValidNumbers; i++) {
    if (millis() - lastReceiveTimes[i] <= udpTimeout && lastReceiveTimes[i] != 0) {
      String numberStr = String(validNumbers[i]);
      
      // Set background and text color based on the range
      if (validNumbers[i] >= 18 && validNumbers[i] <= 28) {
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
      } else if (validNumbers[i] >= 50 && validNumbers[i] <= 60) {
        tft.setTextColor(TFT_BLACK, TFT_RED);
      }

      // Draw the number with black space after each number
      tft.fillRect(xPos, yPos, tft.textWidth(numberStr), 19, tft.textbgcolor);  // Background color for number
      tft.drawString(numberStr, xPos, yPos);  // Draw the number
      xPos += tft.textWidth(numberStr);  // Move x position to the right for next number
      
      // Draw a small black rectangle (spacer) between numbers
      tft.fillRect(xPos, yPos, 5, 19, TFT_BLACK);  // Black space between numbers
      xPos += 5;  // Add space between numbers
    }
  }

  // Reset the position for the next loop
  if (xPos == 12) {
    // If no numbers were displayed, clear the area
    tft.fillRect(0, yPos, tft.width(), 19, TFT_BLACK);
  }

  }
}
