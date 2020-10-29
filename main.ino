unsigned long ts;
// unsigned long ms;
unsigned int width = 1500;
char bytes[2];

void setup() {
    Serial.begin(115200);
    ts = micros();
    // ms = millis();
    DDRB = DDRB | B00100000; // pin 13
}

void loop() {

    unsigned long t = micros() - ts;
    // unsigned long m = millis() - ms;

    // Serial.write(1500 >> 8);
    // Serial.write((1500 & 0xff));


    if (Serial.available() > 1) {
        Serial.readBytes(bytes, 2);
        // Serial.write(bytes[0]);
        // Serial.write(bytes[1]);
        width = (bytes[1] << 8) | (bytes[0] & 0xff);
        // Serial.println(String(width));
        // Serial.println(bytes);
        // width = (bytes[1] << 8) | (bytes[0] & 0xff);
        // width = (bytes[1] << 8) | bytes[0];   
    }

    if (t <= width) {
        PORTB = PORTB | B00100000;
    }
    else {
        PORTB = B00000000;
    }

    if (t >= 20000) {
        ts = micros();
    }

    // if (m >= 3000) {
    //     width = 2000;
    // }
}