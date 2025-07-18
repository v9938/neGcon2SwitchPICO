////////////////////////////////////////////////////////////////////////
//
// PSX NeG Controller to Switch Joystick Sample
// raspberry PICO version
// Copyright 2025 @V9938
//
//	25/06/07 V0.1	1st version
//	25/06/07 V0.2	fix I/II/L Analog Value
//  25/06/10 V0.5   1st PICO version
//  25/06/11 V0.6   Support NeoPixel
//  25/06/11 V0.7   Support DX mode
//  25/07/03 V1.0   Support Aircombat22 mode with SCPH1110
//                  Fixed bug Digtal mode setting
//  25/07/03 V1.1   Support Controller HORI ANALOG SHINDOU PAD
//  25/07/12 V1.2   Support I/II button swap mode
//                  Fixed bug neGcon mode setting
//                  Support save & restore neGcon mode
//                  Change LED Pattern standard PSX Controller
//                  Adjust I/II button value (x1.1)
//  25/07/13 V1.3   Support maximum tilt setting(neGcon/Analog Stick).
//
//
// This program requires same librarys
// "Adafruit_NeoPixel"
//
// "PsxNewLib”
// https://github.com/SukkoPera/PsxNewLib
//
// "SwitchControllerPico"
// https://github.com/sorasen2020/SwitchControllerPico
//
// USB Stack setting "Adafruit_TinyUSB_Library"
// If modifying the PID/VID values in 'board.txt' is not possible at v2.xx IDE,
// please add the settings to the file listed below."
//
// C:\Users\[UserName]\Documents\Arduino\libraries\Adafruit_TinyUSB_Library\src\arduino\Adafruit_USBD_Device.cpp
// // added custom PID
// #define USB_PRODUCT     "Pokken Tournament DX Pro Pad"
// #define USB_MANUFACTURER "HORI"
// #define USB_VID 0x0f0d
// #define USB_PID 0x0092
//
////////////////////////////////////////////////////////////////////////

#include "Arduino.h"
#include "PsxControllerPICO_HwSpi2.h"
#include "SwitchControllerPico.h"
#include "NintendoSwitchControllPico.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

//#define SERIALVAL_OUT

// EEPROMの設定
#define EEPROMSIZE 256
// EEPROMのアドレス配置
#define EEPADR_NEGMODE 3
#define EEPADR_NEG_NEGMAX 4
#define EEPADR_ANALOG_STICKMAX 5

// neGcon ハンドルアナログ値の補正
// これ以上の補正が必要な場合は、設定→ボタン→こだわりのボタン設定→感度設定を弄ってください。
#define NEG_CALIB 1.2

// neGcon ボタン アナログ値の補正
// これ以上の補正が必要な場合は、設定→ボタン→こだわりのボタン設定→感度設定を弄ってください。
#define NEG_CALIB_B 1.1

//NeoPixelの設定
#define NUMPIXELS 1
#define NEO_PWR 11  //GPIO11
#define NEOPIX 12   //GPIO12

//Stick Mode
#define MODE_STD 0
#define MODE_DX 1
#define MODE_AIRCON22 2
#define MODE_ANALOG 3
#define MODE_STDSWAP 4
#define MODE_DXSWAP 5
#define MODE_NEGDIGTAL 6
#define MODE_DIGTAL 7

#define MODE_SETTING_NEG 98
#define MODE_LOST 99

//PWM modeの1LOOP回数 数字を細かくするとON-OFF間隔が短くなる(8ms x PWM_LOOP)
#define PWM_LOOP 30


/** \brief Pin used for Controller Attention (ATTN)
 *
 * This pin makes the controller pay attention to what we're saying. The shield
 * has pin 10 wired for this purpose.
 */
const unsigned int PIN_PS2_CMD = 4;  //MOSI
const unsigned int PIN_PS2_DAT = 3;  //MOSO
const unsigned int PIN_PS2_CLK = 2;  //SCK
const unsigned int PIN_PS2_ATT = 1;  //CS

// Set up the speed, data order and data mode
static SPISettings spiSettings(250000, LSBFIRST, SPI_MODE3);

/** \brief Pin for Button Press Led
 *
 * This led will light up whenever a button is pressed on the controller.
 */
const unsigned int PIN_CONNECT = 25;   // BLUE
const unsigned int PIN_ERRORLED = 17;  // RED
const unsigned int PIN_GOODLED = 16;   // GREEN

bool haveController = false;
/** \brief Dead zone for analog sticks
 *  
 * If the analog stick moves less than this value from the center position, it
 * is considered still.
 * 
 * \sa ANALOG_IDLE_VALUE
 */
const byte ANALOG_DEAD_ZONE = 50U;

PsxControllerPICO_HwSpi<PIN_PS2_ATT> psx;

const uint8_t desc_hid_report[] = {
  CUSTOM_DESCRIPTOR
};

Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIX, NEO_GRB + NEO_KHZ800);

