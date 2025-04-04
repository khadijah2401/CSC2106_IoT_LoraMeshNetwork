#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 901.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);

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
  Serial.println("📡 Node 0 Ready to receive from Node 1");
}

void loop() {
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      Packet p;
      memcpy(&p, buf, sizeof(Packet));

      Serial.print("📥 Received from Node ");
      Serial.print(p.senderId);
      Serial.print(" | MsgID: ");
      Serial.print(p.messageId);
      Serial.print(" | PM2.5: ");
      Serial.print(p.pm25);
      Serial.print(" | Temp: ");
      Serial.print(p.temp);
      Serial.print(" | Hum: ");
      Serial.print(p.hum);
      Serial.print(" | Time: ");
      Serial.print(p.timestamp);
      Serial.print(" | Hops: ");
      for (int i = 0; i < p.hopCount; i++) {
        Serial.print(p.hops[i]);
        if (i < p.hopCount - 1) Serial.print(" → ");
      }
      Serial.println();
    }
  }
}
