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
