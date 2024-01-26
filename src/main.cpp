#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Arduino_JSON.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Replace with your network credentials
const char* wifiSSID = "E308";
const char* wifiPassword = "98806829";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

// GPIO where the DS18B20 sensors are connected to
const int oneWireBus = 4;

// Setup a oneWire instance to communicate with OneWire devices (DS18B20)
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// Address of each sensor
DeviceAddress sensor1 = { 0x28, 0xFF, 0x64, 0x1E, 0x30, 0x74, 0x62, 0xE1 };

// Search for parameters in HTTP POST request
const char* PARAM_INPUT_SSID = "ssid";
const char* PARAM_INPUT_PASS = "pass";
const char* PARAM_INPUT_IP = "ip";
const char* PARAM_INPUT_GATEWAY = "gateway";

// Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";

IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 0, 0);

// Timer variables for Wi-Fi connection
unsigned long previousMillis = 0;
const long interval = 10000;

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

// Initialize WiFi
bool initWiFi() {
  if (ssid == "" || ip == "") {
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  if (!WiFi.config(localIP, localGateway, subnet)) {
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

// Get Sensor Readings and return JSON object
String getSensorReadings() {
  sensors.requestTemperatures();
  readings["sensor1"] = String(sensors.getTempC(sensor1));
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  initSPIFFS();

  // Load values saved in SPIFFS
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  ip = readFile(SPIFFS, ipPath);
  gateway = readFile(SPIFFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  if (initWiFi()) {
    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", "text/html");
    });
    server.serveStatic("/", SPIFFS, "/");

    // Request for the latest sensor readings
    server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
      String json = getSensorReadings();
      request->send(200, "application/json", json);
      json = String();
    });

    // SSE events
    events.onConnect([](AsyncEventSourceClient *client) {
      if (client->lastId()) {
        Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
      }
      client->send("hello!", NULL, millis(), 10000);
    });
    server.addHandler(&events);

    // Handler for the ClearWiFiConfig option
    server.on("/clearWiFiConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        SPIFFS.remove(ssidPath);
        SPIFFS.remove(passPath);
        SPIFFS.remove(ipPath);
        SPIFFS.remove(gatewayPath);

        request->send(200, "text/plain", "SPIFFS cleared. ESP will restart.");

        // Delay before restarting (to allow the response to be sent)
        delay(3000);

        ESP.restart();
    });

    server.begin();
  } else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    WiFi.softAP("ESP-WIFI-MANAGER-USAMA", NULL);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/wifimanager.html", "text/html");
    });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for (int i = 0; i < params; i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          if (p->name() == PARAM_INPUT_SSID) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          if (p->name() == PARAM_INPUT_PASS) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          if (p->name() == PARAM_INPUT_IP) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            writeFile(SPIFFS, ipPath, ip.c_str());
          }
          if (p->name() == PARAM_INPUT_GATEWAY) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            writeFile(SPIFFS, gatewayPath, gateway.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart();
    });

    server.begin();
  }
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    events.send("ping", NULL, millis());
    events.send(getSensorReadings().c_str(), "new_readings", millis());
    lastTime = millis();
  }
}
