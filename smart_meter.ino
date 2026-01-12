#define ZMPT_PIN A0
#define ACS_PIN  A1
#define RELAY_PIN 7

#define ADC_REF 5.0
#define ADC_RES 1023.0
#define SAMPLES 500

float voltageCalibration = 309.0;

// ⚠️ Change if your ACS type is different
#define ACS_SENSITIVITY 0.066   // 30A module

void setup() {
  Serial.begin(9600);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF initially (safe)

  delay(1000);
  Serial.println("Smart Meter Measurement Started");
}

void loop() {

  float v_sq_sum = 0;
  float i_sq_sum = 0;

  for (int i = 0; i < SAMPLES; i++) {

    // Voltage (ZMPT)
    int v_adc = analogRead(ZMPT_PIN);
    float v = (v_adc * ADC_REF / ADC_RES) - (ADC_REF / 2.0);
    v_sq_sum += v * v;

    // Current (ACS712)
    int i_adc = analogRead(ACS_PIN);
    float i_voltage = (i_adc * ADC_REF / ADC_RES) - (ADC_REF / 2.0);
    float current = i_voltage / ACS_SENSITIVITY;
    i_sq_sum += current * current;

    delayMicroseconds(200);
  }

  float voltage_rms = sqrt(v_sq_sum / SAMPLES) * voltageCalibration;
  float current_rms = sqrt(i_sq_sum / SAMPLES);
  float power = voltage_rms * current_rms;

  // -------- USER OFF / NO SUPPLY CONDITION --------
  if (voltage_rms < 5.0) {

    voltage_rms = 0.0;
    current_rms = 0.0;
    power = 0.0;

    digitalWrite(RELAY_PIN, HIGH); // Relay OFF

    Serial.println("Vrms: 0.00 V | Irms: 0.000 A | Power: 0.00 VA");
  }
  else {
    // -------- NORMAL OPERATION --------
    digitalWrite(RELAY_PIN, LOW); // Relay ON

    Serial.print("Vrms: ");
    Serial.print(voltage_rms, 2);
    Serial.print(" V | Irms: ");
    Serial.print(current_rms, 3);
    Serial.print(" A | Power: ");
    Serial.print(power, 2);
    Serial.println(" VA");
  }

  delay(1000);
}
