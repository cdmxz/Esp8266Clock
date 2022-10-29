#include <U8g2lib.h>

#include <Wire.h>
#include <ESP8266WiFi.h>

#define Addr 0x44

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

void setup()
{
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin(); // 关闭WiFi
    Wire.begin();
    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_wqy16_t_gb2312);
    // Serial.begin(9600);
}

void get_data(char *temp, char *hum)
{
    unsigned int data[6];
    // Start I2C Transmission
    Wire.beginTransmission(Addr);
    // Send 16-bit command byte
    Wire.write(0x2C);
    Wire.write(0x06);
    // Stop I2C transmission
    Wire.endTransmission();
    delay(300);
    // Start I2C Transmission
    Wire.beginTransmission(Addr);
    // Stop I2C Transmission
    Wire.endTransmission();
    // Request 6 bytes of data
    Wire.requestFrom(Addr, 6);
    // Read 6 bytes of data
    // temp msb, temp lsb, temp crc, hum msb, hum lsb, hum crc
    if (Wire.available() == 6)
    {
        data[0] = Wire.read();
        data[1] = Wire.read();
        data[2] = Wire.read();
        data[3] = Wire.read();
        data[4] = Wire.read();
        data[5] = Wire.read();
    }
    // Convert the data
    int t = (data[0] * 256) + data[1];
    float cTemp = -45.0 + (175.0 * t / 65535.0);
    float humidity = (100.0 * ((data[3] * 256.0) + data[4])) / 65535.0;
    dtostrf(cTemp, 1, 1, temp);
    dtostrf(humidity, 1, 1, hum);
}

void loop()
{
    char str[20], temp[6], hum[6];
    get_data(temp, hum);
    u8g2.clearBuffer();
    u8g2.setCursor(78, 13);
    u8g2.print("by");
    u8g2.setCursor(10, 35);
    // Serial.println(temp);
    snprintf(str, sizeof(str), "温度：%s C", temp);
    u8g2.print(str);
    u8g2.setCursor(10, 60);
    snprintf(str, sizeof(str), "湿度：%s %%", hum);
    u8g2.print(str);
    u8g2.sendBuffer();
    delay(3000);
}
