#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <cppQueue.h>

static const u1_t PROGMEM APPEUI[8] = { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui(u1_t* buf) { memcpy_P(buf, APPEUI, 8); }

static const u1_t PROGMEM DEVEUI[8] = { 0x47, 0xF1, 0x06, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getDevEui(u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }

static const u1_t PROGMEM APPKEY[16] = { 0xB8, 0x97, 0xB7, 0xFC, 0x6D, 0x46, 0x00, 0x5A, 0x0B, 0xDB, 0xE0, 0xC7, 0xAC, 0x17, 0x43, 0xFD };
void os_getDevKey(u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

static osjob_t sendjob;
const unsigned TX_INTERVAL = 10;
const lmic_pinmap lmic_pins = { .nss = 10, .rxtx = LMIC_UNUSED_PIN, .rst = 7, .dio = {2, 5, 6} };

int fPort = 1;
cppQueue payloadQueue(sizeof(byte[9]), 30, FIFO, true);  // Increased queue size
bool uplinkEnabled = true;
bool pingTriggered = false;

void onEvent(ev_t ev) {
  Serial.print(os_getTime()); Serial.print(": Event: "); Serial.println(ev);
  if (ev == EV_JOINED) {
    Serial.println(F("EV_JOINED"));
    LMIC_setLinkCheckMode(0);
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(5), do_send);
  } else if (ev == EV_TXCOMPLETE) {
    Serial.println(F("EV_TXCOMPLETE"));

    if (LMIC.txrxFlags & TXRX_ACK) Serial.println(F("✅ Received ACK"));

    if (LMIC.dataLen > 0) {
      Serial.print(F("📥 Downlink: "));
      for (int i = 0; i < LMIC.dataLen; i++) {
        if (LMIC.frame[LMIC.dataBeg + i] < 16) Serial.print("0");
        Serial.print(LMIC.frame[LMIC.dataBeg + i], HEX);
      }
      Serial.println();

      if (LMIC.frame[LMIC.dataBeg] == 0x01) {
        Serial.println(F("📡 Forwarding PING to Pi via Serial..."));
        Serial.println("PING~");
        pingTriggered = true;
        uplinkEnabled = false;

        os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(30), [](osjob_t* j) {
          uplinkEnabled = true;
          pingTriggered = false;
          Serial.println(F("✅ Uplink resumed after PING"));
          do_send(j);
        });
        return;
      }
    }

    if (uplinkEnabled) {
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
    }
  }
}

void readSerialDataIfAvailable() {
  static String serialBuffer = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '~') {
      serialBuffer.trim();
      if (serialBuffer.startsWith("Node ")) {
        Serial.println(F("[TX] ✅ New valid serial:"));
        Serial.println(serialBuffer);

        int nodeId = 0, msgId = 0, pm25 = 0, temp = 0, hum = 0;
        uint8_t hopCount = 0;
        char hopsBuffer[50] = "-";

        const char* str = serialBuffer.c_str();
        int matched = sscanf(str, "Node %d (MsgID %d): PM2.5: %d Temp: %d Hum: %d Hops: %[^\\n]",
                             &nodeId, &msgId, &pm25, &temp, &hum, hopsBuffer);

        if (matched >= 5) {
          String hopsStr = String(hopsBuffer); hopsStr.trim();
          if (hopsStr != "-" && hopsStr.length() > 0) {
            hopCount = 1;
            for (int i = 0; i < hopsStr.length(); i++) {
              if (hopsStr[i] == '→') hopCount++;
            }
          }

          byte payload[9];
          uint16_t temp_int = (uint16_t)(temp * 100);
          payload[0] = nodeId;
          payload[1] = highByte(msgId);
          payload[2] = lowByte(msgId);
          payload[3] = highByte(pm25);
          payload[4] = lowByte(pm25);
          payload[5] = highByte(temp_int);
          payload[6] = lowByte(temp_int);
          payload[7] = hum;
          payload[8] = hopCount;

          payloadQueue.push(payload);
          Serial.println(F("📝 Payload queued successfully."));
        } else {
          Serial.println(F("❌ Failed to parse serial string."));
        }
      } else {
        Serial.print(F("[TX] ⚠️ Ignored invalid serial line: "));
        Serial.println(serialBuffer);
      }
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
}

void do_send(osjob_t* j) {
  if (!uplinkEnabled) {
    Serial.println(F("⛔ Uplink paused."));
    return;
  }

  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("⚠️ OP_TXRXPEND, retrying..."));
    os_setTimedCallback(j, os_getTime() + sec2osticks(2), do_send);
    return;
  }

  if (payloadQueue.isEmpty()) {
    Serial.println(F("⚠️ No payloads to send."));
    return;
  }

  byte payload[9];
  payloadQueue.pop(payload);
  Serial.println(F("📦 Sending payload:"));
  for (int i = 0; i < 9; i++) {
    Serial.print(payload[i]); Serial.print(" ");
  }
  Serial.println();

  LMIC_setTxData2(fPort, payload, sizeof(payload), 0);
  Serial.println(F("✅ Packet queued"));
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("[TX] 🔌 Starting..."));
  os_init();
  LMIC_reset();
  LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
  LMIC_startJoining();
}

void loop() {
  os_runloop_once();
  readSerialDataIfAvailable();
}
