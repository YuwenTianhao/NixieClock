#include <WiFi.h>
#include "time.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "stdint.h"

#define SCK_1 GPIO_NUM_12 // 移位寄存器时钟线引脚
#define SCK_2 GPIO_NUM_6

#define RCK_1 GPIO_NUM_11 // 存储寄存器时钟线引脚
#define RCK_2 GPIO_NUM_10

#define SI_1 GPIO_NUM_2 // 数据引脚
#define SI_2 GPIO_NUM_3

const char *ssid = "iPhone";
const char *password = "ywthywth";

const char *ntpServer = "us.pool.ntp.org";
const long gmtOffset_sec = 3600 * 7;
const int daylightOffset_sec = 3600;

struct tm time_pre, time_now;

/* 更新时间 */
void reLocalTime()
{
  time_pre = time_now;
  if (!getLocalTime(&time_now))
  {
    Serial.println("Failed to obtain time");
    return;
  }
}

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  //Serial.println(timeinfo.tm_hour);
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void HC595_Send_Byte(uint8_t byte, int n)
{
  /* 即向74HC595的SI引脚发送一个字节  n为几号移位寄存器 */
  uint8_t i;
  if (n = 1)
  {
    for (i = 0; i < 8; i++) // 一个字节8位，传输8次，一次一位，循环8次，刚好移完8位
    {
      /****  步骤1：将数据传到DS引脚    ****/
      gpio_set_level(SCK_1, LOW); // SCK拉低
      if (byte & 0x80)
      {                             // 先传输高位，通过与运算判断第八是否为1
        gpio_set_level(SI_1, HIGH); // 如果第八位是1，则与 595 DS连接的引脚输出高电平
      }
      else
      { // 否则输出低电平
        gpio_set_level(SI_1, LOW);
      }
      /*** 步骤2：SCK每产生一个上升沿，当前的bit就被送入移位寄存器 ***/
      byte <<= 1;                  // 左移一位，将低位往高位移，通过	if (byte & 0x80)判断低位是否为1
      gpio_set_level(SCK_1, HIGH); // SCK拉高， SCK产生上升沿
                                   /** RCK产生一个上升沿，移位寄存器的数据移入存储寄存器  **/
      gpio_set_level(RCK_1, LOW);
      gpio_set_level(RCK_1, HIGH); // 再将RCK拉高，RCK即可产生一个上升沿
    }
  }
  if (n = 2)
  {
    for (i = 0; i < 8; i++)
    {
      gpio_set_level(SCK_1, LOW);
      if (byte & 0x80)
      {
        gpio_set_level(SI_1, HIGH);
      }
      else
      {
        gpio_set_level(SI_1, LOW);
      }
      byte <<= 1;
      gpio_set_level(SCK_1, HIGH);
      gpio_set_level(RCK_2, LOW);
      gpio_set_level(RCK_2, HIGH);
    }
  }
}

/* PWM刷新辉光管 */
/* 注意传入指针 */

void reNixie(struct tm *time_pre, struct tm *time_now)
{
  /*  int hour_pre_one,hour_pre_ten,hour_now_one,hour_now_ten;    //拆分数据
   int min_pre_one,min_pre_ten,min_now_one,min_now_ten;
   hour_pre_one=(time_pre->tm_hour)%10;
   hour_pre_ten=(time_pre->tm_hour)/10;
   hour_now_one=(time_now->tm_hour)%10;
   hour_now_ten=(time_now->tm_hour)/10;
   min_pre_one=(time_pre->tm_min)%10;
   min_pre_ten=(time_pre->tm_min)/10;
   min_now_one=(time_now->tm_min)%10;
   min_now_ten=(time_now->tm_min)/10; */

  // 若时间没有变化则直接返回
  if ((time_pre->tm_hour == time_now->tm_hour) && (time_pre->tm_min == time_now->tm_min))
    return;

  uint8_t num_pre_1, num_pre_2, num_now_1, num_now_2; // 向595中输入的数据
  num_pre_1 = (((time_pre->tm_hour) % 10) << 4) + ((time_pre->tm_hour) / 10);
  num_pre_2 = (((time_pre->tm_min) % 10) << 4) + ((time_pre->tm_min) / 10);
  num_now_1 = (((time_now->tm_hour) % 10) << 4) + (((time_now->tm_hour) / 10));
  num_now_2 = (((time_now->tm_min) % 10) << 4) + ((time_now->tm_min) / 10);

  // PWM
  int i, j;
  for (i = 0; i < 100; i++)
  {
    for (j = 0; j < i; j++)
    {
      HC595_Send_Byte(num_now_1, 1);
      HC595_Send_Byte(num_now_2, 2);
    }
    for (j = 0; j < 100 - i; j++)
    {
      HC595_Send_Byte(num_pre_1, 1);
      HC595_Send_Byte(num_pre_2, 2);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  // connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  // init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getLocalTime(&time_pre);
  reLocalTime();
  printLocalTime();
}
void loop()
{
  delay(1000);
  reLocalTime();
  reNixie(&time_pre,&time_now);
}