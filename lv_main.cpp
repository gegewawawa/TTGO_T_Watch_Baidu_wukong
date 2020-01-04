/*
  ESP8266 Hello World urlencode by Steve Nelson
  URLEncoding is used all the time with internet urls. This is how urls handle funny characters
  in a URL. For example a space is: %20
  These functions simplify the process of encoding and decoding the urlencoded format.

  It has been tested on an esp12e (NodeMCU development board)
  This example code is in the public domain, use it however you want.
  Prerequisite Examples:
  https://github.com/zenmanenergy/ESP8266-Arduino-Examples/tree/master/helloworld_serial
*/
#include "lv_main.h"

//所有文字都显示在此控件上
lv_obj_t *label1 ;

LV_FONT_DECLARE(myfont);

void lv_set_text1(String outtext)
{
  lv_label_set_text(label1, outtext.c_str());
}



void lv_create_ttgo()
{

  /*创建带自定义字体的类型*/
  static lv_style_t style1;
  lv_style_copy(&style1, &lv_style_plain);
  style1.text.font = &myfont; /*Set the base font whcih is concatenated with the others*/

  //style1.body.main_color = LV_COLOR_WHITE;
  //style1.body.grad_color = LV_COLOR_BLUE;

  /* 创建简单的标签显示  label*/
  //容器对象1，可保证超出的内容不显示
  lv_obj_t  *gContainer1 = lv_cont_create(lv_scr_act(), NULL);
  lv_obj_set_size(gContainer1, 240, 240);
  lv_obj_set_style(gContainer1, &lv_style_transp_fit);
  lv_obj_set_pos(gContainer1, 0, 0);

 
  //lable1放在第1个容器内
  label1 = lv_label_create(gContainer1, NULL);
  lv_obj_set_pos(label1, 0, 0);
  //不会自动换行, 6*6=36个字
  lv_label_set_text(label1, "");
  lv_label_set_style(label1,  &style1);
  lv_obj_set_size(label1, 240, 240); 
}


//处理汉字换行
String split_str(String src_str)
{
  //return src_str;
  String newstr = "";
  String tmpstr = "";
  int tmplen = src_str.length() ;
  int loop1 = 0;
  Serial.println(tmplen);
  while (true)
  {

    if (src_str.length() > 0)
    {
      //处理前两个>>
      if (loop1 == 0)
        tmpstr = src_str.substring(0, 17);
      else
        tmpstr = src_str.substring(0, 21);
      if (tmpstr.length() > 0)
      {
        newstr = newstr + tmpstr + "\n";
        src_str.remove(0, tmpstr.length());
        Serial.println(tmpstr);
      }
      loop1 = loop1 + 1;
    }
    else
      break;
  }
  return newstr;

}
