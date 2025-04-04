#include <SPI.h>
#include <RH_RF95.h>
#include <AESLib.h>

#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 901.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);
AESLib aesLib;

const uint8_t myID = 2;
uint16_t msgCounter = 0;
const int TTL = 5;

// AES Key and IV
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

void setup() {
  Serial.begin(9600);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH); delay(10);
  digitalWrite(RFM95_RST, LOW); delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);

  rf95.init();
  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(14, false);

  Serial.println("📡 Node 2 (Sender) ready");
  Serial.print("Packet size: ");
  Serial.println(sizeof(Packet));  // Should be ~35
}

void loop() {
  static unsigned long lastSend = 0;

  if (millis() - lastSend > 10000) {
    Payload data = {
      random(10, 100),
      random(2000, 3500) / 100.0,
      random(4000, 8000) / 100.0,
      millis() / 1000
    };

    byte encrypted[sizeof(Payload)];
    resetIV();  // reset IV before every encryption
    aesLib.encrypt((byte*)&data, sizeof(Payload), encrypted, aes_key, 128, aes_iv);

    Packet p = {
      myID, msgCounter++, TTL,
      {},  // hops
      0    // hopCount
    };
    memcpy(p.encryptedPayload, encrypted, sizeof(Payload));

    rf95.send((uint8_t*)&p, sizeof(Packet));
    rf95.waitPacketSent();

    Serial.println("📝 Cleartext payload:");
    Serial.print("  timestamp: "); Serial.println(data.timestamp);
    Serial.print("  messageId: "); Serial.println(p.messageId);
    Serial.print("  pm25: "); Serial.println(data.pm25);
    Serial.print("  temp: "); Serial.println(data.temp);
    Serial.print("  hum: "); Serial.println(data.hum);

    lastSend = millis();
  }
}
