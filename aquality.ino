#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#include "DFRobot_ESP_PH.h"
#include "DFRobot_ESP_EC.h"
#include "time.h"
#include <string>
using namespace std;
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

const char* houseNumber = "0002";
const char* ssid = "wifi_name";
const char* password = "wifi_pass";
#define SAMPLE_COUNT 10
#define TIME_UPDATE_INTERVAL 10000 // Time update interval in milliseconds (30 seconds)

#define API_KEY "firebase_api"
#define DATABASE_URL "firebase_db"

FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;
FirebaseJson json;

DFRobot_ESP_PH phSensor;
DFRobot_ESP_EC ec;
#define tempSensorPin 17
#define tbdSensorPin 33
#define tdsSensorPin 34
#define phSensorPin 35
#define kpaSensorPin 2
OneWire oneWire(tempSensorPin);
DallasTemperature tempSensor(&oneWire);
const char* ntpServer = "asia.pool.ntp.org";
const long  gmtOffset_sec = 25200; /*GMT offset +8*/
const int   daylightOffset_sec = 3600; /*1 hour daylight offset*/

const float kpaOffset = 0.483;

LiquidCrystal_I2C lcd(0x27, 20, 4);

float tempSamples[SAMPLE_COUNT];
float tdsSamples[SAMPLE_COUNT];
float phSamples[SAMPLE_COUNT];
float tbdSamples[SAMPLE_COUNT];
float kpaSamples[SAMPLE_COUNT];

float tempMedian;
float tdsMedian;
float phMedian;
float tbdMedian;
float kpaMedian;

struct tm timeinfo;

bool executedThisInterval = false;
bool timeUpdatedWithMillis = false;

String currentDate;
String currentTime;
unsigned long lastUpdateTime = 0;
unsigned long lastFallbackTime = 0;
char dateString[7];
char timeString[5];

// Timer variables
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 10000; // 10 seconds in milliseconds

File myFile;
const int CS = 5;
const char fileName[] = "/Data.txt";
String previousData;
String newData;
String latestDataFirebase;

void setup() {
  setupSerial();
  setupLCD();
  setupSD();
  setupWifi();
  setupDatabase();
  setupSensors();
}

void loop() {
  unsigned long currentMillis = millis();
  // Check if it's time for the next update
  if (currentMillis - lastUpdateTime >= TIME_UPDATE_INTERVAL) {
    float temp = getTemp();
    float tds = getTDS(temp);
    float ph = getPH(temp); 
    float tbd = getTBD();
    float kpa = getKpa();
    displayLCD(temp, tds, ph, tbd, kpa);
    lastUpdateTime = currentMillis;
    if (!getLocalTime(&timeinfo)) {
      lastFallbackTime = currentMillis;
      updateTimeWithMillis();
      Serial.println("Local time synced successfully!");
      printLocalTime();
    } else {
      getLocalTime();
    }
  }
  
  // Execute every 2 minutes (when minutes are divisible by 2)
  if (!executedThisInterval && (timeinfo.tm_min % 5 == 0)) {
    executedThisInterval = true;

    Serial.println("Sampling stage will commence");

    // Print current time for debugging
    printLocalTime();

    // Sync SD Card and Firebase Data
    syncSDFirebase();
    
    // Collect samples and calculate median
    collectSamples();;
    getMedian();
    
    // Display results
    displayResults();

    // Update sd card
    updateSD(fileName);

    if (WiFi.status() == WL_CONNECTED) {
      // Update database
      updateDatabase();

      // Re-update database
      reupdateDatabase();
    } else {
      reconnectWifi();
    }
  }
  
  // Reset the flag at the beginning of the next interval
  if (timeinfo.tm_min % 5 != 0) {
    executedThisInterval = false;
  }

  delay(1000);
}

void setupSerial() {
  Serial.begin(115200);
}

void setupSensors() {
  tempSensor.begin();
  
  phSensor.begin();
  ec.begin();
  displayUpdate("Sensors Initialized");
}

void setupLCD() {
  lcd.begin(); 
  lcd.backlight();
}

void displayUpdate(String text) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(text);
  delay(2000);
  lcd.clear();
}

void getLocalTime() {
  // Attempt to fetch the current time repeatedly until successful
  Serial.println("Obtaining time");
  while (!getLocalTime(&timeinfo)) {
    delay(500); 
    Serial.print(".");
  }

  // Time fetched successfully
  updateTimeWithMillis();
  Serial.println("Local time synced successfully");
  printLocalTime();
}

