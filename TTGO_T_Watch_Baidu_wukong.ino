#include "lv_driver.h"
#include "lv_main.h"

#include "CloudSpeechClient.h"
#include "I2S.h"
#include "time.h"
#include "esp_system.h"
#include <WebServer.h>
#include <Preferences.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>  //必须加上,否则AP模式 配置参数会有问题


String last_voice = "";
const IPAddress apIP(192, 168, 4, 1);
const char* apSSID = "ESP32SETUP";
boolean settingMode = false;
String ssidList1;

bool SPIFFS_ok = false;

long ShowTft_lasttime = 0;
int ShowTft_length = 60; //60秒内关闭TFT
bool Tft_on = false; //tft是否显示中

//Preferences 的参数重烧固件会仍会存在！
//配置参数：


String report_address = "";  //如果配置有值,识别到的文字传给树莓派...
String report_url = "";   //如果配置有值,识别到的文字传给树莓派...

String dog_delay;     //定时狗健康状态监控，多少秒如果程序不在主循环上 esp32自动重启

String baidu_key;     //百度语音账号key
String baidu_secert;  //百度语音账号秘密码 注意：名称过长会有问题

String uuid;        //wukong 用的 uuid
String validate;    //wukong 用的 validate

String volume_low; //静音音量标准
String volume_high ; //噪音音量标准
String volume_double ; //音量乘以倍数

String define_max1 ;  //音量波峰（最高的一个值)
String define_avg1 ;  //音量平均值
String define_zero1 ; //音量全值 (平均值*时长)  这三个值结合起来判断当前秒的音量是否足够大，作用是检测非静音,是否唤醒开始录音

String define_max2 ;  //和上面的三个值的含义一样，区别是唤醒的音量标准会更大一些，在录音时静音的标准略低
String define_avg2;
String define_zero2 ;

String pre_sound;  //预缓冲秒数 2秒足够
String skip_baidu; //声音长度过短跳过识别 1==yes 0==no
String loopsleep;  //每次调用百度文字识别后的休息时间,防止过度频繁调用百度服务

String wifi_ssid1 ;
String wifi_password1  ;


WebServer webServer(80);
Preferences preferences;

//程序功能： 声音监听器。监听周围的声音，并识别成文字。识别的文字发给树莓派制作的智能音箱wukong
//         每段录音最长20秒,平均一段录音的文字识别时间3-25秒，取决于网络速度
//硬件：    TTGO_T_Watch 主板自带有8M PSRAM, 扩展板有多种，有一种扩展板带INMP441 I2S 麦克风录入芯片, 极适合处理语音,就用此扩展板做开发用.
//         源码硬件资料: https://github.com/LilyGO/TTGO-T-Watch
//         介绍指南:          https://t-watch-document-en.readthedocs.io/en/latest/index.html
//         玩家对其介绍:   https://www.instructables.com/id/TTGO-T-Watch/
//编译环境:
//    1.Arduino 1.8.9
//    2.扩展板引用地址配置成: https://dl.espressif.com/dl/package_esp32_dev_index.json
//    3.安装： 安装esp32的官方开发包 esp32 by Espressif Systems 版本 1.03-rc1
//    4.开发板选择: TTGO T-WATCH, PSRAM选择Enabled
//    5.Arduino选好开发板，设置完PSRAM,端口号后就可以连接esp32烧写固件了.
//
//树莓派服务端
//  安装有wukong的树莓派，端口号为5000
//  wukong项目地址: https://github.com/wzpan/wukong-robot
//  API规则: https://wukong.hahack.com/#/api
//  例子: curl -X POST localhost:5000/chat -d "type=text&query=%E7%8E%B0%E5%9C%A8%E5%87%A0%E7%82%B9&validate=f4bde2a342c7c75aa276f78b26cfbd8a&uuid=chated17be5d-0240-c9ba-2b2e-7eb98588cf34"