// Controller Type
const char ctrlTypeUnknown[] PROGMEM = "Unknown";
const char ctrlTypeDualShock[] PROGMEM = "Dual Shock";
const char ctrlTypeDsWireless[] PROGMEM = "Dual Shock Wireless";
const char ctrlTypeGuitHero[] PROGMEM = "3rd Party Controller";
const char ctrlTypeOutOfBounds[] PROGMEM = "(Out of bounds)";

const char* const controllerTypeStrings[PSCTRL_MAX + 1] PROGMEM = {
  ctrlTypeUnknown,
  ctrlTypeDualShock,
  ctrlTypeDsWireless,
  ctrlTypeGuitHero,
  ctrlTypeOutOfBounds
};


// Controller Protocol
const char ctrlProtoUnknown[] PROGMEM = "Unknown";
const char ctrlProtoDigital[] PROGMEM = "Digital";
const char ctrlProtoDualShock[] PROGMEM = "Dual Shock";
const char ctrlProtoDualShock2[] PROGMEM = "Dual Shock 2";
const char ctrlProtoFlightstick[] PROGMEM = "Flightstick（SCPH-1110)";
const char ctrlProtoNegcon[] PROGMEM = "namco neGcon";
const char ctrlProtoJogcon[] PROGMEM = "namco Jogcon";
const char ctrlProtoOutOfBounds[] PROGMEM = "(Out of bounds)";

const char* const controllerProtoStrings[PSPROTO_MAX + 1] PROGMEM = {
  ctrlProtoUnknown,
  ctrlProtoDigital,
  ctrlProtoDualShock,
  ctrlProtoDualShock2,
  ctrlProtoFlightstick,
  ctrlProtoNegcon,
  ctrlProtoJogcon,
  ctrlTypeOutOfBounds
};

const Hat hatValue[0x10] = {
  Hat::CENTER,
  Hat::UP,
  Hat::RIGHT,
  Hat::UP_RIGHT,
  Hat::DOWN,
  Hat::CENTER,
  Hat::RIGHT_DOWN,
  Hat::CENTER,
  Hat::LEFT,
  Hat::LEFT_UP,
  Hat::CENTER,
  Hat::CENTER,
  Hat::DOWN_LEFT,
  Hat::CENTER,
  Hat::CENTER,
  Hat::CENTER
};

USB_JoystickReport_Input_t t_joystickInputData;
byte ledLx, ledLy, ledRx, ledRy, ledB1, ledB2, ledBL;
byte stickMode;
byte lxMax;
byte analogLxMax;


// EEPROMのClear関数
void eepromFormat() {
  Serial.println(F("EEP Write!"));
  for (int i = 0; i < EEPROMSIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.write(0, 'c');
  EEPROM.write(1, 'n');
  EEPROM.write(2, 'f');
  EEPROM.write(EEPADR_NEGMODE, MODE_STD);
  EEPROM.write(EEPADR_NEG_NEGMAX, 255 / ((NEG_CALIB - 1) / 2 + 1));
  EEPROM.write(EEPADR_ANALOG_STICKMAX, 255);
  EEPROM.commit();
}

// EEPROM領域の確認
bool eepromCheck() {
  if (EEPROM.read(0) != 'c') return false;
  if (EEPROM.read(1) != 'n') return false;
  if (EEPROM.read(2) != 'f') return false;
  return true;
}

// neGconモードひねり値の最大値の復帰
byte restoreNegDegMax() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_NEG_NEGMAX);

  // 有効値Check
  if (tmp < 0x80) {
    tmp = 255 / ((NEG_CALIB - 1) / 2 + 1);
    Serial.println(F("EEP Write!"));
    EEPROM.write(EEPADR_NEG_NEGMAX, tmp);
    EEPROM.commit();
  }

  return tmp;
}

// アナログスティックモードひねり値の最大値の復帰
byte restoreAnaDegMax() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_ANALOG_STICKMAX);

  // 有効値Check
  if (tmp < 0x80) {
    tmp = 255;

    Serial.println(F("EEP Write!"));
    EEPROM.write(EEPADR_ANALOG_STICKMAX, tmp);
    EEPROM.commit();
  }

  return tmp;
}

// neGconモード設定の復帰
byte restoreNegStickMode() {
  byte tmp;
  tmp = EEPROM.read(EEPADR_NEGMODE);

  // 有効値Check
  switch (tmp) {
    case MODE_STD:
    case MODE_STDSWAP:
    case MODE_DX:
    case MODE_DXSWAP:
      break;

    default:
      tmp = MODE_STD;
      Serial.println(F("EEP Write!"));
      EEPROM.write(EEPADR_NEGMODE, tmp);
      EEPROM.commit();
      break;
  }

  return tmp;
}

