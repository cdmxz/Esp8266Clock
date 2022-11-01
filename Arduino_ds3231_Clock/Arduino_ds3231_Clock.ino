
#include <Wire.h>
#include <ds3231.h>
#include <TimeLib.h>
#include <dht11.h>
#include <U8g2lib.h> //加载显示库文件
U8G2_SH1106_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// 数组最大大小
#define MAX_BUFFER 32
// DHT11传感器接线引脚
#define DHT11_PIN 4
// 显示模式：正常、超声波
#define DISPLAY_MODE_NORMAL 1
#define DISPLAY_MODE_ULTRASONIC 2
// 超声波引脚
#define TRIG_PIN 5
#define ECHO_PIN 6
#define BUTTON_PIN 2

// 大小：48x16 内容：距离
const uint8_t Distance[] U8X8_PROGMEM =
    {0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0xbe, 0x7f, 0x80, 0x00, 0x00, 0x00, 0xa2, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xa2, 0x00, 0x00, 0x00, 0x00,
     0x00, 0xa2, 0x00, 0x28, 0x0a, 0x00, 0x00, 0xbe, 0x3f, 0xc8, 0x09, 0x00, 0x00, 0x88, 0x20, 0x28, 0x0a, 0x00, 0x00, 0x88, 0x20, 0xf8, 0x0f,
     0x00, 0x00, 0xba, 0x20, 0x80, 0x00, 0x00, 0x00, 0x8a, 0x20, 0xfe, 0x3f, 0x0c, 0x00, 0x8a, 0x3f, 0x42, 0x20, 0x0c, 0x00, 0x8a, 0x00, 0x22,
     0x22, 0x00, 0x00, 0xba, 0x00, 0xf2, 0x27, 0x0c, 0x00, 0x87, 0x00, 0x22, 0x24, 0x0c, 0x00, 0x80, 0x7f, 0x02, 0x28, 0x00, 0x00, 0x00, 0x00,
     0x02, 0x10, 0x00, 0x00};
// 大小：32x16 内容：星期
const uint8_t WeekStr[] U8X8_PROGMEM =
    {0x00, 0x00, 0x44, 0x00, 0xf8, 0x0f, 0x44, 0x3e, 0x08, 0x08, 0xfe, 0x22, 0xf8, 0x0f, 0x44, 0x22, 0x08, 0x08, 0x44, 0x22, 0xf8, 0x0f, 0x7c,
     0x3e, 0x80, 0x00, 0x44, 0x22, 0x88, 0x00, 0x44, 0x22, 0xf8, 0x1f, 0x7c, 0x22, 0x84, 0x00, 0x44, 0x3e, 0x82, 0x00, 0x44, 0x22, 0xf8, 0x0f,
     0xff, 0x22, 0x80, 0x00, 0x20, 0x21, 0x80, 0x00, 0x44, 0x21, 0xfe, 0x3f, 0x82, 0x28, 0x00, 0x00, 0x41, 0x10};
// 每个字的大小都为16x16
const uint8_t WeekDay[8][33] U8X8_PROGMEM =
    {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // 一
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // 二
        {0x00, 0x00, 0x00, 0x00, 0xfe, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00},  // 三
        {0x00, 0x00, 0x00, 0x00, 0xfe, 0x3f, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x12, 0x22, 0x12, 0x3c, 0x0a, 0x20, 0x06, 0x20, 0x02, 0x20, 0xfe, 0x3f, 0x02, 0x20, 0x00, 0x00},  // 四
        {0x00, 0x00, 0xfe, 0x3f, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0xfc, 0x0f, 0x20, 0x08, 0x20, 0x08, 0x20, 0x08, 0x20, 0x08, 0x10, 0x08, 0x10, 0x08, 0x10, 0x08, 0xff, 0x7f, 0x00, 0x00},  // 五
        {0x40, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x20, 0x02, 0x20, 0x04, 0x10, 0x08, 0x10, 0x10, 0x08, 0x10, 0x04, 0x20, 0x02, 0x20, 0x00, 0x00},  // 六
        {0x00, 0x00, 0xf8, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0xf8, 0x0f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0xf8, 0x0f, 0x08, 0x08}}; // 七

// width: 16, height: 16
const uint8_t WifiIcon[] U8X8_PROGMEM = {0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x1c, 0x3c, 0x06, 0x60, 0xe3, 0x47, 0x38, 0x1c, 0x0c, 0x10, 0xc0, 0x03, 0x60, 0x06, 0x00, 0x00, 0x80, 0x01, 0xc0, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00};

// 显示模式
uint16_t displayMode = DISPLAY_MODE_NORMAL;
uint16_t displayWidth;
// 保存从串口接收到的时间
char timeRecv[MAX_BUFFER] = {'\0'};
unsigned int timeRecvSize = 0;
unsigned long lastTime = 0; // 上次同步时间时的时刻
uint16_t interval = 1000;   // 时间同步间隔1000ms
dht11 DHT11;

// 解析命令
void parse_cmd(const char *cmd, unsigned int cmdsize);
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
void ChangeDisplayMode();

