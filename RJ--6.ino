#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <DHT.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

// ============================================================
// RJ ACTIVITY 6
// ESP32 + DHT11 + FIREBASE + DUAL RELAY
// NO SD CARD - LITTLEFS
// ============================================================

// FIREBASE CONFIGURATION
#define API_KEY "AIzaSyBeAHm65ky0p5mDpSMp6obm3FFcHIF-NCs"
#define USER_EMAIL "rjbeboso2@gmail.com"
#define USER_PASSWORD "rjbeboso123"

#define DATABASE_URL \
"https://rjbeboso-5c489-default-rtdb.europe-west1.firebasedatabase.app"

// WIFI PORTAL
const char* AP_SSID = "RJ_Beboso_Setup";

// HARDWARE PINS
#define DHT_PIN 4
#define DHT_TYPE DHT11

#define RELAY1_PIN 26
#define RELAY2_PIN 27

// ACTIVE LOW RELAY
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// TIMING
const unsigned long SAMPLE_INTERVAL_MS = 10000;
const unsigned long RELAY_CHECK_INTERVAL_MS = 2000;

const long GMT_OFFSET_SEC = 8L * 60L * 60L;
const int DAYLIGHT_OFFSET_SEC = 0;

// OBJECTS
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
Preferences preferences;

WiFiClientSecure ssl_client;

using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

UserAuth user_auth(
    API_KEY,
    USER_EMAIL,
    USER_PASSWORD
);

FirebaseApp app;
RealtimeDatabase Database;

// GLOBAL VARIABLES
bool relay1State = false;
bool relay2State = false;

bool relayStatusNeedsPublish = true;

unsigned long lastSampleMs = 0;
unsigned long lastRelayCheckMs = 0;

// ============================================================
// TIME FUNCTIONS
// ============================================================

bool isValidEpoch(time_t epoch) {
    return epoch >= 1704067200;
}

String dateFromEpoch(time_t epoch) {
    struct tm timeinfo;
    localtime_r(&epoch, &timeinfo);

    char buffer[16];

    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d",
        &timeinfo
    );

    return String(buffer);
}

String timeFromEpoch(time_t epoch) {
    struct tm timeinfo;
    localtime_r(&epoch, &timeinfo);

    char buffer[16];

    strftime(
        buffer,
        sizeof(buffer),
        "%H:%M:%S",
        &timeinfo
    );

    return String(buffer);
}

// ============================================================
// NTP TIME INITIALIZATION
// ============================================================

void initializeTime() {
    Serial.println();
    Serial.println("[TIME] Starting NTP synchronization...");

    configTime(
        GMT_OFFSET_SEC,
        DAYLIGHT_OFFSET_SEC,
        "pool.ntp.org",
        "time.nist.gov",
        "time.google.com"
    );

    Serial.print("[TIME] Waiting for time");

    struct tm timeinfo;
    int retry = 0;

    while (
        !getLocalTime(&timeinfo) &&
        retry < 20
    ) {
        Serial.print(".");
        delay(500);
        retry++;
    }

    Serial.println();

    if (getLocalTime(&timeinfo)) {
        Serial.println("[TIME] Synchronized!");

        Serial.print("[TIME] Date: ");
        Serial.println(dateFromEpoch(time(nullptr)));

        Serial.print("[TIME] Time: ");
        Serial.println(timeFromEpoch(time(nullptr)));
    } else {
        Serial.println("[TIME ERROR] Synchronization failed.");
    }
}

// ============================================================
// LITTLEFS INITIALIZATION
// ============================================================