//
//使用说明：
//  ################# ESP32 #################################
//  1.配置: ESP32首次运行时会自动初始化内置参数,自动进入路由器模式,创建一个ESP32SETUP的路由器，电脑连接此路由输入http://192.168.4.1进行配置
//    配置:
//    A.esp32连接的路由器和密码
//    B.百度语音的账号校验码
//      baidu_key: 一个账号字串 (可不改)
//      baidu_secert: 一个账号校验码  (可不改)
//      这两个参数需要注册百度语音服务,在如下网址获取 http://yuyin.baidu.com
//      默认用了wukong 软件编写人免费提供的百度账号, 建议使用自己的百度语音账户
//    C.wukong交互配置
//      参数含义不理解参考： https://wukong.hahack.com/#/api   发起对话协议
//      report_address 装有wukong的树莓派IP （必须设置）
//      report_url  对话协议的URL地址 (不需要调整)
//      validate    wukong鉴权码 （必须设置，并与wukong软件配置的鉴权码一致）
//      uuid        wukong uuid (可不改)
//    D.其它音量监测参数: 默认是在家里安静环境下,如果周围较吵,需要将值调高//
//  2.运行：上电即运行
//
//  ################# 树莓派 #################################
//  树莓派安装有wukong软件，端口号5000
//  软件地址:  https://github.com/wzpan/wukong-robot
//  API协议:  https://wukong.hahack.com/#/api   发起对话协议
//  程序的原理是ESP32识别出文字后传给树莓派运行的wukong智能音箱,并进行语音交互，
//  必须树莓派提前运行wukong智能音箱软件 

//软件代码原理:
//  1.esp32上电后实时读取I2S声音信号，检测到周围声强是否达到指定音量，达到后立即进入录音模式
//  2.如发现3秒内静音录音停止，否则一直录音，直到10秒后停止录音，
//  3.将i2s采集到的wav原始声音数据按http协议用前面配置的百度账号传给百度服务,进行语音转文字
//  4.如果识别出文字，将文字上报服务器，现在用的是树莓派,可处理关灯,开灯等指令
//  声源在1-4米内识别效果都不错，再远了识别率会低.
//
//其它技巧
//  1.wav采集的数字声音有点像水波振动，以数字0不基线上下跳动. 静音时采集到的数值为0.
//  2.程序会预存2秒的声音，这2秒不仅用于检测声强，也会用于文字识别。这样对于监听二个字的短语不会丢失声音数据.
//
//工作用电:
//  5v 70ma电流  TTGO_T_Watch 自带的180 mAh电池理论上可以工作2小时
//
//声音数据: 16khz 16位 wav数据，经测试，此格式下百度文字识别效果最合适  8khz 8位wav 格式识别效果很差


hw_timer_t *timer = NULL;
const char* ntpServer = "ntp1.aliyun.com";
const long  gmtOffset_sec = 3600 * 8;
const int   daylightOffset_sec = 0;
long  last_check = 0;

const int record_time = 10;  // 录音秒数
const int waveDataSize = record_time * 32000 ;
const int numCommunicationData = 8000;
//数组：8000字节缓冲区
char communicationData[numCommunicationData];   //1char=8bits 1byte=8bits 1int=8*2 1long=8*4
long writenum = 0;

struct tm timeinfo;

CloudSpeechClient* cloudSpeechClient;


void writeparams()
{
  Serial.println("Writing params to EEPROM...");

  printparams();


  preferences.putString("report_address", report_address);
  preferences.putString("report_url", report_url);


  preferences.putString("baidu_key", baidu_key);
  preferences.putString("baidu_secert", baidu_secert);

  preferences.putString("uuid", uuid);
  preferences.putString("validate", validate);

  preferences.putString("dog_delay", dog_delay);

  preferences.putString("volume_low", volume_low);
  preferences.putString("volume_high", volume_high);
  preferences.putString("volume_double", volume_double);


  preferences.putString("define_max1", define_max1);
  preferences.putString("define_avg1", define_avg1);
  preferences.putString("define_zero1", define_zero1);
  preferences.putString("define_max2", define_max2);
  preferences.putString("define_avg2", define_avg2);
  preferences.putString("define_zero2", define_zero2);

  preferences.putString("pre_sound", pre_sound);
  preferences.putString("skip_baidu", skip_baidu);
  preferences.putString("loopsleep", loopsleep);

  preferences.putString("wifi_ssid1", wifi_ssid1);
  preferences.putString("wifi_password1", wifi_password1);

  Serial.println("Writing params done!");
}

