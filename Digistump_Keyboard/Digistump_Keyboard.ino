#include <DigiKeyboard.h>

#define REBOOT_PIN 0
#define SHUTDOWN_PIN 1
#define LOGOFF_PIN 2 // pin2默认拉高

void setup()
{
    // 防止出现设备描述符请求失败 
    DigiKeyboard.sendKeyStroke(0);
    // 初始化引脚
    pinMode(REBOOT_PIN, INPUT);
    pinMode(SHUTDOWN_PIN, INPUT);
    pinMode(LOGOFF_PIN, INPUT_PULLUP);
}

void Reboot()
{
    DigiKeyboard.sendKeyStroke(KEY_R, MOD_GUI_LEFT);
    DigiKeyboard.delay(80);
    DigiKeyboard.println("shutdown -r -t 00");
}

void Shutdown()
{
    DigiKeyboard.sendKeyStroke(KEY_R, MOD_GUI_LEFT);
    DigiKeyboard.delay(80);
    DigiKeyboard.println("shutdown -s -t 00");
}

void Logoff()
{
    DigiKeyboard.sendKeyStroke(KEY_R, MOD_GUI_LEFT);
    DigiKeyboard.delay(80);
    DigiKeyboard.println("logoff");
    DigiKeyboard.sendKeyStroke(KEY_ENTER);
}

void loop()
{
    if (digitalRead(REBOOT_PIN))
    {
        delay(80);
        if (digitalRead(REBOOT_PIN))
        {
            Reboot();
            delay(2000);
        }
    }
    else if (digitalRead(SHUTDOWN_PIN))
    {
        delay(80);
        if (digitalRead(SHUTDOWN_PIN))
            Shutdown();
        delay(2000);
    }
    // pin2默认拉高
    else if (!digitalRead(LOGOFF_PIN))
    {
        delay(80);
        if (!digitalRead(LOGOFF_PIN))
            Logoff();
        delay(2000);
    }
}
