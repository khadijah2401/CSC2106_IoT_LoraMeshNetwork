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

byte aes_key[] = {
  0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
  0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81
};

byte aes_iv[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
void resetIV() {
  for (int i = 0; i < 16; i++) aes_iv[i] = i;
}
struct __attribute__((packed)) Packet {
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

  Serial.println("📡 Node 3 (Receiver) ready");
  Serial.print("Packet size: ");
  Serial.println(sizeof(Packet));  // Should be 29
}

void loop() {
  if (rf95.available()) {
    byte buf[64] = {0}; uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
byte decrypted[26];
resetIV();  // right before aesLib.encrypt()

aesLib.decrypt(buf, 26, decrypted, aes_key, 128, aes_iv);

      Packet p;
      memcpy(&p, decrypted, sizeof(Packet));

      Serial.print("📥 Decrypted MsgID: ");
      Serial.print(p.messageId);
      Serial.print(" from Node ");
      Serial.println(p.senderId);
    }
  }
}