bool readparams()
{
  report_address = preferences.getString("report_address");

  //如果这个值还没有，说明本机器没有配置默认
  if (report_address == "")
  {
    Serial.println("首次运行，配置默认参数");

    report_address = "192.168.1.20";
    uuid = "202001010000";
    validate = "f4bde2a342c7c75aa276f78b26cfbd8a";
    report_url = "/chat";
    baidu_key = "qg4haN8b2bGvFtCbBGqhrmZy";
    baidu_secert =  "585d4eccb50d306c401d7df138bb02e7"; //注意：变量名称过长保存有问题？

    volume_low = "15"; //静音音量值
    volume_high = "5000"; //噪音音量值
    volume_double = "40"; //音量乘以倍数

    define_max1 = "150";
    define_avg1 = "10";
    define_zero1 =  "3000";

    define_max2 = "120";
    define_avg2 =  "8";
    define_zero2 =  "2500";
    dog_delay = "10";

    pre_sound = "2";
    skip_baidu = "1";
    loopsleep = "5";  //每识别完文字后等待时间
    wifi_ssid1 = "CMCC-r3Ff";
    wifi_password1 = "9999900000";
    writeparams();
    printparams();
    return false;
  }


  report_address = preferences.getString("report_address");
  report_url =  preferences.getString("report_url");
  baidu_key = preferences.getString("baidu_key");
  baidu_secert = preferences.getString("baidu_secert");
  dog_delay = preferences.getString("dog_delay");

  volume_low = preferences.getString("volume_low");
  volume_high = preferences.getString("volume_high");
  volume_double = preferences.getString("volume_double");
  define_max1 = preferences.getString("define_max1");
  define_avg1 = preferences.getString("define_avg1");
  define_zero1 = preferences.getString("define_zero1");
  define_max2 = preferences.getString("define_max2");
  define_avg2 = preferences.getString("define_avg2");
  define_zero2 = preferences.getString("define_zero2");

  pre_sound = preferences.getString("pre_sound");
  skip_baidu = preferences.getString("skip_baidu");
  loopsleep = preferences.getString("loopsleep");
  uuid = preferences.getString("uuid");
  validate = preferences.getString("validate");

  wifi_ssid1 = preferences.getString("wifi_ssid1");
  wifi_password1 = preferences.getString("wifi_password1");

  printparams();
  return true;
}

void printparams()
{
  // return;
  Serial.println(" report_address: " + report_address);
  Serial.println(" report_url: " + report_url);
  Serial.println(" baidu_key: " + baidu_key);
  Serial.println(" baidu_secert: " + baidu_secert);
  Serial.println(" uuid: " + uuid);
  Serial.println(" validate: " + validate);
  Serial.println(" dog_delay: " + dog_delay);

  Serial.println(" volume_low: " + volume_low);
  Serial.println(" volume_high: " + volume_high);
  Serial.println(" volume_double: " + volume_double);

  Serial.println(" define_max1: " + define_max1);
  Serial.println(" define_avg1: " + define_avg1);
  Serial.println(" define_zero1: " + define_zero1);

  Serial.println(" define_max2: " + define_max2);
  Serial.println(" define_avg2: " + define_avg2);
  Serial.println(" define_zero2: " + define_zero2);
  Serial.println(" pre_sound: " + pre_sound);
  Serial.println(" skip_baidu: " + skip_baidu);
  Serial.println(" loopsleep: " + loopsleep);
  Serial.println(" wifi_ssid1: " + wifi_ssid1);
  Serial.println(" wifi_password1: " + wifi_password1);
}



void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  //esp_restart_noos(); 旧api
  esp_restart();
}

String GetLocalTime()
{
  String timestr = "";
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (timestr);
  }
  timestr = String(timeinfo.tm_mon + 1) + "-" + String(timeinfo.tm_mday) + " " +
            String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec) ;
  return (timestr);
}


int16_t max_int(int16_t a, int16_t b)
{
  if (a > b)
    return a;
  else
    return b;
}

int16_t min_int(int16_t a, int16_t b)
{
  if (b > a)
    return a;
  else
    return b;
}

