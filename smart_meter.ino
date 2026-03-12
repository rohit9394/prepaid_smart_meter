#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------- PIN DEFINITIONS --------
#define ZMPT_PIN A0     // Voltage sensor input
#define ACS_PIN  A1     // Current sensor input
#define RELAY_PIN 7     // Relay control pin

// -------- ADC PARAMETERS --------
#define ADC_REF 5.0
#define ADC_RES 1023.0

// ACS712-30A sensitivity (66 mV/A)
#define ACS_SENSITIVITY 0.066

// LCD object (address 0x27, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27,16,2);

// Number of samples used for RMS calculation
#define SAMPLES 800

// -------- CALIBRATION CONSTANTS --------
// Adjust these once using multimeter
float VOLTAGE_CAL = 380;   
float CURRENT_CAL = 1.56;

// -------- MEASURED PARAMETERS --------
float Vrms, Irms;
float power;
float energyWh;
float frequency;

// Used to calculate energy consumption
unsigned long lastEnergyTime;

void setup()
{
  Serial.begin(9600);

  // Configure relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);   // Relay OFF initially

  // -------- LCD INITIALIZATION --------
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Smart Meter");
  lcd.setCursor(0,1);
  lcd.print("Starting...");
  delay(2000);

  lcd.clear();

  lastEnergyTime = millis();

  Serial.println("Smart Energy Meter Started");
}

void loop()
{

  float sumV = 0;     // Sum of squared voltage samples
  float sumI = 0;     // Sum of squared current samples

  float offsetV = 0;  // DC offset of voltage signal
  float offsetI = 0;  // DC offset of current signal

  // -------- OFFSET CALCULATION --------
  // Average of 200 samples used to remove DC bias
  for(int i=0;i<200;i++)
  {
    offsetV += analogRead(ZMPT_PIN);
    offsetI += analogRead(ACS_PIN);
    delay(2);
  }

  offsetV /= 200;
  offsetI /= 200;

  // -------- FREQUENCY VARIABLES --------
  int lastSign = 0;
  int crossings = 0;

  unsigned long startMicros = micros();

  // -------- MAIN SAMPLING LOOP --------
  for(int i=0;i<SAMPLES;i++)
  {

    int v_adc = analogRead(ZMPT_PIN);
    int i_adc = analogRead(ACS_PIN);

    // Convert ADC reading to voltage (remove offset)
    float v = (v_adc - offsetV) * (ADC_REF / ADC_RES);

    // Convert ADC reading to current
    float c = (i_adc - offsetI) * (ADC_REF / ADC_RES) / ACS_SENSITIVITY;

    // RMS accumulation
    sumV += v*v;
    sumI += c*c;

    // Zero crossing detection for frequency
    int sign = (v > 0);

    if(sign != lastSign)
    {
      crossings++;
      lastSign = sign;
    }

    delayMicroseconds(200);
  }

  unsigned long duration = micros() - startMicros;

  // -------- RMS CALCULATION --------
  float Vrms_adc = sqrt(sumV / SAMPLES);
  float Irms_adc = sqrt(sumI / SAMPLES);

  // Apply calibration factors
  Vrms = Vrms_adc * VOLTAGE_CAL;
  Irms = Irms_adc * CURRENT_CAL;

  // -------- POWER CALCULATION --------
  power = Vrms * Irms;    // Apparent power

  // -------- FREQUENCY CALCULATION --------
  float timeSec = duration / 1000000.0;
  frequency = (crossings / 2.0) / timeSec;

  if(frequency < 45 || frequency > 55)
    frequency = 50;

  // -------- ENERGY CALCULATION --------
  unsigned long now = millis();
  float hours = (now - lastEnergyTime) / 3600000.0;
  energyWh += power * hours;
  lastEnergyTime = now;

  // -------- NO LOAD FILTER --------
  if(Irms < 0.05)
  {
    Irms = 0;
    power = 0;
  }

  // -------- SERIAL OUTPUT --------
  Serial.print("Voltage: ");
  Serial.print(Vrms,1);
  Serial.print(" V | Current: ");
  Serial.print(Irms,3);
  Serial.print(" A | Power: ");
  Serial.print(power,1);
  Serial.print(" W | Energy: ");
  Serial.print(energyWh,4);
  Serial.print(" Wh | Frequency: ");
  Serial.print(frequency,1);
  Serial.println(" Hz");

  // -------- LCD DISPLAY --------
  lcd.setCursor(0,0);
  lcd.print("V:");
  lcd.print(Vrms,0);
  lcd.print(" I:");
  lcd.print(Irms,2);
  lcd.print("   ");   // clear leftover characters

  lcd.setCursor(0,1);
  lcd.print("Power:");
  lcd.print(power,0);
  lcd.print("W   ");

  // -------- RELAY CONTROL --------
  if(Serial.available())
  {
    char cmd = Serial.read();

    if(cmd=='1')
      digitalWrite(RELAY_PIN,LOW);   // Relay ON

    if(cmd=='0')
      digitalWrite(RELAY_PIN,HIGH);  // Relay OFF
  }

  delay(1000);
}
