#include "CRCChecker.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include "DFRobotDFPlayerMini.h"
#include <driver/ledc.h>
#include <SoftwareSerial.h>
#include <TFT_eSPI.h> // Include the graphics library
#include <SPI.h>
// Function declarations
void taskCode1(void *pvParameters);
void taskCode2(void *pvParameters);
void processMessage(int dataArray[], int size);
String getHTML();
void handleSelection(String item, String shotsPerSecStr, String bonusShotsStr, String hqBonusStr, String playerBonusStr, String gameModeStr, String livesStr, String smartBombsStr, String minutesStr, String secondsStr);
int calculateScore(int score, int overflow);
void updateDisplay();

// Global variable to determine connection method (serial or wifi)
String connectionMethod = "wifi"; // or "wifi"

TFT_eSPI tft = TFT_eSPI();  // Create object "tft"
TaskHandle_t taskCore1Handle = NULL; // Handle for the task running on Core 1
TaskHandle_t taskCore2Handle = NULL; // Handle for the task running on Core 2
volatile bool playShotSound = false; // Flag to indicate if the shot sound should be played

int GreenScore = 0; 
int RedScore = 0;
volatile int countdownValue = 0;
volatile bool timerActive = false;
unsigned long previousMillis = 0;
const long interval = 1000;  // Interval at which to update the countdown (1 second)
const int partSize = 7;
const int arraySize = 13;
const int pwmPin = 17; // Make sure this pin supports PWM on your ESP32 model old pin 8
const int frequency = 28800; // Desired frequency
const int buttonPin = 2; // The button pin number
const int BUSY_PIN = 1;
const byte RXD2 = 41; // Connects to module's RX for sound chip for some reason
const byte TXD2 = 42; // Connects to module's TX

int buttonState = 0;        // current state of the button
int lastButtonState = 0;    // previous state of the button
unsigned long lastDebounceTime = 0; // Last debounce time
const int debounceDelay = 100; // Debounce delay in milliseconds

#define RX_CPU 18 // RX pin 6
#define TX_CPU 8 // TX pin 5 old pins

int networkunit = 51; // Original network unit
int cpu2Energizer = 170; // New network unit to capture

// Other Variables
int shotdelay = 0; //greg wanted 0 because hes a monster
bool debug = true; //debug toggle need to make this an ir code later

byte ByteBLOCK_1[] = {0,85,22,233,254,85,54,201,0}; // I am a bisexual energizer
byte gameFormatArray[13] = {165,0,4,96,0,8,21,0,60,0,255,173,5}; //size of byte array for sending out a signal max q=zars game modes cap out at 13
String history = ""; // Initialize history string
byte gameScoreArray[4] = {0,0,0,0};

// WiFi Credentials
const char* ssid = "Green Base Mesh";
const char* password = "Welcome20!";
// Server details
const char* serverIP = "192.168.4.30";
const uint16_t serverPort = 5001;

WiFiClient client;


// WebServer Initialization
WebServer server(80);

// DFPlayer Initialization
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);

// HardwareSerial Initialization
// Initialize HardwareSerial instances
HardwareSerial serialCPU(0); // talk to computer
HardwareSerial serialReadIR(2);  // IR READ
HardwareSerial shotSerial(1);
EspSoftwareSerial::UART SoundSerial;

// Flags to track conditions
bool conditionMetForcpu2nu = false; // Flag for capturing and storing values after 170
bool conditionMetForNU = false; // New flag for the 18 condition

const int maxArraySize = 32; // Max array size to hold for any single array
int expectedSize = 0;

// Array to store captured values
int capturedValues[12];
int capturedIndex = 0;
int dataArray[maxArraySize]; // Global declaration of dataArray
int newDataArray[maxArraySize]; // Global declaration to store processed data
int newDataArraySize = 0; // Size of the processed data
// Arrays to store the received part and the new array
byte gameFormatPart1[partSize];
bool gameFormatPart1Received = false;
// CRC Checker Initialization
CRCChecker crcChecker; // Custom CRC Class By Tobus

byte calculateByteFour(int shotsPerSec, int bonusShots, int hqBonus, int playerBonus) {
    shotsPerSec = max(0, min(shotsPerSec, 5)); // Ensures range is 0 (for 1 shot/sec) to 5 (for 6 shots/sec)
    bonusShots = max(0, min(bonusShots, 5)); // Ensures range is 0 (for 1 bonus shot/sec) to 5 (for 6 bonus shots/sec)

    byte result = 0;
    result |= (shotsPerSec << 0);
    result |= (bonusShots << 3);
    result |= (hqBonus << 6);
    result |= (playerBonus << 7);

    return result;
}

// Function to print the arrays
void printGameFormatPart1() {
    if (gameFormatPart1Received) {
        Serial.print("Game Format Part 1: ");
        for (int i = 0; i < partSize; i++) {
            Serial.print(gameFormatPart1[i]);
            if (i < partSize - 1) {
                Serial.print(",");
            }
        }
        Serial.println();
    }
}

