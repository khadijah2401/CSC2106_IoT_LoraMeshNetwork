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
    0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81};
byte aes_iv[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
void resetIV()
{
    for (int i = 0; i < 16; i++)
        aes_iv[i] = i;
}

struct Payload
{
    float pm25;
    float temp;
    float hum;
    uint32_t timestamp;
};

struct Packet
{
    uint8_t senderId;
    uint16_t messageId;
    uint8_t ttl;
    uint8_t hops[5];
    uint8_t hopCount;
    byte encryptedPayload[sizeof(Payload)];
};

void setup()
{
    Serial.begin(9600);
    while (!Serial)
        ;

    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    if (!rf95.init())
    {
        Serial.println("❌ LoRa init failed");
        while (1)
            ;
    }

    rf95.setFrequency(RF95_FREQ);
    rf95.setTxPower(14, false);
    Serial.println("📡 Node 0 Ready to receive from Node 1");
}

void loop()
{
    if (rf95.available())
    {
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);
        if (rf95.recv(buf, &len))
        {
            Packet p;
            memcpy(&p, buf, sizeof(Packet));


            // Decrypt the payload
            resetIV();
            Payload data;
            byte decrypted[sizeof(Payload)];
            aesLib.decrypt(p.encryptedPayload, sizeof(Payload), decrypted, aes_key, 128, aes_iv);
            memcpy(&data, decrypted, sizeof(Payload));

            // Print in same original format
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
            for (int i = 0; i < p.hopCount; i++)
            {
                Serial.print(p.hops[i]);
                if (i < p.hopCount - 1)
                    Serial.print(" → ");
            }
            Serial.println();


        }
    }
}