void setup()
{
  Serial.begin(9600);
  Wire.begin();
  // 初始化DS3231
  DS3231_init(DS3231_CONTROL_INTCN);
  // 初始化OLED
  u8g2.begin();
  u8g2.enableUTF8Print();
  // 设定时间
  // parse_cmd("T202001312127307", 16);
  // 设定引脚
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(DHT11_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 中断
  attachInterrupt(0, ChangeDisplayMode, FALLING);
  displayMode = DISPLAY_MODE_NORMAL;
}

void loop()
{
  unsigned long nowTime = millis();

  // 通过串口设置时间
  if (Serial.available() > 0)
  {
    char in_ch = Serial.read();
    if ((in_ch == 10 || in_ch == 13) && (timeRecvSize > 0))
    {
      parse_cmd(timeRecv, timeRecvSize);
      timeRecvSize = 0;
      timeRecv[0] = '\0';
    }
    else if (in_ch < 48 || in_ch > 122)
    {
      ; // ignore ~[0-9A-Za-z]
    }
    else if (timeRecvSize > MAX_BUFFER - 2)
    { // 删除过长的行
      timeRecvSize = 0;
      timeRecv[0] = 0;
    }
    else if (timeRecvSize < MAX_BUFFER - 2)
    {
      timeRecv[timeRecvSize] = in_ch;
      timeRecv[timeRecvSize + 1] = 0;
      timeRecvSize += 1;
    }
    return;
  }
  // 每隔一秒刷新一次时间

  // Serial.print("时间差");
  // Serial.println((nowTime - lastTime));
  // Serial.print("显示模式");
  // Serial.println(displayMode);
  if ((nowTime - lastTime > interval) && displayMode == DISPLAY_MODE_NORMAL)
  {
    Serial.println("显示当前时间");
    displayTime(); // 显示当前时间
    lastTime = nowTime;
  }
  else if ((nowTime - lastTime > interval) && displayMode == DISPLAY_MODE_ULTRASONIC)
  {
    Serial.println("显示距离");
    displayUltrasonic();
    lastTime = nowTime;
  }
}

// 正常显示时间
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

  u8g2.firstPage();
  do
  {
    // 居中打印
    // 打印小时分钟部分
    //x = u8g2.getDisplayWidth() / 2 - totalWidth / 2;
    x = 14;
    u8g2.setFont(u8g2_font_logisoso24_tn);
    u8g2.drawStr(x, 25, hourAndMin);
    //Serial.println(hourAndMin);
    // 打印秒钟部分
    x += u8g2.getStrWidth(hourAndMin) + 2;
    u8g2.setFont(u8g2_font_logisoso18_tn);
    u8g2.drawStr(x, 25, sec);
    // 打印年份
    u8g2.setFont(u8g2_font_6x13_tn);
    x = u8g2.getDisplayWidth() / 2 - u8g2.getStrWidth(year) / 2;
    u8g2.drawStr(x, 40, year);
    // 打印温度和湿度
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(0, 60, temper);
    // 打印星期几
    u8g2.drawXBMP(82, 48, 32, 16, WeekStr);
    u8g2.drawXBMP(82 + 32 - 1, 48, 16, 16, WeekDay[curTime.wday - 1]);
    // 显示Wifi图标
    u8g2.drawXBMP(0, 30, 16, 16, WifiIcon);
  } while (u8g2.nextPage());
}

// 显示超声波测量距离
void displayUltrasonic()
{
  float dis = getDistance();
  char str[MAX_BUFFER];
  dtostrf(dis, 1, 2, str);

  u8g2.firstPage();
  do
  {
    u8g2.setFont(u8g2_font_7x14_tr);
    u8g2.drawXBMP(0, 25, 48, 16, Distance);
    u8g2.drawStr(38, 40, str);
    u8g2.drawStr(38 + u8g2.getStrWidth(str) + 5, 40, "CM");
  } while (u8g2.nextPage());
}

// 获取距离
float getDistance()
{
  digitalWrite(TRIG_PIN, LOW); // 发送一个10ms的高脉冲去触发TrigPin
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
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
    Serial.println("DTH11 timeOut");
  else if (chk == DHTLIB_ERROR_CHECKSUM)
    Serial.println("DTH11 校验错误");
  else
    humidity = (int)DHT11.humidity;

  temper = DS3231_get_treg(); // 获取DS3231测量的温度
  dtostrf(temper, 1, 1, temp);
  snprintf(outStr, strSize, "%sC/%d%%", temp, humidity);
}

// 中断函数
void ChangeDisplayMode()
{
  if (displayMode == DISPLAY_MODE_NORMAL)
    displayMode = DISPLAY_MODE_ULTRASONIC;
  else
    displayMode = DISPLAY_MODE_NORMAL;
  Serial.print("中断模式：");
  Serial.println(displayMode);
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
  // Serial.println("-----------------------");
  // Serial.println(str);
  // Serial.println(strSize);
  // Serial.println(startIndex);
  // Serial.println(stopIndex);
  // Serial.println(buf);
  // Serial.println(sizeof(buf));
  //  Serial.println("-----------------------");
  return number;
}