void updateTimeWithMillis() {
  // Calculate how many minutes have passed since the last fallback update
  unsigned long minutesPassed = (millis() - lastFallbackTime) / 60000;
  // Increment the minutes in timeinfo by the calculated value
  timeinfo.tm_min += minutesPassed;

  // Check if minutes exceed 59 (rollover to the next hour)
  if (timeinfo.tm_min >= 60) {
    timeinfo.tm_min %= 60;
    timeinfo.tm_hour += timeinfo.tm_min / 60; // Add the quotient to hours
  }

  // Check if hour exceeds 23 (rollover to the next day)
  if (timeinfo.tm_hour >= 24) {
    timeinfo.tm_hour %= 24;
    timeinfo.tm_mday += timeinfo.tm_hour / 24; // Add the quotient to days
  }

  lastFallbackTime = millis();
  timeUpdatedWithMillis = true;
}

void printLocalTime() {
  Serial.println(&::timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setupSD() {
  Serial.println("Initializing SD card");
  while (!SD.begin(CS)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Success SD initialization");
  displayUpdate("SD Initialized");
}

void setupWifi() {
  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  displayUpdate("Connected to Wi-Fi");
  Serial.println("Connected to Wi-Fi");
  // Initialize and fetch the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getLocalTime();
}

void reconnectWifi() {
  Serial.printf("Reconnecting to %s", ssid);
  WiFi.begin(ssid, password);
  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to reconnect");
  }
  displayUpdate("Reconnected to Wi-Fi");
  Serial.println("Reconnected to Wi-Fi");
  // Initialize and fetch the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void setupDatabase() {
  // Configure Firebase credentials
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if(Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Signup OK");
    displayUpdate("Connected to RTDB");
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
    displayUpdate("Failed RTDB Connection");
  }

  // Assign token status callback function
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

// Token status callback function definition
void tokenStatusCallback(int status) {
  // Handle token status change if needed
  Serial.print("Token status: ");
  Serial.println(status);
}

void syncSDFirebase() {
  // Read recent data from SD
  readLastDataSD(fileName);
  delay(3000);
  // Read recent data from Firebase RTDB
  readLastDataFirebase();
  delay(3000);

  // Remove any unnecessary characters or spaces
  newData.trim();
  latestDataFirebase.trim();

  // Compare each component individually
  if (newData.equals(latestDataFirebase)) {
      Serial.println("Database is already updated");
      displayUpdate("Database Updated");
  } else {
      uploadDataToFirebase();
      Serial.println("Database replicated");
      displayUpdate("Data Replicated");
  }
}

void readLastDataSD(const char* fileName) {
  // Open file in read mode
  Serial.println("Opening file");
  while (!myFile) {
    myFile = SD.open(fileName);
    Serial.print(".");
    delay(500);
  }

  // Start reading from the beginning of the file
  myFile.seek(0);

  String latestData;
  char c;

  // Read the file character by character
  while (myFile.available()) {
    c = myFile.read();
    if (c == '\n' || !myFile.available()) { // Check if we reached the end of the line or the end of the file
      // If the line is not empty, store it as the latest data
      if (!latestData.isEmpty()) {
        newData = latestData;
        latestData = ""; // Reset latestData for the next line
      }
    } else {
      // Append character to latestData
      latestData += c;
    }
  }

  // Print last line
  Serial.print("Latest SD Data: ");
  Serial.println(newData);

  // Close file
  myFile.close();
}

void readLastDataFirebase() {
  // Path to data node
  String path = "/parent/0002/";

  // Read data at specified path
  if (Firebase.RTDB.getString(&fbdo, path.c_str())) {
    // Get the result as a string
    String dataResult = fbdo.stringData();

    // Parse the JSON string
    DynamicJsonDocument jsonBuffer(1024);
    deserializeJson(jsonBuffer, dataResult);
    
    // Variables to store the latest date and time
    String latestDate;
    String latestTime;
    
    // Iterate through JSON to find the latest date and time
    JsonObject jsonObject = jsonBuffer.as<JsonObject>();
    for (JsonObject::iterator it = jsonObject.begin(); it != jsonObject.end(); ++it) {
      // Get the date
      latestDate = it->key().c_str();
      
      // Get the data associated with the date
      JsonObject dateData = jsonObject[latestDate];
      
      // Iterate through the data to find the latest time
      for (JsonObject::iterator it2 = dateData.begin(); it2 != dateData.end(); ++it2) {
        latestTime = it2->key().c_str();
      }
    }

    // Get the data for the latest date and time
    JsonObject latestData = jsonObject[latestDate][latestTime];
    
    // Extract required values
    String phlevel = latestData["phlevel"].as<String>();
    String pressure = latestData["pressure"].as<String>();
    String tds = latestData["tds"].as<String>();
    String temperature = latestData["temperature"].as<String>();
    String turbidity = latestData["turbidity"].as<String>();

    // Construct CSV string
    latestDataFirebase = latestDate + "," + latestTime + "," + phlevel + "," + pressure + "," + tds + "," + temperature + "," + turbidity;
    Serial.print("Latest Firebase Data (CSV): ");
    Serial.println(latestDataFirebase);
  } else {
    // Failed to read data from Firebase
    Serial.println("Failed to read data from Firebase");
  }
}


void uploadDataToFirebase() {
  // Open file from SD card
  File file = SD.open("/Data.txt");
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 0) {
      // Split the line by commas to extract individual values
      String values[7]; // Assuming there are 7 values in each line
      int index = 0;
      int commaIdx;
      while ((commaIdx = line.indexOf(',')) != -1) {
        values[index++] = line.substring(0, commaIdx);
        line.remove(0, commaIdx + 1);
      }
      values[index] = line; // Last value

      if (index != 6) {
        Serial.println("Invalid data format");
        continue;
      }

      // Extracting values from the array
      String date = values[0];
      String time = values[1];
      String phlevel = values[2];
      String pressure = values[3];
      String tds = values[4];
      String temperature = values[5];
      String turbidity = values[6];

      // Remove any trailing commas from the turbidity value
      turbidity.trim(); // Remove leading and trailing whitespaces
      if (turbidity.endsWith(",")) {
        turbidity.remove(turbidity.length() - 1);
      }

      // Constructing Firebase paths
      String basePath = "/parent/0002/" + date + "/" + time + "/";

      // Updating Firebase with the extracted values
      updateFirebaseValue(basePath, "phlevel", phlevel);
      updateFirebaseValue(basePath, "pressure", pressure);
      updateFirebaseValue(basePath, "tds", tds);
      updateFirebaseValue(basePath, "temperature", temperature);
      updateFirebaseValue(basePath, "turbidity", turbidity);
    }
  }

  file.close();
}

// Helper function to update Firebase value
void updateFirebaseValue(String basePath, String parameter, String value) {
  String path = basePath + parameter;
  Serial.print("Updating " + parameter + " with value: ");
  Serial.println(value);
  if (Firebase.RTDB.setString(&fbdo, path.c_str(), value.c_str())) {
    Serial.println(parameter + " update successful");
  } else {
    Serial.print("Failed to update " + parameter + ", Reason: ");
    Serial.println(fbdo.errorReason());
  }
}

void collectSamples() {
  for (int counter = 0; counter < SAMPLE_COUNT; counter++) {
    float temp = getTemp();
    float tds = getTDS(temp);
    float ph = getPH(temp); 
    float tbd = getTBD();
    float kpa = getKpa();

    Serial.print("Sampling: ");
    Serial.print(counter);
    Serial.println("/10");

    // Store readings in array
    tempSamples[counter] = temp;
    tdsSamples[counter] = tds;
    phSamples[counter] = ph;
    tbdSamples[counter] = tbd;
    kpaSamples[counter] = kpa;

    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Collecting Samples");
    lcd.setCursor(0, 1);
    lcd.print(counter + 1);
    lcd.print("/");
    lcd.print(SAMPLE_COUNT);
  }
}

void getMedian() {
  calculateMedian(tempSamples, SAMPLE_COUNT, tempMedian);
  calculateMedian(tdsSamples, SAMPLE_COUNT, tdsMedian);
  calculateMedian(phSamples, SAMPLE_COUNT, phMedian);
  calculateMedian(tbdSamples, SAMPLE_COUNT, tbdMedian);
  calculateMedian(kpaSamples, SAMPLE_COUNT, kpaMedian);
  displayUpdate("Median Calculated");
}


void calculateMedian(float arr[], int size, float &medianVar) {
  // Sort the array in ascending order
  for (int i = 0; i < size - 1; i++) {
    for (int j = 0; j < size - i - 1; j++) {
      if (arr[j] > arr[j + 1]) {
        // Swap arr[j] and arr[j + 1]
        float temp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = temp;
      }
    }
  }
  // Check if the array length is odd or even
  if (size % 2 != 0) {
    // If odd, update the global variable with the middle element
    medianVar = arr[size / 2];
  } else {
    // If even, update the global variable with the average of the two middle elements
    int mid = size / 2;
    medianVar = (arr[mid - 1] + arr[mid]) / 2.0;
  }
}

float getTemp() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  return temp;
}

