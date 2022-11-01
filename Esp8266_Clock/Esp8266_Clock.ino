#include <ds3231.h>
#include <EEPROM.h>
#include <TimeLib.h>
#include <U8g2lib.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#define CONFIG_UNIXTIME
//***********************函数定义****************************
// 获取ntp时间
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
// 通过网络设定时间
void syncTime();
// 微信配网
bool smartConfig();
// 连接WiFi
bool connectWifi(char *ssid, char *pwd);
// 显示时间
void displayTime(struct ts *t, int x, int y);
// 显示时间
void displayTime(time_t t, int x, int y);
// 从EEPROM读取时间
time_t readTime();
// 写入时间到EEPROM
void writeTime(time_t t);
// 比较天数
double compareDay(time_t t1, time_t t2);
// 标准时间转时间戳
time_t toStamp(struct ts *t);
// 显示字符串
void displayStr(int16_t x, int16_t y, int value, bool add = false);
// 显示字符串
void displayStr(int16_t x, int16_t y, const char *str, bool add = false);
//***********************函数定义****************************

// 数组最大大小
#define MAX_BUFFER 32
#define GET_NTP_TIME_ERROR 0
unsigned long lastTime = 0; // 上次同步时间时的时刻
uint16_t interval = 1000;   // 时间同步间隔1000ms
time_t lastStartTime = 0;   // 上次启动时间
time_t startTime = 0;       // 当前启动时间
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

union time_t_value
{
    time_t t;
    byte b[8];
} time_t_tmp;

// NTP服务器，阿里云
char ntpServer[] = {"ntp1.aliyun.com"};
WiFiUDP Udp;

void setup()
{
    Serial.begin(115200);
    // 初始化WIFI
    WiFi.mode(WIFI_STA);
    Udp.begin(8080);
    // WiFi.forceSleepWake(); //  开启WiFi
    // 初始化OLED
    u8g2.begin();
    u8g2.enableUTF8Print();
    // 初始化DS3231
    DS3231_init(DS3231_CONTROL_INTCN);
    Serial.println("初始化...");

    // 连接WiFi同步时间
    if (connectWifi(NULL, NULL))
    {
        // 网络同步时间
        syncTime();
    }

    // 读上次启动时间
    lastStartTime = readTime();
    Serial.printf("上次启动时间：: %lld\n", lastStartTime);
    // 获取当前启动时间
    struct ts curTime;
    DS3231_get(&curTime);
    startTime = toStamp(&curTime);
    Serial.printf("当前时间：: %lld\n", startTime);
    writeTime(startTime);

    u8g2.clearBuffer();
}

void loop()
{
    // 从串口读取数据
    if (Serial.available() > 0)
    {
        // 配网模式
        if ('w' == Serial.read())
        {
            Serial.println("配网模式");
            smartConfig();
            syncTime();
            // 写入当前时间作为上次启动时间
            struct ts curTime;
            DS3231_get(&curTime);
            writeTime(toStamp(&curTime));
            u8g2.clearBuffer();
        }
        // 清空缓冲区
        while (Serial.read() >= 0)
            ;
        return;
    }

    // 刷新时间
    unsigned long nowTime = millis();
    if (nowTime - lastTime > interval)
    {
        Serial.println("显示当前时间");
        // 显示上次启动时间
        displayStr(0, 35, "上次", true);
        displayTime(lastStartTime, 32, 35);
        // 显示当前时间
        struct ts curTime;
        DS3231_get(&curTime);
        displayStr(0, 55, "当前", true);
        displayTime(&curTime, 32, 55);
        // 距离上次启动大于7天
        double differ = compareDay(lastStartTime, startTime);
        if (differ > 7.0)
            displayStr(0, 15, "超过7天未启动打印机", true);
        else
        {
            char tmp[40];
            snprintf(tmp, sizeof(tmp), "距离上次启动%.2lf天", differ);
            displayStr(0, 15, tmp, true);
        }
        lastTime = nowTime;
    }
}

// 标准时间转时间戳
time_t toStamp(struct ts *t)
{
    struct tm _tm;
    char tmp[40];
    _tm.tm_year = t->year - 1900;
    _tm.tm_mon = t->mon - 1;
    _tm.tm_mday = t->mday;
    _tm.tm_hour = t->hour;
    _tm.tm_min = t->min;
    _tm.tm_sec = t->sec;
    _tm.tm_isdst = -1;
    return mktime(&_tm);
}

