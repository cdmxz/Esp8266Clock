#include <LiquidCrystal_I2C.h>

#define BUTTON_1 8   //增加时间
#define BUTTON_2 9   // 确定倒计时
#define RELAY_PIN 10 // 继电器
#define BACKLIGHT_TIME 50
//#define DEBUG 1

// 倒计时的秒数
long countdown_sec = 0;
long long last_time = 0;
LiquidCrystal_I2C lcd(0x27, 16, 2);
int interval = 100;
int num = 0;
int lcd_backlight_sec = 0;

// 判断按钮是否按下
bool ButtonIsPressed(uint8_t pin, unsigned long ms = 50)
{
  if (digitalRead(pin) == LOW)
  {
    delay(ms);
    if (digitalRead(pin) == LOW)
    {
      lcd_backlight_sec = BACKLIGHT_TIME;
      Serial.println("按钮按下");
      return true;
    }
    else
      return false;
  }
  return false;
}

void show_countdown()
{
  long hour, min, sec;
  char str[20];
  hour = countdown_sec / 3600.0;
  min = (countdown_sec % 3600) / 60.0;
  sec = (countdown_sec % 3600) % 60;
  snprintf(str, sizeof(str), "%02ld:%02ld:%02ld", hour, min, sec);
  lcd.clear();
  lcd.setCursor(4, 1);
  lcd.print(str);
#ifdef DEBUG
  Serial.print("显示倒计时：");
  Serial.println(str);
#endif
}

int addMin(int min)
{
  switch (min)
  {
  case 5:
    return 10;
  case 10:
    return 20;
  case 20:
    return 30;
  case 30:
    return 45;
  case 45:
    return 60;
  case 60:
    return 90;
  case 90:
    return 120;
  case 120:
    return 180;
  case 180:
    return 240;
  case 240:
    return 360;
  case 360:
    return 480;
  case 480:
    return 600;
  case 600:
    return 720;
  default:
    return 5;
  }
}

void set_countdown()
{
  char str[20];
  bool show = true;
  int min = 5;
#ifdef DEBUG
  Serial.println("设置倒计时");
#endif
  while (1)
  {
    delay(200);
    // 按下了按键1，增加倒计时
    if (ButtonIsPressed(BUTTON_1))
    {
      min = addMin(min);
#ifdef DEBUG
      char n[10];
      itoa(min, n, 10);
      Serial.print(n);
      Serial.println("分钟");
#endif
    }
    // 按下了按键2，开始倒计时
    if (ButtonIsPressed(BUTTON_2))
    {
      countdown_sec = (min * 60L);
#ifdef DEBUG
      char str[20];
      snprintf(str, sizeof(str), "%ld", countdown_sec);
      Serial.print(str);
      Serial.println("秒钟");
#endif
      return;
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("countdown:");
    if (show) // 闪烁效果
    {
      lcd.setCursor(10, 0);
      lcd.print(min);
    }
    lcd.setCursor(13, 0);
    lcd.print("min");
    show = !show;
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println("初始化...");
  // 初始化LED
  lcd.begin();
  lcd.backlight();
  //设置引脚
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  set_countdown();
}

void loop()
{
  long long nowTime = millis();
  if (nowTime - last_time < interval)
    return;
  else
  {
    last_time = nowTime;
    num++;
  }
#ifdef DEBUG
  char str[15];
  snprintf(str, sizeof(str), "循环：%ld", countdown_sec);
  Serial.println(str);
#endif
  if (countdown_sec >= 0 && num == 10)
  {
    digitalWrite(RELAY_PIN, LOW);
    show_countdown();
    lcd.setCursor(0, 0);
    lcd.print("ON");
    countdown_sec--;
    num = 0;
  }
  else if (countdown_sec < 0)
  {
    digitalWrite(RELAY_PIN, HIGH);
    lcd.setCursor(0, 0);
    lcd.print("OFF");
  }

  // 背光
  ButtonIsPressed(BUTTON_1);
  ButtonIsPressed(BUTTON_2);
  if (lcd_backlight_sec > 0)
  {
    lcd_backlight_sec--;
    lcd.backlight();
  }
  else if (lcd_backlight_sec == 0)
  {
    lcd.noBacklight();
    lcd_backlight_sec = -1;
#ifdef DEBUG
    Serial.println("关闭背光");
#endif
  }
}