void printGameFormatArray() {
    Serial.print("Game Format Array: ");
    for (int i = 0; i < arraySize; i++) {
        Serial.print(gameFormatArray[i]);
        if (i < arraySize - 1) {
            Serial.print(",");
        }
    }
    Serial.println();
}
void printGameScoreArray() {
    Serial.print("Game Score Array: ");
    for (int i = 0; i < 4; i++) {
        Serial.print(gameScoreArray[i]);
        if (i < 4 - 1) {
            Serial.print(",");
        }
    }
    Serial.println();
}

void setup() {
    tft.init(); // Initialize the display
    tft.setRotation(3); // Set screen rotation
    tft.fillScreen(TFT_BLACK);

  // Display "Laser Empire"
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(1.5); // Reset text size to default for the title
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Laser Empire NU-Q NU:50", tft.width() / 2, 2); // Position at top center
  
    serialReadIR.begin(2400, SERIAL_8N1, 40, -1, true); // read IR
    serialCPU.begin(600, SERIAL_8N1, RX_CPU, TX_CPU, false); // Talk to computer
    SoundSerial.begin(9600, EspSoftwareSerial::SWSERIAL_8N1, RXD2, TXD2, false); //sound serial
    Serial.begin(2400); //debug serial
    shotSerial.begin(2400, SERIAL_8N1, -1, 16); // old TX=18

//prime emitter
    pinMode(LED_BUILTIN, OUTPUT);
    ledcSetup(0, frequency, 8); // Channel 0, frequency, 8-bit resolution
    ledcAttachPin(pwmPin, 0); // Attach pin to channel 0
    delay(100);
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(BUSY_PIN, INPUT_PULLUP);
    ledcWrite(0, 128); // 50% duty cycle
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    ledcWrite(0, 0); // Stop PWM
    delay(100);
    //begin wifi connection and print out dots if not yet connected
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
//wifi has connected
    Serial.println();
    Serial.println("WiFi connected");

// Connect to the Qcontrol server
  if (client.connect(serverIP, serverPort)) {
    Serial.println("Connected to the server");
  } else {
    Serial.println("Connection to the server failed");
  }


    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", getHTML());
    });

    server.on("/select", HTTP_GET, []() {
        String item = server.arg("item");
        String shotsPerSec = server.arg("shotsPerSec");
        String bonusShots = server.arg("bonusShots");
        String hqBonus = server.arg("hqBonus");
        String playerBonus = server.arg("playerBonus");
        String gameModeStr = server.arg("gameMode");
        String livesStr = server.arg("lives");
        String smartBombsStr = server.arg("smartBombs");
        String minutesStr = server.arg("minutes");
        String secondsStr = server.arg("seconds");

        handleSelection(item, shotsPerSec, bonusShots, hqBonus, playerBonus, gameModeStr, livesStr, smartBombsStr, minutesStr, secondsStr);
 
        history += "\n Data Array: [";
        for (int i = 0; i < 13; i++) {
            history += String(gameFormatArray[i]);
            if (i < 12) history += ", ";
        }
        history += "]";

        String response = "<html><body>";
        response += "Selection received: " + item + "<br>";
        response += "<textarea id='history' rows='10' cols='50'>" + history + "</textarea>";
        response += "<br><button onclick='window.history.back()'>Go Back</button></body></html>";
        server.send(200, "text/html", response);
    });

    server.begin();
    Serial.println("HTTP server started");
    Serial.println("IP Address: " + WiFi.localIP().toString());

    xTaskCreatePinnedToCore(
        taskCode1,   /* Task function */
        "TaskCode1", /* Name of task */
        10000,      /* Stack size of task */
        NULL,       /* Parameter of the task */
        1,          /* Priority of the task */
        NULL,       /* Task handle to keep track of created task */
        1);         /* Core where the task should run */

    xTaskCreatePinnedToCore(
        taskCode2,   /* Task function */
        "TaskCode2", /* Name of task */
        10000,      /* Stack size of task */
        NULL,       /* Parameter of the task */
        2,          /* Priority of the task */
        &taskCore2Handle,       /* Task handle to keep track of created task */
        0);         /* Core where the task should run */
        

// Check the connection method and set up accordingly
  if (connectionMethod == "serial") {
    Serial.println("Connection Method: Serial mode selected");
    // Add any additional setup for serial mode if necessary
  } else if (connectionMethod == "wifi") {
    Serial.println("Connection Method: Wi-Fi mode selected");
  }
delay(500);
  Serial.println("DFRobot DFPlayer Mini Demo");
    Serial.println("Initializing DFPlayer ... (May take 3~5 seconds)");

    if (!myDFPlayer.begin(SoundSerial, false, true)) {
        Serial.println("Unable to begin:");
        Serial.println("1.Please recheck the connection!");
        Serial.println("2.Please insert the SD card!");
        while(true); // Halt the program
            delay(0); // Code to compatible with ESP8266 watch dog.
    }
  myDFPlayer.setTimeOut(600); //Set serial communictaion time out 500ms

    Serial.println("DFPlayer Mini online.");
    myDFPlayer.volume(25);
    myDFPlayer.play(3);
    void updateDisplay();
}

