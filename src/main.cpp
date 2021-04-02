#include <Arduino.h>
#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>
#include <ui.hpp>
#include <RTC.h>
#include <WiFi.h>
#include <worker.hpp>

namespace
{
  class TouchButton
  {
  public:
    using PressFunc = void (*)();

  private:
    int left;
    int right;
    int top;
    int bottom;
    bool press;
    PressFunc func;

  public:
    TouchButton(int l, int t, int r, int b)
        : left(l), right(r), top(t), bottom(b), press(false), func(nullptr)
    {
    }

    void setPressFunction(PressFunc f) { func = f; }

    void check(int x, int y, bool touch)
    {
      if (touch)
      {
        bool p = left < x && x < right && top < y && y < bottom;
        if (func && p && !press)
          func();
        press = p;
      }
      else
        press = false;
    }

    bool onPressed() const { return press; }
  };
  TouchButton Btn0(10, 241, 120, 280);
  TouchButton Btn1(130, 241, 200, 280);
  TouchButton Btn2(230, 241, 310, 280);

  LGFX gfx;
  UI::Control ctrl;
  RTC rtc;

  UI::CheckBox chkBox1;
  UI::TextButton wifiBtn;
  UI::TextButton dateBtn;
  RTC_TimeTypeDef nTime;

  UI::TextButton reqBtn;
  UI::TextButton retBtn;

  UI::ListBox apList;

  hw_timer_t *timer = nullptr;
  portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
  volatile int vcnt = 0;

  enum LayerID : int
  {
    lyDEFAULT,
    lyWIFI,
    lyDATETIME,
  };

  const char *ssid = "TEST-AP";
  const char *password = "************";
  const char *ntpServer = "ntp.jst.mfeed.ad.jp";
  constexpr int TimeZone = 9 * 3600;

  Worker::Task worker;
}

void adjustDayTime()
{
  WiFi.begin(ssid, password);
  Serial.print("Wifi connect:");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\n");

  // get time from NTP server(UTC+9)
  configTime(TimeZone, 0, ntpServer);

  // Get local time
  struct tm timeInfo;
  if (getLocalTime(&timeInfo))
  {
    // Set RTC time
    RTC_TimeTypeDef rtime;
    rtime.Hours = timeInfo.tm_hour;
    rtime.Minutes = timeInfo.tm_min;
    rtime.Seconds = timeInfo.tm_sec;
    RTC_DateTypeDef rdate;
    rdate.WeekDay = timeInfo.tm_wday;
    rdate.Month = timeInfo.tm_mon + 1;
    rdate.Date = timeInfo.tm_mday;
    rdate.Year = timeInfo.tm_year + 1900;
    // TODO: ここは排他にするべき
    rtc.SetTime(&rtime);
    rtc.SetDate(&rdate);
  }

  //disconnect WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("setting done.");
}

bool updateTime()
{
  RTC_TimeTypeDef t;
  rtc.GetTime(&t);
  if (t.Hours == nTime.Hours && t.Minutes == nTime.Minutes && t.Seconds == nTime.Seconds)
    return false;
  nTime = t;
  return true;
}

void buttonUpdate(int x, int y, bool touch)
{
  Btn0.check(x, y, touch);
  Btn1.check(x, y, touch);
  Btn2.check(x, y, touch);
}

