#include <Arduino.h>
#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>
#include <ui.hpp>
#include <RTC.h>
#include <WiFi.h>
#include <worker.hpp>
#include <store.hpp>
#include <SD.h>

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
  Store::Data store;

  UI::CheckBox chkBox1;
  UI::TextButton wifiBtn;
  UI::TextButton imgBtn;
  UI::TextButton dateBtn;
  RTC_TimeTypeDef nTime;

  UI::TextButton reqBtn;
  UI::TextButton retBtn;

  UI::ListBox apList;

  UI::ListBox imgList;

  UI::Keyboard keyboard;

  hw_timer_t *timer = nullptr;
  portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
  volatile int vcnt = 0;
  bool updateSSID = false;

  enum LayerID : int
  {
    lyDEFAULT,
    lyWIFI,
    lyDATETIME,
    lyWIFIPW,
    lyIMGLIST,
  };

  char ssid[32];
  char password[32];
  const char *ntpServer = "ntp.jst.mfeed.ad.jp";
  constexpr int TimeZone = 9 * 3600;

  Worker::Task worker;
  volatile bool wifiScanLoop = true;
  void cancelScanWifi()
  {
    wifiScanLoop = false;
    Serial.println("wifi scan cancel");
  }
}

//
// 時刻
//
void adjustDayTime()
{
  if (ssid[0] == '\0')
  {
    Serial.println("adjust time failed: no ssid");
    return;
  }
  WiFi.begin(ssid, password);
  Serial.printf("Wifi connect:[%s]", ssid);
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

//
bool updateTime()
{
  RTC_TimeTypeDef t;
  rtc.GetTime(&t);
  if (t.Hours == nTime.Hours && t.Minutes == nTime.Minutes && t.Seconds == nTime.Seconds)
    return false;
  nTime = t;
  return true;
}

//
// Wifi scan
//
void scanWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.scanNetworks(true);

  bool scan = true;
  while (wifiScanLoop)
  {
    int ret = WiFi.scanComplete();
    if (ret == WIFI_SCAN_FAILED)
    {
      Serial.println("failed... retry");
      WiFi.scanDelete();
      WiFi.scanNetworks(true);
      scan = true;
    }
    else if (ret == WIFI_SCAN_RUNNING)
    {
      if (scan)
        Serial.println("scanning...");
      scan = false;
    }
    else if (ret == 0)
    {
      Serial.println("no networks");
      break;
    }
    else
    {
      for (int i = 0; i < ret; i++)
      {
        auto ssid = WiFi.SSID(i);
        Serial.println(ssid);
        apList.append(ssid.c_str());
      }
      WiFi.scanDelete();
      break;
    }
    delay(100);
  }
  Serial.println("wifi scan done");
}

//
// SD "/" scan
//
void scanFileSD()
{
  Serial.println("SD scan");
  if (File dir = SD.open("/"))
  {
    while (File file = dir.openNextFile())
    {
      Serial.println(file.name());
      if (file.isDirectory() == false)
        imgList.append(file.name());
    }
  }
  Serial.println("SD scan done");
}

