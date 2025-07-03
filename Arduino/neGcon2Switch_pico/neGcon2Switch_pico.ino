////////////////////////////////////////////////////////////////////////
//
// PSX NeG Controller to Switch Joystick Sample
// raspberry PICO version
// Copyright 2025 @V9938
//
//	25/06/07 V0.1		1st version
//	25/06/07 V0.2		fix I/II/L Analog Value
//  25/06/10 V0.5   1st PICO version
//  25/06/11 V0.6   Support NeoPixel
//  25/06/11 V0.7   Support DX mode
//  25/07/03 V1.0   Support Aircombat22 mode with SCPH1110
//					Fixed bug Digtal mode setting
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

// neGcon ハンドルアナログ値の補正
// これ以上の補正が必要な場合は、設定→ボタン→こだわりのボタン設定→感度設定を弄ってください。
#define NEG_CALIB 1.2

//NeoPixelの設定
#define NUMPIXELS 1
#define NEO_PWR 11  //GPIO11
#define NEOPIX 12   //GPIO12

//Stick Mode
#define MODE_STD 0
#define MODE_DX 1
#define MODE_AIRCON22 2
#define MODE_ANALOG 3
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
const char ctrlTypeGuitHero[] PROGMEM = "Guitar Hero";
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
byte lx_org, ly_org, b1_org, b2_org, bL_org;
byte rx_org, ry_org;
byte stickMode;

