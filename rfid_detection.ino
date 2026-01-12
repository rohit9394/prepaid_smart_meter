#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 5

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

byte unitBlock = 2;
byte buffer[18];
byte size = sizeof(buffer);

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  // Default key
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println("=== RFID READ MODE ===");
  Serial.println("Tap RFID card...");
}

void loop() {

  // Wait for card
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // -------- PRINT UID --------
  Serial.print("UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // -------- READ UNITS --------
  MFRC522::StatusCode status;

  status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    unitBlock,
    &key,
    &(mfrc522.uid)
  );

  if (status != MFRC522::STATUS_OK) {
    Serial.println("Authentication failed");
    cleanup();
    return;
  }

  status = mfrc522.MIFARE_Read(unitBlock, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.println("Read failed");
    cleanup();
    return;
  }

  Serial.print("Units stored: ");
  for (byte i = 0; i < 16; i++) {
    Serial.write(buffer[i]);
  }
  Serial.println();

  Serial.println("-----------------------");

  cleanup();
  delay(2000);   // avoid repeated reads
}

// -------- CLEANUP --------
void cleanup() {
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