void taskCode2(void *pvParameters) {
    for (;;) { // Infinite loop
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait indefinitely for a notification
         //  Serial.println("Notification received in taskCode2");
        if (playShotSound) {
            ledcWrite(0, 10); // Start PWM
                   vTaskDelay(pdMS_TO_TICKS(200)); // Delay without blocking the task
                    shotSerial.write(ByteBLOCK_1, sizeof(ByteBLOCK_1));
                   vTaskDelay(pdMS_TO_TICKS(100)); // Delay without blocking the task
                  
                    //myDFPlayer.play(4); // Play shot
                    ledcWrite(0, 0); // Stop PWM
        
            playShotSound = false; // Reset the flag
        }
        updateDisplay();
     // Yield to allow other tasks to run
      //  taskYIELD();
    }
}

//
void taskCode1(void *pvParameters) {
    for (;;) { // Infinite loop
        static int currentIndex = 0;
        static bool receiving = false;

        unsigned long currentMillis = millis();
        server.handleClient();


         
        // Read the current state of the button
        int reading = digitalRead(buttonPin);
      
        // Check if the button state has changed
        if (reading != lastButtonState) {
            lastDebounceTime = currentMillis; // Reset the debounce timer
        }

        // If the button state has been stable for the debounce period
        if ((currentMillis - lastDebounceTime) > debounceDelay) {
            // If the button state has changed
            if (reading != buttonState) {
                buttonState = reading;

                // If the button is pressed (assuming LOW means pressed)
                if (buttonState == LOW) {
                   playShotSound = true; // Set the flag to play the shot sound
                        Serial.println("Playing shot sound");
                    myDFPlayer.volume(15);
                    myDFPlayer.play(4); // Play shot sound
                    // Perform the button press actions
                    xTaskNotifyGive(taskCore2Handle); // Notify taskCode2
                  
                            // Yield to allow other tasks to run
       // taskYIELD();
                }
            }
        }
        lastButtonState = reading; 

int ch;
      
     if (connectionMethod == "serial") {
       
     
        if (serialCPU.available() > 0) {
            ch = serialCPU.read();

            if (ch == networkunit && !newDataArraySize > 0) {
                conditionMetForNU = true; // Set the flag to true for the 18 condition
                serialCPU.write(ch);
                serialCPU.write(160);
            }
            else if (ch == networkunit && newDataArraySize > 0) { // this is to send the download to the computer
                serialCPU.write(ch);
                for (int i = 0; i < newDataArraySize; i++) {
                    serialCPU.write(newDataArray[i]);
                }
                playShotSound = true; // Reset the flag
                myDFPlayer.volume(10);
                myDFPlayer.play(2); // play shot
                ledcWrite(0, 10);

                vTaskDelay(pdMS_TO_TICKS(250)); // Delay without blocking the task
                shotSerial.write(gameFormatArray, 13);
                vTaskDelay(pdMS_TO_TICKS(200)); // Delay without blocking the task
                shotSerial.write(gameFormatArray, 13);
                vTaskDelay(pdMS_TO_TICKS(200)); // Delay without blocking the task
                shotSerial.write(gameFormatArray, 13);  
                vTaskDelay(pdMS_TO_TICKS(200)); // Delay without blocking the task

                ledcWrite(0, 0); // Stop PWM


              
             
                playShotSound = false; // Reset the flag
                newDataArraySize = 0; // Reset after sending
            } else {
                serialCPU.write(ch);
            }

            if (ch == 170 && !receiving) {
                receiving = true;
                currentIndex = 0;

                while (serialCPU.available() == 0);
                int nextByte = serialCPU.read();
                serialCPU.write(nextByte); // force second byte because of while timing issue see about fix in v2 when we use both processors

                if (nextByte == 3 || nextByte == 2 || nextByte == 12 || nextByte == 13 || nextByte == 4 || nextByte == 5) {
                    gameFormatPart1[currentIndex++] = (byte)ch;
                    gameFormatPart1[currentIndex++] = (byte)nextByte;
                } else {
                    receiving = false;
                }
            }
            else if (receiving) {
                if (currentIndex < partSize) {
                    gameFormatPart1[currentIndex++] = (byte)ch;
                }
                if (currentIndex == partSize) {
                    receiving = false;
                    gameFormatPart1Received = true;

                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 3 && gameFormatPart1[3] > 0) {
                        gameFormatArray[5] = gameFormatPart1[2];

                        if (gameFormatPart1[3] > 0){
                            gameFormatArray[8] = gameFormatPart1[3];
                            gameFormatArray[8]--;
                            printGameFormatArray();
                            countdownValue = 60;
                            timerActive = true;
                        }      
                    }
                    if (gameFormatPart1[0] == 170 && (gameFormatPart1[1] == 3 || gameFormatPart1[1] == 2) && gameFormatPart1[3] == 0) {
                        countdownValue = 0;
                        gameFormatArray[8] = 0;
                        gameFormatArray[9] = 0;
                        timerActive = true;
                     
                    }

                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 12) {
                        gameFormatArray[6] = gameFormatPart1[3];
                        gameFormatArray[2] = gameFormatPart1[2];
                        gameFormatArray[7] = gameFormatPart1[4];
                        printGameFormatArray();
                    }
                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 13) {
                        gameFormatArray[3] = gameFormatPart1[3]; // Game Options 2
                        gameFormatArray[3] = gameFormatPart1[3]; //
                        printGameFormatArray();
                    }
                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 4) {
                        gameScoreArray[0] = gameFormatPart1[3]; // score
                        gameScoreArray[1] = gameFormatPart1[4]; // 
                        printGameScoreArray();
                    }
                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 5) {
                        gameScoreArray[2] = gameFormatPart1[3]; // score
                        gameScoreArray[3] = gameFormatPart1[4]; // 
                        printGameScoreArray();
                    }

                    RedScore = calculateScore(gameScoreArray[0], gameScoreArray[1]);
                    GreenScore = calculateScore(gameScoreArray[2], gameScoreArray[3]);

                    Serial.print("Red Score: ");
                    Serial.println(RedScore);
                    Serial.print("Green Score: ");
                    Serial.println(GreenScore);

                    String raw_msg_data = "";
                    for (int i = 0; i < 11; i++) {
                        if (gameFormatArray[i] < 16) {
                            raw_msg_data += "0";
                        }
                        raw_msg_data += String(gameFormatArray[i], HEX);
                    }
                    raw_msg_data.toUpperCase();

                    String calculated_crc = crcChecker.getCalculatedCRC(raw_msg_data);
                    String hiCRC_str = calculated_crc.substring(0, 2);
                    String lowCRC_str = calculated_crc.substring(2, 4);

                    unsigned int hiCRC = (unsigned int)strtol(hiCRC_str.c_str(), NULL, 16);
                    unsigned int lowCRC = (unsigned int)strtol(lowCRC_str.c_str(), NULL, 16);
                    gameFormatArray[11] = hiCRC;
                    gameFormatArray[12] = lowCRC;

                   // xTaskNotify(taskCore1Handle, 0, eNoAction); // Notify taskCodeCore1 to run updateDisplay()
                }
            }
        }
     } else if (connectionMethod == "wifi") {
    if (client.connected()) {
      while (client.available()) {
            char c = client.read();
         int ch = c; // Convert character to its numerical value
            if (ch == networkunit && !newDataArraySize > 0) {
                conditionMetForNU = true; // Set the flag to true for the 18 condition
             //   client.print(ch);
             //   client.print((char)160);
            }
            else if (ch == networkunit && newDataArraySize > 0) { // this is to send the download to the computer
                client.print(ch);
                for (int i = 0; i < newDataArraySize; i++) {
                    client.print((char)newDataArray[i]);
                }
                playShotSound = true; // Reset the flag
                myDFPlayer.play(2); // play shot

               ledcWrite(0, 100);

               vTaskDelay(pdMS_TO_TICKS(200)); // Delay without blocking the task
           
                shotSerial.write(gameFormatArray, 13);
           
                vTaskDelay(pdMS_TO_TICKS(125)); // Delay without blocking the task
           
                shotSerial.write(gameFormatArray, 13);
                  
                vTaskDelay(pdMS_TO_TICKS(200)); // Delay without blocking the task
                ledcWrite(0, 0); // Stop PWM


              
             
                playShotSound = false; // Reset the flag
                newDataArraySize = 0; // Reset after sending
            } else {
               // client.print(ch);
            }

            if (ch == 170 && !receiving) {
                receiving = true;
                currentIndex = 0;

                while (client.available() == 0);
                int nextByte = client.read();
                serialCPU.write(nextByte); // force second byte because of while timing issue see about fix in v2 when we use both processors

                if (nextByte == 3 || nextByte == 2 || nextByte == 12 || nextByte == 13 || nextByte == 4 || nextByte == 5) {
                    gameFormatPart1[currentIndex++] = (byte)ch;
                    gameFormatPart1[currentIndex++] = (byte)nextByte;
                } else {
                    receiving = false;
                }
            }
            else if (receiving) {
                if (currentIndex < partSize) {
                    gameFormatPart1[currentIndex++] = (byte)ch;
                }
                if (currentIndex == partSize) {
                    receiving = false;
                    gameFormatPart1Received = true;

                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 3 && gameFormatPart1[3] > 0) {
                        gameFormatArray[5] = gameFormatPart1[2];

                        if (gameFormatPart1[3] > 0){
                            gameFormatArray[8] = gameFormatPart1[3];
                            gameFormatArray[8]--;
                            printGameFormatArray();
                            countdownValue = 60;
                            timerActive = true;
                        }      
                    }
                    if (gameFormatPart1[0] == 170 && (gameFormatPart1[1] == 3 || gameFormatPart1[1] == 2) && gameFormatPart1[3] == 0) {
                        countdownValue = 0;
                        gameFormatArray[8] = 0;
                        gameFormatArray[9] = 0;
                        timerActive = true;
                     
                    }

                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 12) {
                        gameFormatArray[6] = gameFormatPart1[3];
                        gameFormatArray[2] = gameFormatPart1[2];
                        gameFormatArray[7] = gameFormatPart1[4];
                        printGameFormatArray();
                    }
                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 13) {
                        gameFormatArray[3] = gameFormatPart1[3]; // Game Options 2
                        gameFormatArray[3] = gameFormatPart1[3]; //
                        printGameFormatArray();
                    }
                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 4) {
                        gameScoreArray[0] = gameFormatPart1[3]; // score
                        gameScoreArray[1] = gameFormatPart1[4]; // 
                        printGameScoreArray();
                    }
                    if (gameFormatPart1[0] == 170 && gameFormatPart1[1] == 5) {
                        gameScoreArray[2] = gameFormatPart1[3]; // score
                        gameScoreArray[3] = gameFormatPart1[4]; // 
                        printGameScoreArray();
                    }

                    RedScore = calculateScore(gameScoreArray[0], gameScoreArray[1]);
                    GreenScore = calculateScore(gameScoreArray[2], gameScoreArray[3]);

                    Serial.print("Red Score: ");
                    Serial.println(RedScore);
                    Serial.print("Green Score: ");
                    Serial.println(GreenScore);

                    String raw_msg_data = "";
                    for (int i = 0; i < 11; i++) {
                        if (gameFormatArray[i] < 16) {
                            raw_msg_data += "0";
                        }
                        raw_msg_data += String(gameFormatArray[i], HEX);
                    }
                    raw_msg_data.toUpperCase();

                    String calculated_crc = crcChecker.getCalculatedCRC(raw_msg_data);
                    String hiCRC_str = calculated_crc.substring(0, 2);
                    String lowCRC_str = calculated_crc.substring(2, 4);

                    unsigned int hiCRC = (unsigned int)strtol(hiCRC_str.c_str(), NULL, 16);
                    unsigned int lowCRC = (unsigned int)strtol(lowCRC_str.c_str(), NULL, 16);
                    gameFormatArray[11] = hiCRC;
                    gameFormatArray[12] = lowCRC;

                   // xTaskNotify(taskCore1Handle, 0, eNoAction); // Notify taskCodeCore1 to run updateDisplay()
                }
            }
      }
     }
     }
        if (timerActive && currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;

            if (countdownValue > 0 ) {
                countdownValue--;
                timerActive = true;
                gameFormatArray[9] = countdownValue;

                String raw_msg_data = "";
                for (int i = 0; i < 11; i++) {
                    if (gameFormatArray[i] < 16) {
                        raw_msg_data += "0";
                    }
                    raw_msg_data += String(gameFormatArray[i], HEX);
                }
                raw_msg_data.toUpperCase();

                String calculated_crc = crcChecker.getCalculatedCRC(raw_msg_data);
                String hiCRC_str = calculated_crc.substring(0, 2);
                String lowCRC_str = calculated_crc.substring(2, 4);

                unsigned int hiCRC = (unsigned int)strtol(hiCRC_str.c_str(), NULL, 16);
                unsigned int lowCRC = (unsigned int)strtol(lowCRC_str.c_str(), NULL, 16);

                gameFormatArray[11] = hiCRC;
                gameFormatArray[12] = lowCRC;

                Serial.print("Data Array: ");
                for (int i = 0; i < 13; i++) {
                    Serial.print(gameFormatArray[i]);
                    if (i < 12) Serial.print(", ");
                }
                Serial.println();
                xTaskNotify(taskCore2Handle, 0, eNoAction); // Notify taskCodeCore1 to run updateDisplay()
            }
        }

        static bool startMarkerFound = false;

        while (serialReadIR.available() > 0 && playShotSound == false) {
            if (!startMarkerFound) {
                int firstByte = serialReadIR.read();
                Serial.println(firstByte);
                if (firstByte == 90) {
                    startMarkerFound = true;
                    Serial.println("Received Start Marker: 90");
                    dataArray[0] = firstByte;
                    currentIndex = 1;
                }
            } else if (currentIndex < maxArraySize && serialReadIR.available() && playShotSound == false) {
                dataArray[currentIndex] = serialReadIR.read();
                currentIndex++;

                if (currentIndex == 9) {
                    int shotbynum = dataArray[8];
                    expectedSize = 9 + 2 * shotbynum + 2;
                }

                if (currentIndex == expectedSize) {
                    processMessage(dataArray, currentIndex);
                    currentIndex = 0;
                    startMarkerFound = false;
                    memset(dataArray, 0, sizeof(dataArray));
                }
                    if (playShotSound == true) {
                break; // Break out of the loop if playShotSound is set to true
            }
            }
            // vTaskDelay(1); // Yield control to the scheduler
        }
    }
   //  vTaskDelay(1); // Yield control to the scheduler
}