// CPU1では、NeoPixel LEDの点灯処理をさせる
void setup1() {  //core 0
  pixels.begin();
  pinMode(NEO_PWR, OUTPUT);
  digitalWrite(NEO_PWR, HIGH);
  delay(1000);
}
void loop1() {  //core 0
  switch (stickMode) {
      // neGconモード時の点灯パターン
    case MODE_STD:
      pixels.setPixelColor(0, pixels.Color(b1_org/2 + bL_org/2, b2_org/2 + bL_org/2, lx_org));
      pixels.show();
      break;

      // neGconモード時の点灯パターン DXモード時のパターン
    case MODE_DX:
      pixels.setPixelColor(0, pixels.Color(b2_org/2 + bL_org/2, lx_org, b1_org/2 + bL_org/2));
      pixels.show();
      break;

      // フライトコントローラ接続時の点灯パターン
    case MODE_AIRCON22:
      pixels.setPixelColor(0, pixels.Color(ry_org/4, lx_org/8, ly_org/ 8 + 0x40));
      pixels.show();
      break;

      // DualShock接続時の点灯パターン
    case MODE_ANALOG:
      pixels.setPixelColor(0, pixels.Color(0, ly_org/4 + lx_org/2, ry_org/2 + rx_org/4));
      pixels.show();
      break;

      // その他が残念賞で消す
    default:
      pixels.clear();
      pixels.show();
      break;
  }
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

  //	SPI.setSCK(PIN_PS2_CLK);
  //	SPI.setMOSI(PIN_PS2_CMD);
  //	SPI.setMISO(PIN_PS2_DAT);
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
  static int8_t slx, sly, sb1, sb2, sbL, sBootSel = 0;
  static int8_t srx, sry;
  static int loop_num = 0;
  byte l_x, l_y, l_b1, l_b2, l_bL;
  byte r_x, r_y;
  byte l_x_tmp;
  PsxControllerType psxContType;
  PsxControllerProtocol psxStickMode;

  bool bSendData;

  // BOOTスイッチの処理
  // 現状MODE選択に割り当てている
  if (BOOTSEL) {
    digitalWrite(PIN_ERRORLED, LOW);
    if (sBootSel < 10) {
      if (sBootSel == 9) {
        if ((psxStickMode == PSPROTO_NEGCON) || (psxStickMode == PSPROTO_JOGCON)) {
          if (stickMode == MODE_STD) stickMode = MODE_DX;
          else if (stickMode == MODE_DX) stickMode = MODE_STD;
        }
      }
      sBootSel++;
    }
  } else {
    digitalWrite(PIN_ERRORLED, HIGH);
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
        //Dual shock以降はこちらに来る。HORIのアナログ振動ジョイパッドは設定不可
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
      if ((psxStickMode == PSPROTO_DIGITAL) || (psxStickMode == PSPROTO_NEGCON) || (psxStickMode == PSPROTO_JOGCON)) stickMode = MODE_STD;
      if (psxStickMode == PSPROTO_FLIGHTSTICK) stickMode = MODE_AIRCON22;
    }
  } else {
    if (!psx.read()) {
      //     	digitalWrite(PIN_CONNECT, HIGH);
      digitalWrite(PIN_GOODLED, HIGH);

      Serial.println(F("Controller lost :("));
      haveController = false;
      psxStickMode = psx.getProtocol();  //Stickモードを更新しておく
      stickMode = MODE_LOST;
    } else {
      digitalWrite(PIN_GOODLED, LOW);  //Controller認識OK
      // PWM mode用LOOP回数処理
      if (loop_num >= PWM_LOOP) loop_num = 0;
      loop_num++;

      psxStickMode = psx.getProtocol();  //現在のStickモードを取得

      // 動的にコントローラモードの変更されるもののみ列挙
      // neGconは、モード切替が無いのでここでは列挙しない
      if (psxStickMode == PSPROTO_DIGITAL) stickMode = MODE_LOST;
      if (psxStickMode == PSPROTO_FLIGHTSTICK) stickMode = MODE_AIRCON22;
      if ((psxStickMode == PSPROTO_DUALSHOCK) || (psxStickMode == PSPROTO_DUALSHOCK2)) stickMode = MODE_ANALOG;

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
            // 最終的な値をセット (ハンドル)
            t_joystickInputData.LX = (uint8_t)(l_x);
            t_joystickInputData.LY = (uint8_t)(l_y);
            t_joystickInputData.RX = (uint8_t)(r_x);
            t_joystickInputData.RY = (uint8_t)(r_y);
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
            b1_org = psx.getAnalogButton(PsxAnalogButton::PSAB_CROSS);
            b2_org = psx.getAnalogButton(PsxAnalogButton::PSAB_SQUARE);
            bL_org = psx.getAnalogButton(PsxAnalogButton::PSAB_L1);

            l_x = lx_org;
            l_y = ly_org;

            l_b1 = b1_org / 2;
            l_b2 = b2_org / 2;
            l_bL = bL_org / 2;

            if (stickMode == MODE_STD) {
              digitalWrite(PIN_CONNECT, LOW);

              // L buttonのデジタル化処理（アソビ領域判定）
              if (l_bL > 0x60) t_joystickInputData.Button |= (uint16_t)(Button::L);
            }

            if (stickMode == MODE_DX) {
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

              // 各々のアナログ値がちょっと鈍い感じもあるので少しだけ補正する
              if (l_x > 0x80) {
                l_x_tmp = (l_x - 0x7f) * NEG_CALIB;
                if (l_x_tmp > 0x7f) l_x_tmp = 0x7f;
                l_x = 0x80 + l_x_tmp;
              } else {
                l_x_tmp = (0x80 - l_x) * NEG_CALIB;
                if (l_x_tmp > 0x80) l_x_tmp = 0x80;
                l_x = 0x80 - l_x_tmp;
              }
              l_b1 = l_b1 * 1;
              if (l_b1 > 0x80) l_b1 = 0x80;
              l_b2 = l_b2 * 1;
              if (l_b2 > 0x7f) l_b2 = 0x7f;
              l_bL = l_bL * 1;
              if (l_bL > 0x7f) l_bL = 0x7f;

              // 最終的な値をセット (ハンドル)
              t_joystickInputData.LX = (uint8_t)(l_x);
              t_joystickInputData.LY = (uint8_t)(0x80);

              // ゲームモードがSTDモードの場合アナログ値でアクセル・ブレーキが設定可能
              if (stickMode == MODE_STD) {
                t_joystickInputData.RX = (uint8_t)(0x80);
                t_joystickInputData.RY = (uint8_t)(0x80 - l_b1 + l_b2);
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
