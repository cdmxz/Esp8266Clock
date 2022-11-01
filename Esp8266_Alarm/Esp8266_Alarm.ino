// OLED 12864 和 DS3231 的SCL引脚都接GPIO4、SDA引脚都接GPIO5
// DS3231 SQW接GPIO13（不使用闹钟可以不接）
// DHT11接GPIO0（和按键3共用一个GPIO）
// 超声波TRIG接GPIO16、ECHO接GPIO15
// 按键的一端：按键1接GPIO14，按键2接GPIO12，按键3接GPIO0
// 按键的另一端：串联10K电阻连接GND

#include <dht11.h>
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

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// 天气结构体
typedef struct
{
  String updateTime; // 天气数据更新时间
  String text;       // 天气状况的文字描述，包括阴晴雨雪等天气状态的描述
  String windDir;    // 实况风向
  String precip;     // 实况风力等级
  String windScale;  // 实况降水量
  String sunrise;    // 日出时间
  String sunset;     // 日落时间
} weather_date;

// 闹钟结构体
typedef struct
{
  uint8_t min;                  // 分钟
  uint8_t hour;                 // 小时
  uint8_t weekDay;              // 每周的日期
  uint8_t flags[5];             // 响铃标志
  bool activate;                // 是否激活闹钟
  const uint8_t alarmTime = 60; // 响铃时长 单位：秒
} alarm_data;

// 倒计时结构体
typedef struct
{
  uint8_t min;
  uint8_t hour;
  uint8_t sec;
  long totalSec;
} countdown_data;

// 45x43
const unsigned char icon[] U8X8_PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x80, 0x0f, 0x00, 0x00, 0x00, 0x80, 0x1f, 0x00, 0x00, 0x00,
    0x80, 0x1f, 0x00, 0x00, 0xc0, 0xe0, 0x7f, 0x30, 0x00, 0x60, 0xf8, 0xff, 0x61, 0x00, 0x30, 0xfc, 0xff, 0xc3, 0x00, 0x10, 0xfe,
    0xff, 0x87, 0x00, 0x18, 0xfe, 0xff, 0x87, 0x01, 0x08, 0xff, 0xff, 0x0f, 0x01, 0x08, 0xff, 0xff, 0x0f, 0x01, 0x88, 0xff, 0xff,
    0x1f, 0x01, 0x88, 0xff, 0xff, 0x1f, 0x01, 0x88, 0xff, 0xff, 0x1f, 0x01, 0x88, 0xff, 0xff, 0x9f, 0x01, 0x90, 0xff, 0xff, 0x9f,
    0x00, 0x80, 0xff, 0xff, 0x1f, 0x00, 0x80, 0xff, 0xff, 0x1f, 0x00, 0x80, 0xff, 0xff, 0x1f, 0x00, 0x80, 0xff, 0xff, 0x1f, 0x00,
    0x80, 0xff, 0xff, 0x1f, 0x00, 0x80, 0xff, 0xff, 0x1f, 0x00, 0xc0, 0xff, 0xff, 0x1f, 0x00, 0xc0, 0xff, 0xff, 0x1f, 0x00, 0xc0,
    0xff, 0xff, 0x3f, 0x00, 0xe0, 0xff, 0xff, 0x3f, 0x00, 0xe0, 0xff, 0xff, 0x7f, 0x00, 0xc0, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0xc0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 显示模式：正常、超声波
#define DISPLAY_MODE_NORMAL 1
#define DISPLAY_MODE_ULTRASONIC 2
// 睡眠模式
#define SLEEP_MODE_OFF_SLEEP 0
#define SLEEP_MODE_ON_SLEEP 1

#define EEPROM_SIZE 13
// 可以使用的引脚 0、2、4、5、12、13、14、15、16
//***********************引脚定义****************************
// DHT11传感器接线引脚
#define DHT11_PIN D3
// 超声波引脚
#define TRIG_PIN D8
#define ECHO_PIN D0
// 第一个按钮 切换超声波/正常显示
#define BUTTON_1_PIN D5
// 第二个按钮 网络调时，显示天气
#define BUTTON_2_PIN D6
// 第三个按钮 设置闹钟，睡眠时间
#define BUTTON_3_PIN D3
#define ALARM_PIN D7
// 蜂鸣器
#define buzzer TRIG_PIN
// #define buzzer 3
//***********************引脚定义****************************

//***********************函数定义****************************
// 获取ntp时间
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
// 通过网络设定时间
void setTime();
// 初始化WIFI
void initWifi();
// 微信配网
bool smartConfig(uint8_t buttonPin);
// 连接WiFi
bool connectWifi(char *ssid, char *pwd, uint8_t buttonPin);
// 获取温度和湿度，并返回组合的字符串
void getTemperAndHumStr(char *outStr, unsigned int strSize);
// 正常显示时间
void displayTime();
// 显示超声波
void displayUltrasonic();
// 获取超声波测量距离
float getDistance();
// 字符串转数字
int strToInt(const char *str, unsigned int strSize, int startIndex, int stopIndex);
// 复制字符串
void copyStr(const char *source, unsigned int sourceSize, int startIndex, int stopIndex, char *dest, unsigned int destSize);
// 中断函数
ICACHE_RAM_ATTR void onChangeDisplayMode();
// 获取星期字符串
String getWeekDayStr(int weekDay);
// 关闭或启动wifi
void wifiSleep(bool sleep);
// 重置WiFi数据
void resetWiFi();
// 解析天气数据
bool parseJson(String json, weather_date *weather);
// 显示天气数据
void displayWeather();
// 显示字符串
void displayStr(int16_t x, int16_t y, const char *str, bool add = false);
void displayStr(int16_t x, int16_t y, int value, bool add = false);
void displayStr(int16_t x, int16_t y, const char *str, const uint8_t *font, bool add);
void displayValue(int16_t x, int16_t y, int val);
int getIndex(const char *str, int ch);
// 判断按钮是否按下
bool ButtonIsPressed(uint8_t pin, unsigned long ms = 50);
// 从EEPROM读取睡眠时间
void readSleepTime();
// 写入睡眠时间到EEPROM
void writeSleepTime();
void addSleepTime();
// 开启或关闭睡眠
void turnSleepOnOrOff();
// 设置闹钟
void setAlarm();
// 激活闹钟
void activateAlarm();
// 读取闹钟数据
bool readAlarmData();
// 写入闹钟数据
void writeAlarmData(uint8_t min, uint8_t hour, uint8_t weekDay, uint8_t *flags, bool activate);
// 闹钟响铃
ICACHE_RAM_ATTR void Ring();
// 蜂鸣器发声
void play(uint8_t pin);
// 设置倒计时
void setCountDown();
// 显示倒计时
void showCountDown();