void initializeLittleFS() {
    Serial.println("[LittleFS] Mounting filesystem...");

    if (!LittleFS.begin(true)) {
        Serial.println("[LittleFS ERROR] Mount failed.");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("[LittleFS] Mounted successfully.");

    Serial.println(
        LittleFS.exists("/index.html")
        ? "[FILE] index.html FOUND"
        : "[FILE ERROR] index.html MISSING"
    );

    Serial.println(
        LittleFS.exists("/style.css")
        ? "[FILE] style.css FOUND"
        : "[FILE ERROR] style.css MISSING"
    );

    Serial.println(
        LittleFS.exists("/script.js")
        ? "[FILE] script.js FOUND"
        : "[FILE ERROR] script.js MISSING"
    );

    Serial.println(
        LittleFS.exists("/wifi.html")
        ? "[FILE] wifi.html FOUND"
        : "[FILE ERROR] wifi.html MISSING"
    );
}

// ============================================================
// WEBSITE FILE HANDLER
// ============================================================

void handleFile(String path, String contentType) {
    if (!LittleFS.exists(path)) {
        server.send(
            404,
            "text/plain",
            "File not found: " + path
        );

        Serial.println("[FILE ERROR] Missing: " + path);
        return;
    }

    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
}

// ============================================================
// WIFI PORTAL PAGE ROUTES
// ============================================================

void setupWiFiPortalRoutes() {

    server.on("/", HTTP_GET, []() {
        handleFile("/wifi.html", "text/html");
    });

    server.on("/wifi.html", HTTP_GET, []() {
        handleFile("/wifi.html", "text/html");
    });

    server.on("/save", HTTP_POST, []() {
        String ssid = server.arg("ssid");
        String password = server.arg("pass");

        ssid.trim();

        if (ssid.length() == 0) {
            server.send(
                400,
                "text/plain",
                "WiFi SSID is required."
            );
            return;
        }

        if (
            password.length() > 0 &&
            password.length() < 8
        ) {
            server.send(
                400,
                "text/plain",
                "WiFi password must be at least 8 characters."
            );
            return;
        }

        if (password.length() > 63) {
            server.send(
                400,
                "text/plain",
                "WiFi password is too long."
            );
            return;
        }

        Serial.println();
        Serial.println("[WiFi] Saving credentials...");

        preferences.begin("rj-wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();

        Serial.println("[WiFi] Credentials saved.");

        server.send(
            200,
            "text/html",
            "<!DOCTYPE html>"
            "<html><head>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>RJ WiFi Saved</title>"
            "</head>"
            "<body style='font-family:Arial;text-align:center;"
            "padding:35px;background:#0f172a;color:#38bdf8;'>"
            "<h2>WiFi Settings Saved!</h2>"
            "<p>RJ ESP32 is restarting.</p>"
            "<p>Please wait while it connects to your home WiFi.</p>"
            "<p>Check Serial Monitor for the new IP address.</p>"
            "</body></html>"
        );

        Serial.println("[WiFi] Restarting ESP32...");
        delay(2000);
        ESP.restart();
    });

    server.onNotFound([]() {
        server.sendHeader(
            "Location",
            "http://192.168.4.1/",
            true
        );

        server.send(302, "text/plain", "");
    });

    server.begin();

    Serial.println("[WEB SERVER] WiFi portal started.");
}

// ============================================================
// START OPEN WIFI PORTAL
// ============================================================

void startWiFiPortal() {
    Serial.println();
    Serial.println("====================================");
    Serial.println("[WiFi] STARTING RJ WIFI MANAGER");
    Serial.println("====================================");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    delay(500);

    bool apStarted = WiFi.softAP(AP_SSID);

    if (!apStarted) {
        Serial.println("[WiFi ERROR] Cannot start access point.");
        delay(3000);
        ESP.restart();
        return;
    }

    Serial.println("[WiFi] Access Point Started.");
    Serial.print("[WiFi] Network Name: ");
    Serial.println(AP_SSID);

    Serial.println("[WiFi] Security: OPEN - NO PASSWORD");

    Serial.print("[WiFi] Setup IP: ");
    Serial.println(WiFi.softAPIP());

    setupWiFiPortalRoutes();

    Serial.println();
    Serial.println("====================================");
    Serial.println(" CONNECT TO RJ_Beboso_Setup");
    Serial.println(" OPEN http://192.168.4.1");
    Serial.println("====================================");

    while (true) {
        server.handleClient();
        delay(2);
    }
}

// ============================================================
// WIFI INITIALIZATION
// ============================================================

void initializeWiFi() {
    Serial.println();
    Serial.println("[WiFi] Checking saved credentials...");

    preferences.begin("rj-wifi", true);

    String savedSSID =
        preferences.getString("ssid", "");

    String savedPassword =
        preferences.getString("password", "");

    preferences.end();

    if (savedSSID.length() == 0) {
        Serial.println("[WiFi] No saved credentials.");
        startWiFiPortal();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    Serial.print("[WiFi] Connecting to: ");
    Serial.println(savedSSID);

    WiFi.begin(
        savedSSID.c_str(),
        savedPassword.c_str()
    );

    unsigned long startTime = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < 20000
    ) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi ERROR] Connection failed.");
        Serial.println("[WiFi] Starting RJ WiFi Manager.");

        startWiFiPortal();
        return;
    }

    Serial.println("[WiFi] Connected successfully!");

    Serial.print("[WiFi] Home WiFi IP: ");
    Serial.println(WiFi.localIP());
}

