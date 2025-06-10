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

// NeGcon ハンドルアナログ値の補正
#define NEG_CALIB 1.2

//NeoPixelの設定
#define NUMPIXELS 1
#define NEO_PWR 11 //GPIO11
#define NEOPIX 12 //GPIO12

//Stick Mode
#define MODE_STD  0
#define MODE_DX   1

//PWM modeの1LOOP回数 数字を細かくするとON-OFF間隔が短くなる(8ms x PWM_LOOP) 
#define PWM_LOOP   30


/** \brief Pin used for Controller Attention (ATTN)
 *
 * This pin makes the controller pay attention to what we're saying. The shield
 * has pin 10 wired for this purpose.
 */
const unsigned int PIN_PS2_CMD = 4;		//MOSI
const unsigned int PIN_PS2_DAT = 3;		//MOSO
const unsigned int PIN_PS2_CLK = 2;		//SCK
const unsigned int PIN_PS2_ATT = 1;		//CS

// Set up the speed, data order and data mode
static SPISettings spiSettings (250000, LSBFIRST, SPI_MODE3);

/** \brief Pin for Button Press Led
 *
 * This led will light up whenever a button is pressed on the controller.
 */
const unsigned int PIN_CONNECT = 25;    // BLUE
const unsigned int PIN_ERRORLED = 17;       // RED
const unsigned int PIN_GOODLED = 16;        // GREEN

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
const char ctrlProtoFlightstick[] PROGMEM = "Flightstick";
const char ctrlProtoNegcon[] PROGMEM = "neGcon";
const char ctrlProtoJogcon[] PROGMEM = "Jogcon";
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
byte stickMode;

