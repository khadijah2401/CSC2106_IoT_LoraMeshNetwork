#include <SPI.h>
#include <RH_RF95.h>
#include <AESLib.h>

#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);
AESLib aesLib;

// AES Key and IV
byte aes_key[] = {
    0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
    0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81
};
byte aes_iv[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

void resetIV() {
  for (int i = 0; i < 16; i++) aes_iv[i] = i;
}

struct Payload {
  float pm25;
  float temp;
  float hum;
  uint32_t timestamp;
};

struct Packet {
  uint8_t senderId;
  uint16_t messageId;
  uint8_t ttl;
  uint8_t hops[5];
  uint8_t hopCount;
  byte encryptedPayload[sizeof(Payload)];
};

// Duplicate filter
String seen[20];
int seenIndex = 0;

bool isDuplicate(uint8_t id, uint16_t mid) {
  String tag = String(id) + "-" + String(mid);
  for (int i = 0; i < 20; i++) {
    if (seen[i] == tag) return true;
  }
  seen[seenIndex++] = tag;
  seenIndex %= 20;
  return false;
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  digitalWrite(RFM95_RST, LOW); delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);

  if (!rf95.init()) {
    Serial.println("❌ LoRa init failed");
    while (1);
  }

  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(14, false);
  Serial.println("📡 Node 0 Ready to receive from mesh");
}

void loop() {
  // Check for PING command from Pi
  if (Serial.available()) {
    String command = Serial.readStringUntil('~');
    command.trim();
    if (command == "PING") {
      Serial.println("📡 Received PING command from Pi. Broadcasting to mesh...");

      Packet pingPacket;
      pingPacket.senderId = 0;
      pingPacket.messageId = 999;
      pingPacket.ttl = 5;
      pingPacket.hopCount = 1;
      pingPacket.hops[0] = 0;

      Payload dummy;
      dummy.pm25 = -1.0;
      dummy.temp = -1.0;
      dummy.hum = -1.0;
      dummy.timestamp = millis();

      resetIV();
      aesLib.encrypt((byte *)&dummy, sizeof(Payload), pingPacket.encryptedPayload, aes_key, 128, aes_iv);

      rf95.send((uint8_t *)&pingPacket, sizeof(Packet));
      rf95.waitPacketSent();
      Serial.println("📤 PING broadcasted");
    }
  }

  // Listen for incoming LoRa packets
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      if (len == sizeof(Packet)) {
        Packet p;
        memcpy(&p, buf, sizeof(Packet));

        if (isDuplicate(p.senderId, p.messageId)) {
          // Serial.print("⚠️ Duplicate detected from Node ");
          // Serial.print(p.senderId);
          // Serial.print(" | MsgID: ");
          // Serial.println(p.messageId);
          return;  // skip duplicate
        }

        // Decrypt payload
        resetIV();
        Payload data;
        byte decrypted[sizeof(Payload)];
        aesLib.decrypt(p.encryptedPayload, sizeof(Payload), decrypted, aes_key, 128, aes_iv);
        memcpy(&data, decrypted, sizeof(Payload));

        // Print in formatted output
        Serial.print("📥 Received from Node ");
        Serial.print(p.senderId);
        Serial.print(" | MsgID: ");
        Serial.print(p.messageId);
        Serial.print(" | PM2.5: ");
        Serial.print(data.pm25);
        Serial.print(" | Temp: ");
        Serial.print(data.temp);
        Serial.print(" | Hum: ");
        Serial.print(data.hum);
        Serial.print(" | Time: ");
        Serial.print(data.timestamp);
        Serial.print(" | Hops: ");
        for (int i = 0; i < p.hopCount; i++) {
          Serial.print(p.hops[i]);
          if (i < p.hopCount - 1) Serial.print(" → ");
        }
        Serial.println();
      }
    }
  }
}
