#include <SPI.h>
#include <RH_RF95.h>
#include <AESLib.h>

#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 901.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);
AESLib aesLib;

const uint8_t myID = 3;
uint16_t msgCounter = 0;
const int TTL = 5;

// 128-bit AES key
byte aes_key[] = {
  0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
  0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81
};

// IV (optional but required by function signature)
byte aes_iv[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

struct Packet {
  uint8_t senderId;
  uint16_t messageId;
  uint8_t ttl;
  float pm25;
  float temp;
  float hum;
  uint32_t timestamp;
  uint8_t hops[5];
  uint8_t hopCount;
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

  Serial.println("🔐 AESLib Example Ready");
}

void loop() {
  static unsigned long lastSend = 0;

  if (millis() - lastSend > 10000) {
    Packet p = {
      myID, msgCounter++, TTL,
      random(10, 100),
      random(2000, 3500) / 100.0,
      random(4000, 8000) / 100.0,
      millis() / 1000, {}, 0
    };

    byte cleartext[64];
    memcpy(cleartext, &p, sizeof(p));

    byte encrypted[64];
    aesLib.encrypt(cleartext, sizeof(p), encrypted, aes_key, 128, aes_iv);

    rf95.send(encrypted, sizeof(p));
    rf95.waitPacketSent();

    Serial.print("📤 Encrypted & sent MsgID: ");
    Serial.println(p.messageId);

    lastSend = millis();
  }

  if (rf95.available()) {
    byte buf[64]; uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      byte decrypted[64];
      aesLib.decrypt(buf, len, decrypted, aes_key, 128, aes_iv);

      Packet received;
      memcpy(&received, decrypted, sizeof(Packet));

      Serial.print("📥 Decrypted MsgID: ");
      Serial.println(received.messageId);
    }
  }
}