// CPU1では、NeoPixel LEDの点灯処理をさせる
void setup1() { //core 0
  pixels.begin();
  pinMode(NEO_PWR,OUTPUT);  digitalWrite(NEO_PWR, HIGH);
  delay(1000);
}
void loop1() { //core 0
  if (stickMode == MODE_STD)  pixels.setPixelColor(0, pixels.Color( b1_org/2 + bL_org/2 , b2_org/2 + bL_org/2,  lx_org));
  if (stickMode == MODE_DX)  pixels.setPixelColor(0, pixels.Color(  b2_org/2 + bL_org/2 , lx_org,               b1_org/2 + bL_org/2));
  pixels.show();
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


	gpio_init(PIN_PS2_CLK);					//SCK
	gpio_set_dir(PIN_PS2_CLK, GPIO_OUT);
	gpio_put(PIN_PS2_CLK, HIGH);

  gpio_init(PIN_PS2_CMD);					//MOSI
  gpio_set_dir(PIN_PS2_CMD, GPIO_OUT);
	gpio_put(PIN_PS2_CMD, HIGH);

	gpio_init(PIN_PS2_DAT);					//MISO
	gpio_set_dir(PIN_PS2_DAT, GPIO_IN);
  gpio_pull_up(PIN_PS2_DAT);

	gpio_init(PIN_PS2_ATT);					//SS(GPIO)
	gpio_set_dir(PIN_PS2_ATT, GPIO_OUT);
	gpio_put(PIN_PS2_ATT, HIGH);

	SPI.beginTransaction (spiSettings);

//	SPI.setSCK(PIN_PS2_CLK);
//	SPI.setMOSI(PIN_PS2_CMD);
//	SPI.setMISO(PIN_PS2_DAT);
  stickMode = MODE_STD;
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
  static int loop_num = 0;
  byte l_x, l_y, l_b1, l_b2, l_bL;
  byte l_x_tmp;
  bool bSendData;

  // MODE選択
  if(BOOTSEL){
    digitalWrite(PIN_ERRORLED, LOW);
    if (sBootSel < 10){
      if (sBootSel == 9){
        if (stickMode == MODE_STD)  stickMode = MODE_DX;
        else if (stickMode == MODE_DX)   stickMode = MODE_STD;
      }
      sBootSel++;
    }
  }else{
    digitalWrite(PIN_ERRORLED, HIGH);
    sBootSel = 0;
  }


  if (!haveController) {
    if (psx.begin()) {
//    	digitalWrite(PIN_CONNECT, LOW);
      Serial.println(F("Controller found!"));
      delay(300);
      if (!psx.enterConfigMode()) {
        Serial.println(F("Cannot enter config mode"));
      } else {
        PsxControllerType ctype = psx.getControllerType();
//        Serial.print(F("Controller Type is: "));
//        Serial.println(&(controllerTypeStrings[ctype < PSCTRL_MAX ? static_cast<byte>(ctype) : PSCTRL_MAX]));

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

      PsxControllerProtocol proto = psx.getProtocol();
      Serial.print(F("Controller Protocol is: "));
      Serial.println(proto);

      haveController = true;
    }
  } else {
    if (!psx.read()) {
//     	digitalWrite(PIN_CONNECT, HIGH);
     	digitalWrite(PIN_GOODLED, HIGH);

      Serial.println(F("Controller lost :("));
      haveController = false;

    } else {
   	  digitalWrite(PIN_GOODLED, LOW);                       //Controller認識OK

      // PWM mode用LOOP回数処理
      if (loop_num >= PWM_LOOP) loop_num = 0;
      loop_num++;

      // button処理
      t_joystickInputData.Button = 0;

      // buttonSelect = Button::MINUS
      if (psx.getButtonWord() & 0x0001)
        t_joystickInputData.Button |= (uint16_t)(Button::MINUS);

      // buttonL3 = Button::LCLICK
      if (psx.getButtonWord() & 0x0002)
        t_joystickInputData.Button |= (uint16_t)(Button::LCLICK);

      // buttonR3 = Button::RCLICK
      if (psx.getButtonWord() & 0x0004)
        t_joystickInputData.Button |= (uint16_t)(Button::RCLICK);

      // buttonStart = Button::PLUS
      if (psx.getButtonWord() & 0x0008)
        t_joystickInputData.Button |= (uint16_t)(Button::PLUS);

      // hat_switch
      t_joystickInputData.Hat = (uint8_t)(hatValue[(psx.getButtonWord() & 0x00f0) >> 4]);

      // buttonL2 = Button::ZL
      if (psx.getButtonWord() & 0x0100)
        t_joystickInputData.Button |= (uint16_t)(Button::ZL);

      // buttonR2 = Button::ZR
      if (psx.getButtonWord() & 0x0200)
        t_joystickInputData.Button |= (uint16_t)(Button::ZR);

      // buttonR1 = Button::R
      if (psx.getButtonWord() & 0x0800)
        t_joystickInputData.Button |= (uint16_t)(Button::R);

      // buttonTriangle = Button::X
      if (psx.getButtonWord() & 0x1000)
        t_joystickInputData.Button |= (uint16_t)(Button::X);

      // buttonCircle = Button::A
      if (psx.getButtonWord() & 0x2000)
        t_joystickInputData.Button |= (uint16_t)(Button::A);

      // NeGcon固有処理
      if ((psx.getProtocol() == PSPROTO_NEGCON) || (psx.getProtocol() != PSPROTO_JOGCON)) {
        //
        if (psx.getLeftAnalog(lx_org, ly_org)) {

          b1_org = psx.getAnalogButton(PsxAnalogButton::PSAB_CROSS);
          b2_org = psx.getAnalogButton(PsxAnalogButton::PSAB_SQUARE);
          bL_org = psx.getAnalogButton(PsxAnalogButton::PSAB_L1);

          l_x = lx_org;
          l_y = ly_org;

          l_b1 = b1_org / 2;
          l_b2 = b2_org / 2;
          l_bL = bL_org / 2;

          if (stickMode == MODE_STD){ 
            digitalWrite(PIN_CONNECT, LOW);

            // L buttonのデジタル化処理（アソビ領域判定）
            if (l_bL > 0x60) t_joystickInputData.Button |= (uint16_t)(Button::L);
          }

          if (stickMode == MODE_DX){ 
            digitalWrite(PIN_CONNECT, HIGH);
            // ZR buttonのデジタル化処理 buttonはMAXまで行かないので1/3にしている
            if (l_b1/3 > loop_num) t_joystickInputData.Button |= (uint16_t)(Button::ZR);
            // ZL buttonのデジタル化処理
            if (l_b2/3 > loop_num) t_joystickInputData.Button |= (uint16_t)(Button::ZL);
          }

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

              t_joystickInputData.LX = (uint8_t)(l_x);
              t_joystickInputData.LY = (uint8_t)(0x80);

            if (stickMode == MODE_STD){ 
              t_joystickInputData.RX = (uint8_t)(0x80);
              t_joystickInputData.RY = (uint8_t)(0x80 - l_b1 + l_b2);
            }

          }
//          t_joystickInputData.Button = t_joystickInputData.Button & (~((Button::Y)|(Button::B)|(Button::L)));
        }
      }else{
        // 通常PSXController処理
        // buttonL1 = Button::L
        if (psx.getButtonWord() & 0x0400)
          t_joystickInputData.Button |= (uint16_t)(Button::L);

        // buttonCross = Button::B
        if (psx.getButtonWord() & 0x4000)
          t_joystickInputData.Button |= (uint16_t)(Button::B);

        // buttonSquare = Button::Y
        if (psx.getButtonWord() & 0x8000)
          t_joystickInputData.Button |= (uint16_t)(Button::Y);

      }
      // USBジョイスティック データ送信
      SwitchController().sendReportOnly(t_joystickInputData);

    }
  }

  //ネジコンはフレーム待ちしないでも正常に値が取れるので削除
  // Only poll "once per frame" ;)
  //	delay (1000 / 60);
}
