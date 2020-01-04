
这个软件项目做了二个，硬件都是ESP32+INMP441 I2S 麦克风 <br/>
TTGO_T_Watch是电话手表形态，组装性更好，价格略贵一点. 动手能力强的直接买一块ESP32开发板及I2S 麦克风一样可以玩 <br/>
两项目地址分别是: <br/>
https://github.com/lixy123/ESP32_Baidu_wukong/ <br/>
https://github.com/lixy123/TTGO_T_Watch_Baidu_wukong/ <br/>
<br/>
<b>程序功能：</b> <br/>
 声音监听器。监听周围的声音，并识别成文字。识别的文字发给树莓派制作的智能音箱wukong<br/>
         每段录音最长20秒,平均一段录音的文字识别时间3-25秒，取决于网络速度<br/>
  <br/>
<b>硬件：</b> <br/>
    TTGO_T_Watch 主板自带有8M PSRAM, 扩展板有多种，有一种扩展板带INMP441 I2S 麦克风录入芯片, 极适合处理语音,就用此扩展板做开发用.<br/>
         源码硬件资料: https:github.com/LilyGO/TTGO-T-Watch<br/>
         介绍指南:          https:t-watch-document-en.readthedocs.io/en/latest/index.html<br/>
         玩家对其介绍:   https:www.instructables.com/id/TTGO-T-Watch/<br/>
 <br/>
<b>编译环境:</b><br/>
    1.Arduino 1.8.9<br/>
    2.扩展板引用地址配置成: https:dl.espressif.com/dl/package_esp32_dev_index.json<br/>
    3.安装： 安装esp32的官方开发包 esp32 by Espressif Systems 版本 1.03-rc1<br/>
    4.开发板选择: TTGO T-WATCH, PSRAM选择Enabled<br/>
    5.Arduino选好开发板，设置完PSRAM,端口号后就可以连接esp32烧写固件了.<br/>
<br/>
<b>使用说明：</b><br/>
  ################# ESP32 #################################<br/>
  1.配置: ESP32首次运行时会自动初始化内置参数,自动进入路由器模式,创建一个ESP32SETUP的路由器，电脑连接此路由输入http:192.168.4.1进行配置<br/>
    配置:<br/>
    A.esp32连接的路由器和密码<br/>
    B.百度语音的账号校验码<br/>
      baidu_key: 一个账号字串 (可不改)<br/>
      baidu_secert: 一个账号校验码  (可不改)<br/>
      这两个参数需要注册百度语音服务,在如下网址获取 http:yuyin.baidu.com<br/>
      默认用了wukong 软件编写人免费提供的百度账号, 建议使用自己的百度语音账户<br/>
    C.wukong交互配置<br/>
      参数含义不理解参考： https:wukong.hahack.com/#/api   发起对话协议<br/>
      report_address 装有wukong的树莓派IP （必须设置）<br/>
      report_url  对话协议的URL地址 (不需要调整)<br/>
      validate    wukong鉴权码 （必须设置，并与wukong软件配置的鉴权码一致）<br/>
      uuid        wukong uuid (可不改)<br/>
    D.其它音量监测参数: 默认是在家里安静环境下,如果周围较吵,需要将值调高<br/>
  2.运行：上电即运行<br/>
<br/>
  ################# 树莓派 #################################<br/>
  树莓派安装有wukong软件，端口号5000<br/>
  软件地址:  https:github.com/wzpan/wukong-robot<br/>
  API协议:  https:wukong.hahack.com/#/api   发起对话协议<br/>
  程序的原理是ESP32识别出文字后传给树莓派运行的wukong智能音箱,并进行语音交互，<br/>
  必须树莓派提前运行wukong智能音箱软件 <br/>
<br/>
<b>软件代码原理:</b><br/>
  1.esp32上电后实时读取I2S声音信号，检测到周围声强是否达到指定音量，达到后立即进入录音模式<br/>
  2.如发现3秒内静音录音停止，否则一直录音，直到10秒后停止录音，<br/>
  3.将i2s采集到的wav原始声音数据按http协议用前面配置的百度账号传给百度服务,进行语音转文字<br/>
  4.如果识别出文字，将文字上报服务器，现在用的是树莓派,可处理关灯,开灯等指令<br/>
  声源在1-4米内识别效果都不错，再远了识别率会低.<br/>
<br/>
<b>其它技巧</b><br/>
  1.wav采集的数字声音有点像水波振动，以数字0不基线上下跳动. 静音时采集到的数值为0.<br/>
  2.程序会预存2秒的声音，这2秒不仅用于检测声强，也会用于文字识别。这样对于监听二个字的短语不会丢失声音数据.<br/>
<br/>
<b>工作用电:</b><br/>
  5v 70ma电流  TTGO_T_Watch 自带的180 mAh电池理论上可以工作2小时<br/>
<br/>
<b>声音数据:</b> <br/>
16khz 16位 wav数据，经测试，此格式下百度文字识别效果最合适  8khz 8位wav 格式识别效果很差<br/>
