# 俺はneGconをアケアカのリッジレーサでつかいたいんじゃ for RP2040
![装置イメージ](./img/img001.jpg)  
先日作ったNAMCO neGcon to Nintendo Switch ConverterはAVRで作っているため、  
色々と今だとイマイチなところが多いので改めてraspberry PICO系のマイコンボードで作り直しました。  

ターゲットはNitendo Switch用アケアカ版リッジレーサ!!  
Switch2で動くかは実機を持っていないので分からないです。誰かください。  

## 作り方
どこの誤家庭にもある、Seeed XIAO RP2040 または raspberry PICOボートとプレステのコントローラをつないでください。  
1KΩのプルアップ抵抗は必須です。DATAと3.3Vの間につけてください  


## PS1/PS2 joypad との配線 

|PSX Pin | PSX Signal | XIAO RP2040 | raspberry PICO | RP2040 Signal |
| :- |  :- |  :- |  :- |  :- |
| 1 | DAT | 9 | GP4 | MISO<BR>[need pullup by 1k owm registor to 3.3V] |
| 2 | CMD | 10 | GP3 | MOSI |
| 3 | 9V<BR> (for motor, If you not necessary NC)| (VCC) | (VSYS) | +5V |
| 4 | GND | GND | GND | Power GND |
| 5 | 3.3V | +3V3 | 3V3 | Power 3.3V |
| 6 | Attention | 7 | GP1 | GPIO1 (Controller Select) |
| 7 | CLK | 8 | GP2 | SCK |
| 8 | NC | - | - |
| 9 | ACK | - | - |

![XIAO配線イメージ](./img/img002.png)  

![PICO装置イメージ](./img/img003.png)  

## Firmware書き込み方法
マイコンボード上にあるBボタンまたはBOOTSELボタンを押しながらUSBケーブルを接続してください。  
PCに接続した後も念のため数秒程度はボタンを離さないようにしてください。  
上手く接続できれば回路が「RPI-RP2」という名前で、USBメモリのように認識されます。  
「RPI-RP2」を開き、「xxxxx.uf2」ファイルをドラッグ&ドロップしたら完了です。  

「./firmware」のフォルダにコンパイル済のファイルが置いてあります。  

## ボタン配置
BOOTボタンを押す事で2つの設定を切り替えられます。  

### 初期設定(STD筐体モード)
|neGconボタン | Nintendo Switchコントローラ | 備考 |
| :- |  :- |  :- |
| ねじり (アナログ) | 左アナログスティックのX | 左アナログスティックのYは常にCenterです。 |
| I (アナログ) | 左アナログスティックのY 上方向 |  |
| II (アナログ) | 左アナログスティックのY 下方向 |  |
| L (アナログ) | Lボタン | 押し込まないと反応しません |
| R (デジタル) | Rボタン | |
| A (デジタル) | Aボタン | |
| B (デジタル) | Yボタン | |
| START (デジタル) | ＋ボタン |  |

### 初期設定(DX筐体モード)
|neGconボタン | Nintendo Switchコントローラ | 備考 |
| :- |  :- |  :- |
| ねじり (アナログ) | 左アナログスティックのX | 左アナログスティックのYは常にCenterです。 |
| I (アナログ) | ZRボタン | 押し込み量によって連射速度が変わります |
| II (アナログ) | ZLボタン | 押し込み量によって連射速度が変わります |
| L (アナログ) | Lボタン |  押し込まないと反応しません |
| R (デジタル) | Rボタン | |
| A (デジタル) | Aボタン | |
| B (デジタル) | Yボタン | |
| START (デジタル) | ＋ボタン |  |

## 基板頒布について
専用基板の現在計画中です。  
![基板イメージ](./img/img004.png)  

## 使用したlibraryについて  
本ソフトは下記libraryを利用しています。  

### Adafruit_NeoPixel
<https://github.com/adafruit/Adafruit_NeoPixel>  

### Adafruit_TinyUSB_Library
<https://github.com/adafruit/Adafruit_TinyUSB_Arduino>

### PsxNewLib  
<https://github.com/SukkoPera/PsxNewLib>  

### SwitchControllerPico  
<https://github.com/sorasen2020/SwitchControllerPico>  