//每半秒一次检测噪音
bool wait_loud()
{

  String timelong_str = "";
  float val_avg = 0;
  int16_t val_max = 0;
  int32_t all_val_zero = 0;
  int32_t tmpval = 0;
  int16_t val16 = 0;
  uint8_t val1, val2;
  bool aloud = false;
  Serial.println(">");
  int32_t j = 0;
  while (true)
  {

    //检测是否到了关闭TFT的时间
    if ( Tft_on && (millis() / 1000 - ShowTft_lasttime > ShowTft_length) )
    {
      backlight_adjust(0);
      Tft_on = false;
    }


    j = j + 1;
    //每25秒处理一次即可
    if (j % 100 == 0)
      timerWrite(timer, 0); //reset timer (feed watchdog)

    //读满缓冲区8000字节
    //此函数会自动调节时间，只要后续的操作不要让缓冲区占满即可
    //1/4秒 8000字节 4000次
    I2S_Read(communicationData, numCommunicationData);

    for (int loop1 = 0; loop1 < numCommunicationData / 2 ; loop1++)
    {
      val1 = communicationData[loop1 * 2];
      val2 = communicationData[loop1 * 2 + 1] ;
      val16 = val1 + val2 *  256;
      if (val16 > 0)
      {
        val_avg = val_avg + val16;
        val_max = max_int( val_max, val16);
      }
      //有声音的次数
      if (abs(val16) > 15 )
        all_val_zero = all_val_zero + 1;

      //乘以40 ：音量提升20db
      tmpval = val16 * 40;
      if (abs(tmpval) > 32767 )
      {
        if (val16 > 0)
          tmpval = 32767;
        else
          tmpval = -32767;
      }
      //Serial.println(String(val1) + " " + String(val2) + " " + String(val16) + " " + String(tmpval));
      communicationData[loop1 * 2] =  (byte)(tmpval & 0xFF);
      communicationData[loop1 * 2 + 1] = (byte)((tmpval >> 8) & 0xFF);

    }

    //填充声音信息到缓存区 (配置:2秒或n秒)
    //每个缓存环存满了换下一个缓存环
    cloudSpeechClient->pre_push_sound_buff((byte *)communicationData, numCommunicationData);


    //半秒检查一次  16000字节 8000次数据记录
    if (j % 2 == 0 && j > 0)
    {
      val_avg = val_avg / numCommunicationData ;
      if ( val_max > define_max1.toInt() && val_avg > define_avg1.toInt()   && all_val_zero > define_zero1.toInt() )
        aloud = true;
      else
        aloud = false;

      if (aloud)
      {
#ifdef SHOW_DEBUG
        timelong_str = ">>>>> high_max:" + String(val_max) +  " high_avg:" + String(val_avg) +   " all_val_zero:" + String(all_val_zero) ;
        Serial.println(timelong_str);
#endif
        last_voice = "voice:" + String(val_max) +  "|" + String(val_avg) +   "|" + String(all_val_zero) ;
        break;
      }
      val_avg = 0;
      val_max = 0;
      all_val_zero = 0;

      //防止溢出
      if (j >= 1000000)
        j = 0;
    }
  }
  last_check = millis() ;
  return (true);
}

