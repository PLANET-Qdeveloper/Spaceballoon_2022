#include <mbed.h>

#include "PQ_GPS.h"
#include "PQ_LPS22HB.h"
#include "SDFileSystem.h"
//#include "SoftwareSerial.h"
#include "SerialGPS.h"

/************************************************
 * インスタンスの生成
 * *********************************************/
Serial pc(USBTX, USBRX, 115200);
Serial rm_serial(p13, p14, 115200);
Serial gps_serial(p9, p10, 115200);
//SoftwareSerial adafruit_gps(p22, p23, "");    // 今回は使わない

I2C i2c(p28, p27);

GPS gps(gps_serial);                // 秋月みちびきGPS
//SerialGPS gps2(p22, p23, 115200);   // Adafruit Ultimate GPS

SDFileSystem sd(p5, p6, p7, p8, "sd");   // MOSI, MISO, SCK, CS
LPS22HB lps(i2c, LPS22HB::SA0_HIGH);      // 気圧センサー

FILE *fp;

Timer mission_timer;    // 電源投入でスタートするタイマー
Timer sd_timer;         // SDのファイル更新用タイマー

Ticker downlink_ticker; // ダウンリンク
Ticker record_ticker;   // データ保存
Ticker dump_ticker;     // プリント
Ticker read_ticker;     //センサーの読み取り

/***********************************************
 * 関数のプロトタイプ宣言
 * ********************************************/
void init();        // 初期化関数
void read();        // センサー読み取り関数
void downlink();    // RM92ASとの通信用関数
void record();      // SDへのデータ保存関数
void dump();        // パソコンへプリントする関数


/***********************************************
 * 変数宣言
 * ********************************************/

char file_name[64]; // SD用

int mission_timer_reset;  // mission_timerをリセットした回数
int mission_time;         //電源投入からの経過時間

//動作確認用のフラグ：1=有効, 0=無効
char f_sd;      // SDカード
char f_gps;     // GPS
bool f_lps;     // LPS22HB

float lat;      // 緯度[deg]
float lon;      // 経度[deg]
int sat;        // 捕捉衛星数
int fix;        // 位置特定品質(0だと位置特定できない)
float hdop;     // 水平精度低下率
float alt;      // 海抜高度[m]
float geoid;    // ジオイド[m]

float lat2;     // 緯度[deg]
float lon2;     // 経度[deg]
int sat2;       // 捕捉衛星数
float alt2;     // 海抜高度[m]
float geoid2;   // ジオイド[m]

float press;    // 気圧[hPa]
float temp;     // 温度[℃]

char time_char[3];
char lat_char[6];
char lon_char[7];
char alt_char[5];
char press_char[6];
char temp_char[3];
char reset_char[3];

int lat_int;
int lon_int;
int alt_int;
int press_int;
int temp_int;


unsigned short dst = 1;  // 送信先ノード0x0001

int main(){
    wait(12.0f);    // 自動実行モード待機時間
    init();
}

void init(){
    mission_timer.reset();
    mission_timer.start();
    
    char file_name_format[] = "/sd/SpaceBalloon_mbed_%d.dat";
    int file_number = 1;
    while(1) {
        sprintf(file_name, file_name_format, file_number);
        fp = fopen(file_name, "r");
        if(fp != NULL) {
            fclose(fp);
            file_number++;
        } else {
            sprintf(file_name, file_name_format, file_number);
            break;
        }
    }
    fp = fopen(file_name, "w");
    sd_timer.start();

    if(fp){
        fprintf(fp, "mission_time,");
        fprintf(fp, "mission_timer_reset,");
        fprintf(fp, "lat,");
        fprintf(fp, "lon,");
        fprintf(fp, "sat,");
        fprintf(fp, "fix,");
        fprintf(fp, "hdop,");
        fprintf(fp, "alt,");
        fprintf(fp, "press,");
        fprintf(fp, "temp");
        fprintf(fp, "\r\n");
    }
    
    lps.begin();

    downlink_ticker.attach(downlink, 5.0f);
    read_ticker.attach(read, 1.0f);
    record_ticker.attach(record, 5.0f);
    dump_ticker.attach(dump, 1.0f);
}

