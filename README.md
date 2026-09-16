[README.MD.txt](https://github.com/user-attachments/files/32293569/README.MD.txt)
# 📡 IoT-Based Smart Attendance & Entry–Exit Monitoring System

An IoT-enabled smart lab monitoring system that automatically detects student entry and exit, identifies students using RFID, records timestamps, maintains digital attendance records, and detects unusual attendance events.

## 📌 Overview

Traditional attendance systems require manual verification and record keeping. This project provides an automated solution using **ESP32, RFID, dual IR sensors, RTC, OLED display, Wi-Fi, and Google Sheets**.

The system determines whether a student is entering or leaving based on the sequence of two IR sensors. After movement is detected, the student scans their registered RFID card. The ESP32 verifies the RFID and records the student's attendance information.

The attendance data is displayed on a local web dashboard and stored in Google Sheets for digital record keeping.

## 🎯 Features

* 🔹 Automatic entry and exit detection
* 🔹 RFID-based student identification
* 🔹 Direction detection using dual IR sensors
* 🔹 Real-time date and time using DS3231 RTC
* 🔹 Student verification using registered RFID
* 🔹 OLED display for immediate feedback
* 🔹 Buzzer for confirmation and alerts
* 🔹 Local web dashboard
* 🔹 Google Sheets integration
* 🔹 Digital attendance history
* 🔹 Lab occupancy/attendance monitoring
* 🔹 Anomaly detection

## ⚙️ Hardware Requirements

| Component                 | Purpose                                     |
| ------------------------- | ------------------------------------------- |
| ESP32                     | Main microcontroller and Wi-Fi connectivity |
| RC522 RFID Reader         | Student identification                      |
| RFID Cards/Tags           | Unique student identification               |
| Dual IR Sensors           | Entry/exit direction detection              |
| DS3231 RTC                | Accurate date and time                      |
| OLED Display              | Immediate system feedback                   |
| Buzzer                    | Confirmation and alert indication           |
| Wi-Fi                     | Internet connectivity                       |
| Jumper Wires & Breadboard | Circuit connections                         |

## 💻 Software / Technologies

* **ESP32**
* **Arduino IDE**
* **Embedded C/C++**
* **RFID / RC522**
* **IR Sensors**
* **DS3231 RTC**
* **OLED Display**
* **Wi-Fi**
* **Web Dashboard**
* **Google Sheets**

## 🔄 System Workflow

```text
        Person Movement
              ↓
      Dual IR Sensors
              ↓
      Direction Detection
       ↙              ↘
   IR1 → IR2        IR2 → IR1
     ENTRY             EXIT
       ↓                ↓
       └──────┬─────────┘
              ↓
         RFID Scanning
              ↓
       RFID Verification
              ↓
       Student Identification
              ↓
        RTC Time Recording
              ↓
       Attendance Update
         ↙           ↘
   OLED Display   Google Sheets
                      ↓
               Web Dashboard
                      ↓
              Anomaly Analysis
```

## 🚪 Entry Process

```text
Walk through IR1 → IR2
          ↓
   Entry Detected
          ↓
      Scan RFID
          ↓
   Verify Student
          ↓
  Record Entry Time
          ↓
 Update Dashboard &
   Google Sheets
```

## 🚶 Exit Process

```text
Walk through IR2 → IR1
          ↓
    Exit Detected
          ↓
      Scan RFID
          ↓
   Verify Student
          ↓
   Record Exit Time
          ↓
 Update Dashboard &
   Google Sheets
```

## 🧠 How the System Works

### 1. Direction Detection

The two IR sensors determine the direction of movement:

* **IR1 → IR2 = Entry**
* **IR2 → IR1 = Exit**

### 2. RFID Identification

After movement is detected, the student scans their registered RFID card using the RC522 RFID reader.

### 3. Student Verification

The ESP32 compares the scanned RFID with the registered student details.

### 4. Attendance Recording

The system records:

* Student name
* RFID ID
* Date
* Entry/exit time
* Attendance status

### 5. Data Storage & Dashboard

The recorded information is displayed on the local web dashboard and stored in Google Sheets.

### 6. Anomaly Detection

The system can identify unusual events such as:

* Unregistered RFID
* RFID scan without movement
* Movement without RFID verification
* Duplicate entry
* Exit without a recorded entry
* Unusually short visits
* Unusually long visits

## 📊 User Outputs

| Output        | Function                       |
| ------------- | ------------------------------ |
| OLED          | Immediate entry/exit feedback  |
| Buzzer        | Confirmation and alert         |
| Web Dashboard | Live attendance and lab status |
| Google Sheets | Digital attendance history     |

## 🏗️ System Architecture

```text
             ┌─────────────────┐
             │   IR Sensor 1   │
             └────────┬────────┘
                      │
                      ↓
             ┌─────────────────┐
             │      ESP32      │
             │  Main Controller│
             └───────┬─────────┘
                     │
       ┌─────────────┼─────────────┐
       ↓             ↓             ↓
 ┌──────────┐  ┌──────────┐  ┌──────────┐
 │ IR Sensor│  │   RFID   │  │ DS3231   │
 │    2     │  │  RC522   │  │   RTC    │
 └──────────┘  └──────────┘  └──────────┘
                     │
                     ↓
              ┌─────────────┐
              │ OLED +       │
              │ Buzzer       │
              └─────────────┘
                     │
                     ↓
                  Wi-Fi
                     │
          ┌──────────┴──────────┐
          ↓                     ↓
 ┌────────────────┐    ┌────────────────┐
 │ Web Dashboard  │    │ Google Sheets  │
 └────────────────┘    └────────────────┘
```

## 📁 Suggested Repository Structure

```text
Smart-Attendance-Entry-Exit-Monitoring/
│
├── README.md
│
├── src/
│   └── smart_attendance.ino
│
├── circuit/
│   ├── circuit_diagram.png
│   └── pin_configuration.md
│
├── dashboard/
│   └── dashboard_files
│
├── docs/
│   ├── project_report.pdf
│   └── system_architecture.png
│
└── images/
    ├── prototype.jpg
    ├── circuit.jpg
    └── dashboard.jpg
```

## 🚀 Future Enhancements

Possible future improvements include:

* 📱 Mobile application integration
* 🔔 Real-time notifications
* 👤 Admin authentication
* 📈 Attendance analytics and visualization
* ☁️ Cloud database integration
* 📷 Face recognition as an additional verification layer
* 📊 Advanced anomaly analytics
* 🏫 Multi-lab support

## 📚 Project Workflow

```text
Person Movement
      ↓
IR Direction Detection
      ↓
RFID Identification
      ↓
Student Verification
      ↓
Time Recording
      ↓
Attendance Update
      ↓
Dashboard + Google Sheets
      ↓
Anomaly Analysis
```

## 👩‍💻 Project Team

**Smart Idea Lab**

IoT-Based Smart Attendance & Entry–Exit Monitoring System

## 🛠️ Technologies Used

```text
ESP32
RC522 RFID
Dual IR Sensors
DS3231 RTC
OLED Display
Wi-Fi
Google Sheets
Web Dashboard
Arduino IDE
Embedded C/C++
```

## 📄 Project Description

This project demonstrates how IoT technologies can be combined to automate attendance management and monitor student movement in a laboratory environment. The system provides identification, direction detection, time recording, digital attendance storage, dashboard visualization, and anomaly detection in a single workflow.

---

⭐ **If you find this project useful, consider giving the repository a star!**