void processMessage(int dataArray[], int size) {
    int startIndex = 0;
    int startMarkerValue = 0;

    while (startIndex < size) {
        bool startMarkerFound = false;
        for (int i = startIndex; i < size; i++) {
            if (dataArray[i] == 90) {
                startIndex = i;
                startMarkerValue = dataArray[i];
                startMarkerFound = true;
                break;
            }
        }

        if (!startMarkerFound) {
            if (startIndex == 0) {
                Serial.println("No '90' found in the message.");
                Serial.print("Data received: ");
                for (int i = 0; i < size; i++) {
                    Serial.print(dataArray[i]);
                    Serial.print(" ");
                }
                Serial.println();
            }
            return;
        }

        Serial.print("Processing Message starting at index ");
        Serial.println(startIndex);

        if (startMarkerValue == 90) {
            Serial.println("DataArray values when '90' is received:");
            for (int i = 0; i < size; i++) {
                Serial.print(dataArray[i]);
                Serial.print(" ");
            }
            Serial.println();

            Serial.println("This is a game Download.");
            int shotbynum = dataArray[startIndex + 8];
            expectedSize = 9 + 2 * shotbynum + 2;

            if (startIndex + expectedSize - 1 < size) {
                int GunNum = dataArray[startIndex + 1];
                int NumOfDL = dataArray[startIndex + 2];
                int Mins = dataArray[startIndex + 3];
                int Secs = dataArray[startIndex + 4];
                int ShotsFired = dataArray[startIndex + 5];
                int ShotsFiredM = dataArray[startIndex + 6];
                int NumEnerg = dataArray[startIndex + 7];
                int crclow = dataArray[startIndex + 9 + 2 * shotbynum];
                int crchigh = dataArray[startIndex + 10 + 2 * shotbynum];

                newDataArray[0] = startMarkerValue;
                newDataArray[1] = GunNum;
                newDataArray[2] = NumOfDL;
                newDataArray[3] = Mins;
                newDataArray[4] = Secs;
                newDataArray[5] = ShotsFired;
                newDataArray[6] = ShotsFiredM;
                newDataArray[7] = NumEnerg;
                newDataArray[8] = shotbynum;

                for (int i = 0; i < shotbynum; i++) {
                    newDataArray[9 + 2 * i] = dataArray[startIndex + 9 + 2 * i];
                    newDataArray[10 + 2 * i] = dataArray[startIndex + 10 + 2 * i];
                }

                newDataArray[9 + 2 * shotbynum] = crclow;
                newDataArray[10 + 2 * shotbynum] = crchigh;

                newDataArraySize = 11 + 2 * shotbynum;

                if (debug) {
                    Serial.print("Received Start Marker: ");
                    Serial.println(startMarkerValue);
                    Serial.print("Gun Number: ");
                    Serial.println(GunNum);
                    Serial.print("Number of downloads: ");
                    Serial.println(NumOfDL);
                    Serial.print("Mins: ");
                    Serial.println(Mins);
                    Serial.print("Secs: ");
                    Serial.println(Secs);
                    Serial.print("ShotsFired: ");
                    Serial.println(ShotsFired);
                    Serial.print("ShotsFired Multiplier: ");
                    Serial.println(ShotsFiredM);
                    Serial.print("Number of Energizes: ");
                    Serial.println(NumEnerg);
                    Serial.print("Shot by X number of packs: ");
                    Serial.println(shotbynum);

                    for (int i = 0; i < shotbynum; i++) {
                        Serial.print("shotbypack");
                        Serial.print(i + 1);
                        Serial.print(": ");
                        Serial.println(dataArray[startIndex + 9 + 2 * i]);

                        Serial.print("shottimes");
                        Serial.print(i + 1);
                        Serial.print(": ");
                        Serial.println(dataArray[startIndex + 10 + 2 * i]);
                    }

                    Serial.print("crclow: ");
                    Serial.println(crclow);
                    Serial.print("crchigh: ");
                    Serial.println(crchigh);
                }
            } else {
                Serial.println("Incomplete game download message after start marker.");
            }
        }

        startIndex += expectedSize;
        memset(dataArray, 0, sizeof(int) * size);
    }
}

