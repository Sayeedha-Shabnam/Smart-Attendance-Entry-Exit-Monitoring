/*
   ============================================================
   SMART IDEA LAB
   IoT-Based Smart Attendance & Entry-Exit Monitoring System

   FEATURES
   ------------------------------------------------------------
   RFID Student Identification
   Dual IR Direction Detection
   DS3231 RTC Date & Time
   OLED Display
   Wi-Fi
   Live Web Dashboard
   Attendance Duration
   AI-Assisted Anomaly Detection
   Google Sheets Support
   Buzzer Alerts
   ============================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <SPI.h>
#include <MFRC522.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <RTClib.h>


// ============================================================
// WIFI
// ============================================================

const char* ssid = "NAME";
const char* password = "PASSWORD";


// ============================================================
// GOOGLE SHEETS
// ============================================================

#define GOOGLE_SHEETS_ENABLED true

const char* scriptURL =
  "https://script.google.com/macros/s/AKfycbyF1UyfaqvvckreJmWgDDAvwYdO_-yR_U4xe7lyfZqRxa0qCcKxAYzQMqYPhxnX5rg2/exec";


// ============================================================
// WEB SERVER
// ============================================================

WebServer server(80);


// ============================================================
// RFID
// ============================================================

#define SS_PIN       5
#define RST_PIN      27

#define RFID_SCK     18
#define RFID_MISO    19
#define RFID_MOSI    23

MFRC522 rfid(SS_PIN, RST_PIN);


// ============================================================
// OLED
// ============================================================

#define OLED_SDA     21
#define OLED_SCL     22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);


// ============================================================
// DS3231 RTC
// ============================================================

RTC_DS3231 rtc;


// ============================================================
// IR SENSORS
// ============================================================

#define IR1_PIN      32
#define IR2_PIN      33

// Most IR obstacle modules are LOW when object detected.
#define IR_ACTIVE_LOW true


// ============================================================
// BUZZER
// ============================================================

#define BUZZER_PIN   25


// ============================================================
// TIMING SETTINGS
// ============================================================

const unsigned long SENSOR_SEQUENCE_WINDOW = 5000;

const unsigned long RFID_CONFIRM_WINDOW = 10000;

const unsigned long IR_DEBOUNCE_TIME = 300;


// ============================================================
// ATTENDANCE LIMITS
// ============================================================

const unsigned long MIN_REASONABLE_DURATION = 120;

const unsigned long MAX_REASONABLE_DURATION =
  6UL * 60UL * 60UL;


// ============================================================
// STUDENT STRUCTURE
// ============================================================

struct StudentRecord {

  String uid;
  String name;

  bool inside;

  unsigned long entryEpoch;

  String entryTime;
  String exitTime;
  String date;

  unsigned long durationSeconds;
};


StudentRecord students[] = {

  {
    "22 D7 0D 06",
    "Student 1",
    false,
    0,
    "--:--:--",
    "--:--:--",
    "--/--/----",
    0
  },

  {
    "31 DF A6 55",
    "Student 2",
    false,
    0,
    "--:--:--",
    "--:--:--",
    "--/--/----",
    0
  }

};

const int STUDENT_COUNT =
  sizeof(students) / sizeof(students[0]);


// ============================================================
// CURRENT DASHBOARD DATA
// ============================================================

String lastUID = "No scan";
String lastStudent = "Waiting";

String currentDate = "--/--/----";
String currentTime = "--:--:--";

String entryTimeDisplay = "--:--:--";
String exitTimeDisplay = "--:--:--";

String currentDirection = "WAITING";

String anomalyStatus = "No anomaly detected";

String liveActivity = "System ready";

String entranceStatus = "Waiting";


// ============================================================
// IR STATE MACHINE
// ============================================================

enum SensorState {

  SENSOR_IDLE,
  IR1_FIRST,
  IR2_FIRST
};

SensorState sensorState = SENSOR_IDLE;

unsigned long firstSensorTime = 0;

bool pendingDirection = false;

String pendingDirectionText = "";

unsigned long directionDetectedTime = 0;


// ============================================================
// IR DEBOUNCE
// ============================================================

int lastIR1State = HIGH;
int lastIR2State = HIGH;

unsigned long lastIR1Change = 0;
unsigned long lastIR2Change = 0;


// ============================================================
// UTILITY
// ============================================================

bool sensorDetected(int state) {

  if (IR_ACTIVE_LOW) {
    return state == LOW;
  }

  return state == HIGH;
}


// ============================================================
// BUZZER
// ============================================================

void beepSuccess() {

  digitalWrite(BUZZER_PIN, HIGH);

  delay(120);

  digitalWrite(BUZZER_PIN, LOW);
}


void beepError() {

  for (int i = 0; i < 2; i++) {

    digitalWrite(BUZZER_PIN, HIGH);

    delay(100);

    digitalWrite(BUZZER_PIN, LOW);

    delay(100);
  }
}


// ============================================================
// FIND STUDENT
// ============================================================

int findStudent(String uid) {

  uid.trim();
  uid.toUpperCase();

  for (int i = 0; i < STUDENT_COUNT; i++) {

    String storedUID = students[i].uid;

    storedUID.trim();
    storedUID.toUpperCase();

    if (uid == storedUID) {

      return i;
    }
  }

  return -1;
}


// ============================================================
// FORMAT TIME
// ============================================================

String getTimeString(DateTime now) {

  char buffer[12];

  sprintf(
    buffer,
    "%02d:%02d:%02d",
    now.hour(),
    now.minute(),
    now.second()
  );

  return String(buffer);
}


// ============================================================
// FORMAT DATE
// ============================================================

String getDateString(DateTime now) {

  char buffer[15];

  sprintf(
    buffer,
    "%02d/%02d/%04d",
    now.day(),
    now.month(),
    now.year()
  );

  return String(buffer);
}


// ============================================================
// UPDATE RTC DISPLAY DATA
// ============================================================

void updateClock() {

  DateTime now = rtc.now();

  currentDate = getDateString(now);

  currentTime = getTimeString(now);
}


// ============================================================
// OLED READY SCREEN
// ============================================================

void showReadyScreen() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(10, 5);
  display.println("SMART IDEA LAB");

  display.setCursor(5, 23);
  display.println("RFID + IR MONITOR");

  display.setCursor(5, 40);
  display.println("Scan your card");

  display.setCursor(5, 54);
  display.println("System Ready");

  display.display();
}


// ============================================================
// OLED PERSON DETECTED
// ============================================================

void showPersonDetected(String direction) {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(10, 5);
  display.println("PERSON DETECTED");

  display.setCursor(5, 25);

  if (direction == "ENTRY") {

    display.println("ENTRY DETECTED");

  }
  else {

    display.println("EXIT DETECTED");
  }

  display.setCursor(5, 45);
  display.println("SCAN RFID CARD");

  display.display();
}


// ============================================================
// OLED ATTENDANCE
// ============================================================

void showAttendance(
  String student,
  String direction,
  String time
) {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 2);
  display.println("SMART IDEA LAB");

  display.setCursor(0, 17);
  display.println(student);

  display.setCursor(0, 31);

  if (direction == "ENTRY") {

    display.println("ENTRY RECORDED");

  }
  else {

    display.println("EXIT RECORDED");
  }

  display.setCursor(0, 46);

  display.print("TIME: ");

  display.println(time);

  display.display();
}


// ============================================================
// OLED ANOMALY
// ============================================================

void showAnomaly(String message) {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(5, 5);
  display.println("ATTENTION");

  display.setCursor(5, 25);
  display.println("ANOMALY DETECTED");

  display.setCursor(5, 45);

  if (message.length() > 19) {

    display.println(
      message.substring(0, 19)
    );

  }
  else {

    display.println(message);
  }

  display.display();
}


// ============================================================
// GOOGLE SHEETS URL ENCODING
// ============================================================

String urlEncode(String value) {

  String encoded = "";

  char c;

  char code0;
  char code1;

  for (int i = 0; i < value.length(); i++) {

    c = value.charAt(i);

    if (
      (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') ||
      c == '-' ||
      c == '_' ||
      c == '.' ||
      c == '~'
    ) {

      encoded += c;

    }
    else {

      code1 = (c & 0x0F) + '0';

      if ((c & 0x0F) > 9) {

        code1 =
          (c & 0x0F) - 10 + 'A';
      }

      c =
        (c >> 4) & 0x0F;

      code0 =
        c + '0';

      if (c > 9) {

        code0 =
          c - 10 + 'A';
      }

      encoded += '%';

      encoded += code0;

      encoded += code1;
    }
  }

  return encoded;
}


// ============================================================
// GOOGLE SHEETS
// ============================================================

void sendToGoogleSheets(
  String student,
  String uid,
  String direction,
  String status
) {

#if GOOGLE_SHEETS_ENABLED

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println(
      "Google Sheets skipped - Wi-Fi disconnected"
    );

    return;
  }

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;

  String url =
    String(scriptURL);

  url += "?student=";

  url +=
    urlEncode(student);

  url += "&uid=";

  url +=
    urlEncode(uid);

  url += "&entryExit=";

  url +=
    urlEncode(direction);

  url += "&status=";

  url +=
    urlEncode(status);

  Serial.println(
    "Sending data to Google Sheets..."
  );

  http.begin(
    client,
    url
  );

  http.setFollowRedirects(
    HTTPC_STRICT_FOLLOW_REDIRECTS
  );

  int httpCode =
    http.GET();

  if (httpCode > 0) {

    Serial.print(
      "Google Sheets HTTP Code: "
    );

    Serial.println(
      httpCode
    );

    Serial.println(
      http.getString()
    );

  }
  else {

    Serial.print(
      "Google Sheets Error: "
    );

    Serial.println(
      http.errorToString(httpCode)
    );
  }

  http.end();

#endif
}


// ============================================================
// REGISTER ANOMALY
// ============================================================

void setAnomaly(String message) {

  anomalyStatus =
    message;

  liveActivity =
    "AI Alert: " + message;

  Serial.print(
    "ANOMALY: "
  );

  Serial.println(
    message
  );

  showAnomaly(
    message
  );

  beepError();
}


// ============================================================
// CLEAR ANOMALY
// ============================================================

void clearAnomaly() {

  anomalyStatus =
    "No anomaly detected";
}


// ============================================================
// PROCESS ENTRY
// ============================================================

void processEntry(
  int studentIndex,
  DateTime now
) {

  StudentRecord &student =
    students[studentIndex];


  // ----------------------------------------------------------
  // DUPLICATE ENTRY
  // ----------------------------------------------------------

  if (student.inside) {

    setAnomaly(
      "Duplicate entry"
    );

    sendToGoogleSheets(
      student.name,
      student.uid,
      "Entry",
      "Anomaly - Duplicate Entry"
    );

    return;
  }


  // ----------------------------------------------------------
  // RECORD ENTRY
  // ----------------------------------------------------------

  student.inside =
    true;

  student.entryEpoch =
    now.unixtime();

  student.entryTime =
    getTimeString(now);

  student.exitTime =
    "--:--:--";

  student.date =
    getDateString(now);

  student.durationSeconds =
    0;


  // Dashboard

  lastStudent =
    student.name;

  lastUID =
    student.uid;

  currentDate =
    getDateString(now);

  currentTime =
    getTimeString(now);

  entryTimeDisplay =
    student.entryTime;

  exitTimeDisplay =
    "--:--:--";

  currentDirection =
    "ENTRY";

  entranceStatus =
    "IR1 → IR2";

  clearAnomaly();

  liveActivity =
    student.name +
    " entered the lab";


  showAttendance(
    student.name,
    "ENTRY",
    student.entryTime
  );

  beepSuccess();


  // ----------------------------------------------------------
  // GOOGLE SHEETS
  // ----------------------------------------------------------

  sendToGoogleSheets(
    student.name,
    student.uid,
    "Entry",
    "Present"
  );
}


// ============================================================
// PROCESS EXIT
// ============================================================

void processExit(
  int studentIndex,
  DateTime now
) {

  StudentRecord &student =
    students[studentIndex];


  // ----------------------------------------------------------
  // EXIT WITHOUT ENTRY
  // ----------------------------------------------------------

  if (!student.inside) {

    setAnomaly(
      "Exit without entry"
    );

    sendToGoogleSheets(
      student.name,
      student.uid,
      "Exit",
      "Anomaly - No Entry"
    );

    return;
  }


  // ----------------------------------------------------------
  // CALCULATE DURATION
  // ----------------------------------------------------------

  unsigned long duration =
    now.unixtime() -
    student.entryEpoch;

  student.durationSeconds =
    duration;

  student.exitTime =
    getTimeString(now);


  // ----------------------------------------------------------
  // SHORT STAY ANOMALY
  // ----------------------------------------------------------

  if (
    duration <
    MIN_REASONABLE_DURATION
  ) {

    anomalyStatus =
      "Unusually short visit";

    liveActivity =
      "AI Alert: Short visit";

    Serial.println(
      "AI anomaly: unusually short visit"
    );
  }


  // ----------------------------------------------------------
  // LONG STAY ANOMALY
  // ----------------------------------------------------------

  else if (
    duration >
    MAX_REASONABLE_DURATION
  ) {

    anomalyStatus =
      "Unusually long visit";

    liveActivity =
      "AI Alert: Long visit";

    Serial.println(
      "AI anomaly: unusually long visit"
    );
  }


  else {

    clearAnomaly();

    liveActivity =
      student.name +
      " exited the lab";
  }


  // ----------------------------------------------------------
  // UPDATE STUDENT
  // ----------------------------------------------------------

  student.inside =
    false;


  // ----------------------------------------------------------
  // DASHBOARD
  // ----------------------------------------------------------

  lastStudent =
    student.name;

  lastUID =
    student.uid;

  currentDate =
    getDateString(now);

  currentTime =
    getTimeString(now);

  entryTimeDisplay =
    student.entryTime;

  exitTimeDisplay =
    student.exitTime;

  currentDirection =
    "EXIT";

  entranceStatus =
    "IR2 → IR1";


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  showAttendance(
    student.name,
    "EXIT",
    student.exitTime
  );

  beepSuccess();


  // ----------------------------------------------------------
  // GOOGLE SHEETS
  // ----------------------------------------------------------

  String statusText =
    "Exited";

  if (
    duration <
    MIN_REASONABLE_DURATION
  ) {

    statusText =
      "Anomaly - Short Visit";

  }

  else if (
    duration >
    MAX_REASONABLE_DURATION
  ) {

    statusText =
      "Anomaly - Long Visit";
  }


  sendToGoogleSheets(
    student.name,
    student.uid,
    "Exit",
    statusText
  );
}


// ============================================================
// RFID PROCESSING
// ============================================================

void processRFID() {

  if (
    !rfid.PICC_IsNewCardPresent()
  ) {

    return;
  }

  if (
    !rfid.PICC_ReadCardSerial()
  ) {

    return;
  }


  // ----------------------------------------------------------
  // CREATE UID
  // ----------------------------------------------------------

  String uidText = "";


  for (
    byte i = 0;
    i < rfid.uid.size;
    i++
  ) {

    if (
      rfid.uid.uidByte[i] < 0x10
    ) {

      uidText += "0";
    }

    String part =
      String(
        rfid.uid.uidByte[i],
        HEX
      );

    part.toUpperCase();

    uidText +=
      part;

    if (
      i < rfid.uid.size - 1
    ) {

      uidText += " ";
    }
  }


  Serial.print(
    "Card UID: "
  );

  Serial.println(
    uidText
  );


  // ----------------------------------------------------------
  // FIND STUDENT
  // ----------------------------------------------------------

  int studentIndex =
    findStudent(uidText);


  // ----------------------------------------------------------
  // UNKNOWN CARD
  // ----------------------------------------------------------

  if (
    studentIndex == -1
  ) {

    lastUID =
      uidText;

    lastStudent =
      "Unknown Student";

    currentDirection =
      "UNKNOWN";

    setAnomaly(
      "Unregistered RFID"
    );

    sendToGoogleSheets(
      "Unknown Student",
      uidText,
      "Unknown",
      "Anomaly - Unregistered RFID"
    );


    rfid.PICC_HaltA();

    rfid.PCD_StopCrypto1();

    delay(1500);

    showReadyScreen();

    return;
  }


  DateTime now =
    rtc.now();


  // ----------------------------------------------------------
  // RFID WITHOUT IR DIRECTION
  // ----------------------------------------------------------

  if (
    !pendingDirection ||
    (
      millis() -
      directionDetectedTime >
      RFID_CONFIRM_WINDOW
    )
  ) {

    lastStudent =
      students[studentIndex].name;

    lastUID =
      students[studentIndex].uid;

    currentDirection =
      "RFID ONLY";

    setAnomaly(
      "RFID without IR"
    );

    sendToGoogleSheets(
      students[studentIndex].name,
      students[studentIndex].uid,
      "RFID Only",
      "Anomaly - No IR Direction"
    );


    rfid.PICC_HaltA();

    rfid.PCD_StopCrypto1();

    delay(1500);

    showReadyScreen();

    return;
  }


  // ----------------------------------------------------------
  // PROCESS LOCKED DIRECTION
  // ----------------------------------------------------------

  if (
    pendingDirectionText ==
    "ENTRY"
  ) {

    processEntry(
      studentIndex,
      now
    );

  }

  else if (
    pendingDirectionText ==
    "EXIT"
  ) {

    processExit(
      studentIndex,
      now
    );
  }


  // ----------------------------------------------------------
  // RESET DIRECTION AFTER RFID
  // ----------------------------------------------------------

  pendingDirection =
    false;

  pendingDirectionText =
    "";

  directionDetectedTime =
    0;

  sensorState =
    SENSOR_IDLE;


  rfid.PICC_HaltA();

  rfid.PCD_StopCrypto1();


  delay(1500);

  showReadyScreen();
}


// ============================================================
// IR PROCESSING
// ============================================================

void processIRSensors() {

  int ir1 =
    digitalRead(IR1_PIN);

  int ir2 =
    digitalRead(IR2_PIN);


  // ==========================================================
  // IMPORTANT:
  // If an IR direction has already been detected, DO NOT
  // start another direction sequence.
  //
  // This prevents:
  // EXIT detected -> IR1 triggered again -> ENTRY overwrite
  // ==========================================================

  if (
    pendingDirection
  ) {

    // Wait for RFID confirmation.

    if (
      millis() -
      directionDetectedTime >
      RFID_CONFIRM_WINDOW
    ) {

      pendingDirection =
        false;

      pendingDirectionText =
        "";

      directionDetectedTime =
        0;

      sensorState =
        SENSOR_IDLE;

      setAnomaly(
        "IR passage without RFID"
      );

      sendToGoogleSheets(
        "Unknown",
        "No RFID",
        "Unverified",
        "Anomaly - IR without RFID"
      );

      delay(500);

      showReadyScreen();
    }

    // VERY IMPORTANT:
    // Do not process any new IR sequence while waiting
    // for RFID.

    return;
  }


  // ==========================================================
  // SENSOR 1 FIRST
  // ==========================================================

  if (
    sensorState ==
    SENSOR_IDLE
  ) {

    // --------------------------------------------------------
    // IR1 FIRST
    // Only accept IR1 if IR2 is currently clear.
    // --------------------------------------------------------

    if (
      sensorDetected(ir1) &&
      !sensorDetected(ir2)
    ) {

      sensorState =
        IR1_FIRST;

      firstSensorTime =
        millis();

      entranceStatus =
        "IR1 detected";

      liveActivity =
        "IR1 detected - waiting IR2";

      Serial.println(
        "IR1 detected first"
      );

    }


    // --------------------------------------------------------
    // IR2 FIRST
    // Only accept IR2 if IR1 is currently clear.
    // --------------------------------------------------------

    else if (
      sensorDetected(ir2) &&
      !sensorDetected(ir1)
    ) {

      sensorState =
        IR2_FIRST;

      firstSensorTime =
        millis();

      entranceStatus =
        "IR2 detected";

      liveActivity =
        "IR2 detected - waiting IR1";

      Serial.println(
        "IR2 detected first"
      );
    }
  }


  // ==========================================================
  // IR1 → IR2 = ENTRY
  // ==========================================================

  if (
    sensorState ==
    IR1_FIRST
  ) {

    if (
      sensorDetected(ir2)
    ) {

      if (
        millis() -
        firstSensorTime <=
        SENSOR_SEQUENCE_WINDOW
      ) {

        pendingDirection =
          true;

        pendingDirectionText =
          "ENTRY";

        directionDetectedTime =
          millis();

        currentDirection =
          "ENTRY";

        entranceStatus =
          "IR1 → IR2";

        liveActivity =
          "Entry direction detected";

        showPersonDetected(
          "ENTRY"
        );

        Serial.println(
          "ENTRY sequence detected: IR1 -> IR2"
        );
      }

      sensorState =
        SENSOR_IDLE;
    }
  }


  // ==========================================================
  // IR2 → IR1 = EXIT
  // ==========================================================

  if (
    sensorState ==
    IR2_FIRST
  ) {

    if (
      sensorDetected(ir1)
    ) {

      if (
        millis() -
        firstSensorTime <=
        SENSOR_SEQUENCE_WINDOW
      ) {

        pendingDirection =
          true;

        pendingDirectionText =
          "EXIT";

        directionDetectedTime =
          millis();

        currentDirection =
          "EXIT";

        entranceStatus =
          "IR2 → IR1";

        liveActivity =
          "Exit direction detected";

        showPersonDetected(
          "EXIT"
        );

        Serial.println(
          "EXIT sequence detected: IR2 -> IR1"
        );
      }

      sensorState =
        SENSOR_IDLE;
    }
  }


  // ==========================================================
  // SEQUENCE TIMEOUT
  // ==========================================================

  if (
    sensorState !=
    SENSOR_IDLE
  ) {

    if (
      millis() -
      firstSensorTime >
      SENSOR_SEQUENCE_WINDOW
    ) {

      sensorState =
        SENSOR_IDLE;

      liveActivity =
        "Incomplete IR sequence";

      Serial.println(
        "Incomplete IR sequence"
      );
    }
  }
}


// ============================================================
// DASHBOARD HTML
// ============================================================

String dashboardHTML() {

  String html = "";

  html += "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";

  html +=
    "<meta name='viewport' content='width=device-width,initial-scale=1'>";

  html +=
    "<meta http-equiv='refresh' content='3'>";

  html +=
    "<title>Smart Idea Lab</title>";


  // ----------------------------------------------------------
  // CSS
  // ----------------------------------------------------------

  html += "<style>";

  html +=
    "*{box-sizing:border-box;}";

  html +=
    "body{margin:0;font-family:Arial,Helvetica,sans-serif;background:#eef2f7;color:#182230;}";

  html +=
    ".header{background:#111827;color:white;padding:28px;text-align:center;}";

  html +=
    ".header h1{margin:0;font-size:32px;}";

  html +=
    ".header p{margin:8px 0 0;color:#cbd5e1;}";

  html +=
    ".container{max-width:1150px;margin:auto;padding:25px;}";

  html +=
    ".status{background:white;padding:18px;border-radius:16px;margin-bottom:20px;display:flex;justify-content:space-between;box-shadow:0 5px 20px rgba(0,0,0,.07);}";

  html +=
    ".online{font-weight:bold;}";

  html +=
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:16px;}";

  html +=
    ".card{background:white;padding:22px;border-radius:18px;box-shadow:0 5px 20px rgba(0,0,0,.07);min-height:120px;}";

  html +=
    ".label{font-size:14px;color:#64748b;margin-bottom:12px;}";

  html +=
    ".value{font-size:24px;font-weight:bold;}";

  html +=
    ".entry{border-left:5px solid #16a34a;}";

  html +=
    ".exit{border-left:5px solid #f97316;}";

  html +=
    ".ai{border-left:5px solid #7c3aed;}";

  html +=
    ".alert{background:#fff7ed;border-left:5px solid #f97316;}";

  html +=
    ".normal{background:#f0fdf4;border-left:5px solid #16a34a;}";

  html +=
    ".section{margin-top:30px;}";

  html +=
    ".section h2{font-size:22px;}";

  html +=
    ".activity{background:white;padding:22px;border-radius:18px;box-shadow:0 5px 20px rgba(0,0,0,.07);}";

  html +=
    ".footer{text-align:center;padding:30px;color:#64748b;}";

  html += "</style>";

  html += "</head>";


  // ----------------------------------------------------------
  // BODY
  // ----------------------------------------------------------

  html += "<body>";

  html += "<div class='header'>";

  html +=
    "<h1>SMART IDEA LAB</h1>";

  html +=
    "<p>IoT-Based Smart Attendance & Entry-Exit Monitoring System</p>";

  html += "</div>";

  html +=
    "<div class='container'>";


  // ----------------------------------------------------------
  // SYSTEM STATUS
  // ----------------------------------------------------------

  html +=
    "<div class='status'>";

  html += "<div>";

  html +=
    "<b>System Status</b><br>";

  html +=
    "<span class='online'>● ESP32 ONLINE</span>";

  html += "</div>";

  html += "<div>";

  html +=
    "<b>Wi-Fi Connected</b><br>";

  html +=
    WiFi.localIP().toString();

  html += "</div>";

  html += "</div>";


  // ----------------------------------------------------------
  // MAIN CARDS
  // ----------------------------------------------------------

  html +=
    "<div class='grid'>";

  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Last Student</div>";

  html +=
    "<div class='value'>";

  html +=
    lastStudent;

  html +=
    "</div>";

  html +=
    "</div>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>RFID UID</div>";

  html +=
    "<div class='value'>";

  html +=
    lastUID;

  html +=
    "</div>";

  html +=
    "</div>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Date</div>";

  html +=
    "<div class='value'>";

  html +=
    currentDate;

  html +=
    "</div>";

  html +=
    "</div>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Current Time</div>";

  html +=
    "<div class='value'>";

  html +=
    currentTime;

  html +=
    "</div>";

  html +=
    "</div>";

  html +=
    "</div>";


  // ----------------------------------------------------------
  // ENTRY EXIT
  // ----------------------------------------------------------

  html +=
    "<div class='grid' style='margin-top:16px;'>";


  html +=
    "<div class='card entry'>";

  html +=
    "<div class='label'>Entry Time</div>";

  html +=
    "<div class='value'>";

  html +=
    entryTimeDisplay;

  html +=
    "</div>";

  html +=
    "</div>";


  html +=
    "<div class='card exit'>";

  html +=
    "<div class='label'>Exit Time</div>";

  html +=
    "<div class='value'>";

  html +=
    exitTimeDisplay;

  html +=
    "</div>";

  html +=
    "</div>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Direction</div>";

  html +=
    "<div class='value'>";

  html +=
    currentDirection;

  html +=
    "</div>";

  html +=
    "</div>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Entrance Sensors</div>";

  html +=
    "<div class='value'>";

  html +=
    "IR1 + IR2";

  html +=
    "</div>";

  html +=
    "</div>";

  html +=
    "</div>";


  // ----------------------------------------------------------
  // AI SECTION
  // ----------------------------------------------------------

  html +=
    "<div class='section'>";

  html +=
    "<h2>🧠 AI-Assisted Attendance Analysis</h2>";

  String aiClass =
    "normal";

  if (
    anomalyStatus !=
    "No anomaly detected"
  ) {

    aiClass =
      "alert";
  }


  html +=
    "<div class='card ai ";

  html +=
    aiClass;

  html +=
    "'>";


  html +=
    "<div class='label'>Anomaly Detection</div>";

  html +=
    "<div class='value'>";

  html +=
    anomalyStatus;

  html +=
    "</div>";

  html +=
    "<p>The system checks RFID, IR direction and attendance duration for unusual events.</p>";

  html +=
    "</div>";

  html +=
    "</div>";


  // ----------------------------------------------------------
  // LIVE ACTIVITY
  // ----------------------------------------------------------

  html +=
    "<div class='section'>";

  html +=
    "<h2>Live Activity</h2>";

  html +=
    "<div class='activity'>";

  html +=
    "<b>";

  html +=
    liveActivity;

  html +=
    "</b>";

  html +=
    "<br><br>";

  html +=
    "Entrance status: ";

  html +=
    entranceStatus;

  html +=
    "</div>";

  html +=
    "</div>";


  // ----------------------------------------------------------
  // SYSTEM INFORMATION
  // ----------------------------------------------------------

  html +=
    "<div class='section'>";

  html +=
    "<h2>System Information</h2>";

  html +=
    "<div class='grid'>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Network</div>";

  html +=
    "<div class='value'>Wi-Fi</div>";

  html +=
    "</div>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Controller</div>";

  html +=
    "<div class='value'>ESP32</div>";

  html +=
    "</div>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Clock</div>";

  html +=
    "<div class='value'>DS3231 RTC</div>";

  html +=
    "</div>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='label'>Cloud Storage</div>";

#if GOOGLE_SHEETS_ENABLED

  html +=
    "<div class='value'>Google Sheets</div>";

#else

  html +=
    "<div class='value'>Ready</div>";

#endif

  html +=
    "</div>";


  html +=
    "</div>";

  html +=
    "</div>";


  // ----------------------------------------------------------
  // FOOTER
  // ----------------------------------------------------------

  html +=
    "<div class='footer'>";

  html +=
    "SMART IDEA LAB • IoT Attendance Monitoring";

  html +=
    "</div>";

  html +=
    "</div>";

  html +=
    "</body>";

  html +=
    "</html>";


  return html;
}


// ============================================================
// WEB SERVER
// ============================================================

void handleRoot() {

  server.send(
    200,
    "text/html",
    dashboardHTML()
  );
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(1000);


  // ----------------------------------------------------------
  // BUZZER
  // ----------------------------------------------------------

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  digitalWrite(
    BUZZER_PIN,
    LOW
  );


  // ----------------------------------------------------------
  // IR
  // ----------------------------------------------------------

  pinMode(
    IR1_PIN,
    INPUT
  );

  pinMode(
    IR2_PIN,
    INPUT
  );


  // ----------------------------------------------------------
  // I2C
  // ----------------------------------------------------------

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      0x3C
    )
  ) {

    Serial.println(
      "OLED not found!"
    );

    while (true) {

      delay(1000);
    }
  }


  showReadyScreen();


  // ----------------------------------------------------------
  // RTC
  // ----------------------------------------------------------

  if (
    !rtc.begin()
  ) {

    Serial.println(
      "DS3231 RTC not found!"
    );

    display.clearDisplay();

    display.setCursor(
      0,
      20
    );

    display.println(
      "RTC NOT FOUND"
    );

    display.display();

    while (true) {

      delay(1000);
    }
  }


  // If RTC lost power, initialize it using compile time.

  if (
    rtc.lostPower()
  ) {

    Serial.println(
      "RTC lost power."
    );

    rtc.adjust(
      DateTime(
        F(__DATE__),
        F(__TIME__)
      )
    );
  }


  updateClock();


  // ----------------------------------------------------------
  // RFID
  // ----------------------------------------------------------

  SPI.begin(
    RFID_SCK,
    RFID_MISO,
    RFID_MOSI,
    SS_PIN
  );

  rfid.PCD_Init();


  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  Serial.println();

  Serial.println(
    "Connecting to Wi-Fi..."
  );

  WiFi.begin(
    ssid,
    password
  );


  while (
    WiFi.status() !=
    WL_CONNECTED
  ) {

    delay(500);

    Serial.print(".");
  }


  Serial.println();

  Serial.println(
    "Wi-Fi Connected!"
  );

  Serial.print(
    "ESP32 IP Address: "
  );

  Serial.println(
    WiFi.localIP()
  );


  // ----------------------------------------------------------
  // WEB SERVER
  // ----------------------------------------------------------

  server.on(
    "/",
    handleRoot
  );

  server.begin();


  Serial.println(
    "Web server started!"
  );

  Serial.println(
    "Open the IP address in your browser."
  );


  // ----------------------------------------------------------
  // OLED IP
  // ----------------------------------------------------------

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    0,
    5
  );

  display.println(
    "SMART IDEA LAB"
  );

  display.setCursor(
    0,
    25
  );

  display.println(
    "Dashboard IP:"
  );

  display.setCursor(
    0,
    42
  );

  display.println(
    WiFi.localIP()
  );

  display.display();

  delay(5000);

  showReadyScreen();
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  server.handleClient();

  updateClock();

  processIRSensors();

  processRFID();

  delay(10);
}