// 比较天数
double compareDay(time_t t1, time_t t2)
{
    double tmp = t2 - t1;
    Serial.printf("t2-t1=：%lf，相差天数：%lf\n", tmp, tmp / (24 * 3600));
    if (tmp > 0.0)
        return tmp / (24 * 3600); // 秒数转天数
    else
        return 0.0;
}

// 显示时间
void displayTime(struct ts *t, int x, int y)
{
    char tmp[40];
    snprintf(tmp, sizeof(tmp), "%d/%02d/%02d %02d:%02d:%02d", t->year, t->mon, t->mday, t->hour, t->min, t->sec);
    u8g2.setFont(u8g2_font_6x13_tn);
    u8g2.drawStr(x, y, tmp);
    u8g2.sendBuffer();
}

// 显示时间
void displayTime(time_t t, int x, int y)
{
    char tmp[40];
    struct tm *_tm;
    // 转换时间戳
    _tm = localtime(&t);
    snprintf(tmp, sizeof(tmp), "%d/%02d/%02d %02d:%02d:%02d", _tm->tm_year + 1900, _tm->tm_mon + 1, _tm->tm_wday, _tm->tm_hour, _tm->tm_min, _tm->tm_sec);
    u8g2.setFont(u8g2_font_6x13_tn);
    u8g2.drawStr(x, y, tmp);
    u8g2.sendBuffer();
}

// 微信配网
bool smartConfig()
{
    Serial.println("\r\n等待配网...");
    WiFi.beginSmartConfig();

    displayStr(0, 15, "开启配网模式");
    displayStr(0, 28, "请微信关注公众号", true);
    displayStr(0, 41, "“安信可科技”", true);
    displayStr(0, 54, "右下角进行配网", true);

    while (1)
    {
        delay(100);
        Serial.print(".");
        // 等待配网
        if (WiFi.smartConfigDone())
        {
            displayStr(0, 15, "配网成功！");
            displayStr(0, 30, "正在连接WiFi...", true);
            WiFi.waitForConnectResult(); // 等待WiFi连接
            break;
        }
        delay(100);
    }
    return true;
}

// 连接WIFI
bool connectWifi(char *ssid, char *pwd)
{
    WiFi.mode(WIFI_STA);       // 切换为STA模式
    WiFi.setAutoConnect(true); //设置自动连接
    // WiFi.disconnect();   // 断开连接
    if (ssid != NULL && pwd != NULL)
    {
        WiFi.begin(ssid, pwd);
        Serial.printf("\r\nssid:%s\r\n密码：%s\r\n", ssid, pwd);
    }
    else
        WiFi.begin(); // 连接上一次连接成功的wifi
    Serial.println("正在连接WiFi...");
    // 等待连接wifi
    for (int count = 1; WiFi.status() != WL_CONNECTED && count <= 4; count++)
    {
        delay(500); // 延迟500s
        displayStr(0, 15, "正在连接WiFi...");
        displayStr(64, 30, count, true);
        Serial.print(".");
        delay(500); // 延迟500s
    }
    if (WiFi.status() == WL_CONNECTED)
    { // 如果连接成功，输出IP信息
        displayStr(0, 15, "成功连接到WiFi");
        Serial.println("\r\n成功连接到WiFi");
        Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
        Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP()); // 打印IP地址
    }
    else
    {
        displayStr(0, 15, "WiFi未连接");
        Serial.println("\r\nWiFi未连接");
        return false;
    }
    return true;
}

// 连接ntp，同步时间
void syncTime()
{
    // 如果wifi未连接
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }
    // 获取ntp时间
    time_t curTime = getNtpTime();
    if (curTime == GET_NTP_TIME_ERROR)
    {
        Serial.println("获取ntp时间失败");
        displayStr(10, 13, "从ntp同步时间失败");
        delay(1000);
        return;
    }
    struct ts t;
    t.year = year(curTime);
    t.mon = month(curTime);
    t.mday = day(curTime);
    t.hour = hour(curTime);
    t.min = minute(curTime);
    t.sec = second(curTime);
    t.wday = weekday(curTime);
    // 将时间保存到ds3231模块
    DS3231_set(t);
    Serial.println("--------------------------------");
    Serial.println("Set time OK");
    Serial.printf("%d年%d月%d日 %d时%d分%d秒", t.year, t.mon, t.mday, t.hour, t.min, t.sec);
    Serial.println("--------------------------------");

    displayStr(10, 13, "从ntp同步时间成功");
    delay(1000);
}