String getHTML() {
    String html = R"(
    <html>
    <head>
    <style>
      body {
        font-family: Arial, sans-serif;
        background-color: #f4f4f4;
        text-align: center;
        padding: 20px;
        margin: 0;
      }
      h1 {
        color: #333;
        font-size: 24px;
      }
      .container {
        max-width: 400px;
        width: 100%;
        margin: auto;
        background-color: white;
        padding: 20px;
        border-radius: 8px;
        box-sizing: border-box;
      }
      label {
        margin-top: 10px;
        display: block;
        color: #666;
      }
      select, input[type='submit'] {
        width: 100%;
        padding: 8px 10px;
        margin-top: 5px;
        border-radius: 4px;
        border: 1px solid #ddd;
      }
      input[type='submit'] {
        background-color: #4CAF50;
        color: white;
        border: none;
        cursor: pointer;
      }
      input[type='submit']:hover {
        background-color: #45a049;
      }
      @media (max-width: 600px) {
        h1 {
          font-size: 20px;
        }
        .container {
          width: 90%;
          max-width: none;
        }
      }
    </style>
    </head>
    <body>
    <div class="container">
      <h1>Game Option Configuration</h1>
      <form action="/select">
        <label for="item">Choose a type (Byte 0):</label>
        <select id="item" name="item">
          <option value="Energizer">Energizer</option>
          <option value="Gun">Gun</option>
        </select><br>
        <h1>Byte 1 is always 0</h1>
        <label for='timing'>Timing (Byte 2 Bit 0) Not used:</label>
        <select id='timing' name='timing'>
          <option value='0'>Team</option>
          <option value='1'>Individual</option>
        </select><br>
        <label for='teams'>Teams (Byte 2 Bit 1):</label>
        <select id='teams' name='teams'>
          <option value='0'>Team</option>
          <option value='1'>Solo</option>
        </select><br>
        <label for='defShields'>Defense Shields (Byte 2 Bit 2):</label>
        <select id='defShields' name='defShields'>
          <option value='0'>Off</option>
          <option value='1'>On</option>
        </select><br>
        <label for='spies'>Spies (Byte 2 Bit 3):</label>
        <select id='spies' name='spies'>
          <option value='0'>Off</option>
          <option value='1'>On</option>
        </select><br>
        <label for='lethalGen'>Lethal Generator (Byte 2 Bit 4):</label>
        <select id='lethalGen' name='lethalGen'>
          <option value='0'>Off</option>
          <option value='1'>On</option>
        </select><br>
        <label for='reflex'>Reflex (Byte 2 Bit 5):</label>
        <select id='reflex' name='reflex'>
          <option value='0'>On</option>
          <option value='1'>Off</option>
        </select><br>
        <label for='bit6'>Byte 2 Bit 6:</label>
        <select id='bit6' name='bit6'>
          <option value='0'>Option 0</option>
          <option value='1'>Option 1</option>
        </select><br>
        <label for='bit7'>Byte 2 Bit 7:</label>
        <select id='bit7' name='bit7'>
          <option value='0'>Option 0</option>
          <option value='1'>Option 1</option>
        </select><br>
        <label for='shotsPerSec'>Shots per Sec (Byte 3 Bits 1-3):</label>
        <select id='shotsPerSec' name='shotsPerSec'>
            <option value='0'>1 shot/sec</option>
            <option value='1'>2 shots/sec</option>
            <option value='2'>3 shots/sec</option>
            <option value='3'>4 shots/sec</option>
            <option value='4'>5 shots/sec</option>
            <option value='5'>6 shots/sec</option>
        </select><br>
        <label for='bonusShots'>Bonus Shots per Sec (Byte 3 Bits 4-6):</label>
        <select id='bonusShots' name='bonusShots'>
            <option value='0'>1 bonus shot/sec</option>
            <option value='1'>2 bonus shots/sec</option>
            <option value='2'>3 bonus shots/sec</option>
            <option value='3'>4 bonus shots/sec</option>
            <option value='4'>5 bonus shots/sec</option>
            <option value='5'>6 bonus shots/sec</option>
        </select><br>
        <label for='hqBonus'>HQ Bonus (Byte 3 Bit 7):</label>
        <select id='hqBonus' name='hqBonus'>
            <option value='0'>Off</option>
            <option value='1'>On</option>
        </select><br>
        <label for='playerBonus'>Player Bonus (Byte 3 Bit 8):</label>
        <select id='playerBonus' name='playerBonus'>
            <option value='0'>Off</option>
            <option value='1'>On</option>
        </select><br>
        <label for="gameMode">Game Mode (byte 4 bit 1-8):</label>
        <select id="gameMode" name="gameMode">
            <option value="1">Energizer</option>
            <option value="3">Stun</option>
            <option value="4">Battle Field</option>
            <option value="5">Eliminator</option>
            <option value="8">Super Charge</option>
        </select><br>
        <label for='lives'>Lives (Default 21, Max 99):</label>
        <input type='number' id='lives' name='lives' value='21' min='0' max='99'><br>
        <label for='smartBombs'>Smart Bombs (Default 0, Max 99):</label>
        <input type='number' id='smartBombs' name='smartBombs' value='0' min='0' max='99'><br>
        <label for='minutes'>Minutes (Default 60, Max 99):</label>
        <input type='number' id='minutes' name='minutes' value='60' min='0' max='99'><br>
        <label for='seconds'>Seconds (Default 0, Max 99):</label>
        <input type='number' id='seconds' name='seconds' value='0' min='0' max='99'><br>
        <input type='submit' value='Submit'>
      </form>
    </div>
    </body>
    </html>
    )";
    return html;
}