void read(){
    if(mission_timer.read() > 1800){    // 30分おきにリセット
        mission_timer.reset();
        mission_timer_reset ++;
    }
    mission_time = mission_timer.read();

    f_gps = (bool)fix;
    lat   = gps.get_lat();
    lon   = gps.get_lon();
    sat   = gps.get_sat();
    fix   = gps.get_fix();
    hdop  = gps.get_hdop();
    alt   = gps.get_alt();
    geoid = gps.get_geoid();
    
    /*
    if(gps2.sample()){
        lat2 = gps2.latitude;
        lon2 = gps2.longitude;
        sat2 = gps2.sats;
        alt2 = gps2.alt;
        geoid2 = gps2.geoid;
    }
    */
    
    f_lps = lps.test();
    if(f_lps) {
        lps.read_press(&press);
        lps.read_temp(&temp);
    }
    
    lat_int = (int)(lat*10000);
    lon_int   = (int)(lon*10000);
    alt_int   = (int)(alt);
    press_int = (int)(press*100);
    temp_int  = (int)(temp*10);
}

void downlink() {
    sprintf(time_char, "%d", mission_time);
    sprintf(reset_char, "%d", mission_timer_reset);
    sprintf(lat_char, "%d", lat_int);
    sprintf(lon_char, "%d", lon_int);
    sprintf(alt_char, "%d", alt_int);
    sprintf(press_char, "%d", press_int);
    sprintf(temp_char, "%d", temp_int);
    
    char data[63];
    data[0]  = '@';
    data[1]  = '@';
    data[2]  = 57;
    data[3]  = (unsigned char)((dst >> 8) & 0xFF);
    data[4]  = (unsigned char)((dst >> 0) & 0xFF);
    data[5]  = time_char[0];
    data[6]  = time_char[1];
    data[7]  = time_char[2];
    data[8]  = 115; // 's'
    data[9]  = 44;  // ','
    data[10] = 32;  // ' '
    data[11] = reset_char[0];
    data[12] = reset_char[1];
    data[13] = reset_char[2];
    data[14]  = 44;  // ','
    data[15] = 32;  // ' '
    data[16] = 78;  // 'N'
    data[17] = lat_char[0];
    data[18] = lat_char[1];
    data[19] = 46;  // '.'
    data[20] = lat_char[2];
    data[21] = lat_char[3];
    data[22] = lat_char[4];
    data[23] = lat_char[5];
    data[24] = 44;  // ','
    data[25] = 32;  // ' '
    data[26] = 69;  // 'E'
    data[27] = lon_char[0];
    data[28] = lon_char[1];
    data[29] = lon_char[2];
    data[30] = 46;  // '.'
    data[31] = lon_char[3];
    data[32] = lon_char[4];
    data[33] = lon_char[5];
    data[34] = lon_char[6];
    data[35] = 44;  // ','
    data[36] = 32;  // ' '
    data[37] = alt_char[0];
    data[38] = alt_char[1];
    data[39] = alt_char[2];
    data[40] = alt_char[3];
    data[41] = alt_char[4];
    data[42] = 109; // 'm'
    data[43] = 44;  // ','
    data[44] = 32;  // ' '
    data[45] = press_char[0];
    data[46] = press_char[1];
    data[47] = press_char[2];
    data[48] = press_char[3];
    data[49] = 46;  // '.'
    data[50] = press_char[4];
    data[51] = press_char[5];
    data[52] = 104; // 'h'
    data[53] = 80;  // 'P'
    data[54] = 97;  // 'a'
    data[55] = 44;  // ','
    data[56] = 32;  // ' '
    data[57] = temp_char[0];
    data[58] = temp_char[1];
    data[59] = 46;  // '.'
    data[60] = temp_char[2];
    data[61] = 67;  // 'C' 
    data[62] = 0xAA;

    for(int i=0; i<63; i++){
        rm_serial.putc(data[i]);
    }
}

void record(){
    if(fp){
        fprintf(fp, "%.3d,%d,", mission_time, mission_timer_reset);
        fprintf(fp, "%.6f,%.6f,%d,%d,%f,%f,", lat, lon, sat, fix, hdop, alt);
        // fprintf(fp, "%.6f,%.6f,%d,%f", lat2, lon2, sat2, alt2);
        fprintf(fp, "%.3f,%.3f", press, temp);
        fprintf(fp, "\r\n");
    }

    if(sd_timer.read() >= 60*30){   // 30分おきにfcloseする
        sd_timer.reset();
        if(fp){
            fclose(fp);
            fp = fopen(file_name, "a");
        }
    }
}

void dump(){
    pc.printf("TIME:%4d,RESET:%3d,", mission_time, mission_timer_reset);
    pc.printf("LAT:%.6f,LON:%.6f,SAT:%d,FIX:%d,HDOP:%f,ALT:%f,GEOID:%f,", lat, lon, sat, fix, hdop, alt, geoid);
    // pc.printf("%.6f,%.6f,%d,%f,%f", lat2, lon2, sat2, alt2, geoid2);
    pc.printf("PRESS:%.3f,TEMP:%.1f", press, temp);
    pc.printf("\r\n");
}   