// ============================================================
// FIREBASE INITIALIZATION
// ============================================================

void initializeFirebase() {
    Serial.println("[Firebase] Initializing...");

    ssl_client.setInsecure();

    initializeApp(
        aClient,
        app,
        getAuth(user_auth)
    );

    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);

    Serial.println("[Firebase] Initialization started.");
}

// ============================================================
// FIREBASE SENSOR UPLOAD
// ============================================================

bool uploadReading(
    float temperature,
    float humidity
) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Firebase] No WiFi connection.");
        return false;
    }

    if (!app.ready()) {
        Serial.println("[Firebase] Authentication not ready.");
        return false;
    }

    time_t now = time(nullptr);

    if (!isValidEpoch(now)) {
        Serial.println("[TIME ERROR] Invalid timestamp.");
        return false;
    }

    String path =
        "/ESP32_Data/" +
        dateFromEpoch(now) +
        "/" +
        timeFromEpoch(now);

    String json =
        "{\"temperature\":" +
        String(temperature, 2) +
        ",\"humidity\":" +
        String(humidity, 2) +
        "}";

    bool success = Database.set<object_t>(
        aClient,
        path,
        object_t(json)
    );

    if (success) {
        Serial.println("[Firebase] Upload successful.");
        Serial.println("[Firebase] Path: " + path);
        return true;
    }

    Serial.printf(
        "[Firebase ERROR] Code: %d | %s\n",
        aClient.lastError().code(),
        aClient.lastError().message().c_str()
    );

    return false;
}

// ============================================================
// DHT11 SENSOR SAMPLING
// ============================================================

void sampleSensor() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (
        isnan(temperature) ||
        isnan(humidity)
    ) {
        Serial.println("[DHT11 ERROR] Check sensor wiring.");
        return;
    }

    Serial.println();
    Serial.println("========== SENSOR READING ==========");

    Serial.printf(
        "Temperature: %.1f C\n",
        temperature
    );

    Serial.printf(
        "Humidity: %.1f %%\n",
        humidity
    );

    if (!isValidEpoch(time(nullptr))) {
        Serial.println("[TIME ERROR] Waiting for valid time.");
        return;
    }

    uploadReading(temperature, humidity);

    Serial.println("====================================");
}

// ============================================================
// RELAY CONTROL
// ============================================================

void applyRelayStates() {
    digitalWrite(
        RELAY1_PIN,
        relay1State ? RELAY_ON : RELAY_OFF
    );

    digitalWrite(
        RELAY2_PIN,
        relay2State ? RELAY_ON : RELAY_OFF
    );
}