void handleSelection(String item, String shotsPerSecStr, String bonusShotsStr, String hqBonusStr, String playerBonusStr, String gameModeStr, String livesStr, String smartBombsStr, String minutesStr, String secondsStr) {
    gameFormatArray[0] = (item == "Energizer") ? 165 : 85;

    byte gameOptionsByte = 0;
    gameOptionsByte |= (server.arg("bit7").toInt() << 7);        // Bit 7: Custom Option
    gameOptionsByte |= (server.arg("bit6").toInt() << 6);        // Bit 6: Custom Option
    gameOptionsByte |= (server.arg("reflex").toInt() << 5);      // Bit 5: Reflex
    gameOptionsByte |= (server.arg("lethalGen").toInt() << 4);   // Bit 4: Lethal Generator
    gameOptionsByte |= (server.arg("spies").toInt() << 3);       // Bit 3: Spies
    gameOptionsByte |= (server.arg("defShields").toInt() << 2);  // Bit 2: Defense Shields
    gameOptionsByte |= (server.arg("teams").toInt() << 1);       // Bit 1: Teams
    gameOptionsByte |= (server.arg("timing").toInt() << 0);      // Bit 0: Timing

    gameFormatArray[2] = gameOptionsByte;

    int shotsPerSec = shotsPerSecStr.toInt();
    int bonusShots = bonusShotsStr.toInt();
    int hqBonus = hqBonusStr.toInt();
    int playerBonus = playerBonusStr.toInt();

    gameFormatArray[3] = calculateByteFour(shotsPerSec, bonusShots, hqBonus, playerBonus);

    int gameMode = gameModeStr.toInt();
    gameFormatArray[5] = gameMode; // Direct assignment since the value is already in bits

    int lives = livesStr.toInt();
    int smartBombs = smartBombsStr.toInt();
    int minutes = minutesStr.toInt();
    int seconds = secondsStr.toInt();

    gameFormatArray[6] = lives;
    gameFormatArray[7] = smartBombs;
    gameFormatArray[8] = minutes;
    gameFormatArray[9] = seconds;
    gameFormatArray[10] = 255; // Always 255 Placeholder maybe

    String raw_msg_data = "";
    for (int i = 0; i < 11; i++) {
        if (gameFormatArray[i] < 16) {
            raw_msg_data += "0";
        }
        raw_msg_data += String(gameFormatArray[i], HEX);
    }
    raw_msg_data.toUpperCase();

    Serial.println(raw_msg_data); // Print the hex string
    String calculated_crc = crcChecker.getCalculatedCRC(raw_msg_data);
    Serial.println("Calculated CRC: " + calculated_crc);

    String hiCRC_str = calculated_crc.substring(0, 2); // Extracts the first two characters
    String lowCRC_str = calculated_crc.substring(2, 4); // Extracts the last two characters

    unsigned int hiCRC = (unsigned int)strtol(hiCRC_str.c_str(), NULL, 16);
    unsigned int lowCRC = (unsigned int)strtol(lowCRC_str.c_str(), NULL, 16);

    Serial.print("hiCRC (decimal): ");
    Serial.println(hiCRC);
    Serial.print("lowCRC (decimal): ");
    Serial.println(lowCRC);

    gameFormatArray[11] = hiCRC;
    gameFormatArray[12] = lowCRC;

    Serial.print("Data Array: ");
    for (int i = 0; i < 13; i++) {
        Serial.print(gameFormatArray[i]);
        if (i < 12) Serial.print(", ");
    }
    Serial.println();
}