int record_sound()
{
  uint32_t all_starttime;
  uint32_t all_endtime;
  uint32_t timelong = 0;
  String timelong_str = "";
  uint32_t last_starttime = 0;

  float val_avg = 0;
  int16_t val_max = 0;
  int32_t all_val_zero = 0;
  int16_t val16 = 0;
  uint8_t val1, val2;
  bool aloud = false;
  int32_t tmpval = 0;
  int all_alound;
  writenum = 0;


  //初始化0
  cloudSpeechClient->sound_bodybuff_p = 0;

  //用双声道，32位并没什么关系，因为拷数据时间很快！完全不占用多少时间！
  //Serial.println("record start 16k,16位,单声道");
  Serial.println( ">" + GetLocalTime() );
#ifdef SHOW_DEBUG
  Serial.println(GetLocalTime() + "> " + "record... 反应时间:" + String(millis() - last_check) + "毫秒");
#endif
  //Serial.println("录音中， 声音格式:16khz 16位 单声道， 最长10秒自动结束...");
  // last_press = millis() / 1000;
  all_starttime = millis() / 1000;
  last_starttime = millis() / 1000;

  timerWrite(timer, 0); //reset timer (feed watchdog)
  //反复循环最长时间I2S录音
  for (uint32_t j = 0; j < waveDataSize / numCommunicationData; ++j) {
    //timelong_str = "";
    // Serial.println("loop");
    //读满缓冲区8000字节
    //此函数会自动调节时间，只要后续的操作不要让缓冲区占满即可
    I2S_Read(communicationData, numCommunicationData);

    //timelong_str = timelong_str + "," + j;
    //Serial.println(timelong_str);

    //平均值，最大值记录，检测静音参数用
    for (int loop1 = 0; loop1 < numCommunicationData / 2 ; loop1++)
    {
      val1 = communicationData[loop1 * 2];
      val2 = communicationData[loop1 * 2 + 1] ;
      val16 = val1 + val2 *  256;
      if (val16 > 0)
      {
        val_avg = val_avg + val16;
        val_max = max( val_max, val16);
      }
      if (abs(val16) > volume_low.toInt() )
        all_val_zero = all_val_zero + 1;

      //乘以40 ：音量提升20db
      tmpval = val16 * volume_double.toInt();
      if (abs(tmpval) > 32767 )
      {
        if (val16 > 0)
          tmpval = 32767;
        else
          tmpval = -32767;
      }
      communicationData[loop1 * 2] =  (byte)(tmpval & 0xFF);
      communicationData[loop1 * 2 + 1] = (byte)((tmpval >> 8) & 0xFF);
    }

    //声音信息保存至缓冲区
    cloudSpeechClient->push_bodybuff_buff((byte*)communicationData, numCommunicationData);

    writenum = writenum + numCommunicationData;
    //半秒检查一次静音  16000字节 8000次数据记录
    if (j % 2 == 0 && j > 0)
    {
      val_avg = val_avg / numCommunicationData ;

      if ( val_max > define_max2.toInt() && val_avg > define_avg2.toInt()   && all_val_zero > define_zero2.toInt() )
        aloud = true;
      else
        aloud = false;


      if (aloud)
      {
        all_alound = all_alound + 1;
        //录音过程中，调试输出不要轻易用，会影响识别率！
        //timelong_str = ">>>>> " + String( millis() / 1000 - all_starttime) + String("秒 ");
        //timelong_str = timelong_str + " high_max:" + String(val_max) +  " high_avg:" + String(val_avg) +   " all_val_zero:" + String(all_val_zero) ;
        //Serial.println(timelong_str);
        last_starttime = millis() / 1000;
      }

      val_avg = 0;
      val_max = 0;
      all_val_zero = 0;
    }

    //3秒仍静音，中断退出
    if ( millis() / 1000 - last_starttime > 2)
    {
#ifdef SHOW_DEBUG
      Serial.println("静音检测，退出");
#endif
      break;
    }
  }
  all_endtime = millis() / 1000;

#ifdef SHOW_DEBUG
  Serial.println("文件字节数:" + String(writenum) + ",理论秒数:" + String(writenum / 32000) + "秒") ;
  Serial.println("录音结束,时长:" + String(all_endtime - all_starttime) + "秒" );
#endif
  return (all_alound);
}

//如果flag 1 必须连接才算over,  如果为0 只试30秒
bool connectwifi(int flag)
{
  if (WiFi.status() == WL_CONNECTED) return true;

  while (true)
  {
    if (WiFi.status() == WL_CONNECTED) break;

    int trynum = 0;
    Serial.print("Connecting to ");

    Serial.println(wifi_ssid1);

    //静态IP有时会无法被访问，原因不明！
    WiFi.disconnect(true); //关闭网络
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_STA);

    WiFi.begin(wifi_ssid1.c_str(), wifi_password1.c_str());

    while (WiFi.status() != WL_CONNECTED) {
      delay(2000);
      Serial.print(".");
      trynum = trynum + 1;
      //30秒 退出
      if (trynum > 14) break;
    }
    if (flag == 0) break;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi connected with IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.gatewayIP());
    Serial.println(WiFi.subnetMask());
    Serial.println(WiFi.dnsIP(0));
    Serial.println(WiFi.dnsIP(1));
    return true;
  }
  else
    return false;
}



//TFT文字显示
void ShowTft(String rec_text)
{
  Serial.println("ShowTft:" + rec_text);
  lv_set_text1(split_str(">>" + rec_text));

  //记录显示屏开启时间，60秒后显示屏在loop中判断关灭
  //开启显示
  backlight_adjust(180);
  ShowTft_lasttime = millis() / 1000;
  Tft_on = true;
}

void begin_recordsound()
{
  //Serial.println("press 最长录音10秒，检测静音会提前结束" );
  //调用录音函数，直到结束
  int rec_ok = record_sound();
  String retstr = "";

  //录入声音都是静音
  if (skip_baidu == "1" && rec_ok == 0)
  {
#ifdef SHOW_DEBUG
    Serial.println("无用声音信号...");
#endif
    return;
  }
  else
  {
#ifdef SHOW_DEBUG
    Serial.println("进行文字识别" );
#endif
    uint32_t all_starttime = millis() / 1000;
    String VoiceText = cloudSpeechClient->getVoiceText();
    Serial.println("识别用时: " + String ( millis() / 1000 - all_starttime) + "秒" );

    int error = 0;

    if (VoiceText.indexOf("speech quality error") > -1)
    {
      Serial.println("find speech quality error");
      error = 1;
    }

    VoiceText.replace("speech quality error", "");
    VoiceText.replace("。", "");

    //如果识别到文字，对文字进行处理
    if (VoiceText.length() > 0)
    {
      record_succ(VoiceText);
    }
  }
}