float getTDS(float temp) {
  float voltage = analogRead(tdsSensorPin) / 10.0;
  ec.calibration(voltage, temp);
  float ecValue = ec.readEC(voltage, temp);
  float tds = ((ecValue * 1000) / 2);
  return tds;
}

float getPH(float temp) {
  float voltage = analogRead(phSensorPin) / 4096.0 * 3300;
  phSensor.calibration(voltage, temp);
  float ph = phSensor.readPH(voltage, temp);
  return ph;
}

float getTBD() {
  int voltage = analogRead(tbdSensorPin);
  float tds = voltage * (100 / 1000.0);
  float ntu = (-1 * tds) + 70;
  if (ntu < 0) {
    ntu = 0;
  }
  return ntu;
}

float getKpa() {
  float voltage = analogRead(kpaSensorPin) * 5.00 / 1024;
  float pressure = (voltage - kpaOffset) * 250;
  return pressure;
}

void updateSD(const char * path) {
  previousData = "";

  updateDateTime();

  updateNewDataString();
  
  myFile = SD.open(path);
  if (myFile) {
    while (myFile.available()) {
      char c = myFile.read();
      Serial.write(c);
      previousData += c;
    }
    myFile.close();
  } else {
    Serial.println("Error opening file");
  }

  myFile = SD.open(path, FILE_WRITE);
  if (myFile) {
    Serial.printf("Writing to %s\n", path);
    myFile.print(previousData);
    myFile.println(newData);
    myFile.close();
    Serial.println("SD update completed");
    Serial.println(newData);
    displayUpdate("SD Card Updated");
  } else {
    Serial.println("Error updating file");
    displayUpdate("Error SD Update");
  }
}

