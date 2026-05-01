#include <SPI.h>
#include <MFRC522.h>
#include <EmonLib.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LoRa.h>   // ✅ ADDED

// ---------------- PIN CONFIG ----------------
#define RELAY_PIN 3
#define VOLT_PIN  A0
#define CURR_PIN  A1
#define SS_PIN    8
#define RST_PIN   9
#define LORA_SS   10
#define LORA_RST  7    // ⚠️ IMPORTANT: change this!
#define LORA_DIO0 2

char payload[80];

// ---------------- OBJECTS ----------------
EnergyMonitor emon;
LiquidCrystal_I2C lcd(0x27,16,2);
MFRC522 mfrc522(SS_PIN,RST_PIN);
MFRC522::MIFARE_Key key;

// ---------------- VARIABLES ----------------
byte authorizedUID[] = {0xC3,0xBF,0xDD,0x34};

double Voltage,Current,Power,PF,frequency;
double energyBalance=0;

bool cardLoaded=false;
bool loadState=false;
bool previousLoadState=false;
bool cardPresent = false;

const byte blockAddr=2;
byte buffer[18];
byte size=sizeof(buffer);

unsigned long lastUpdate=0;
unsigned long lastTime=0;

double lastWritten = -1;

// ---------------- FREQUENCY ----------------
double calculateFrequency()
{
  int prev=analogRead(VOLT_PIN);
  int curr;
  int crossings=0;

  unsigned long startTime=0;
  unsigned long endTime=0;

  while(crossings<6)
  {
    curr=analogRead(VOLT_PIN);

    if(prev<512 && curr>=512)
    {
      crossings++;

      if(crossings==1) startTime=micros();
      if(crossings==6) endTime=micros();
    }

    prev=curr;
  }

  double period=(endTime-startTime)/1000000.0;
  if(period==0) return 0;

  return (crossings-1)/period;
}

// ---------------- READ CARD ----------------
void readCardEnergy()
{
  byte atqa[2];
  byte atqaSize = sizeof(atqa);

  mfrc522.PICC_WakeupA(atqa, &atqaSize);

  if(!mfrc522.PICC_ReadCardSerial()){
    cardPresent= false;
    return;
  }

  cardPresent = true;

  for(byte i=0;i<4;i++)
  {
    if(mfrc522.uid.uidByte[i]!=authorizedUID[i])
    {
      Serial.println("Unauthorized card");
      return;
    }
  }

  MFRC522::StatusCode status;

  status=mfrc522.PCD_Authenticate(
  MFRC522::PICC_CMD_MF_AUTH_KEY_A,
  blockAddr,
  &key,
  &(mfrc522.uid));

  if(status!=MFRC522::STATUS_OK)
  {
    Serial.println("Auth failed");
    return;
  }

  status=mfrc522.MIFARE_Read(blockAddr,buffer,&size);

  if(status==MFRC522::STATUS_OK)
  {
    uint16_t val;
    memcpy(&val,buffer,sizeof(val));

    energyBalance=val/100.0;
    cardLoaded=true;

    Serial.print("Units Loaded: ");
    Serial.println(energyBalance);

    lcd.clear();
    lcd.print("Units:");
    lcd.setCursor(0,1);
    lcd.print(energyBalance);
    delay(1500);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

// ---------------- WRITE CARD ----------------
void writeCardEnergy()
{
  uint16_t val = (uint16_t)(energyBalance*100);

  byte dataBlock[16] = {0};
  memcpy(dataBlock, &val, sizeof(val));

  MFRC522::StatusCode status;

  byte atqa[2];
  byte atqaSize = sizeof(atqa);

  mfrc522.PICC_WakeupA(atqa, &atqaSize);

  if(!mfrc522.PICC_ReadCardSerial()){
    cardPresent = false;
    return;
  }

  cardPresent = true;

  status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    blockAddr,
    &key,
    &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) return;

  status = mfrc522.MIFARE_Write(blockAddr, dataBlock, 16);

  if (status == MFRC522::STATUS_OK)
  {
    Serial.print("Card Updated -> ");
    Serial.println(val);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

// ---------------- POWER PROCESS ----------------
void processPower()
{
  emon.calcVI(20,2000);

  Voltage=emon.Vrms;
  Current=emon.Irms;
  Power=emon.realPower;
  PF=emon.powerFactor;

  if(Voltage < 100){
    Voltage = 0;
    Current = 0;
    Power = 0;
  }

  frequency=calculateFrequency();

  unsigned long now = millis();
  double dt = (now - lastTime)/1000.0;
  lastTime = now;

  if(energyBalance>0)
  {
    loadState = true;

    double consumed = Power * dt / 3600.0;
    energyBalance -= consumed;

    if(energyBalance <= 0)
    {
      energyBalance = 0;
      loadState = false;

      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Units finished");
    }
  }
}

// ---------------- SERIAL OUTPUT ----------------
void serialOutput()
{
  Serial.print("V="); Serial.print(Voltage);
  Serial.print(" I="); Serial.print(Current);
  Serial.print(" P="); Serial.print(Power);
  Serial.print(" PF="); Serial.print(PF);
  Serial.print(" f="); Serial.print(frequency);
  Serial.print(" Bal="); Serial.print(energyBalance);
  Serial.print(" Load=");
  Serial.println(loadState?"ON":"OFF");
}

// ---------------- CREATE PAYLOAD ----------------
void createPayload()
{
  char v[10], i[10], f[10], pf[10], bal[10];

  dtostrf(Voltage, 5, 2, v);
  dtostrf(Current, 5, 2, i);
  dtostrf(frequency, 5, 2, f);
  dtostrf(PF, 5, 2, pf);
  dtostrf(energyBalance, 5, 2, bal);

  sprintf(payload,
    "<Node1:%s;%s;%s;%s;%s>",
    v, i, f, pf, bal
  );
}

// ---------------- SEND LORA ----------------
void sendLoRa()
{
  createPayload();

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  Serial.println("Sent:");
  Serial.println(payload);
}

// ---------------- SETUP ----------------
void setup()
{
  Serial.begin(9600);

  SPI.begin();
  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);
  mfrc522.PCD_Init();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  // ✅ LoRa init added
  if (!LoRa.begin(865E6)) {
    Serial.println("LoRa init failed");
    while (1);
  }

  pinMode(RELAY_PIN,OUTPUT);
  digitalWrite(RELAY_PIN,HIGH);

  lcd.init();
  lcd.backlight();

  emon.voltage(VOLT_PIN,417,-0.6);
  emon.current(CURR_PIN,14);

  for(byte i=0;i<6;i++) key.keyByte[i]=0xFF;

  lastTime = millis();

  lcd.print("Tap Card");
}

// ---------------- LOOP ----------------
void loop()
{
  if(!cardLoaded)
  {
    readCardEnergy();
    delay(300);
    return;
  }

  processPower();

  if(cardPresent && energyBalance>0){
    loadState = true;
    digitalWrite(RELAY_PIN,LOW); 
  }  
  else{
    loadState = false;
    digitalWrite(RELAY_PIN,HIGH);
  }

  serialOutput();

  if(millis() - lastUpdate > 2000)
  {
    writeCardEnergy();
    sendLoRa();   // ✅ added
    lastUpdate = millis();
  }

  previousLoadState = loadState;
}