//成功识别文字后的处理(VoiceText 不会为空)
//1.显示内容到显示屏.
//2.和图灵对话，对话文字显示到显示屏
//3.识别的文字和原始wav传给树莓派
void record_succ(String VoiceText)
{
  if (VoiceText.length() == 0) return;

  String retstr = "";
  //每个汉字占3个长度
  Serial.println(String("识别结果:") + GetLocalTime() + "> " + VoiceText + " len=" + VoiceText.length());

  //1.文字输出到TFT
  ShowTft( VoiceText);

  //2.文字传给树莓派
  if (  String(report_url).length() > 0 && String(report_address).length() > 0 )
  {
    retstr = cloudSpeechClient->posturl_2020(report_address, 5000, report_url ,
             "type=text&query=" + urlencode(VoiceText) + "&validate=" + validate + "&uuid=" + uuid );
    Serial.println("posturl_2020 retstr:" + retstr);
    //curl -X POST localhost:5000/chat -d "type=text&query=%E7%8E%B0%E5%9C%A8%E5%87%A0%E7%82%B9&validate=f4bde2a342c7c75aa276f78b26cfbd8a&uuid=chated17be5d-0240-c9ba-2b2e-7eb98588cf34"
    retstr = cloudSpeechClient->PrintJson_wukong_ret(retstr);
    if (retstr == "0")
    {
      Serial.println("posturl_2020 succ!");
      //等10秒，确保声音播放完成
      delay(10 * 1000);
    }
  }
}


void setupMode() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  Serial.println("scanNetworks");

  ssidList1 = "";

  for (int i = 0; i < n; ++i) {
    ssidList1 += "<option value=\"";
    ssidList1 += WiFi.SSID(i);
    ssidList1 += "\"";

    if (WiFi.SSID(i) == wifi_ssid1)
      ssidList1 += " selected ";
    ssidList1 += ">";
    ssidList1 += WiFi.SSID(i);
    ssidList1 += "</option>";
  }
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID);
  WiFi.mode(WIFI_MODE_AP);
  startWebServer();
  Serial.println("Starting Access Point at \"" + String(apSSID) + "\"");
  //ShowTft( "连接路由器ESP32SETUP");
  ShowTft( "电脑连接 http://192.168.4.1");
}



String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}


String new_urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}