//***********************函数定义****************************

// 数组最大大小
#define MAX_BUFFER 32
#define GET_NTP_TIME_ERROR 0
// NTP服务器，阿里云
char ntpServer[] = {"ntp1.aliyun.com"};
char url[] = {"https://devapi.qweather.com/v7/weather/now?&key=52ff7d5fe650417291eb69f11a7faef0&location=101280207&gzip=n"};
weather_date weather;          // 天气数据结构体
alarm_data alarmData;          // 闹钟数据结构体
countdown_data countdownData;  // 倒计时数据结构体
unsigned long ringingTime = 0; // 响铃的时刻
bool ringdown = false;         // 是否响铃

WiFiUDP Udp;
bool noChangeDisplayMode = false;          // 是否改变当前的显示模式（中断函数里用到）
uint8_t displayMode = DISPLAY_MODE_NORMAL; // 显示模式
unsigned long lastTime = 0;                // 上次同步时间时的时刻
uint16_t interval = 500;                   // 时间同步间隔500ms
unsigned long button_3_oldTime = 0;        // 按钮_3_上次按下的时间
unsigned int numOfBtnPre_3 = 0;            // 按钮_3_按下次数
unsigned long button_2_oldTime = 0;        // 按钮_2_上次按下的时间
unsigned int numOfBtnPre_2 = 0;            // 按钮_2_按下次数
uint8_t sleepMode = SLEEP_MODE_OFF_SLEEP;  // 睡眠模式
uint8_t sleepTime = 5;                     // 睡眠时间 单位：分钟
bool countDownActivate = false;            // 倒计时是否开启
dht11 DHT11;

