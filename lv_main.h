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
#include <String.h>
#include <Arduino.h>
#include <lvgl.h>


String split_str(String src_str);
void lv_create_ttgo();
void lv_set_text1(String outtext);
//void lv_set_text2(String outtext);