//配置模式在笔记本电脑win10上 192.168.4.1 ping不通，网页打不开
//但是使用手机可以连接此ip,可能是win10安全机制问题？
void startWebServer() {

  //设置模式
  //if (settingMode) {
  //if (true)
  //{
  Serial.print("Starting Web Server at ");
  if (settingMode)
    Serial.println(WiFi.softAPIP());
  else
    Serial.println(WiFi.localIP());
  //设置主页

  // readparams();
  webServer.on("/settings", []() {
    String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
    s += "<form method=\"get\" action=\"setap\">" ;


    s += "<br>report_address: <input name=\"report_address\" style=\"width:350px\" value='" + report_address + "'type=\"text\">";
    s += "<br>report_url: <input name=\"report_url\" style=\"width:350px\"  value='" + report_url + "'type=\"text\">";

    s += "<br>baidu_key: <input name=\"baidu_key\" style=\"width:350px\"  value='" + baidu_key + "'type=\"text\">";
    s += "<br>baidu_secert: <input name=\"baidu_secert\" style=\"width:350px\"  value='" + baidu_secert + "'type=\"text\">";
    s += " <hr>";
    s += "<br>wukong_uuid: <input name=\"uuid\" style=\"width:350px\"  value='" + uuid + "'type=\"text\">";
    s += "<br>wukong_validate: <input name=\"validate\" style=\"width:350px\"  value='" + validate + "'type=\"text\">";
    s += " <hr>";
    s += "<br>dog_delay: <input name=\"dog_delay\" style=\"width:100px\"  value='" + dog_delay + "'type=\"text\">mins";

    s += "<br>volume_low: <input name=\"volume_low\" style=\"width:100px\"  value='" + volume_low + "'type=\"text\">";
    s += "volume_high: <input name=\"volume_high\" style=\"width:100px\"  value='" + volume_high + "'type=\"text\">";
    s += "volume_double: <input name=\"volume_double\" style=\"width:100px\"  value='" + volume_double + "'type=\"text\">";

    s += "<br>define_max1: <input name=\"define_max1\" style=\"width:100px\"  value='" + define_max1 + "'type=\"text\">";
    s += "define_avg1: <input name=\"define_avg1\" style=\"width:100px\"  value='" + define_avg1 + "'type=\"text\">";
    s += "define_zero1: <input name=\"define_zero1\" style=\"width:100px\" value='" + define_zero1 + "'type=\"text\">";
    s += "<br>define_max2: <input name=\"define_max2\" style=\"width:100px\" value='" + define_max2 + "'type=\"text\">";
    s += "define_avg2: <input name=\"define_avg2\" style=\"width:100px\" value='" + define_avg2 + "'type=\"text\">";
    s += "define_zero2: <input name=\"define_zero2\" style=\"width:100px\" value='" + define_zero2 + "'type=\"text\">";

    s += "<br>pre_sound:<input name=\"pre_sound\" style=\"width:100px\" value='" + pre_sound + "'type=\"text\">";
    if (skip_baidu == "")
      s += "skip_baidu: <select name=\"skip_baidu\" ><option  value=\"1\" selected>yes</option> <option  value=\"0\">no</option>  </select>";
    else if (skip_baidu == "1")
      s += "skip_baidu: <select name=\"skip_baidu\" ><option  value=\"1\" selected>yes</option> <option  value=\"0\">no</option>  </select>";
    else
      s += "skip_baidu: <select name=\"skip_baidu\" ><option  value=\"1\">yes</option> <option  value=\"0\" selected>no</option>  </select>";
    s += "loopsleep:<input name=\"loopsleep\" style=\"width:100px\" value='" + loopsleep + "'type=\"text\">";

    s += " <hr>";
    s += "<label>SSID1: </label><select style=\"width:200px\"  name=\"wifi_ssid1\" >" + ssidList1 +  "</select>";
    s += "Password1: <input name=\"wifi_password1\" style=\"width:100px\"  value='" + wifi_password1 + "' type=\"text\">";

    s += "<hr>";



    s += "<br><input type=\"submit\"></form>";
    webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
  });
  //设置写入页(后台)
  webServer.on("/setap", []() {

    report_address = new_urlDecode(webServer.arg("report_address"));
    report_url = new_urlDecode(webServer.arg("report_url"));

    baidu_key = new_urlDecode(webServer.arg("baidu_key"));
    baidu_secert = new_urlDecode(webServer.arg("baidu_secert"));
    uuid = new_urlDecode(webServer.arg("uuid"));
    validate = new_urlDecode(webServer.arg("validate"));
    dog_delay = new_urlDecode(webServer.arg("dog_delay"));

    volume_low = new_urlDecode(webServer.arg("volume_low"));
    volume_high = new_urlDecode(webServer.arg("volume_high"));

    volume_double = new_urlDecode(webServer.arg("volume_double"));

    define_max1 = new_urlDecode(webServer.arg("define_max1"));
    define_avg1 = new_urlDecode(webServer.arg("define_avg1"));
    define_zero1 = new_urlDecode(webServer.arg("define_zero1"));
    define_max2 = new_urlDecode(webServer.arg("define_max2"));
    define_avg2 = new_urlDecode(webServer.arg("define_avg2"));
    define_zero2 = new_urlDecode(webServer.arg("define_zero2"));

    pre_sound = new_urlDecode(webServer.arg("pre_sound"));
    skip_baidu = new_urlDecode(webServer.arg("skip_baidu"));
    loopsleep = new_urlDecode(webServer.arg("loopsleep"));

    wifi_ssid1 = new_urlDecode(webServer.arg("wifi_ssid1"));
    wifi_password1 = new_urlDecode(webServer.arg("wifi_password1"));

    //   Serial.print("baidu_secert: " + baidu_secert);

    //写入配置
    writeparams();
    String wifi_ssid = "";
    String wifi_password = "";

    wifi_ssid = wifi_ssid1;
    wifi_password = wifi_password1;

    String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
    s += wifi_ssid;
    s += "\" after the restart.";
    webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    delay(3000);
    ESP.restart();
  });
  webServer.onNotFound([]() {
    String s = "<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
    webServer.send(200, "text/html", makePage("AP mode", s));
  });

  webServer.begin();
}



