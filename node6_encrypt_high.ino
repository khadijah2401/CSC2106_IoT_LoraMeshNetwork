#include <SPI.h>
#include <RH_RF95.h>
#include <AESLib.h>

#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);
AESLib aesLib;

const uint8_t myID = 6; // Change to 3 or 4 for other nodes
uint16_t msgCounter = 0;
const int TTL = 5;

byte aes_key[] = {
  0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
  0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81
};

byte aes_iv[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
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

String seen[20]; int seenIndex = 0;
bool isDuplicate(uint8_t id, uint16_t mid) {
  String tag = String(id) + "-" + String(mid);
  for (int i = 0; i < 20; i++) if (seen[i] == tag) return true;
  seen[seenIndex++] = tag; seenIndex %= 20;
  return false;
}

void setup() {
  Serial.begin(9600);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  digitalWrite(RFM95_RST, LOW); delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  rf95.init(); rf95.setFrequency(RF95_FREQ); rf95.setTxPower(14, false);
  Serial.print("🔁 Node "); Serial.print(myID); Serial.println(" Ready (Encrypted Flooding)");
  randomSeed(analogRead(0));
}

void loop() {
  static unsigned long lastSend = 0;

  // Simulate sending a new message
  if (millis() - lastSend > 10000) {
    Payload data = {
      random(60, 99),
      random(3000, 3500) / 100.0,
      random(6000, 8000) / 100.0,
      millis() / 1000
    };

    byte encrypted[sizeof(Payload)];
    resetIV();
    aesLib.encrypt((byte*)&data, sizeof(Payload), encrypted, aes_key, 128, aes_iv);

    Packet p = {
      myID, msgCounter++, TTL,
      {}, 0
    };
    memcpy(p.encryptedPayload, encrypted, sizeof(Payload));

    rf95.send((uint8_t*)&p, sizeof(Packet)); rf95.waitPacketSent();
    Serial.print("📤 Sent MsgID "); Serial.println(p.messageId);
    lastSend = millis();
  }

  // Relay received packets
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      Packet p; memcpy(&p, buf, sizeof(Packet));
      if (p.senderId == myID) return; 
      if (!isDuplicate(p.senderId, p.messageId)) {
        if (p.ttl > 0 && p.hopCount < 5) {
          delay(random(100, 400));
          p.hops[p.hopCount++] = myID;
          p.ttl--;
          rf95.send((uint8_t*)&p, sizeof(Packet)); rf95.waitPacketSent();
          Serial.print("🔁 Forwarded MsgID "); Serial.println(p.messageId);
        }
      }
    }
  }
}
