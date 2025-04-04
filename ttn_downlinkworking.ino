#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// TTN Credentials
#ifdef COMPILE_REGRESSION_TEST
#define FILLMEIN 0
#else
#warning "You must replace the values marked FILLMEIN with real values from the TTN control panel!"
#define FILLMEIN (#dont edit this, edit the lines that use FILLMEIN)
#endif

bool uplinkEnabled = true;  // Controls whether uplinks are sent

static const u1_t PROGMEM APPEUI[8] = { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui(u1_t* buf) {
  memcpy_P(buf, APPEUI, 8);
}

static const u1_t PROGMEM DEVEUI[8] = { 0x47, 0xF1, 0x06, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getDevEui(u1_t* buf) {
  memcpy_P(buf, DEVEUI, 8);
}

static const u1_t PROGMEM APPKEY[16] = { 0xB8, 0x97, 0xB7, 0xFC, 0x6D, 0x46, 0x00, 0x5A, 0x0B, 0xDB, 0xE0, 0xC7, 0xAC, 0x17, 0x43, 0xFD };
void os_getDevKey(u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}

static osjob_t sendjob;

const unsigned TX_INTERVAL = 10;  // Transmit every 60 seconds

// Pin mapping
const lmic_pinmap lmic_pins = {
  .nss = 10,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 7,
  .dio = { 2, 5, 6 },
};

int fPort = 1;  // fPort = 1 for environmental data

void printHex2(unsigned v) {
  v &= 0xff;
  if (v < 16)
    Serial.print('0');
  Serial.print(v, HEX);
}

void onEvent(ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch (ev) {
    case EV_JOINING:
      Serial.println(F("EV_JOINING"));
      break;

    case EV_JOINED:
      Serial.println(F("EV_JOINED"));
      LMIC_setLinkCheckMode(0);
      LMIC_setDrTxpow(DR_SF7, 14);
      do_send(&sendjob);  // Start sending
      break;

    case EV_TXSTART:
      Serial.println(F("EV_TXSTART"));
      break;

    case EV_TXCOMPLETE:
      Serial.println(F("EV_TXCOMPLETE (includes RX window)"));

      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("✅ Received ACK"));

      if (LMIC.dataLen > 0) {
        Serial.print(F("📥 Received downlink ("));
        Serial.print(LMIC.dataLen);
        Serial.println(F(" byte):"));

        Serial.print("Payload: ");
        for (int i = 0; i < LMIC.dataLen; i++) {
          Serial.print("0x");
          Serial.print(LMIC.frame[LMIC.dataBeg + i], HEX);
          Serial.print(" ");
        }
        Serial.println();

        byte cmd = LMIC.frame[LMIC.dataBeg];  // First byte of payload
        if (cmd == 0x01) {
          Serial.println(F("🛑 Command 0x01 received: Temporarily disabling uplinks..."));
          uplinkEnabled = false;

          // Schedule re-enable after 30 seconds (3 TX_INTERVALs)
          os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(30), [](osjob_t*) {
            uplinkEnabled = true;
            Serial.println(F("✅ Auto re-enabled uplink after timeout"));
            do_send(&sendjob);
          });
          return;  // Don't send next packet now
        }
      }

      // If no downlink or not 0x01, just continue uplinking
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      break;

    case EV_RXCOMPLETE:
      Serial.println(F("📡 EV_RXCOMPLETE – LoRa data received"));
      break;

    case EV_JOIN_FAILED:
      Serial.println(F("❌ EV_JOIN_FAILED"));
      break;

    case EV_REJOIN_FAILED:
      Serial.println(F("❌ EV_REJOIN_FAILED"));
      break;

    default:
      Serial.print(F("Unknown event: "));
      Serial.println((unsigned)ev);
      break;
  }
}




// Global buffer for the last valid data line from Pi
String lastValidSerialData = "";

void readSerialDataIfAvailable() {
  while (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    int nodeIndex = data.indexOf("Node ");
    int pmIndex = data.indexOf("PM2.5:");
    int tempIndex = data.indexOf("Temp:");
    int humIndex = data.indexOf("Hum:");

    if (nodeIndex != -1 && pmIndex != -1 && tempIndex != -1 && humIndex != -1) {
      lastValidSerialData = data;
      Serial.print("📥 New valid serial: ");
      Serial.println(lastValidSerialData);
    } else {
      Serial.println("⚠️  Ignored invalid serial line.");
    }
  }
}