void setup()
{
  Serial.begin(115200);
  // 初始化OLED
  u8g2.begin();
  u8g2.enableUTF8Print();
  // 初始化DS3231
  DS3231_init(DS3231_CONTROL_INTCN);
  // 初始化WIFI
  initWifi();
  // 初始化引脚
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(ALARM_PIN, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // 中断
  attachInterrupt(digitalPinToInterrupt(BUTTON_1_PIN), onChangeDisplayMode, FALLING);
  attachInterrupt(digitalPinToInterrupt(ALARM_PIN), Ring, FALLING);

  // 设置显示模式
  displayMode = DISPLAY_MODE_NORMAL;
  wifiSleep(true);
  Serial.println("初始化...");
  readSleepTime();
  if (readAlarmData() && alarmData.activate)
  {
    // Serial.println("启动时注册闹钟");
    activateAlarm();
  }
  u8g2.setContrast(200); // 设置亮度
}

// 保存从串口接收到的时间
// char timeRecv[MAX_BUFFER] = {'\0'};
// unsigned int timeRecvSize = 0;
// void parse_cmd(const char *cmd, unsigned int cmdsize)
// {
//   uint8_t time[8];
//   uint8_t i;
//   uint8_t reg_val;
//   char buff[MAX_BUFFER];
//   struct ts t;

//   // 设定时间
//   if (cmd[0] == 84 && cmdsize == 16)
//   {
//     // T202101311805477
//     t.year = strToInt(cmd, cmdsize, 1, 4);
//     t.mon = strToInt(cmd, cmdsize, 5, 6);
//     t.mday = strToInt(cmd, cmdsize, 7, 8);
//     t.hour = strToInt(cmd, cmdsize, 9, 10);
//     t.min = strToInt(cmd, cmdsize, 11, 12);
//     t.sec = strToInt(cmd, cmdsize, 13, 14);
//     t.wday = cmd[15] - 48;
//     DS3231_set(t);
//     // Serial.println("-----------------------");
//     // Serial.println("Set time OK");
//     // Serial.println(t.year);
//     // Serial.println(t.mon);
//     // Serial.println(t.mday);
//     // Serial.println(t.hour);
//     // Serial.println(t.min);
//     // Serial.println(t.sec);
//     // Serial.println(t.wday);
//     // Serial.println("-----------------------");
//   }
// }

void loop()
{
  // // 通过串口设置时间
  // if (// Serial.available() > 0)
  // {
  //   char in_ch = // Serial.read();
  //   if ((in_ch == 10 || in_ch == 13) && (timeRecvSize > 0))
  //   {
  //     parse_cmd(timeRecv, timeRecvSize);
  //     timeRecvSize = 0;
  //     timeRecv[0] = '\0';
  //   }
  //   else if (in_ch < 48 || in_ch > 122)
  //   {
  //     ; // ignore ~[0-9A-Za-z]
  //   }
  //   else if (timeRecvSize > MAX_BUFFER - 2)
  //   { // 删除过长的行
  //     timeRecvSize = 0;
  //     timeRecv[0] = 0;
  //   }
  //   else if (timeRecvSize < MAX_BUFFER - 2)
  //   {
  //     timeRecv[timeRecvSize] = in_ch;
  //     timeRecv[timeRecvSize + 1] = 0;
  //     timeRecvSize += 1;
  //   }
  //   return;
  // }
  unsigned long nowTime = millis();
  unsigned long timediff = nowTime - ringingTime;
  if (ringdown && ringingTime == 0) // 闹钟响铃，设置响铃的时刻（之所以在这里设置响铃时刻，是因为在中断函数用不了millis()）
  {
    // Serial.printf("赋值ringingTime=%ld，ringdown=%d\n", nowTime, ringdown);
    ringingTime = nowTime;
    return;
  }
  // 闹钟/倒计时 响铃
  if (ringdown && timediff <= (alarmData.alarmTime + 1) * 1000)
  {
    if ((nowTime - lastTime) <= interval)
      return;
    char buf[16];
    snprintf(buf, sizeof(buf), "响铃第%d秒", (int)(timediff / 1000));
    displayStr(32, 20, buf);
    // Serial.println(buf);
    if (lastTime % 2 == 0)
    {
      digitalWrite(buzzer, HIGH);
      u8g2.drawXBMP(46, 30, 37, 37, icon);
      u8g2.sendBuffer();
    }
    else
      digitalWrite(buzzer, LOW);

    // 按下按键1或按键2，退出响铃
    if (ButtonIsPressed(BUTTON_1_PIN) || ButtonIsPressed(BUTTON_2_PIN))
    {
      ringdown = false;
      digitalWrite(buzzer, LOW);
      displayMode = DISPLAY_MODE_NORMAL;
      drawCircle();
      delay(500);
    } // 如果超出响铃时长，也退出响铃
    else if (timediff >= alarmData.alarmTime * 1000)
    {
      digitalWrite(buzzer, LOW);
      displayMode = DISPLAY_MODE_NORMAL;
      ringdown = false;
    }
    lastTime = nowTime;
    return;
  }
  // 倒计时激活
  if (countDownActivate)
  {
    if (ButtonIsPressed(BUTTON_2_PIN))
    {
      countDownActivate = false;
      drawCircle();
      delay(500);
    }
    else if (nowTime - lastTime >= 1000)
    {
      // Serial.println("显示倒计时");
      showCountDown();
      lastTime = nowTime;
    }
    return;
  }

  // 自动睡眠
  if (sleepMode == SLEEP_MODE_ON_SLEEP && nowTime >= sleepTime * 60 * 1000)
  {
    // char temp[MAX_BUFFER];
    // snprintf(temp, sizeof(temp), "自动睡眠时间：%d分", sleepTime);
    // displayStr(45, 37, "睡眠中...");
    // displayStr(0, 60, temp, true);
    u8g2.setContrast(0); // 设置亮度
   // ESP.deepSleep(0);    // 深度睡眠模式
  }

  // 按钮3被按下
  if (nowTime - button_3_oldTime > 100)
  {                                       // 每100ms运行一次
    if (digitalRead(BUTTON_3_PIN) == LOW) // 检测按钮按下(BUTTON_3_PIN已在初始化时拉高，按下时检测是否拉低)
      numOfBtnPre_3++;                    // 按钮按下计数
    button_3_oldTime = millis();
  }
  // 按钮3被按下且当前已松开
  if (numOfBtnPre_3 > 1 && numOfBtnPre_3 < 30 && digitalRead(BUTTON_3_PIN))
  { // 按下1-3秒，切换显示模式
    // Serial.printf("更改睡眠模式，numOfBtnPre=%ld\r\n", numOfBtnPre_3);
    // Serial.printf("按钮3电平：%d\r\n", digitalRead(BUTTON_3_PIN));
    // Serial.println("设置 倒计时/闹钟/睡眠");
    noChangeDisplayMode = true;
    setCountDown();
    noChangeDisplayMode = false;
    numOfBtnPre_3 = 0;
  }
  // 按钮3被按下且未松开
  else if (numOfBtnPre_3 > 30 && !digitalRead(BUTTON_3_PIN) && displayMode == DISPLAY_MODE_NORMAL)
  {
    // 按下大于3秒，重置Wifi
    // Serial.printf("重置Wifi，numOfBtnPre=%ld\r\n", numOfBtnPre_3);
    // Serial.printf("按钮3电平：%d\r\n", digitalRead(BUTTON_3_PIN));
    resetWiFi();
    numOfBtnPre_3 = 0;
  }

  if (nowTime - button_2_oldTime > 100 && displayMode == DISPLAY_MODE_NORMAL)
  {                                       // 每100ms运行一次
    if (digitalRead(BUTTON_2_PIN) == LOW) // 检测按钮按下(BUTTON_2_PIN已在初始化时拉高，按下时检测是否拉低)
      numOfBtnPre_2++;                    // 按钮按下计数
    button_2_oldTime = millis();
  }
  // 按钮被按下且当前已松开
  if (numOfBtnPre_2 > 1 && numOfBtnPre_2 < 30 && digitalRead(BUTTON_2_PIN))
  { // 按下1-3秒，wifi调时
    // Serial.printf("wifi调时，numOfBtnPre=%ld\r\n", numOfBtnPre_2);
    // Serial.printf("按钮2电平：%d\r\n", digitalRead(BUTTON_2_PIN));
    setTime();
    numOfBtnPre_2 = 0;
  }
  // 按钮被按下且未松开
  else if (numOfBtnPre_2 > 30 && !digitalRead(BUTTON_2_PIN))
  {
    // 按下大于3秒，显示天气
    // Serial.printf("显示天气，numOfBtnPre=%ld\r\n", numOfBtnPre_2);
    // Serial.printf("按钮2电平：%d\r\n", digitalRead(BUTTON_2_PIN));
    displayWeather();
    numOfBtnPre_2 = 0;
  }
  // 刷新时间
  if ((nowTime - lastTime > interval) && displayMode == DISPLAY_MODE_NORMAL)
  {
    // Serial.println("显示当前时间");
    displayTime(); // 显示当前时间
    lastTime = nowTime;
  }
  // 超声波
  else if ((nowTime - lastTime > interval) && displayMode == DISPLAY_MODE_ULTRASONIC)
  {
    // Serial.println("显示距离");
    displayUltrasonic();
    lastTime = nowTime;
  }
}

void displayValue(int16_t x, int16_t y, int val)
{
  u8g2.setCursor(x, y);
  u8g2.printf("%02d", val);
}

void drawCircle()
{
  u8g2.drawCircle(4, 58, 4);
  u8g2.sendBuffer();
}

// 设置倒计时的时间
void setCountDown()
{
  bool activate = countDownActivate;
  String startStr = activate ? "ON" : "OFF";
  bool show = true;
  int i = 1;
  int hour = countdownData.hour, min = countdownData.min, sec = countdownData.sec;
  do
  {
    if (digitalRead(BUTTON_1_PIN) == LOW) // 按下按钮1，设置下一项
      i++;
    if (digitalRead(BUTTON_3_PIN) == LOW) // 按下按钮3，退出设置不保存
    {
      drawCircle();
      delay(500);
      setAlarm();
      return;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
    u8g2.drawUTF8(0, 13, "倒计时");
    u8g2.setFont(u8g2_font_logisoso18_tn);
    u8g2.drawStr(87, 45, ":");
    if (i == 1) // 设置秒钟
    {
      if (ButtonIsPressed(BUTTON_2_PIN))
        sec++;
      sec = (sec >= 60) ? 0 : sec;
      if (show)
        displayValue(95, 45, sec);
    }
    else
      displayValue(95, 45, sec);

    u8g2.setFont(u8g2_font_logisoso24_tn);
    if (i == 2)
    { // 设置分钟
      if (ButtonIsPressed(BUTTON_2_PIN))
        min++;
      min = (min >= 60) ? 0 : min;
      if (show)
        displayValue(55, 45, min);
    }
    else
      displayValue(55, 45, min);

    u8g2.drawStr(45, 45, ":");
    if (i == 3)
    {
      // 设置小时
      if (ButtonIsPressed(BUTTON_2_PIN))
        hour++;
      hour = (hour >= 24) ? 0 : hour;
      if (show)
        displayValue(15, 45, hour);
    }
    else
      displayValue(15, 45, hour);

    u8g2.setFont(u8g2_font_9x18B_tf);
    if (i == 4) // 设置是否激活倒计时
    {
      if (ButtonIsPressed(BUTTON_2_PIN))
        activate = !activate;
      startStr = activate ? "ON" : "OFF";
      if (show)
        u8g2.drawStr(0, 60, startStr.c_str());
    }
    else
      u8g2.drawStr(0, 60, startStr.c_str());

    u8g2.sendBuffer();
    show = !show;
    delay(200);
  } while (i <= 4);
  countDownActivate = activate;
  countdownData.sec = sec;
  countdownData.min = min;
  countdownData.hour = hour;
  countdownData.totalSec = hour * 3600L + min * 60L + sec;
  // Serial.printf("countdownData.totalSec=%ld\n", countdownData.totalSec);
  delay(500);
}

// 显示倒计时
void showCountDown()
{
  // Serial.println(countdownData.totalSec);
  countdownData.totalSec--;
  if (countdownData.totalSec < 0)
  {
    countDownActivate = false;
    ringdown = true;
    ringingTime = 0;
    return;
  }
  uint16_t hour = 0, min = 0, sec;
  if (countdownData.totalSec >= 60L)
  {
    min = countdownData.totalSec / 60L;
    sec = countdownData.totalSec % 60L;
    if (min >= 60)
    {
      hour = min / 60;
      min = min % 60;
    }
  }
  else
    sec = (uint16_t)countdownData.totalSec;

  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, 128, 64);
  u8g2.setFont(u8g2_font_logisoso24_tn);
  displayValue(15, 45, hour);
  u8g2.drawStr(45, 45, ":");
  displayValue(55, 45, min);
  u8g2.setFont(u8g2_font_logisoso18_tn);
  u8g2.setCursor(85, 45);
  u8g2.printf(":%02d", sec);
  u8g2.sendBuffer();
}

// 设置闹钟
void setAlarm()
{
  int weekDay = alarmData.weekDay;
  String weekDayStr = (weekDay == 0) ? "每天" : getWeekDayStr(weekDay);
  bool activate = alarmData.activate;
  String startStr = activate ? "ON" : "OFF";
  bool show = true;
  int i = 1;
  int hour = alarmData.hour, min = alarmData.min;
  uint8_t flags[5] = {0, 0, 0, 0, 1};

  do
  {
    if (digitalRead(BUTTON_1_PIN) == LOW) // 按下按钮1，设置下一项
      i++;
    if (digitalRead(BUTTON_3_PIN) == LOW) // 按下按钮3，退出设置不保存
    {
      drawCircle();
      delay(500);
      turnSleepOnOrOff();
      return;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
    u8g2.drawUTF8(0, 13, "闹钟");
    u8g2.setFont(u8g2_font_9x18B_tf);
    if (i == 1) // 设置是否激活闹钟
    {
      if (ButtonIsPressed(BUTTON_2_PIN))
        activate = !activate;
      startStr = activate ? "ON" : "OFF";
      if (show) // 闪烁显示
        u8g2.drawStr(100, 60, startStr.c_str());
    }
    else
      u8g2.drawStr(100, 60, startStr.c_str());
    u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
    if (i == 2) // 响铃日期（一周的日期）
    {
      if (ButtonIsPressed(BUTTON_2_PIN))
        weekDay++;
      weekDay = (weekDay > 7) ? 0 : weekDay;
      weekDayStr = (weekDay == 0) ? "每天" : getWeekDayStr(weekDay);
      if (show)
        u8g2.drawUTF8(90, 15, weekDayStr.c_str());
    }
    else
      u8g2.drawUTF8(90, 13, weekDayStr.c_str());
    u8g2.setFont(u8g2_font_logisoso24_tn);
    u8g2.drawStr(55, 45, ":");
    if (i == 3)
    { // 设置分钟
      if (ButtonIsPressed(BUTTON_2_PIN))
        min++;
      min = (min >= 60) ? 0 : min;
      if (show)
        displayValue(65, 45, min);
    }
    else
      displayValue(65, 45, min);
    u8g2.setFont(u8g2_font_logisoso24_tn);
    if (i == 4)
    {
      // 设置小时
      if (ButtonIsPressed(BUTTON_2_PIN))
        hour++;
      hour = (hour >= 24) ? 0 : hour;
      if (show)
        displayValue(25, 45, hour);
    }
    else
      displayValue(25, 45, hour);

    u8g2.sendBuffer();
    show = !show;
    delay(200);
  } while (i <= 4);

  // 判断数据是否发生更改，未更改则不重新写入数据
  if (min != alarmData.min || hour != alarmData.hour || weekDay != alarmData.weekDay || activate != alarmData.activate)
  {
    flags[3] = (weekDay == 0) ? 1 : 0;
    writeAlarmData(min, hour, weekDay, flags, activate);
  }
  if (!activate) // 如果闹钟关闭
  {
    DS3231_clear_a1f(); // 清除闹钟
    // Serial.printf("关闭闹钟\n");
  }
  else
    activateAlarm(); // 激活闹钟
  delay(500);
}

// 激活闹钟
void activateAlarm()
{
  // Serial.println("激活闹钟");
  DS3231_clear_a1f();
  DS3231_set_a1(0, alarmData.min, alarmData.hour, alarmData.weekDay, alarmData.flags);
  DS3231_set_creg(DS3231_CONTROL_INTCN | DS3231_CONTROL_A1IE);
}

// 写入闹钟数据
void writeAlarmData(uint8_t min, uint8_t hour, uint8_t weekDay, uint8_t *flags, bool activate)
{
  alarmData.min = min;
  alarmData.hour = hour;
  alarmData.weekDay = weekDay;
  alarmData.activate = activate;
  for (int j = 0; j < 5; j++)
    alarmData.flags[j] = flags[j];

  // 写入闹钟时间到EEPROM
  EEPROM.begin(EEPROM_SIZE); // 申请13个字节的空间
  EEPROM.write(4, alarmData.min);
  EEPROM.write(5, alarmData.hour);
  EEPROM.write(6, alarmData.weekDay);
  for (int i = 0; i < 5; i++)
  {
    EEPROM.write(i + 7, alarmData.flags[i]);
  }
  EEPROM.write(12, alarmData.activate);
  EEPROM.commit(); // 保存更改
  // Serial.printf("写入数据\nhour:%d，min: %d，weekDay:%d，activate:%d\n", alarmData.hour, alarmData.min, alarmData.weekDay, alarmData.activate);
  // for (int j = 0; j < 5; j++)
  // Serial.printf("%d,", alarmData.flags[j]);
}

// 读取闹钟数据
bool readAlarmData()
{
  // 将时间写入到EEPROM
  EEPROM.begin(EEPROM_SIZE);
  alarmData.min = EEPROM.read(4);
  alarmData.hour = EEPROM.read(5);
  alarmData.weekDay = EEPROM.read(6);
  for (int i = 0; i < 5; i++)
  {
    alarmData.flags[i] = EEPROM.read(i + 7);
  }
  alarmData.activate = EEPROM.read(12);
  if (alarmData.min > 59 || alarmData.hour > 23 || alarmData.weekDay > 7)
  {
    alarmData.hour = 8;
    alarmData.min = 0;
    alarmData.weekDay = 0;
    alarmData.activate = false;
    // Serial.println("数据有误");
    return false;
  }
  // Serial.printf("读取数据\nhour:%d，min: %d，weekDay:%d，activate:%d\n", alarmData.hour, alarmData.min, alarmData.weekDay, alarmData.activate);
  for (int j = 0; j < 5; j++)
    // Serial.printf("%d,", alarmData.flags[j]);
    return true;
}

// 闹钟响铃中断函数
ICACHE_RAM_ATTR void Ring()
{
  if (ringdown)
    return;
  // T202102080101561
  // Serial.println("闹钟响铃...........");
  ringdown = true;
  ringingTime = 0;
  displayStr(0, 35, "闹钟响了");
  activateAlarm();
}

void addSleepTime()
{
  switch (sleepTime)
  {
  case 1:
    sleepTime = 5;
    break;
  case 5:
    sleepTime = 10;
    break;
  case 10:
    sleepTime = 20;
    break;
  case 20:
    sleepTime = 40;
    break;
  case 40:
    sleepTime = 60;
    break;
  default:
    sleepTime = 1;
    break;
  }
}
// 开启或关闭睡眠
void turnSleepOnOrOff()
{
  bool show = true;     // 为了实现字符闪烁
  int mode = sleepMode; // 先保存当前的睡眠数据
  int time = sleepTime;
  int i = 1;
  String str = (sleepMode == SLEEP_MODE_ON_SLEEP) ? "开启" : "关闭";
  do
  {
    if (ButtonIsPressed(BUTTON_1_PIN))
      i++;

    // 退出，不保存
    if (digitalRead(BUTTON_3_PIN) == LOW) // 按下按钮3，退出设置不保存
    {
      drawCircle();
      delay(500);
      return;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
    u8g2.drawUTF8(0, 15, "睡眠模式：");
    if (i == 1)
    {
      if (ButtonIsPressed(BUTTON_2_PIN)) // 开启或关闭睡眠模式
      {
        if (sleepMode == SLEEP_MODE_OFF_SLEEP)
        {
          sleepMode = SLEEP_MODE_ON_SLEEP;
          str = "开启";
        }
        else
        {
          sleepMode = SLEEP_MODE_OFF_SLEEP;
          str = "关闭";
        }
      }
      if (show)
        u8g2.drawUTF8(65, 15, str.c_str());
    }
    else
      u8g2.drawUTF8(65, 15, str.c_str());
    u8g2.drawUTF8(0, 30, "在");
    if (i == 2)
    {
      if (ButtonIsPressed(BUTTON_2_PIN))
        addSleepTime(); // 增加睡眠时间
      if (show)         // 闪烁效果
      {
        u8g2.setCursor(15, 30);
        u8g2.print(sleepTime);
      }
    }
    else
    {
      u8g2.setCursor(15, 30);
      u8g2.print(sleepTime);
    }
    u8g2.drawUTF8(30, 30, "分钟后睡眠");
    u8g2.setCursor(30, 30);
    u8g2.sendBuffer();
    show = !show;
    delay(500);
  } while (i <= 2);
  // 如果更改数据，就保存
  if (mode != sleepMode || time != sleepTime)
    writeSleepTime();
  delay(500);
}

// 蜂鸣器发声
void play(uint8_t pin)
{
  // digitalWrite(pin, HIGH);
  // delay(100);
  // digitalWrite(pin, LOW);

  // delay(50);
  // digitalWrite(pin, HIGH);
  // delay(100);
  // digitalWrite(pin, LOW);
  // delay(500);
  digitalWrite(pin, HIGH);
  delay(500);
  digitalWrite(pin, LOW);
  delay(50);
  digitalWrite(pin, HIGH);
  delay(500);
  digitalWrite(pin, LOW);
}

// 初始化WIFI
void initWifi()
{
  WiFi.mode(WIFI_STA);
  Udp.begin(8080);
}

// 显示时间
void displayTime()
{
  struct ts curTime;
  char temper[15]; // 储存温度数据
  int16_t totalWidth, x;
  char year[15], hourAndMin[10], sec[5];

  DS3231_get(&curTime); // 获取当前时间
  snprintf(year, sizeof(year), "%d/%02d/%02d", curTime.year, curTime.mon, curTime.mday);
  snprintf(hourAndMin, sizeof(hourAndMin), "%02d:%02d", curTime.hour, curTime.min);
  snprintf(sec, sizeof(sec), ":%02d", curTime.sec);
  getTemperAndHumStr(temper, sizeof(temper));

  // 计算小时分钟部分和秒钟部分字体总宽度
  // 因为小时分钟部分和秒钟部分字体大小不一致，所以分开计算
  // u8g2.setFont(u8g2_font_logisoso24_tn);
  // totalWidth = u8g2.getStrWidth(hourAndMin); // 小时分钟部分宽度
  // u8g2.setFont(u8g2_font_logisoso18_tn);
  // totalWidth += u8g2.getStrWidth(sec); // 秒钟部分宽度

  u8g2.clearBuffer();
  // 打印小时分钟部分
  //x = u8g2.getDisplayWidth() / 2 - totalWidth / 2;
  x = 14;
  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.drawStr(x, 25, hourAndMin);
  // 打印秒钟部分
  x += u8g2.getStrWidth(hourAndMin) + 2;
  u8g2.setFont(u8g2_font_logisoso18_tn);
  u8g2.drawStr(x, 25, sec);
  // 打印年份
  u8g2.setFont(u8g2_font_6x13_tr);
  x = u8g2.getDisplayWidth() / 2 - u8g2.getStrWidth(year) / 2;
  u8g2.drawStr(x, 40, year);
  // 打印温度和湿度
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(0, 60, temper);
  // 打印星期几
  u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
  String weekDay = getWeekDayStr(curTime.wday);
  u8g2.drawUTF8(90, 58, weekDay.c_str());
  u8g2.sendBuffer();
}

String getWeekDayStr(int day)
{
  String week = "星期";
  switch (day)
  {
  case 1:
    return week + "一";
  case 2:
    return week + "二";
  case 3:
    return week + "三";
  case 4:
    return week + "四";
  case 5:
    return week + "五";
  case 6:
    return week + "六";
  case 7:
    return week + "日";
  default:
    return week + "UN";
  }
}

// 连接ntp，设置时间
void setTime()
{
  wifiSleep(false); // 开启wifi连接
  // 如果wifi未连接
  if (WiFi.status() != WL_CONNECTED)
  {
    if (!connectWifi(NULL, NULL, BUTTON_2_PIN)) // 连接wifi
    {
      wifiSleep(true); // 关闭WiFi连接
      return;
    }
    // Serial.println("连接wifi");
  }
  // 获取ntp时间
  time_t curTime = getNtpTime();
  wifiSleep(true); // 关闭WiFi连接
  if (curTime == GET_NTP_TIME_ERROR)
  {
    // Serial.println("获取ntp时间失败");
    displayStr(10, 15, "从ntp同步时间失败");
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
  t.wday = weekday(curTime) - 1;
  t.wday = (t.wday == 0) ? 7 : t.wday;
  // 将时间保存到ds3231模块
  DS3231_set(t);
  char buf[MAX_BUFFER];
  // Serial.println("--------------------------------");
  // Serial.println("Set time OK");
  Serial.printf("%d年%d月%d日 %d时%d分%d秒 星期%d", t.year, t.mon, t.mday, t.hour, t.min, t.sec, t.wday);
  // Serial.println("--------------------------------");

  snprintf(buf, sizeof(buf), "%d/%02d/%02d %02d:%02d:%02d", t.year, t.mon, t.mday, t.hour, t.min, t.sec, t.wday);
  displayStr(10, 15, "从ntp同步时间成功");
  displayStr(0, 35, buf, true);
  displayStr(0, 48, getWeekDayStr(t.wday).c_str(), true);
  delay(3000);
}

// 连接WIFI
bool connectWifi(char *ssid, char *pwd, uint8_t buttonPin)
{
  WiFi.mode(WIFI_STA);       // 切换为STA模式
  WiFi.setAutoConnect(true); //设置自动连接
  // WiFi.disconnect();   // 断开连接
  if (ssid != NULL && pwd != NULL)
  {
    WiFi.begin(ssid, pwd);
    // Serial.printf("\r\nssid:%s\r\n密码：%s\r\n", ssid, pwd);
  }
  else
    WiFi.begin(); // 连接上一次连接成功的wifi
  // Serial.println("正在连接WiFi...");
  for (int count = 0; WiFi.status() != WL_CONNECTED && count <= 10; count++)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    // 检测是否按下按钮，按下按钮就取消连接
    if (ButtonIsPressed(buttonPin))
    {
      drawCircle();
      delay(500);
      return false;
    }
    displayStr(0, 15, "正在连接WiFi...");
    displayStr(64, 30, count, true);
    if (count >= 10) // 如果10秒内没有连上，就开启配网
    {
      displayStr(0, 15, "未能连接到WiFi");
      delay(1000);
      if (!smartConfig(buttonPin)) // 微信配网
        return false;
    }
    // Serial.print(".");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED)
  { // 如果连接成功，输出IP信息
    u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
    u8g2.clearBuffer();
    u8g2.drawUTF8(0, 13, "成功连接到WiFi");
    u8g2.setCursor(0, 26);
    u8g2.printf("SSID:%s", WiFi.SSID().c_str());
    u8g2.setCursor(0, 39);
    u8g2.printf("PSW:%s", WiFi.psk().c_str());
    const char *ip = "IP:";
    u8g2.drawUTF8(0, 52, ip);
    u8g2.setCursor(u8g2.getStrWidth(ip), 52);
    u8g2.print(WiFi.localIP());
    u8g2.sendBuffer();

    // Serial.println("\r\n成功连接到WiFi");
    // Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
    // Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
    // Serial.print("IP address: ");
    // Serial.println(WiFi.localIP()); // 打印IP地址
    delay(3000);
  }
  else
  {
    displayStr(0, 15, "WiFi未连接");
    // Serial.println("\r\nWiFi未连接");
    delay(1000);
    return false;
  }
  return true;
}

// 微信配网
bool smartConfig(uint8_t buttonPin)
{
  // Serial.println("\r\n等待配网...");
  WiFi.beginSmartConfig();

  displayStr(0, 13, "开启配网模式");
  displayStr(0, 26, "请微信关注公众号", true);
  displayStr(0, 39, "“安信可科技”", true);
  displayStr(0, 52, "右下角进行配网", true);

  while (1)
  {
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    // Serial.print(".");
    // 等待配网的同时，按下了按键，则取消配网
    if (ButtonIsPressed(buttonPin))
    {
      drawCircle();
      delay(500);
      return false;
    }
    if (WiFi.smartConfigDone())
    {
      displayStr(0, 15, "配网成功！");
      displayStr(0, 30, "正在连接WiFi...", true);
      WiFi.waitForConnectResult(); // 等待WiFi连接
      break;
    }
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  digitalWrite(LED_BUILTIN, HIGH);
  return true;
}

// 调制解调器睡眠（避免耗电）
void wifiSleep(bool sleep)
{
  if (sleep)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    WiFi.forceSleepBegin(); // 关闭WiFi
    // Serial.println("关闭wifi");
  }
  else
  {
    WiFi.forceSleepWake(); //  开启WiFi
    // Serial.println("开启wifi");
  }
}

// 重置WiFi数据
void resetWiFi()
{
  wifiSleep(false);
  WiFi.disconnect(); // 断开连接并清除保存的最近一次连接的wifi名称和密码
  WiFi.begin();
  displayStr(0, 15, "已清除最近一次连接的");
  displayStr(0, 30, "WiFi数据", true);
  delay(2000);
  wifiSleep(true);
}

// 显示超声波测量距离
void displayUltrasonic()
{
  float dis = getDistance();
  char str[MAX_BUFFER];
  dtostrf(dis, 1, 2, str);

  // Serial.printf("超声波距离：%s", str);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
  u8g2.drawUTF8(0, 40, "距离：");
  u8g2.setFont(u8g2_font_7x14_tr);
  u8g2.drawStr(38, 40, str);                              // 打印数字
  u8g2.drawStr(38 + u8g2.getStrWidth(str) + 5, 40, "CM"); // 打印最后的CM符号
  u8g2.sendBuffer();
}

// 获取超声波测量的距离
float getDistance()
{
  digitalWrite(TRIG_PIN, LOW); 
  delayMicroseconds(8);
  digitalWrite(TRIG_PIN, HIGH);// 发送一个40us的高脉冲去触发TrigPin
  delayMicroseconds(40);
  digitalWrite(TRIG_PIN, LOW);
  // 算出距离
  return pulseIn(ECHO_PIN, HIGH) / 58.0;
}

// 获取温度和湿度字符串
void getTemperAndHumStr(char *outStr, unsigned int strSize)
{
  // 温度使用DS3231测量，湿度使用DHT11测量
  int humidity = 0; // 湿度
  float temper;     // 温度
  char temp[5];
  // 获取DTH11测量数据
  int chk = DHT11.read(DHT11_PIN);
  if (chk == DHTLIB_ERROR_TIMEOUT)
    Serial.println("DTH11 超时");
  else if (chk == DHTLIB_ERROR_CHECKSUM)
    Serial.println("DTH11 校验错误");
  else
    humidity = (int)DHT11.humidity;

  temper = DS3231_get_treg(); // 获取DS3231测量的温度
  dtostrf(temper, 1, 1, temp);
  snprintf(outStr, strSize, "%sC/%d%%", temp, humidity);
}

// 显示天气
void displayWeather()
{
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  char buf[MAX_BUFFER];
  wifiSleep(false); // 开启wifi连接
  // 如果wifi未连接
  if (WiFi.status() != WL_CONNECTED)
  {
    if (!connectWifi(NULL, NULL, BUTTON_2_PIN)) // 连接wifi
    {
      wifiSleep(true); // 关闭WiFi连接
      return;
    }
    // Serial.println("连接wifi");
  }
  https.setTimeout(5000);
  https.begin(*client, url);
  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    // Serial.printf("请求失败，http状态码：%d\n", httpCode);
    displayStr(0, 15, "请求失败!");
    snprintf(buf, sizeof(buf), "http状态码：%d", httpCode);
    displayStr(0, 28, buf, true);
    displayStr(0, 41, "错误内容：", true);
    displayStr(0, 54, https.errorToString(httpCode).c_str(), true);
    delay(3000);
    return;
  }
  //Serial.println("请求成功");
  String json = https.getString();
  wifiSleep(true); // 关闭WiFi连接
  https.end();
  if (!parseJson(json, &weather))
    return;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
  u8g2.setCursor(0, 15);
  u8g2.printf("更新时间:%s", weather.updateTime.c_str());
  u8g2.setCursor(0, 30);
  u8g2.printf("天气状况:%s", weather.text.c_str());
  snprintf(buf, sizeof(buf), "风向:%s 等级:%s级", weather.windDir.c_str(), weather.windScale.c_str());
  u8g2.setCursor(0, 45);
  u8g2.printf(buf);
  u8g2.setCursor(0, 60);
  u8g2.printf("降水量:%sMM", weather.precip.c_str());
  // u8g2.setCursor(0, 51);
  // u8g2.printf("日出:%s", weather.sunrise.c_str());
  // u8g2.setCursor(0, 64);
  // u8g2.printf("日落:%s", weather.sunset.c_str());
  u8g2.sendBuffer();
  for (int num = 1; num <= 100; num++)
  {
    if (ButtonIsPressed(BUTTON_3_PIN))
    {
      drawCircle();
      delay(500);
      return;
    }
    delay(100);
  }
}

// 解析天气的Json数据
bool parseJson(String json, weather_date *weather)
{
  StaticJsonDocument<800> doc;
  DeserializationError error = deserializeJson(doc, json); // 反序列化json数据
  if (error)
  {
    Serial.printf("deserializeJson() failed:%s\n ", error.f_str());
    return false;
  }
  Serial.printf("json数据：%s\n", json.c_str());
  int code = doc["code"];
  if (code != 200)
  {
    char buf[MAX_BUFFER];
    snprintf(buf, sizeof(buf), "返回的错误代码：%d", code);
    displayStr(0, 15, "请求失败!");
    displayStr(0, 30, "buf", true);
    delay(3000);
    return false;
  }

  JsonObject now = doc["now"];
  const char *updateTime = doc["updateTime"]; // 天气数据更新时间
  const char *text = now["text"];             // 天气状况的文字描述，包括阴晴雨雪等天气状态的描述
  const char *windDir = now["windDir"];       // 实况风向
  const char *windScale = now["windScale"];   // 实况风力等级
  float precip = now["precip"];               // 实况降雨量
  char str[MAX_BUFFER];
  int startIndex = getIndex(updateTime, 'T') + 1;
  int stopIndex = getIndex(updateTime, '+') - 1;
  copyStr(updateTime, strlen(updateTime) + 1, startIndex, stopIndex, str, sizeof(str));
  // const char *sunset = now["sunset"];   // 日落时间
  weather->updateTime = String(str);
  weather->text = String(text);
  weather->windDir = String(windDir);
  weather->windScale = String(windScale);
  weather->precip = String(precip);
  // Serial.printf("降雨量:%f", precip);
  // weather->sunrise = String(sunrise);
  // weather->sunset = String(sunset);
  return true;
}

// 判断按钮是否按下
bool ButtonIsPressed(uint8_t pin, unsigned long ms)
{
  if (digitalRead(pin) == LOW)
  {
    delay(ms);
    if (digitalRead(pin) == LOW)
      return true;
    else
      return false;
  }
  return false;
}

// 显示字符串
void displayStr(int16_t x, int16_t y, const char *str, bool add)
{
  if (!add)
    u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
  u8g2.setCursor(x, y);
  u8g2.print(str);
  u8g2.sendBuffer();
}

// 显示字符串
void displayStr(int16_t x, int16_t y, int value, bool add)
{
  if (!add)
    u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy13_t_gb2312a);
  u8g2.setCursor(x, y);
  u8g2.print(value);
  u8g2.sendBuffer();
}

void displayStr(int16_t x, int16_t y, const char *str, const uint8_t *font, bool add)
{
  if (!add)
    u8g2.clearBuffer();
  u8g2.setFont(font);
  u8g2.setCursor(x, y);
  u8g2.print(str);
  u8g2.sendBuffer();
}

// 改变显示模式
ICACHE_RAM_ATTR void onChangeDisplayMode()
{
  if (noChangeDisplayMode)
  {
    // Serial.println("不改变显示模式");
    return;
  }
  if (displayMode == DISPLAY_MODE_NORMAL)
    displayMode = DISPLAY_MODE_ULTRASONIC;
  else
    displayMode = DISPLAY_MODE_NORMAL;
  drawCircle();
  delayMicroseconds(100);
  // Serial.print("改变显示模式（中断模式）：");
  // Serial.println(displayMode);
}

// 复制字符串
void copyStr(const char *source, unsigned int sourceSize, int startIndex, int stopIndex, char *dest, unsigned int destSize)
{
  unsigned int j = 0;
  for (unsigned int i = startIndex; i < sourceSize && i <= stopIndex && j < destSize; i++, j++)
  {
    dest[j] = source[i];
  }
  dest[j] = '\0';
}

// 字符串转数字
int strToInt(const char *str, unsigned int strSize, int startIndex, int stopIndex)
{
  char buf[5];
  int number;
  copyStr(str, strSize, startIndex, stopIndex, buf, sizeof(buf));
  sscanf(buf, "%d", &number);
  return number;
}

int getIndex(const char *str, int ch)
{
  int strSize = strlen(str);
  int i = 0;
  for (; i <= strSize; i++)
  {
    if (str[i] == ch)
      return i;
  }
  return -1;
}

// 写入睡眠时间到EEPROM
void writeSleepTime()
{
  EEPROM.begin(EEPROM_SIZE); // 申请13个字节的空间
  EEPROM.write(0, sleepMode);
  EEPROM.write(1, sleepTime);
  EEPROM.commit(); // 保存更改
  // Serial.printf("写入数据\nsleepMode: %d，sleepTime:%d\n", sleepMode, sleepTime);
}

// 从EEPROM读取睡眠时间
void readSleepTime()
{
  EEPROM.begin(EEPROM_SIZE);
  int mode = EEPROM.read(0);
  if (mode != SLEEP_MODE_ON_SLEEP) // 睡眠模式
    sleepMode = SLEEP_MODE_OFF_SLEEP;
  else
    sleepMode = SLEEP_MODE_ON_SLEEP;
  int time = EEPROM.read(1); // 睡眠时间
  if (time <= 0 || time > 60)
    sleepTime = 5;
  else
    sleepTime = time;
  // Serial.printf("读取数据\nsleepMode: %d，sleepTime:%d\n", sleepMode, sleepTime);
}

/*-------- NTP 代码 ----------*/
#define NTP_PACKET_SIZE 48          // NTP时间在消息的前48个字节里
byte packetBuffer[NTP_PACKET_SIZE]; // 输入输出包的缓冲区
time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP服务器的地址
  while (Udp.parsePacket() > 0)
    ; // 丢弃以前接收的任何数据包
  // Serial.println("传输NTP请求");
  // 从池中获取随机服务器
  WiFi.hostByName(ntpServer, ntpServerIP);
  // Serial.printf("ntp服务器：%s\r\n", ntpServer);
  // Serial.print("ntp服务器ip：");
  // Serial.println(ntpServerIP);
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
      // Serial.println(secsSince1900);
      // Serial.println(secsSince1900 - 2208988800UL + 8 * SECS_PER_HOUR);
      return secsSince1900 - 2208988800UL + 8 * SECS_PER_HOUR;
    }
  }
  // Serial.println("无NTP响应"); // 无NTP响应
  return GET_NTP_TIME_ERROR; // 如果未得到时间则返回0
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
  Udp.beginPacket(address, 123); //NTP需要使用的UDP端口号为123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