// 显示字符串
void displayStr(int16_t x, int16_t y, const char *str, bool add)
{
    if (!add)
        u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy13_t_gb2312);
    u8g2.setCursor(x, y);
    u8g2.print(str);
    u8g2.sendBuffer();
}

// 显示字符串
void displayStr(int16_t x, int16_t y, int value, bool add)
{
    if (!add)
        u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy13_t_gb2312);
    u8g2.setCursor(x, y);
    u8g2.print(value);
    u8g2.sendBuffer();
}

// 写入睡眠时间到EEPROM
void writeTime(time_t t)
{
    EEPROM.begin(9); // 申请9个字节的空间（一个字节能存0-255之内的一个数据）
    time_t_tmp.t = t;
    EEPROM.write(0, time_t_tmp.b[0]);
    EEPROM.write(1, time_t_tmp.b[1]);
    EEPROM.write(2, time_t_tmp.b[2]);
    EEPROM.write(3, time_t_tmp.b[3]);
    EEPROM.write(4, time_t_tmp.b[4]);
    EEPROM.write(5, time_t_tmp.b[5]);
    EEPROM.write(6, time_t_tmp.b[6]);
    EEPROM.write(7, time_t_tmp.b[7]);
    EEPROM.commit(); // 保存更改
    Serial.printf("写入数据：t: %ld\n", t);
}

// 从EEPROM读取睡眠时间
time_t readTime()
{
    EEPROM.begin(5);
    time_t_tmp.t = 0LL;
    time_t_tmp.b[0] = EEPROM.read(0);
    time_t_tmp.b[1] = EEPROM.read(1);
    time_t_tmp.b[2] = EEPROM.read(2);
    time_t_tmp.b[3] = EEPROM.read(3);
    time_t_tmp.b[4] = EEPROM.read(4);
    time_t_tmp.b[5] = EEPROM.read(5);
    time_t_tmp.b[6] = EEPROM.read(6);
    time_t_tmp.b[7] = EEPROM.read(7);
    Serial.printf("读取数据：t: %ld\n", time_t_tmp.t);
    return time_t_tmp.t;
}

/*-------- NTP 代码 ----------*/
#define NTP_PACKET_SIZE 48          // NTP时间在消息的前48个字节里
byte packetBuffer[NTP_PACKET_SIZE]; // 输入输出包的缓冲区
time_t getNtpTime()
{
    IPAddress ntpServerIP; // NTP服务器的地址
    while (Udp.parsePacket() > 0)
        ; // 丢弃以前接收的任何数据包
    Serial.println("传输NTP请求");
    // 从池中获取随机服务器
    WiFi.hostByName(ntpServer, ntpServerIP);
    Serial.printf("ntp服务器：%s\r\n", ntpServer);
    Serial.print("ntp服务器ip：");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            Udp.read(packetBuffer, NTP_PACKET_SIZE); // 将数据包读取到缓冲区
            unsigned long secsSince1900;
            // 将从位置40开始的四个字节转换为长整型，只取前32位整数部分
            secsSince1900 = (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            Serial.println(secsSince1900);
            Serial.println(secsSince1900 - 2208988800UL + 8 * SECS_PER_HOUR);
            return secsSince1900 - 2208988800UL + 8 * SECS_PER_HOUR;
        }
    }
    Serial.println("无NTP响应"); // 无NTP响应
    return GET_NTP_TIME_ERROR;   // 如果未得到时间则返回0
}

// 向给定地址的时间服务器发送NTP请求
void sendNTPpacket(IPAddress &address)
{
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011; // LI, Version, Mode
    packetBuffer[1] = 0;          // Stratum, or type of clock
    packetBuffer[2] = 6;          // Polling Interval
    packetBuffer[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    Udp.beginPacket(address, 123); // NTP需要使用的UDP端口号为123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}