void parse_cmd(const char *cmd, unsigned int cmdsize)
{
  uint8_t time[8];
  uint8_t i;
  uint8_t reg_val;
  char buff[MAX_BUFFER];
  struct ts t;

  // 设定时间
  if (cmd[0] == 84 && cmdsize == 16)
  {
    // T202101311805477
    t.year = strToInt(cmd, cmdsize, 1, 4);
    t.mon = strToInt(cmd, cmdsize, 5, 6);
    t.mday = strToInt(cmd, cmdsize, 7, 8);
    t.hour = strToInt(cmd, cmdsize, 9, 10);
    t.min = strToInt(cmd, cmdsize, 11, 12);
    t.sec = strToInt(cmd, cmdsize, 13, 14);
    t.wday = cmd[15] - 48;
    DS3231_set(t);
    Serial.println("-----------------------");
    Serial.println("Set time OK");
    Serial.println(t.year);
    Serial.println(t.mon);
    Serial.println(t.mday);
    Serial.println(t.hour);
    Serial.println(t.min);
    Serial.println(t.sec);
    Serial.println(t.wday);
    Serial.println("-----------------------");
  }
  // if (cmd[0] == 84 && cmdsize == 16)
  // {
  //   //T355720619112011
  //   t.sec = inp2toi(cmd, 1);
  //   t.min = inp2toi(cmd, 3);
  //   t.hour = inp2toi(cmd, 5);
  //   t.wday = cmd[7] - 48;
  //   t.mday = inp2toi(cmd, 8);
  //   t.mon = inp2toi(cmd, 10);
  //   t.year = inp2toi(cmd, 12) * 100 + inp2toi(cmd, 14);
  //   DS3231_set(t);
  //   Serial.println("OK");
  // }
  // else if (cmd[0] == 49 && cmdsize == 1)
  // { // "1" get alarm 1
  //   DS3231_get_a1(&buff[0], 59);
  //   Serial.println(buff);
  // }
  // else if (cmd[0] == 50 && cmdsize == 1)
  // { // "2" get alarm 1
  //   DS3231_get_a2(&buff[0], 59);
  //   Serial.println(buff);
  // }
  // else if (cmd[0] == 51 && cmdsize == 1)
  // { // "3" get aging register
  //   Serial.print("aging reg is ");
  //   Serial.println(DS3231_get_aging(), DEC);
  // }
  // else if (cmd[0] == 65 && cmdsize == 9)
  // { // "A" set alarm 1
  //   DS3231_set_creg(DS3231_CONTROL_INTCN | DS3231_CONTROL_A1IE);
  //   //ASSMMHHDD
  //   for (i = 0; i < 4; i++)
  //   {
  //     time[i] = (cmd[2 * i + 1] - 48) * 10 + cmd[2 * i + 2] - 48; // ss, mm, hh, dd
  //   }
  //   uint8_t flags[5] = {0, 0, 0, 0, 0};
  //   DS3231_set_a1(time[0], time[1], time[2], time[3], flags);
  //   DS3231_get_a1(&buff[0], 59);
  //   Serial.println(buff);
  // }
  // else if (cmd[0] == 66 && cmdsize == 7)
  // { // "B" Set Alarm 2
  //   DS3231_set_creg(DS3231_CONTROL_INTCN | DS3231_CONTROL_A2IE);
  //   //BMMHHDD
  //   for (i = 0; i < 4; i++)
  //   {
  //     time[i] = (cmd[2 * i + 1] - 48) * 10 + cmd[2 * i + 2] - 48; // mm, hh, dd
  //   }
  //   uint8_t flags[5] = {0, 0, 0, 0};
  //   DS3231_set_a2(time[0], time[1], time[2], flags);
  //   DS3231_get_a2(&buff[0], 59);
  //   Serial.println(buff);
  // }
  // else if (cmd[0] == 67 && cmdsize == 1)
  // { // "C" - get temperature register
  //   Serial.print("temperature reg is ");
  //   Serial.println(DS3231_get_treg(), DEC);
  // }
  // else if (cmd[0] == 68 && cmdsize == 1)
  // { // "D" - reset status register alarm flags
  //   reg_val = DS3231_get_sreg();
  //   reg_val &= B11111100;
  //   DS3231_set_sreg(reg_val);
  // }
  // else if (cmd[0] == 70 && cmdsize == 1)
  // { // "F" - custom fct
  //   reg_val = DS3231_get_addr(0x5);
  //   Serial.print("orig ");
  //   Serial.print(reg_val, DEC);
  //   Serial.print("month is ");
  //   Serial.println(bcdtodec(reg_val & 0x1F), DEC);
  // }
  // else if (cmd[0] == 71 && cmdsize == 1)
  // { // "G" - set aging status register
  //   DS3231_set_aging(0);
  // }
  // else if (cmd[0] == 83 && cmdsize == 1)
  // { // "S" - get status register
  //   Serial.print("status reg is ");
  //   Serial.println(DS3231_get_sreg(), DEC);
  // }
  // else
  // {
  //   Serial.print("unknown command prefix ");
  //   Serial.println(cmd[0]);
  //   Serial.println(cmd[0], DEC);
  // }
}