void do_send(osjob_t* j) {
  if (!uplinkEnabled) {
    Serial.println(F("⛔ Uplink disabled. Skipping send."));
    return;
  }

  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("⚠️ OP_TXRXPEND, not sending"));
    return;
  }

  // Use fallback dummy data if nothing from Pi
  if (lastValidSerialData.length() == 0) {
    lastValidSerialData = "Node 0: PM2.5:42 Temp:25.00 Hum:50.00";
    Serial.println(F("⚠️ No valid serial yet, using dummy line"));
  }

  // Parse dummy or Pi data
  int nodeId = 0, pm25 = 0;
  float temp = 0.0, hum = 0.0;
  String data = lastValidSerialData;

  int nodeIndex = data.indexOf("Node ");
  int pmIndex = data.indexOf("PM2.5:");
  int tempIndex = data.indexOf("Temp:");
  int humIndex = data.indexOf("Hum:");

  if (nodeIndex != -1 && pmIndex != -1 && tempIndex != -1 && humIndex != -1) {
    nodeId = data.substring(nodeIndex + 5, data.indexOf(':')).toInt();
    pm25 = data.substring(pmIndex + 6, data.indexOf(' ', pmIndex)).toInt();
    temp = data.substring(tempIndex + 5, data.indexOf(' ', tempIndex)).toFloat();
    hum = data.substring(humIndex + 4).toFloat();
  }

  // Debug
  Serial.println(F("📤 Sending uplink payload:"));
  Serial.print("Node: ");
  Serial.println(nodeId);
  Serial.print("PM2.5: ");
  Serial.println(pm25);
  Serial.print("Temp: ");
  Serial.println(temp);
  Serial.print("Hum: ");
  Serial.println(hum);

  byte payload[7];
  payload[0] = (uint8_t)nodeId;
  payload[1] = highByte(pm25);
  payload[2] = lowByte(pm25);
  payload[3] = highByte((uint16_t)(temp * 100));
  payload[4] = lowByte((uint16_t)(temp * 100));
  payload[5] = highByte((uint16_t)(hum * 100));
  payload[6] = lowByte((uint16_t)(hum * 100));

  LMIC_setTxData2(fPort, payload, sizeof(payload), 0);
  Serial.println(F("✅ Packet queued"));
}


// void do_send(osjob_t* j) {
//   if (LMIC.opmode & OP_TXRXPEND) {
//     Serial.println(F("OP_TXRXPEND, not sending"));
//   } else {
//     // Buffer to store serial data from Pi

//     char serialData[64];
//     int nodeId = 0;
//     int pm25 = 0;
//     float temp = 0.0;
//     float hum = 0.0;

//     // Read from Pi via Serial
//     if (Serial.available() > 0) {
//       String data = Serial.readStringUntil('\n');
//       data.trim();
//       // data.toCharArray(serialData, 64);

//       // Serial.print("Received from Pi: ");
//       // Serial.println(serialData);

//       // Example: "Node 3: PM2.5:28 Temp:27 Hum:41"
//       int nodeIndex = data.indexOf("Node ");
//       int pmIndex   = data.indexOf("PM2.5:");
//       int tempIndex = data.indexOf("Temp:");
//       int humIndex  = data.indexOf("Hum:");

//       if (nodeIndex != -1 && pmIndex != -1 && tempIndex != -1 && humIndex != -1) {
//         nodeId = data.substring(nodeIndex + 5, data.indexOf(':')).toInt();
//         pm25   = data.substring(pmIndex + 6, data.indexOf(' ', pmIndex)).toInt();
//         temp   = data.substring(tempIndex + 5, data.indexOf(' ', tempIndex)).toFloat();
//         hum    = data.substring(humIndex + 4).toFloat();
//       }

//       // Expecting: "Node X: PM2.5:Y Temp:Z Hum:W"
//       // Extract Node ID
//       // sscanf(serialData, " Node %d: PM2.5:%d Temp:%f Hum:%f", &nodeId, &pm25, &temp, &hum);
//     }
//     // else {
//       // Fallback random data if no serial input (for testing)
//       // Serial.println(F("No data from Pi, using fallback"));
//       // pm25 = random(10, 100);
//       // temp = random(2000, 3500) / 100.0;  // 20.00 to 35.00
//       // hum = random(4000, 8000) / 100.0;   // 40.00 to 80.00
//     // }

//     Serial.print("Node ID: ");
//     Serial.println(nodeId);
//     // Log parsed values
//     Serial.print("PM2.5: ");
//     Serial.println(pm25);
//     Serial.print("Temperature: ");
//     Serial.println(temp);
//     Serial.print("Humidity: ");
//     Serial.println(hum);

//     byte payload[7];
//     uint16_t pm25_int = (uint16_t)pm25;              // 0-65535 range
//     uint16_t temp_int = (uint16_t)(temp * 100);      // Scale to 0-65535
//     uint16_t hum_int = (uint16_t)(hum * 100);        // Scale to 0-65535

//     payload[0] = (uint8_t)nodeId;
//     payload[1] = highByte(pm25);
//     payload[2] = lowByte(pm25);
//     payload[3] = highByte(temp_int);
//     payload[4] = lowByte(temp_int);
//     payload[5] = highByte(hum_int);
//     payload[6] = lowByte(hum_int);

//     // payload[0] = highByte(pm25_int);  // PM2.5 high byte
//     // payload[1] = lowByte(pm25_int);   // PM2.5 low byte
//     // payload[2] = highByte(temp_int);  // Temperature high byte
//     // payload[3] = lowByte(temp_int);   // Temperature low byte
//     // payload[4] = highByte(hum_int);   // Humidity high byte
//     // payload[5] = lowByte(hum_int);    // Humidity low byte

//     fPort = 1;
//     LMIC_setTxData2(fPort, payload, sizeof(payload), 0);
//     Serial.println(F("Packet queued"));
//   }
// }

void setup() {
  Serial.begin(9600);  // Match Pi’s baud rate
  Serial.println(F("Starting"));

  os_init();
  LMIC_reset();
  LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);  // Compensate for clock error
  // do_send(&sendjob);
  LMIC_startJoining();  // ✅ Begin the OTAA join process

  delay(5000);  // ⏳ Wait 5 seconds before starting transmission
}

void loop() {
  os_runloop_once();
  readSerialDataIfAvailable();  // ⬅️ Constantly poll for incoming data
}