void setup()
{
  Serial.begin(115200);
  Serial.println("setup begin");
  SPIFFS_ok = false;
  //初始化SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS init failed");
    Serial.println("SPIFFS format ...");
    if (SPIFFS.format())
    {
      Serial.println("SPIFFS format ok");
      Serial.println("SPIFFS re_init");
      if (SPIFFS.begin(true))
      {
      }
      else
      {
        Serial.println("SPIFFS re_init error");
        ets_printf("reboot\n");
        esp_restart();
        return;
      }
    }
    else
    {
      Serial.println("SPIFFS format failed");
      ets_printf("reboot\n");
      esp_restart();
      return;
    }
  }
  Serial.println("SPIFFS init ok");
  SPIFFS_ok = true;
  display_init();
  lv_create_ttgo();

  //初始化配置类
  preferences.begin("wifi-config");
  readparams();

  //如果进入配置模式，10分钟后看门狗会让esp32自动重启
  int wdtTimeout = dog_delay.toInt() * 60 * 1000; //设置分钟 watchdog

  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000 , false); //set time in us
  timerAlarmEnable(timer);                          //enable interrupt

  //有限模式进入连接，如果30秒连接不上，返回false
  bool ret_bol = connectwifi(0);

  //ret_bol = false;
  //wifi连接不上，进入配置模式
  if (ret_bol == false)
  {
    settingMode = true;
    ShowTft("进入设置模式");
    setupMode();
    return;
  }

  //I2S_BITS_PER_SAMPLE_8BIT 配置的话，下句会报错，
  //最小必须配置成I2S_BITS_PER_SAMPLE_16BIT
  I2S_Init(I2S_MODE_RX, 16000, I2S_BITS_PER_SAMPLE_16BIT);

  cloudSpeechClient = new CloudSpeechClient(pre_sound.toInt());
  //此方法必须调用成功，否则语音识别会无法进行
  while (true)
  {
    String baidu_Token = cloudSpeechClient->getToken(baidu_key, baidu_secert);
    Serial.println("baidu_Token:" + baidu_Token);
    if (baidu_Token.length() > 0 )
      break;
  }

  //NTP 时间
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  String retstr = "";
  if (String(report_url).length() > 0 && String(report_address).length() > 0)
  {
    retstr = cloudSpeechClient->posturl_2020(report_address, 5000, report_url ,
             "type=text&query=" + urlencode("现在几点") + "&validate=" + validate + "&uuid=" + uuid );
    Serial.println("posturl_2020 retstr:" + retstr);
    //curl -X POST localhost:5000/chat -d "type=text&query=%E7%8E%B0%E5%9C%A8%E5%87%A0%E7%82%B9&validate=f4bde2a342c7c75aa276f78b26cfbd8a&uuid=chated17be5d-0240-c9ba-2b2e-7eb98588cf34"
    retstr = cloudSpeechClient->PrintJson_wukong_ret(retstr);
    if (retstr == "0")
    {
      Serial.println("posturl_2020 succ!");
      //等待，确保声音播放完成
      delay(10 * 1000);
    }
  }

  ShowTft_lasttime = millis() / 1000;
  Tft_on = true;
  ShowTft("启动");
  Serial.println("start...");
}



void loop() {
  //return;

  if (SPIFFS_ok == false) return;

  //检测是否到了关闭TFT的时间
  if ( Tft_on && (millis() / 1000 - ShowTft_lasttime > ShowTft_length) )
  {
    backlight_adjust(0);
    Tft_on = false;
  }

  //如果是配置模式，不录音，识音
  if (settingMode)
  {
    //处理网页服务（必须有)
    webServer.handleClient();
    return;
  }

  //检测到wifi失联，自动连接
  connectwifi(1);

  //检测是噪音（声音识别开始)
  wait_loud();
  //进入录音及识别模式
  begin_recordsound();

  //每个百度账号只能2个声音转文字的并发，不能调用太频繁
  //需要给点时间让wukong把语音播放完
#ifdef SHOW_DEBUG
  Serial.println("loop sleep:" + String(loopsleep) + "秒");
#endif

  delay(loopsleep.toInt() * 1000);
}
