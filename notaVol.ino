/**
 * @file        notaVol.ino
 * @author      aSumo (1xtelescope@gmail.com)
 * @version     0.1
 * @date        2024-08-16
 * @copyright   CC BY-SA 4.0
 * @brief       コンパイルOK
 */

#include <SPI.h>
#include <Control_Surface.h> //!< https://github.com/tttapa/Control-Surface

// Variable to set
const bool temp = false;    //!< EXPペダルの極性: false=正転, true=逆転
const bool mode = false;    //!< レベルメータICのモード: false=BAR, true=Point

// Main Global Variables
volatile long val_EXP = 0;  //!< EXPペダルから読み込む値: 0~1024
volatile long val_ten = 0;  //!< EXPペダルから読み込む値: 0~10
volatile long val_MCP = 0;  //!< MCP41100に送る値: 0~255
volatile long val_XD  = 0;  //!< XD3914に送る値: 0~MYref



// pinAssign
const int sw_up     = 3;    //!< 音量Upスイッチ
const int sw_down   = 2;    //!< 音量Downスイッチ
const int pedal_EXP = A1;   //!< EXPペダル読み込み
const int LED_zero  = 10;   //!< ゼロLED
// XD3914
const int XD_level  = 9;    //!< レベル入力
const int XD_mode   = 8;    //!< モード選択
// MCP41100
const int address = 0x11;   //!< アドレス
const int CS      = 18;     //!< CSピン

// Control-Surfaceライブラリのアナログ入力フィルターが優秀なので拝借
FilteredAnalog<> theEXP = A1;   //!< EXPペダルのアナログ入力



// culculate Vref in XD3914
float R1    = 1200;             //!< 1.2[kΩ]: LEDの明るさも決まるので注意
float R2    = 3300;             //!< 3.3[kΩ]: Vrefが4.75とかになると嬉しかった
float Vref  = 1.25*(1.0+R2/R1); //!< =4.6875[V]: XD3914はこの値を10%ずつに分けて使う！
long MYref  = map(ceil(Vref*10), 0, 50, 0, 256); //!< 0~255の範囲内に切り上げ変換
// ほんの少しだけ大きめの値を使うことで、レベルメータの最大ゲージを確実に点灯させる作戦



// チャタリング対策用変数
volatile unsigned long time_pre = 0;    //!< 前回の割り込み時刻
volatile unsigned long time_now = 0;    //!< 今回の割り込み時刻
volatile unsigned long time_chat= 20;   //!< チャタリング時間[ms]



/**
 * @brief MCP41100へ値を送信する関数
 * @param val 0~255の値
 */
void digitalPotWrite(long val) {
    digitalWrite(CS, LOW);
    SPI.transfer(address);
    SPI.transfer(val);
    digitalWrite(CS, HIGH);
}



/**
 * @brief Upスイッチの割り込み関数
 */
void upVol() {
    time_now = millis(); //現在の割り込み時刻を取得
    if (time_now-time_pre > time_chat) { // 十分に時間が経過しているか確認
        if (val_ten < 10) {
            // まだ10段階中10未満の場合
            val_ten++;
            val_MCP = map(val_ten, 0, 10, 0, 256);    // 値を更新
            val_XD  = map(val_ten, 0, 10, 0, MYref);  // 値を更新            
        } else {
            // 10に達している場合
            val_MCP = 256;      // 完全なる最大値に設定
            val_XD  = MYref;    // 完全なる最大値に設定
        }
    }
    time_pre = time_now;
}



/**
 * @brief Downスイッチの割り込み関数
 */
void downVol() {
    time_now = millis(); //現在の割り込み時刻を取得
    if (time_now-time_pre > time_chat) { // 十分に時間が経過しているか確認
        if (val_ten > 0) {
            // まだ10段階中1以上の場合
            val_ten--;
            val_MCP = map(val_ten, 0, 10, 0, 256);    // 値を更新
            val_XD  = map(val_ten, 0, 10, 0, MYref);  // 値を更新         
        } else {
            // 0に達している場合
            val_MCP = 0;    // 完全なる最小値に設定
            val_XD  = 0;    // 完全なる最小値に設定
        }
    }
    time_pre = time_now;
}



/**
 * @brief setup関数
 */
void setup() {
    // pinMode
    pinMode(sw_up,     INPUT_PULLUP);
    pinMode(sw_down,   INPUT_PULLUP);
    pinMode(LED_zero,  OUTPUT);
    pinMode(XD_level,  OUTPUT);
    pinMode(XD_mode,   OUTPUT);
    pinMode(CS,        OUTPUT);

    // フットスイッチの割り込み設定
    attachInterrupt(digitalPinToInterrupt(sw_up),   upVol,  FALLING);
    attachInterrupt(digitalPinToInterrupt(sw_down), downVol,FALLING);

    // ADコンバータの設定
    FilteredAnalog<>::setupADC();
    if (temp) { theEXP.invert(); }    // 極性の設定

    SPI.begin();

    digitalPotWrite(val_MCP);       // 初期化
    analogWrite(XD_level, val_XD);  // 初期化
    analogWrite(LED_zero, HIGH);    // ゼロLED点灯
}



/**
 * @brief loop関数
 */
void loop() {
    // EXPペダルの値を読み込み続ける
    static Timer<millis> timer = 1;
    if (timer && theEXP.update()) {
        val_EXP = theEXP.getValue();
        val_ten = map(val_EXP, 0, 1024, 0, 10);     // 値を更新
        val_MCP = map(val_EXP, 0, 1024, 0, 256);    // 値を更新
        val_XD  = map(val_EXP, 0, 1024, 0, MYref);  // 値を更新
    }

    digitalPotWrite(val_MCP);
    analogWrite(XD_level, val_XD);

    if (val_ten == 0) {
        analogWrite(LED_zero, HIGH);
    } else {
        analogWrite(LED_zero, LOW);
    }
}