void updateNewDataString() { 
  newData = currentDate + "," + currentTime + "," + String(phMedian) + "," + String(kpaMedian) + "," + String(tdsMedian) + "," + String(tempMedian) + "," + String(tbdMedian);
}

void updateDatabase() {
  // Update database 
  Serial.println("Sending data to Firebase... ");

  // Get the current date and time
  updateDateTime();

  // Construct the base path
  String basePath = "/parent/" + String(houseNumber) + "/" + currentDate + "/" + currentTime + "/";
    
  // Updating Firebase with the extracted values
  updateFirebaseValue(basePath, "phlevel", String(phMedian));
  updateFirebaseValue(basePath, "pressure", String(kpaMedian));
  updateFirebaseValue(basePath, "tds", String(tdsMedian));
  updateFirebaseValue(basePath, "temperature", String(tempMedian));
  updateFirebaseValue(basePath, "turbidity", String(tbdMedian));

  delay(1000);
}

void updateDateTime() {
  // Get current time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  // Format date
  currentDate = String(timeinfo.tm_mon + 1);
  currentDate = (currentDate.length() == 1 ? "0" : "") + currentDate;
  currentDate += String(timeinfo.tm_mday < 10 ? "0" : "") + String(timeinfo.tm_mday);
  currentDate += String(timeinfo.tm_year + 1900).substring(2);

  // Format time
  currentTime = String(timeinfo.tm_hour < 10 ? "0" : "") + String(timeinfo.tm_hour);
  currentTime += String(timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);
}

void reupdateDatabase() {
  updateDatabase();
}

void displayResults() {
  serialPrint(tempMedian, tdsMedian, phMedian, tbdMedian, kpaMedian);
  displayLCD(tempMedian, tdsMedian, phMedian, tbdMedian, kpaMedian);
}

void displayLCD(float temp, float tds, float ph, float tbd, float kpa) {
  lcd.clear();
  lcd.setCursor(6, 0);
  lcd.print("AQUALITY");
  lcd.setCursor(0, 1);
  lcd.print("Tem:");
  char tempStr[7]; 
  dtostrf(temp, 6, 2, tempStr);
  lcd.print(tempStr);
  lcd.setCursor(11, 1);
  lcd.print("Tur:");
  char tbdStr[7]; 
  dtostrf(tbd, 5, 2, tbdStr); 
  lcd.print(tbdStr); 
  lcd.setCursor(0, 2);
  lcd.print("TDS:");
  char tdsStr[7]; 
  dtostrf(tds, 6, 2, tdsStr); 
  lcd.print(tdsStr); 
  lcd.setCursor(11, 2);
  lcd.print("pH :");
  char phStr[7]; 
  dtostrf(ph, 5, 2, phStr); 
  lcd.print(phStr); 
  lcd.setCursor(2, 3);
  lcd.print("Pressure: ");
  lcd.print(kpa);
}

void serialPrint(float temp, float tds, float ph, float tbd, float kpa) {
  Serial.print("Temp: ");
  Serial.println(temp);
  Serial.print("Tur: ");
  Serial.println(tbd);
  Serial.print("TDS: ");
  Serial.println(tds);
  Serial.print("pH: ");
  Serial.println(ph);
  Serial.print("Pressure: ");
  Serial.println(kpa);
}