void setRelay(int relayNumber, bool state) {

    if (relayNumber == 1) {
        relay1State = state;

        digitalWrite(
            RELAY1_PIN,
            relay1State ? RELAY_ON : RELAY_OFF
        );

        Serial.printf(
            "[RELAY] LED 1: %s\n",
            relay1State ? "ON" : "OFF"
        );

    } else if (relayNumber == 2) {
        relay2State = state;

        digitalWrite(
            RELAY2_PIN,
            relay2State ? RELAY_ON : RELAY_OFF
        );

        Serial.printf(
            "[RELAY] LED 2: %s\n",
            relay2State ? "ON" : "OFF"
        );
    }
}

// ============================================================
// FIREBASE RELAY STATUS
// ============================================================

bool publishRelayStatus() {
    if (!app.ready() || WiFi.status() != WL_CONNECTED) {
        return false;
    }

    bool led1Saved = Database.set<bool>(
        aClient,
        "/ESP32_Status/LED1",
        relay1State
    );

    bool led2Saved = Database.set<bool>(
        aClient,
        "/ESP32_Status/LED2",
        relay2State
    );

    if (led1Saved && led2Saved) {
        Serial.println("[Firebase] Relay status updated.");
        return true;
    }

    Serial.printf(
        "[Firebase STATUS ERROR] %s\n",
        aClient.lastError().message().c_str()
    );

    return false;
}

// ============================================================
// READ FIREBASE RELAY COMMANDS
// ============================================================

void checkFirebaseRelayCommands() {
    if (
        !app.ready() ||
        WiFi.status() != WL_CONNECTED
    ) {
        return;
    }

    bool changed = false;
    bool led1ReadOK = false;
    bool led2ReadOK = false;

    // READ LED 1 COMMAND
    bool led1Command = Database.get<bool>(
        aClient,
        "/ESP32_Control/LED1"
    );

    if (aClient.lastError().code() == 0) {
        led1ReadOK = true;

        if (led1Command != relay1State) {
            setRelay(1, led1Command);
            changed = true;
        }
    } else {
        Serial.printf(
            "[Firebase] LED1 read error: %s\n",
            aClient.lastError().message().c_str()
        );
    }

    // READ LED 2 COMMAND
    bool led2Command = Database.get<bool>(
        aClient,
        "/ESP32_Control/LED2"
    );

    if (aClient.lastError().code() == 0) {
        led2ReadOK = true;

        if (led2Command != relay2State) {
            setRelay(2, led2Command);
            changed = true;
        }
    } else {
        Serial.printf(
            "[Firebase] LED2 read error: %s\n",
            aClient.lastError().message().c_str()
        );
    }

    // Publish initial status after both commands are readable,
    // or whenever a relay state changes.
    if (
        changed ||
        (!relayStatusNeedsPublish && false) ||
        (led1ReadOK && led2ReadOK && relayStatusNeedsPublish)
    ) {
        relayStatusNeedsPublish = true;
    }

    if (relayStatusNeedsPublish) {
        if (publishRelayStatus()) {
            relayStatusNeedsPublish = false;
        }
    }
}

// ============================================================
// MAIN WEBSITE ROUTES
// ============================================================