// 絶対値 計算関数
int absoluteXY(byte lx) {
  int lx_tmp;

  if (lx < 0x80) lx_tmp = (int)(0xFF - lx);
  else lx_tmp = (int)lx;
  return lx_tmp;
}

// 補正値計算
int adjustXY(byte lx, byte max) {
  int lx_tmp;

  if (lx > 0x80) {
    lx_tmp = (int)((lx - 0x80)) * 0x7f / (max - 0x80);
    if (lx_tmp > 0x7f) lx_tmp = 0x7f;
    lx_tmp = 0x80 + lx_tmp;
  } else {
    lx_tmp = (int)((0x80 - lx)) * 0x7f / (max - 0x80);
    if (lx_tmp > 0x80) lx_tmp = 0x80;
    lx_tmp = 0x80 - lx_tmp;
  }
  return lx_tmp;
}


// CPU1では、NeoPixel LEDの点灯処理をさせる
void setup1() {  //core 0
  pixels.begin();
  pinMode(NEO_PWR, OUTPUT);
  digitalWrite(NEO_PWR, HIGH);
  delay(1000);
}
void loop1() {  //core 0
  static byte heartbeat_num = 0;
  static bool heartbeat_flag = true;

  switch (stickMode) {
      // neGconモード時の点灯パターン
    case MODE_STD:
      pixels.setPixelColor(0, pixels.Color(ledB1 / 2 + ledBL / 2, ledB2 / 2 + ledBL / 2, ledLx));
      pixels.show();
      break;

      // neGconモード時の点灯パターン（I/II SWAPモード)
    case MODE_STDSWAP:
      pixels.setPixelColor(0, pixels.Color(0x80 - ledB1 + ledB2, 0, ledLx));
      pixels.show();
      break;

      // neGconモード時の点灯パターン DXモード時のパターン
    case MODE_DX:
      pixels.setPixelColor(0, pixels.Color(ledB2 / 2 + ledBL / 2, ledLx, ledB1 / 2 + ledBL / 2));
      pixels.show();
      break;

      // neGconモード時の点灯パターン DXモード時のパターン（I/II SWAPモード)
    case MODE_DXSWAP:
      pixels.setPixelColor(0, pixels.Color(0, 0x80 - ledB1 + ledB2, ledLx / 2 + ledBL / 2));
      pixels.show();
      break;

    // neGconモード時の点灯パターン デジタルモード時
    case MODE_NEGDIGTAL:
      pixels.setPixelColor(0, pixels.Color(0x20, 0x20, 0x20));
      pixels.show();
      break;

      // フライトコントローラ接続時の点灯パターン
    case MODE_AIRCON22:
      pixels.setPixelColor(0, pixels.Color(ledRy / 4, ledLx / 8, ledRy / 8 + 0x40));
      pixels.show();
      break;

      // DualShock接続時の点灯パターン
    case MODE_ANALOG:
      pixels.setPixelColor(0, pixels.Color(0, ledRy / 4 + ledLx / 2, ledRy / 2 + ledRx / 4));
      pixels.show();
      break;

      // その他接続時の点灯パターン
    case MODE_DIGTAL:
      pixels.setPixelColor(0, pixels.Color(0x0, 0x4, 0x8));
      pixels.show();
      break;

    case MODE_SETTING_NEG:
      pixels.setPixelColor(0, pixels.Color(0x0, 0x0, heartbeat_num));
      pixels.show();
      break;


      // 残念賞
    default:
      pixels.clear();
      pixels.show();
      break;
  }

  //LEDの点滅処理数値計算
  if (heartbeat_num == 0) heartbeat_flag = true;
  else if (heartbeat_num == 255) heartbeat_flag = false;

  if (heartbeat_flag) heartbeat_num++;
  else heartbeat_num--;
}

void setup() {
  switchcontrollerpico_init();
  // wait until device mounted
  while (!TinyUSBDevice.mounted()) delay(1);
  switchcontrollerpico_reset();
  pinMode(PIN_CONNECT, OUTPUT);
  pinMode(PIN_ERRORLED, OUTPUT);
  pinMode(PIN_GOODLED, OUTPUT);

  digitalWrite(PIN_CONNECT, HIGH);
  digitalWrite(PIN_ERRORLED, HIGH);
  digitalWrite(PIN_GOODLED, HIGH);


  gpio_init(PIN_PS2_CLK);  //SCK
  gpio_set_dir(PIN_PS2_CLK, GPIO_OUT);
  gpio_put(PIN_PS2_CLK, HIGH);

  gpio_init(PIN_PS2_CMD);  //MOSI
  gpio_set_dir(PIN_PS2_CMD, GPIO_OUT);
  gpio_put(PIN_PS2_CMD, HIGH);

  gpio_init(PIN_PS2_DAT);  //MISO
  gpio_set_dir(PIN_PS2_DAT, GPIO_IN);
  gpio_pull_up(PIN_PS2_DAT);

  gpio_init(PIN_PS2_ATT);  //SS(GPIO)
  gpio_set_dir(PIN_PS2_ATT, GPIO_OUT);
  gpio_put(PIN_PS2_ATT, HIGH);

  SPI.beginTransaction(spiSettings);
  EEPROM.begin(EEPROMSIZE);
  if (eepromCheck() != true) eepromFormat();

  //	SPI.setSCK(PIN_PS2_CLK);
  //	SPI.setMOSI(PIN_PS2_CMD);
  //	SPI.setMISO(PIN_PS2_DAT);

  lxMax = restoreNegDegMax();
  analogLxMax = restoreAnaDegMax();
  stickMode = MODE_LOST;
  delay(300);

  //	Serial.begin (115200);
  //	while (!Serial) {
  // Wait for serial port to connect on Leonardo boards
  //  		digitalWrite (PIN_BUTTONPRESS, (millis () / 333) % 2);
  //  }
  Serial.println(F("Ready!"));
}