//
// Application
//
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
  SD.begin(4);
  store.init("TEST", 128);

  gfx.setFont(&fonts::lgfxJapanGothic_24);
  ctrl.init(&gfx);

  // main
  ctrl.setLayer(lyDEFAULT);
  ctrl.appendWidget(&chkBox1);
  ctrl.appendWidget(&imgBtn);
  ctrl.appendWidget(&wifiBtn);
  ctrl.appendWidget(&dateBtn);

  int topY = 10;
  chkBox1.setCaption("情報表示");
  chkBox1.setGeometory(40, topY);
  chkBox1.setValue(true);
  topY += chkBox1.getHeight() + 5;
  imgBtn.setCaption("画像リスト");
  imgBtn.setGeometory(40, topY);
  imgBtn.setPressFunction([](UI::Widget *) {
    ctrl.setLayer(lyIMGLIST);
    worker.signal([](int) { scanFileSD(); }, 0);
  });
  topY += imgBtn.getHeight() + 5;
  wifiBtn.setCaption("Wifi設定");
  wifiBtn.setGeometory(40, topY);
  wifiBtn.setPressFunction([](UI::Widget *) {
    ctrl.setLayer(lyWIFI);
    apList.clear();
    wifiScanLoop = true;
    worker.signal([](int) { scanWifi(); }, 0);
  });
  topY += wifiBtn.getHeight() + 5;
  dateBtn.setCaption("日付・時刻");
  dateBtn.setGeometory(40, topY);
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

  // wifi
  ctrl.setLayer(lyWIFI);
  ctrl.appendWidget(&apList);
  topY = 10;
  apList.setGeometory(20, topY);
  apList.init(6, 240);
  apList.setSelectFunction([](int idx, const char *str) {
    cancelScanWifi();
    strlcpy(ssid, str, sizeof(ssid));
    Serial.println(ssid);
    ctrl.setLayer(lyWIFIPW);
  });

  // keyboard
  ctrl.setLayer(lyWIFIPW);
  ctrl.appendWidget(&keyboard);
  topY = 20;
  keyboard.init(24);
  keyboard.setGeometory(10, topY);
  keyboard.setPlaceHolder("wifi password");
  if (store.loadString(0, password, sizeof(password)))
    keyboard.setString(password);
  else
    strlcpy(password, "", sizeof(password));
  if (store.loadString(1, ssid, sizeof(ssid)))
    updateSSID = true;
  else
    strlcpy(ssid, "", sizeof(ssid));

  // image list
  ctrl.setLayer(lyIMGLIST);
  ctrl.appendWidget(&imgList);
  topY = 10;
  imgList.init(20, 240, 180);
  imgList.setGeometory(20, topY);
  imgList.setSelectFunction([](int idx, const char *str) {
    Serial.println(str);
  });

  //
  ctrl.setLayer(lyDEFAULT);
  Btn0.setPressFunction([] {
    switch (ctrl.getLayer())
    {
    case lyWIFI:
      cancelScanWifi();
      ctrl.setLayer(lyDEFAULT);
      break;
    case lyWIFIPW:
      ctrl.setLayer(lyDEFAULT);
      break;
    case lyIMGLIST:
      ctrl.setLayer(lyDEFAULT);
      break;
    default:
      break;
    }
  });
  Btn1.setPressFunction([] {
    switch (ctrl.getLayer())
    {
    case lyWIFIPW:
      keyboard.getString(password, sizeof(password));
      store.clearIndex();
      store.storeString(password);
      store.storeString(ssid);
      ctrl.setLayer(lyDEFAULT);
      updateSSID = true;
      break;
    case lyIMGLIST:
      imgList.scoll(-1);
      break;
    default:
      break;
    }
  });
  Btn2.setPressFunction([] {
    switch (ctrl.getLayer())
    {
    case lyIMGLIST:
      imgList.scoll(1);
      break;
    default:
      break;
    }
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
    if (chkBox1.getValue())
    {
      gfx.setTextColor(TFT_YELLOW);
      static int ns = 0;
      if (ns != nTime.Seconds)
      {
        gfx.fillRect(5, 205, 110, 24, TFT_BLACK);
        snprintf(buff, sizeof(buff), "%02d:%02d.%02d", nTime.Hours, nTime.Minutes, nTime.Seconds);
        gfx.drawString(buff, 5, 205);
        ns = nTime.Seconds;
      }
      if (updateSSID)
      {
        gfx.fillRect(120, 205, 200, 24, TFT_BLACK);
        gfx.drawString(ssid, 120, 205);
        updateSSID = false;
      }
    }
    else
    {
      gfx.fillRect(0, 205, 320, 24, TFT_BLACK);
      updateSSID = true;
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
