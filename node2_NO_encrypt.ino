#include <SPI.h>
  #include <RH_RF95.h>

  #define RFM95_CS 10
  #define RFM95_RST 9
  #define RFM95_INT 2
  #define RF95_FREQ 915.0

  RH_RF95 rf95(RFM95_CS, RFM95_INT);
  const uint8_t myID = 3; // Change to 3 or 4 for other nodes
  uint16_t msgCounter = 0;
  const int TTL = 5;

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
    Serial.print("🔁 Node "); Serial.print(myID); Serial.println(" Ready (Flooding)");
    randomSeed(analogRead(0));
  }

  void loop() {
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 7000 + random(0, 3000)) {
      Packet p = {myID, msgCounter++, TTL, random(10,100), random(2000,3500)/100.0, random(4000,8000)/100.0, millis()/1000, {}, 0};
      rf95.send((uint8_t*)&p, sizeof(p)); rf95.waitPacketSent();
      Serial.print("📤 Sent MsgID "); Serial.println(p.messageId);
      lastSend = millis();
    }

    if (rf95.available()) {
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; uint8_t len = sizeof(buf);
      if (rf95.recv(buf, &len)) {
        Packet p; memcpy(&p, buf, sizeof(Packet));
        if (!isDuplicate(p.senderId, p.messageId)) {
          if (p.ttl > 0 && p.hopCount < 5) {
            delay(random(100, 400));  // avoid collisions
            p.hops[p.hopCount++] = myID;
            p.ttl--;
            rf95.send((uint8_t*)&p, sizeof(p)); rf95.waitPacketSent();
            Serial.print("🔁 Forwarded MsgID "); Serial.println(p.messageId);
          }
        }
      }
    }
  }