void loop() {
  static int loop_num = 0;
  static int sBootSel = 0;
  static byte beforeStickMode;
  static bool changeNegStickMode;
  static byte slx, sly, sb1, sb2, sbL;
  static byte srx, sry;

  byte l_x, l_y, l_b1, l_b2, l_bL;
  byte lx_org, ly_org, b1_org, b2_org, bL_org;
  byte rx_org, ry_org;
  byte r_x, r_y;
  int l_x_tmp, xy_tmp;


  static PsxControllerType psxContType;
  PsxControllerProtocol psxStickMode;
  static PsxControllerProtocol OldpsxStickMode;

  bool bSendData;

  // BOOTスイッチの処理
  // 現状MODE選択に割り当てている
  if (BOOTSEL) {
    digitalWrite(PIN_ERRORLED, LOW);
    if (sBootSel < 3000) {
      if (sBootSel == 0) changeNegStickMode = false;
      if (sBootSel == 9) {
        changeNegStickMode = true;
        Serial.print(F("Current Stick mode is: "));
        Serial.println(stickMode);
      }
      if (sBootSel == 2999) {
        if ((OldpsxStickMode == PSPROTO_NEGCON) || (OldpsxStickMode == PSPROTO_JOGCON) || (OldpsxStickMode == PSPROTO_FLIGHTSTICK)) {
          Serial.println(F("Set Config mode"));
          changeNegStickMode = false;
          beforeStickMode = stickMode;
          stickMode = MODE_SETTING_NEG;
        }
      }
      sBootSel++;
    }
  } else {
    digitalWrite(PIN_ERRORLED, HIGH);
    if (changeNegStickMode) {

      // ネジコンモード時のモード選択処理
      if ((OldpsxStickMode == PSPROTO_NEGCON) || (OldpsxStickMode == PSPROTO_JOGCON)) {
        if (stickMode == MODE_STD) stickMode = MODE_DX;
        else if (stickMode == MODE_DX) stickMode = MODE_STDSWAP;
        else if (stickMode == MODE_STDSWAP) stickMode = MODE_DXSWAP;
        else if (stickMode == MODE_DXSWAP) stickMode = MODE_NEGDIGTAL;
        else if (stickMode == MODE_NEGDIGTAL) stickMode = MODE_STD;
        else stickMode == MODE_STD;

        Serial.println(F("EEP Write!"));
        EEPROM.write(EEPADR_NEGMODE, stickMode);
        EEPROM.commit();
      }

      // 設定のキャンセル処理
      if (stickMode == MODE_SETTING_NEG) stickMode = beforeStickMode;

      Serial.print(F("Change Stick mode is: "));
      Serial.println(stickMode);
    }

    changeNegStickMode = false;
    sBootSel = 0;
  }


  if (!haveController) {
    if (psx.begin()) {
      //    	digitalWrite(PIN_CONNECT, LOW);
      Serial.println(F("Controller found!"));
      delay(300);
      if (!psx.enterConfigMode()) {
        //古いタイプのコントローラではHOSTから設定できないのでこちらに来る
        Serial.println(F("Cannot enter config mode"));
        psxContType = psx.getControllerType();
        Serial.print(F("Controller Type is: "));
        Serial.println(controllerTypeStrings[psxContType < PSCTRL_MAX ? static_cast<byte>(psxContType) : PSCTRL_MAX]);
        psx.read();  // Make sure the protocol is up to date
                     //     digitalWrite(PIN_ERRORLED, LOW);

      } else {
        //Dual shock以降はこちらに来る。HORIのアナログ振動ジョイパッドはDUALSHOCKに設定される
        psxContType = psx.getControllerType();
        Serial.print(F("Controller Type is: "));
        Serial.println(controllerTypeStrings[psxContType < PSCTRL_MAX ? static_cast<byte>(psxContType) : PSCTRL_MAX]);

        if (!psx.enableAnalogSticks()) {
          Serial.println(F("Cannot enable analog sticks"));
        }

        if (!psx.enableAnalogButtons()) {
          Serial.println(F("Cannot enable analog buttons"));
        }

        if (!psx.exitConfigMode()) {
          Serial.println(F("Cannot exit config mode"));
        }
      }

      psx.read();  // Make sure the protocol is up to date
                   //     	digitalWrite(PIN_ERRORLED, LOW);

      psxStickMode = psx.getProtocol();
      Serial.print(F("Controller Protocol is: "));
      Serial.println(controllerProtoStrings[psxStickMode < PSPROTO_MAX ? static_cast<byte>(psxStickMode) : PSPROTO_MAX]);
      haveController = true;
    }
  } else {
    if (!psx.read()) {
      //     	digitalWrite(PIN_CONNECT, HIGH);
      digitalWrite(PIN_GOODLED, HIGH);

      Serial.println(F("Controller lost :("));
      haveController = false;
      psxContType = PSCTRL_UNKNOWN;  //Stickモードを更新しておく
      psxStickMode = PSPROTO_UNKNOWN;
      OldpsxStickMode = PSPROTO_UNKNOWN;
      stickMode = MODE_LOST;

    } else {
      digitalWrite(PIN_GOODLED, LOW);  //Controller認識OK
      // PWM mode用LOOP回数処理
      if (loop_num >= PWM_LOOP) loop_num = 0;
      else loop_num++;
      psxStickMode = psx.getProtocol();  //現在のStickモードを取得

      if (psxStickMode != OldpsxStickMode) {
        // 動的にコントローラモードの変更されるもののみ列挙
        // neGconは、モード切替が無いのでここでは列挙しない
        if (psxStickMode == PSPROTO_DIGITAL) stickMode = MODE_DIGTAL;
        if (psxStickMode == PSPROTO_FLIGHTSTICK) stickMode = MODE_AIRCON22;
        if ((psxStickMode == PSPROTO_DUALSHOCK) || (psxStickMode == PSPROTO_DUALSHOCK2)) stickMode = MODE_ANALOG;
        if ((psxStickMode == PSPROTO_NEGCON) || (psxStickMode == PSPROTO_JOGCON)) stickMode = restoreNegStickMode();

        Serial.print(F("Controller Protocol is: "));
        Serial.println(controllerProtoStrings[psxStickMode < PSPROTO_MAX ? static_cast<byte>(psxStickMode) : PSPROTO_MAX]);
      }
      OldpsxStickMode = psxStickMode;

      // buttonデータを一端初期化
      t_joystickInputData.Button = 0;


      // 以下各スティック向け処理を以下に記述
      switch (psxStickMode) {

        // SPCH-1110 FLIGHTSTICK mode (AirCombat22 mode)
        // ボタン設定はdefaultのモノに最適化してあります
        case PSPROTO_FLIGHTSTICK:
          //右スティックの処理
          if (psx.getRightAnalog(lx_org, ly_org)) {
            l_x = lx_org;
            l_y = ly_org;
          } else {
            l_x = 0x80;
            l_y = 0x80;
          }

          //左スティックの処理
          if (psx.getLeftAnalog(rx_org, ry_org)) {
            r_x = rx_org;
            r_y = ry_org;
          } else {
            r_x = 0x80;
            r_y = 0x80;
          }
          if (l_x != slx || l_y != sly || r_x != srx || r_y != sry) {
            slx = l_x;
            sly = l_y;
            srx = r_x;
            sry = r_y;

#ifdef SERIALVAL_OUT
            Serial.print(F("DEBUG : (ORG)LX: "));
            Serial.print(l_x);
            Serial.print(F(" LY: "));
            Serial.print(l_y);
            Serial.print(F(" RX: "));
            Serial.print(r_x);
            Serial.print(F(" RY: "));
            Serial.print(r_y);
#endif
            l_x = (byte)adjustXY(l_x, analogLxMax);
            l_y = (byte)adjustXY(l_y, analogLxMax);
            r_x = (byte)adjustXY(r_x, analogLxMax);
            r_y = (byte)adjustXY(r_y, analogLxMax);

#ifdef SERIALVAL_OUT
            Serial.print(F(" | (FIXED)LX: "));
            Serial.print(l_x);
            Serial.print(F(" LY: "));
            Serial.print(l_y);
            Serial.print(F(" RX: "));
            Serial.print(r_x);
            Serial.print(F(" RY: "));
            Serial.println(r_y);
#endif
            ledLx = l_x;
            ledLy = l_y;
            ledRx = r_x;
            ledRy = r_y;

            // 最終的な値をセット (ハンドル)
            t_joystickInputData.LX = (uint8_t)(l_x);
            t_joystickInputData.LY = (uint8_t)(l_y);
            t_joystickInputData.RX = (uint8_t)(r_x);
            t_joystickInputData.RY = (uint8_t)(r_y);
          }
          //最大角、設定モード
          if (stickMode == MODE_SETTING_NEG) {
            if (psx.getButtonWord() & PSB_START) {
              Serial.print(F("Analog lxMax before: "));
              Serial.println(analogLxMax);
              //センターからの相対値に変更
              l_x_tmp = absoluteXY(lx_org);

              xy_tmp = absoluteXY(ly_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              xy_tmp = absoluteXY(rx_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              xy_tmp = absoluteXY(ry_org);
              if (xy_tmp > l_x_tmp) l_x_tmp = xy_tmp;

              //センター値近辺の場合は初期値に設定
              if (l_x_tmp < (0x80 + 10)) l_x_tmp = 255;

              //設定値を保存
              analogLxMax = (byte)l_x_tmp;
              Serial.println(F("EEP Write!"));
              EEPROM.write(EEPADR_ANALOG_STICKMAX, analogLxMax);
              EEPROM.commit();

              Serial.print(F("Analog lxMax after: "));
              Serial.println(analogLxMax);
              stickMode = beforeStickMode;
            }
          }


          // hat_switch（これは共通処理)
          t_joystickInputData.Hat = (uint8_t)(hatValue[(psx.getButtonWord() & 0x00f0) >> 4]);

          // buttonSelect = Button::MINUS
          if (psx.getButtonWord() & PSB_SELECT)
            t_joystickInputData.Button |= (uint16_t)(Button::MINUS);

          // buttonStart = Button::PLUS
          if (psx.getButtonWord() & PSB_START)
            t_joystickInputData.Button |= (uint16_t)(Button::PLUS);
          // buttonL2 = Button::L
          if (psx.getButtonWord() & PSB_L2)
            t_joystickInputData.Button |= (uint16_t)(Button::L);

          // buttonR2 = Button::R
          if (psx.getButtonWord() & PSB_R2)
            t_joystickInputData.Button |= (uint16_t)(Button::R);

          // buttonL1 = Button::ZL
          if (psx.getButtonWord() & PSB_L1)
            t_joystickInputData.Button |= (uint16_t)(Button::ZL);

          // buttonR1 = Button::ZR
          if (psx.getButtonWord() & PSB_R1)
            t_joystickInputData.Button |= (uint16_t)(Button::ZR);

          // buttonTriangle = Button::X
          if (psx.getButtonWord() & PSB_TRIANGLE)
            t_joystickInputData.Button |= (uint16_t)(Button::X);

          // buttonCircle = Button::A
          if (psx.getButtonWord() & PSB_CIRCLE)
            t_joystickInputData.Button |= (uint16_t)(Button::A);


          // buttonCross = Button::Y
          if (psx.getButtonWord() & PSB_CROSS)
            t_joystickInputData.Button |= (uint16_t)(Button::Y);

          // buttonSquare = Button::B
          if (psx.getButtonWord() & PSB_SQUARE)
            t_joystickInputData.Button |= (uint16_t)(Button::B);
          break;


        // NeGcon/joGconのボタン配置処理(リッジレーサモード)
        case PSPROTO_NEGCON:
        case PSPROTO_JOGCON:
          // hat_switch（これは共通処理)
          t_joystickInputData.Hat = (uint8_t)(hatValue[(psx.getButtonWord() & 0x00f0) >> 4]);

          // buttonSelect = Button::MINUS
          if (psx.getButtonWord() & PSB_SELECT)
            t_joystickInputData.Button |= (uint16_t)(Button::MINUS);

          // buttonStart = Button::PLUS
          if (psx.getButtonWord() & PSB_START)
            t_joystickInputData.Button |= (uint16_t)(Button::PLUS);

          // buttonL2 = Button::ZL
          if (psx.getButtonWord() & PSB_L2)
            t_joystickInputData.Button |= (uint16_t)(Button::ZL);

          // buttonR2 = Button::ZR
          if (psx.getButtonWord() & PSB_R2)
            t_joystickInputData.Button |= (uint16_t)(Button::ZR);

          // buttonR1 = Button::R
          if (psx.getButtonWord() & PSB_R1)
            t_joystickInputData.Button |= (uint16_t)(Button::R);

          // buttonTriangle = Button::X
          if (psx.getButtonWord() & PSB_TRIANGLE)
            t_joystickInputData.Button |= (uint16_t)(Button::X);

          // buttonCircle = Button::A
          if (psx.getButtonWord() & PSB_CIRCLE)
            t_joystickInputData.Button |= (uint16_t)(Button::A);

          // ネジコンアナログ値取得
          if (psx.getLeftAnalog(lx_org, ly_org)) {

            if ((stickMode == MODE_STDSWAP) || (stickMode == MODE_DXSWAP)) {
              // I/II button SWAP mode
              b2_org = psx.getAnalogButton(PsxAnalogButton::PSAB_CROSS);
              b1_org = psx.getAnalogButton(PsxAnalogButton::PSAB_SQUARE);
            } else {
              // I/II button NORMAL mode
              b1_org = psx.getAnalogButton(PsxAnalogButton::PSAB_CROSS);
              b2_org = psx.getAnalogButton(PsxAnalogButton::PSAB_SQUARE);
            }

            bL_org = psx.getAnalogButton(PsxAnalogButton::PSAB_L1);

            l_x = lx_org;
            l_y = ly_org;

            l_b1 = b1_org / 2;
            l_b2 = b2_org / 2;
            l_bL = bL_org / 2;

            if (stickMode == MODE_NEGDIGTAL) {
              digitalWrite(PIN_CONNECT, LOW);
              // I buttonのデジタル化処理（アソビ領域判定）
              if (l_b1 > 0x10) t_joystickInputData.Button |= (uint16_t)(Button::ZR);
              // II buttonのデジタル化処理（アソビ領域判定）
              if (l_b2 > 0x10) t_joystickInputData.Button |= (uint16_t)(Button::ZL);
              // L buttonのデジタル化処理（アソビ領域判定）
              if (l_bL > 0x60) t_joystickInputData.Button |= (uint16_t)(Button::L);
            }

            if ((stickMode == MODE_STD) || (stickMode == MODE_STDSWAP)) {
              digitalWrite(PIN_CONNECT, LOW);

              // L buttonのデジタル化処理（アソビ領域判定）
              if (l_bL > 0x60) t_joystickInputData.Button |= (uint16_t)(Button::L);
            }

            if ((stickMode == MODE_DX) || (stickMode == MODE_DXSWAP)) {
              digitalWrite(PIN_CONNECT, HIGH);
              // ZR buttonのデジタル化処理 buttonはMAXまで行かないので1/3にしている
              if (l_b1 / 3 > loop_num) t_joystickInputData.Button |= (uint16_t)(Button::ZR);
              // ZL buttonのデジタル化処理
              if (l_b2 / 3 > loop_num) t_joystickInputData.Button |= (uint16_t)(Button::ZL);
            }
            // 値の更新がある場合は更新処理を実施する
            if (l_x != slx || l_b1 != sb1 || l_b2 != sb2 || l_bL != sbL) {
              slx = l_x;
              sb1 = l_b1;
              sb2 = l_b2;
              sbL = l_bL;

#ifdef SERIALVAL_OUT
              Serial.print(F("DEBUG : X(ORG): "));
              Serial.print(l_x);
              Serial.print(F(" LX(MAX): "));
              Serial.print(lxMax);
#endif
              // 各々のアナログ値がちょっと鈍い感じもあるので少しだけ補正する
              l_x = (byte)adjustXY(l_x, lxMax);

              l_b1 = l_b1 * NEG_CALIB_B;
              if (l_b1 > 0x80) l_b1 = 0x80;
              l_b2 = l_b2 * NEG_CALIB_B;
              if (l_b2 > 0x7f) l_b2 = 0x7f;
              l_bL = l_bL * 1;
              if (l_bL > 0x7f) l_bL = 0x7f;

#ifdef SERIALVAL_OUT
              Serial.print(F(" X(FIX): "));
              Serial.print(l_x);
              Serial.print(F(" B1: "));
              Serial.print(b1_org);
              Serial.print(F(" B2: "));
              Serial.print(b2_org);
              Serial.print(F(" BL: "));
              Serial.println(bL_org);
#endif

              //LEDへの値渡し
              ledLx = l_x;
              ledB1 = b1_org;
              ledB2 = b2_org;
              ledBL = bL_org;

              // 最終的な値をセット (ハンドル)
              if (stickMode == MODE_NEGDIGTAL) {
                t_joystickInputData.LX = (uint8_t)(0x80);
                t_joystickInputData.LY = (uint8_t)(0x80);
              } else {
                t_joystickInputData.LX = (uint8_t)(l_x);
                t_joystickInputData.LY = (uint8_t)(0x80);
              }

              // ゲームモードがSTDモードの場合アナログ値でアクセル・ブレーキが設定可能
              if ((stickMode == MODE_STD) || (stickMode == MODE_STDSWAP)) {
                t_joystickInputData.RX = (uint8_t)(0x80);
                t_joystickInputData.RY = (uint8_t)(0x80 - l_b1 + l_b2);
              } else {
                t_joystickInputData.RX = (uint8_t)(0x80);
                t_joystickInputData.RY = (uint8_t)(0x80);
              }
            }
            //最大角、設定モード
            if (stickMode == MODE_SETTING_NEG) {
              if (psx.getButtonWord() & PSB_START) {
                Serial.print(F("neG lxMax before: "));
                Serial.println(lxMax);
                //センターからの相対値に変更
                l_x_tmp = absoluteXY(lx_org);

                //センター値近辺の場合は初期値に設定
                if (l_x_tmp < (0x80 + 10)) l_x_tmp = 0xff / ((NEG_CALIB - 1) / 2 + 1);

                //設定値を保存
                lxMax = (byte)l_x_tmp;
                Serial.println(F("EEP Write!"));
                EEPROM.write(EEPADR_NEG_NEGMAX, lxMax);
                EEPROM.commit();

                Serial.print(F("neG lxMax after: "));
                Serial.println(lxMax);
                stickMode = beforeStickMode;
              }
            }
          }

          break;

        // Dual Shock/2のボタン配置
        case PSPROTO_DUALSHOCK2:
          // Dual Shock2ではアナログボタンの値が取得できるが、使わない
          b1_org = psx.getAnalogButton(PsxAnalogButton::PSAB_CROSS);
          b2_org = psx.getAnalogButton(PsxAnalogButton::PSAB_SQUARE);

        case PSPROTO_DUALSHOCK:

          if (psx.getLeftAnalog(lx_org, ly_org)) {
            l_x = lx_org;
            l_y = ly_org;
          } else {
            l_x = 0x80;
            l_y = 0x80;
          }

          if (psx.getRightAnalog(rx_org, ry_org)) {
            r_x = rx_org;
            r_y = ry_org;
          } else {
            r_x = 0x80;
            r_y = 0x80;
          }
          if (l_x != slx || l_y != sly || r_x != srx || r_y != sry) {
            slx = l_x;
            sly = l_y;
            srx = r_x;
            sry = r_y;
            ledLx = l_x;
            ledLy = l_y;
            ledRx = r_x;
            ledRy = r_y;

            // 最終的な値をセット (ハンドル)
            t_joystickInputData.LX = (uint8_t)(l_x);
            t_joystickInputData.LY = (uint8_t)(l_y);
            t_joystickInputData.RX = (uint8_t)(r_x);
            t_joystickInputData.RY = (uint8_t)(r_y);
          }

        // その他雑多なコントローラ(共通)
        default:
          // hat_switch（これは共通処理)
          t_joystickInputData.Hat = (uint8_t)(hatValue[(psx.getButtonWord() & 0x00f0) >> 4]);

          // buttonSelect = Button::MINUS
          if (psx.getButtonWord() & PSB_SELECT)
            t_joystickInputData.Button |= (uint16_t)(Button::MINUS);

          // buttonL3 = Button::LCLICK
          if (psx.getButtonWord() & PSB_L3)
            t_joystickInputData.Button |= (uint16_t)(Button::LCLICK);

          // buttonR3 = Button::RCLICK
          if (psx.getButtonWord() & PSB_R3)
            t_joystickInputData.Button |= (uint16_t)(Button::RCLICK);

          // buttonStart = Button::PLUS
          if (psx.getButtonWord() & PSB_START)
            t_joystickInputData.Button |= (uint16_t)(Button::PLUS);


          // buttonL2 = Button::ZL
          if (psx.getButtonWord() & PSB_L2)
            t_joystickInputData.Button |= (uint16_t)(Button::ZL);

          // buttonR2 = Button::ZR
          if (psx.getButtonWord() & PSB_R2)
            t_joystickInputData.Button |= (uint16_t)(Button::ZR);

          // buttonR1 = Button::R
          if (psx.getButtonWord() & PSB_R1)
            t_joystickInputData.Button |= (uint16_t)(Button::R);

          // buttonTriangle = Button::X
          if (psx.getButtonWord() & PSB_TRIANGLE)
            t_joystickInputData.Button |= (uint16_t)(Button::X);

          // buttonCircle = Button::A
          if (psx.getButtonWord() & PSB_CIRCLE)
            t_joystickInputData.Button |= (uint16_t)(Button::A);

          // buttonL1 = Button::L
          if (psx.getButtonWord() & PSB_L1)
            t_joystickInputData.Button |= (uint16_t)(Button::L);

          // buttonCross = Button::B
          if (psx.getButtonWord() & PSB_CROSS)
            t_joystickInputData.Button |= (uint16_t)(Button::B);

          // buttonSquare = Button::Y
          if (psx.getButtonWord() & PSB_SQUARE)
            t_joystickInputData.Button |= (uint16_t)(Button::Y);

          t_joystickInputData.LX = 0x80;
          t_joystickInputData.LY = 0x80;
          t_joystickInputData.RX = 0x80;
          t_joystickInputData.RY = 0x80;


          break;
      }
      // USBジョイスティック データ送信
      SwitchController().sendReportOnly(t_joystickInputData);
    }
  }

  //ネジコンはフレーム待ちしないでも正常に値が取れるので削除
  // Only poll "once per frame" ;)
  //	delay (1000 / 60);
}
