#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <Servo.h>

#define SERVO_1_PIN D3 // 舵机1引脚
#define SERVO_2_PIN D2 // 舵机2引脚

#define ANGLE_ON 0    // 开灯时舵机角度
#define ANGLE_OFF 165  // 关灯时舵机角度
#define ANGLE_NONE 70  // 置空时舵机角度

#define AP_SSID "智能宿舍-5049" // esp8266创建的wifi的名称
#define AP_PWD "12345678"     // esp8266创建的wifi的密码

// 网页请求参数
#define LIGHT_1_ON_PARAMTER "/gpio/0"
#define LIGHT_2_ON_PARAMTER "/gpio/1"
#define LIGHT_1_OFF_PARAMTER "/gpio/2"
#define LIGHT_2_OFF_PARAMTER "/gpio/3"
#define LIGHT_All_ON_PARAMTER "/gpio/4"
#define LIGHT_All_OFF_PARAMTER "/gpio/5"

const char h1[] = { "<!DOCTYPE html>"
    "<html lang=\"en\">"
    "<head><meta charset=\"UTF-8\"><title>智能宿舍</title><style type=\"text/css\">#link a {color: #FFFFFF;text-decoration: none;}"
    "#link a:hover {color: #00ff00; text-decoration: none;}"
    "#fonts {font-size: 20px;text-decoration: none;}</style></head>"
    "<body id=\"fonts\"><br /><br /><table width=\"300\" border=\"0\" cellspacing=\"5\" cellpadding=\"5\" align=\"center\" bordercolor=\"#FFFFFF\" id=\"link\">"
    "<tr align=\"center\" bgcolor=\"#FFFFFF\" height=\"10\">"
    "<td colspan=\"2\">灯1 状态：<font color=\"#EA7B21\">" };

   const char h2[] = { "</font>"
    "</td></tr><tr align=\"center\" bgcolor=\"#1E90FF\">"
    "<td width=\"100\"><a href=\""
    LIGHT_1_ON_PARAMTER
    "\">灯1 开</a></td>"
    "<td width=\"100\"><a href=\""
    LIGHT_1_OFF_PARAMTER
    "\">灯1 关</a></td></tr>"
    "<tr align=\"center\" bgcolor=\"#FFFFFF\" height=\"15\"></tr><tr align=\"center\" bgcolor=\"#FFFFFF\" height=\"10\">"
    "<td colspan=\"2\">灯2 状态：<font color=\"#EA7B21\">" };

    char h3[] = { "</font>"
    "</td></tr><tr align=\"center\" bgcolor=\"#1E90FF\">"
    "<td width=\"100\"><a href=\""
    LIGHT_2_ON_PARAMTER
    "\">灯2 开</a></td>"
    "<td width=\"100\"><a href=\""
    LIGHT_2_OFF_PARAMTER
    "\">灯2 关</a></td></tr>"
    "<tr align=\"center\" bgcolor=\"#FFFFFF\" height=\"15\"></tr><tr align=\"center\" bgcolor=\"#FFFFFF\" height=\"10\">"
    "</td></tr><tr align=\"center\" bgcolor=\"#1E90FF\">"
    "<td width=\"100\"><a href=\""
    LIGHT_All_ON_PARAMTER
    "\">所有灯 开</a></td>"
    "<td width=\"100\"><a href=\""
    LIGHT_All_OFF_PARAMTER
    "\">所有灯 关</a></td></tr>"
    "</table></body></html>"
    };


// 灯的状态
bool light_1_On = false;
bool light_2_On = false;

const byte DNS_PORT = 53;       // DNS端口号
ESP8266WebServer server(80);    // Web服务
Servo servo_1;                  // 舵机1
Servo servo_2;                  // 舵机2
IPAddress apIp(192, 168, 4, 1); // IP地址
DNSServer dnsServer;            // dns服务