void setupWebRoutes() {

    server.on("/", HTTP_GET, []() {
        handleFile("/index.html", "text/html");
    });

    server.on("/style.css", HTTP_GET, []() {
        handleFile("/style.css", "text/css");
    });

    server.on("/script.js", HTTP_GET, []() {
        handleFile(
            "/script.js",
            "application/javascript"
        );
    });

    server.on("/wifi.html", HTTP_GET, []() {
        handleFile("/wifi.html", "text/html");
    });

    // LOCAL RELAY STATUS ROUTE
    server.on("/relay-status", HTTP_GET, []() {
        String json = "{";

        json += "\"LED1\":";
        json += relay1State ? "true" : "false";

        json += ",\"LED2\":";
        json += relay2State ? "true" : "false";

        json += "}";

        server.send(
            200,
            "application/json",
            json
        );
    });

    // LOCAL RELAY CONTROL ROUTE
    server.on("/relay", HTTP_GET, []() {

        if (
            !server.hasArg("relay") ||
            !server.hasArg("state")
        ) {
            server.send(
                400,
                "application/json",
                "{\"success\":false,\"message\":\"Missing parameters\"}"
            );
            return;
        }

        int relayNumber =
            server.arg("relay").toInt();

        String state =
            server.arg("state");

        state.toLowerCase();

        if (
            relayNumber != 1 &&
            relayNumber != 2
        ) {
            server.send(
                400,
                "application/json",
                "{\"success\":false,\"message\":\"Invalid relay\"}"
            );
            return;
        }

        if (
            state != "on" &&
            state != "off"
        ) {
            server.send(
                400,
                "application/json",
                "{\"success\":false,\"message\":\"Invalid state\"}"
            );
            return;
        }

        bool turnOn = (state == "on");

        setRelay(relayNumber, turnOn);

        relayStatusNeedsPublish = true;

        String json = "{\"success\":true,";
        json += "\"relay\":";
        json += String(relayNumber);
        json += ",\"state\":\"";
        json += state;
        json += "\"}";

        server.send(
            200,
            "application/json",
            json
        );
    });

    server.onNotFound([]() {
        server.send(
            404,
            "text/plain",
            "404 - Page not found"
        );
    });

    server.begin();

    Serial.println("[WEB SERVER] RJ Dashboard Started.");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("       RJ ACTIVITY 6");
    Serial.println(" ESP32 + DHT11 + FIREBASE + RELAY");
    Serial.println(" NO SD CARD - LITTLEFS");
    Serial.println("====================================");

    // RELAY INITIALIZATION
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);

    relay1State = false;
    relay2State = false;

    applyRelayStates();

    Serial.println("[RELAY] Both LEDs initialized OFF.");

    // DHT11
    dht.begin();
    Serial.println("[DHT11] Initialized.");

    // LITTLEFS
    initializeLittleFS();

    // WIFI
    initializeWiFi();

    // PHILIPPINES TIME
    initializeTime();

    // FIREBASE
    initializeFirebase();

    // WEB SERVER
    setupWebRoutes();

    Serial.println();
    Serial.println("====================================");
    Serial.println("[SYSTEM] RJ ACTIVITY 6 READY");

    Serial.print("[WEBSITE] http://");
    Serial.println(WiFi.localIP());

    Serial.println("[FIREBASE] Monitoring enabled.");
    Serial.println("[RELAY] GPIO 26 and GPIO 27.");
    Serial.println("====================================");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

    // FIREBASE BACKGROUND PROCESSING
    app.loop();

    // HANDLE WEBSITE REQUESTS
    server.handleClient();

    // WIFI RECONNECTION
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastReconnectMs = 0;

        if (millis() - lastReconnectMs >= 10000) {
            lastReconnectMs = millis();

            Serial.println("[WiFi] Reconnecting...");
            WiFi.reconnect();
        }
    }

    // FIREBASE RELAY COMMAND CHECK - EVERY 2 SECONDS
    if (
        WiFi.status() == WL_CONNECTED &&
        app.ready() &&
        millis() - lastRelayCheckMs >= RELAY_CHECK_INTERVAL_MS
    ) {
        lastRelayCheckMs = millis();
        checkFirebaseRelayCommands();
    }

    // DHT11 SENSOR SAMPLE - EVERY 10 SECONDS
    if (millis() - lastSampleMs >= SAMPLE_INTERVAL_MS) {
        lastSampleMs = millis();

        if (WiFi.status() == WL_CONNECTED) {
            sampleSensor();
        } else {
            Serial.println("[WiFi] Offline. Sensor upload skipped.");
        }
    }

    delay(2);
}