int calculateScore(int score, int overflow) {
    int totalScore = score;

    if (overflow > 0 && overflow <= 128) {
        totalScore += overflow * 256;
    } else if (overflow >= 129 && overflow <= 255) {
        totalScore -= (256 - overflow) * 256;
    }

    return totalScore;
}

void loop() {
    // Main loop remains empty because the task running on the second core handles the logic
}

void updateDisplay() {


  char redScoreStr[4];  // Buffer to hold the formatted RedScore string
  char greenScoreStr[4]; // Buffer to hold the formatted GreenScore string
  char countdownValueStr[3]; // Buffer to hold the formatted countdownValue string
  char gameFormatStr[3]; // Buffer to hold the formatted gameFormatArray[8] string

  // Format the scores with leading zeroes
  sprintf(redScoreStr, "%03d", RedScore);
  sprintf(greenScoreStr, "%03d", GreenScore);

  // Format countdownValue with leading zero, treating 60 as 00
  if (countdownValue == 60) {
    sprintf(countdownValueStr, "00");
  } else {
    sprintf(countdownValueStr, "%02d", countdownValue);
  }

  // Format gameFormatArray[8] with leading zero if needed
  sprintf(gameFormatStr, "%02d", gameFormatArray[8]);


  // Display RedScore in red
  tft.setTextFont(7);
  tft.setTextSize(1); // Set larger text size for the numbers
  tft.setTextColor(TFT_RED, TFT_BLACK); // Set text color to red
 
  tft.drawString(String(redScoreStr).c_str(), tft.width() / 2 - 70, 40); // Adjust position and y-coordinate

  // Display GreenScore in green
  tft.setTextColor(TFT_GREEN, TFT_BLACK); // Set text color to green
  tft.drawString(String(greenScoreStr).c_str(), tft.width() / 2 + 70, 40); // Adjust position and y-coordinate



  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color to green
  tft.drawString(String(countdownValueStr).c_str(), tft.width() / 2 + 50, 100); // Adjust position and y-coordinate
  tft.drawString(String(":").c_str(), tft.width() / 2 + 5, 100); // Adjust position and y-coordinate
  tft.drawString(String(gameFormatStr).c_str(), tft.width() / 2 - 50, 100); // Adjust position and y-coordinate
  
}