void setup() {
  Serial.begin(115200);
  Serial.println("start...");

  // 点亮板载小灯
  pinMode(LED_BUILTIN, OUTPUT);  
  analogWriteFreq(1000);      // 频率 1kHz， 周期 1ms
  analogWriteRange(1000);     // 范围 1000， 占空比步长 1us
  analogWrite(LED_BUILTIN, 950); // esp8266是反过来的1000是暗，0是亮
  // 初始化舵机
  servo_1.attach(SERVO_1_PIN);
  servo_1.write(ANGLE_NONE); // 舵机角度初始化
  servo_2.attach(SERVO_2_PIN);
  servo_2.write(ANGLE_NONE); // 舵机角度初始化
  // 初始化WIFI
  WiFi.mode(WIFI_AP);// Ap模式
  WiFi.hostname("ESP8266-宿舍智能化");  // 设置ESP8266设备名
  WiFi.softAPConfig(apIp,apIp, IPAddress(255, 255, 255, 0));
  if (WiFi.softAP(AP_SSID))// 无密码，开放式WIFI
  //  if (WiFi.softAP(AP_SSID, AP_PWD))
    Serial.println("Esp8266 SoftAp started successfully");
  else
    Serial.println("Esp8266 SoftAp started failed!");
  // 输出ESP8266 IP信息
  Serial.print("Esp8266 Ip:"); 
  Serial.println(WiFi.softAPIP());
  // 初始化DNS服务器
  if (dnsServer.start(DNS_PORT, "*", apIp)) // 判断将所有地址映射到esp8266的ip上是否成功
    Serial.println("dnsserver started successfully");
  else
    Serial.println("dnsserver started failed!");

  // 初始化WebServer
  server.on("/", HTTP_GET, handleRoot);       // 设置主页回调函数
  server.onNotFound(handleRoot);              // 设置无法响应的http请求的回调函数
  server.on(LIGHT_1_ON_PARAMTER, HTTP_GET, handleAction);    
  server.on(LIGHT_1_OFF_PARAMTER, HTTP_GET, handleAction);    
  server.on(LIGHT_2_ON_PARAMTER, HTTP_GET, handleAction);    
  server.on(LIGHT_2_OFF_PARAMTER, HTTP_GET, handleAction);    
  server.on(LIGHT_All_ON_PARAMTER, HTTP_GET, handleAction);    
  server.on(LIGHT_All_OFF_PARAMTER, HTTP_GET, handleAction);    
  server.begin();
  Serial.println("Start web service");
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();
}

// 访问主页回调函数
void handleRoot(){
 sendHtml();
}

void sendHtml(){
   String light_1 = light_1_On ? "开" : "关";
  String light_2 = light_2_On ? "开" : "关";
  char html[1300];
  snprintf(html, sizeof(html), "%s%s%s%s%s", h1, light_1.c_str(), h2, light_2.c_str(), h3);
 // Serial.println(html);
  Serial.println("Send Html Page");
  server.send(200, "text/html", html);
}

// 解析请求参数，并让舵机执行相应动作
void action(String req)
{
  if (req.indexOf(LIGHT_1_ON_PARAMTER) != -1)
  {
    servo_1.write(ANGLE_ON); // 设置舵机角度
    light_1_On = true;
    Serial.println(F("Light 1 on"));
  }
  else if (req.indexOf(LIGHT_1_OFF_PARAMTER) != -1)
  {
    servo_1.write(ANGLE_OFF);
    light_1_On = false;
    Serial.println(F("Light 1 off"));
  }
  else if (req.indexOf(LIGHT_2_ON_PARAMTER) != -1)
  {// 舵机2和舵机1安装方向相反
    servo_2.write(ANGLE_OFF);// 开关角度设置相反
    light_2_On = true;
    Serial.println(F("Light 2 on"));
  }
  else if (req.indexOf(LIGHT_2_OFF_PARAMTER) != -1)
  {
    servo_2.write(ANGLE_ON);
    light_2_On = false;
    Serial.println(F("Light 2 off"));
  }
  else if (req.indexOf(LIGHT_All_ON_PARAMTER) != -1)
  {// 舵机2和舵机1安装方向相反
  servo_1.write(ANGLE_ON); // 设置舵机角度
  delay(500);
  servo_1.write(ANGLE_NONE);
  delay(500);
  servo_2.write(ANGLE_OFF);// 开关角度设置相反
    light_1_On = true;
    light_2_On = true;
    Serial.println(F("Light All on"));
  }
  else if (req.indexOf(LIGHT_All_OFF_PARAMTER) != -1)
  { 
    servo_1.write(ANGLE_OFF);
    servo_2.write(ANGLE_ON);
    light_1_On = false;
    light_2_On = false;
    Serial.println(F("Light All off"));
  }
  else
  {
    Serial.println(F("invalid parameter, Not action"));
    return;
  }
  delay(500);
  // 将舵机重新置空，以免损坏
  servo_1.write(ANGLE_NONE);
  servo_2.write(ANGLE_NONE); 
}

void handleAction() { 
  Serial.println("handleAction");
  action(server.uri());
  Serial.println(server.uri());
  server.sendHeader("Location","/");          // 跳转回页面根目录
  server.send(303);
}