void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  vcnt++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Launch");
  gfx.init();
  rtc.begin();

  gfx.setFont(&fonts::lgfxJapanGothic_24);
  ctrl.init(&gfx);

  // main
  ctrl.setLayer(lyDEFAULT);
  ctrl.appendWidget(&chkBox1);
  ctrl.appendWidget(&wifiBtn);
  ctrl.appendWidget(&dateBtn);

  int topY = 10;
  chkBox1.setCaption("ボタンです");
  chkBox1.setGeometory(40, topY);
  topY += chkBox1.getHeight() + 5;
  wifiBtn.setCaption("Wifi設定");
  wifiBtn.setGeometory(40, topY);
  wifiBtn.setPressFunction([](UI::Widget *) { ctrl.setLayer(lyWIFI); });
  topY += wifiBtn.getHeight() + 5;
  dateBtn.setCaption("日付・時刻");
  dateBtn.setGeometory(60, topY);
  dateBtn.setPressFunction([](UI::Widget *) { ctrl.setLayer(lyDATETIME); });

  // time
  ctrl.setLayer(lyDATETIME);
  ctrl.appendWidget(&reqBtn);
  ctrl.appendWidget(&retBtn);
  topY = 50;
  reqBtn.setCaption("時刻合わせ");
  reqBtn.setGeometory(50, topY);
  reqBtn.setPressFunction([](UI::Widget *) {
    static int test = 0;
    worker.signal(
        [](int n) {
          adjustDayTime();
        },
        test);
    test++;
  });
  topY += reqBtn.getHeight() + 5;
  retBtn.setCaption("戻る");
  retBtn.setGeometory(50, topY);
  retBtn.setPressFunction([](UI::Widget *) { ctrl.setLayer(lyDEFAULT); });

  // wifi8
  ctrl.setLayer(lyWIFI);
  ctrl.appendWidget(&apList);
  topY = 10;
  apList.setGeometory(20, topY);
  apList.init(6, 200);
  apList.append("List1");
  apList.append("Item 2");
  apList.append("Box-3");
  apList.append("String_4");
  apList.append("Wifi5");
  apList.append("TEST=6");
  apList.setSelectFunction([](int idx, const char *str) {
    char buff[32];
    snprintf(buff, sizeof(buff), "Select: %d %s", idx, str);
    Serial.println(buff);
  });

  //
  ctrl.setLayer(lyDEFAULT);
  Btn0.setPressFunction([] {
    if (ctrl.getLayer() == lyWIFI)
      ctrl.setLayer(lyDEFAULT);
  });

  //
  int vsync = 100 * 1000 * 1000 / 5995;
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, vsync, true);
  timerAlarmEnable(timer);

  worker.start();
}

void loop()
{
  int lvcnt;
  portENTER_CRITICAL(&timerMux);
  lvcnt = vcnt;
  portEXIT_CRITICAL(&timerMux);

  static bool touch_first = true;
  static int x, y;
  int tch = 0;
  if (gfx.touch())
  {
    if (gfx.getTouch(&x, &y, 0))
    {
      ctrl.touchCheck(x, y, touch_first);
      tch++;
    }
    buttonUpdate(x, y, tch > 0);
    touch_first = tch == 0;
  }

  updateTime();
  {
    char buff[24];
    gfx.startWrite();
    ctrl.drawWidgets();
    gfx.setTextColor(TFT_YELLOW);
    // static int dx = 0, dy = 0;
    // bool dtch = false;
    // if (dx != x || dy != y || dtch != tch)
    // {
    //   gfx.fillRect(20, 180, 240, 20, TFT_BLACK);
    //   snprintf(buff, sizeof(buff), "%d x %d(%d)", x, y, tch);
    //   gfx.drawString(buff, 20, 180);
    //   dx = x;
    //   dy = y;
    //   dtch = tch;
    // }
    static int ns = 0;
    if (ns != nTime.Seconds)
    {
      gfx.fillRect(20, 200, 240, 20, TFT_BLACK);
      snprintf(buff, sizeof(buff), "%02d:%02d.%02d", nTime.Hours, nTime.Minutes, nTime.Seconds);
      gfx.drawString(buff, 50, 200);
      ns = nTime.Seconds;
    }
    gfx.fillRect(20, 230, 60, 10, Btn0.onPressed() ? TFT_BLUE : TFT_BLACK);
    gfx.fillRect(130, 230, 60, 10, Btn1.onPressed() ? TFT_RED : TFT_BLACK);
    gfx.fillRect(240, 230, 60, 10, Btn2.onPressed() ? TFT_GREEN : TFT_BLACK);
    gfx.endWrite();
  }

  bool wait = true;
  while (wait)
  {
    portENTER_CRITICAL(&timerMux);
    wait = lvcnt == vcnt;
    portEXIT_CRITICAL(&timerMux);
  }
  //delay(50);
}
