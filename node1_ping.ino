#include <SPI.h>
#include <RH_RF95.h>
#include <AESLib.h>

#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);
AESLib aesLib;

const uint8_t myID = 1;

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

struct PongResponse {
  uint8_t nodeId;
  uint32_t timestamp;
};

String seen[20];
int seenIndex = 0;
bool isDuplicate(uint8_t id, uint16_t mid) {
  String tag = String(id) + "-" + String(mid);
  for (int i = 0; i < 20; i++) if (seen[i] == tag) return true;
  seen[seenIndex++] = tag;
  seenIndex %= 20;
  return false;
}

void setup() {
  Serial.begin(9600);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  digitalWrite(RFM95_RST, LOW); delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  rf95.init(); rf95.setFrequency(RF95_FREQ); rf95.setTxPower(14, false);
  Serial.println("📡 Node 1 Collector Ready");
}

void loop() {
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      // Handle text-based PING
      if (len == 4 && strncmp((char*)buf, "PING", 4) == 0) {
        PongResponse pong;
        pong.nodeId = myID;
        pong.timestamp = millis();
        rf95.send((uint8_t*)&pong, sizeof(PongResponse));
        rf95.waitPacketSent();
        Serial.println("🔁 PING received → PONG sent to Node 0");
        return;
      }

      Packet p;
      memcpy(&p, buf, sizeof(Packet));
      if (!isDuplicate(p.senderId, p.messageId)) {
        if (p.hopCount == 1 && p.hops[0] == p.senderId) return;
        rf95.send((uint8_t*)&p, sizeof(Packet));
        rf95.waitPacketSent();
        Serial.println("Forwarded to Node 0");
      }
    }
  }
}