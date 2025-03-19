// Blynk Credentials
#define BLYNK_TEMPLATE_ID "TMPL32kacf8bH"
#define BLYNK_TEMPLATE_NAME "SmartBin"
#define BLYNK_AUTH_TOKEN "rHMTIG7NQGZJ2_OintLpxofV28As_Kmc"

#include <ESP32Servo.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "Sbin";      // WiFi SSID
char pass[] = "smartbin";  // WiFi Password

BlynkTimer timer;
Servo myServo;

// Ultrasonic Sensor Pins
#define TRIG_1 5   // Ultrasonic Sensor 1 (Front) Trig
#define ECHO_1 18  // Ultrasonic Sensor 1 (Front) Echo
#define TRIG_2 12  // Ultrasonic Sensor 2 (Inside Bin) Trig
#define ECHO_2 13  // Ultrasonic Sensor 2 (Inside Bin) Echo

#define SERVO_PIN 33  // Servo Motor Pin

// Smoke Sensor Pin
#define SMOKE_SENSOR_PIN 34  // Connect A0 of MQ-2/MQ-135 sensor here

// Buzzer Pin
#define BUZZER_PIN 15 // Buzzer on GPIO 15

void setup() {
    Serial.begin(115200);
    Serial.println("Connecting to WiFi...");

    // Blynk Connection
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    Serial.println("Connected to Blynk!");

    // Ultrasonic Sensors
    pinMode(TRIG_1, OUTPUT);
    pinMode(ECHO_1, INPUT);
    pinMode(TRIG_2, OUTPUT);
    pinMode(ECHO_2, INPUT);

    // Servo
    myServo.attach(SERVO_PIN, 500, 2400); 
    myServo.write(0); // Start with the bin closed

    // Buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW); // Start with the buzzer OFF

    // Timer to send data every 2 seconds
    timer.setInterval(2000L, sendData);
    timer.setInterval(5000L, sendSmokeData);  // Send smoke data every 5 sec
}

long getDistance(int trigPin, int echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH);
    return duration * 0.034 / 2; // Convert to cm
}

void loop() {
    Blynk.run();  // Run Blynk connection
    timer.run();  // Run timers for sending data

    long distance1 = getDistance(TRIG_1, ECHO_1); // Distance from front sensor
    long distance2 = getDistance(TRIG_2, ECHO_2); // Distance inside the bin

    Serial.print("Distance 1 (Front): ");
    Serial.print(distance1);
    Serial.println(" cm");
    
    Serial.print("Distance 2 (Inside): ");
    Serial.print(distance2);
    Serial.println(" cm");

    if (distance1 > 0 && distance1 <= 20) {  
        // Object detected, open bin
        Serial.println("Object detected! Opening bin...");
        myServo.write(140);
        Blynk.virtualWrite(V4, myServo.read());
        delay(5000);  // Wait 5 sec before checking again
    } else {
        // If no object is detected, close bin
        closeBinSlowly();
        Serial.println("Bin closed.");
        Blynk.virtualWrite(V4, myServo.read());
    }
}

void closeBinSlowly() {
    for (int pos = myServo.read(); pos >= 0; pos--) {  // Start from the current position and move towards 0
        myServo.write(pos);  // Move to the next position
        delay(15);  // Add delay to slow down the movement (adjust the value for speed control)
    }
}

// Function to send bin fill level to Blynk
void sendData() {
    long distance1 = getDistance(TRIG_1, ECHO_1); // Check front sensor
    long distance2 = getDistance(TRIG_2, ECHO_2); // Get storage level distance

        // Convert distance to meters and send to V3
    double distance1InMeters = distance1 / 100.0;  // Convert cm to meters
    Blynk.virtualWrite(V3, distance1InMeters); // Send distance to Blynk Virtual Pin V3

    // Send data **ONLY IF the bin lid is closed** (distance1 > 20 cm)
    if (distance1 > 40) {
        int fillLevel = map(distance2, 24, 2, 0, 100); // Map 24cm (empty) → 2cm (full) to 0-100%
        fillLevel = constrain(fillLevel, 0, 100); // Ensure it's within 0-100%

        Serial.print("Bin Fill Level: ");
        Serial.print(fillLevel);
        Serial.println("%");

        // Send data to Blynk Virtual Pin V0
        Blynk.virtualWrite(V0, fillLevel);
    } else {
        Serial.println("Lid is open, not sending data.");
    }
}

// Function to send smoke level to Blynk
void sendSmokeData() {
    int smokeValue = analogRead(SMOKE_SENSOR_PIN); // Read smoke sensor value
    float smokePPM = map(smokeValue, 0, 4095, 0, 1000); // Convert to approximate PPM

    Serial.print("Smoke Level: ");
    Serial.print(smokePPM);
    Serial.println(" PPM");

    Blynk.virtualWrite(V1, smokePPM); // Send to Blynk Virtual Pin V1

    // Set LED color in Blynk based on smoke level
    if (smokePPM < 420) {
        Blynk.virtualWrite(V2, 0); //  (Safe)
    } else if (smokePPM >= 420 && smokePPM <= 520) {
        Blynk.virtualWrite(V2, 127); // (Moderate)
    } else {
        Blynk.virtualWrite(V2, 255); // (Danger)
    }

    // Activate buzzer if smoke is above 520 PPM
    if (smokePPM > 520) {
        Serial.println("!!! DANGER: High smoke level detected !!!");
        digitalWrite(BUZZER_PIN, HIGH); // Turn ON buzzer
        Blynk.logEvent("smoke_alert", "Critical Smoke Level Detected!"); // Send notification
    } else {
        digitalWrite(BUZZER_PIN, LOW); // Turn OFF buzzer
    }
}
