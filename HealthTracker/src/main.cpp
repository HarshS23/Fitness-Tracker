#include <Arduino.h>

// // put function declarations here:
// int myFunction(int, int);

// void setup() {
//   // put your setup code here, to run once:
//   int result = myFunction(2, 3);
// }

// void loop() {
//   // put your main code here, to run repeatedly:
// }

// // put function definitions here:
// int myFunction(int x, int y) {
//   return x + y;
// }


int count = 0;
bool running = false;

void setup() {
    Serial.begin(115200);
    Serial.println("Send START to begin, STOP to pause");
}

void loop() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command == "START") {
            running = true;
            Serial.println("\nStarted\n");
        }
        else if (command == "STOP") {
            running = false;
            Serial.println("\nStopped\n");
            count = 0;
        }
    }
    if (running) {
        Serial.println(count);
        count++;
        delay(1000);
    }
}