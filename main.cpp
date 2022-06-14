#include <mbed.h>

#include "PQ_RM92AS.h"
#include "PQ_GPS.h"
#include "PQ_LPS22HB.h"
#include "SDFileSystem.h"

#define DOWNLINK_RATE 1.0
#define READ_RATE 1.0
#define REACH_TOP 7200.0    // シミュによる頂点到達時間


Serial pc(USBTX, USBRX, 115200);
Serial rm_serial(p9, p10, 115200);
Serial gps_serial(p13, p14, 115200);
SoftwareSerial adafruit_gps(p);

I2C i2c(p28, p27);

RM92AS rm(rm_serial);
GPS gps(gps_serial);

SDFileSystem sd(p5, p6, p7, p8, "sd");  //MOSI,MISO,SCK,CS
LPS22HB lps(i2c, LPS22HB::SA0_LOW); // 気圧，温度

Timer mission_timer;
Timer flight_timer;
Timer sd_timer;

Ticker downlink_ticker; // ダウンリンク
Ticker record_ticker;   // データ保存
Ticker dump_ticker;     // プリント
Ticker read_ticker;     //センサーの読み取り
Ticker is_top_ticker;

DigitalOut sig_berry1(p23);     // カメラ起動用
DigitalIn sig_berry1(p24)
DigitalIn sig_berry2(p25)


/***********************************************
 * 関数のプロトタイプ宣言
 * ********************************************/
void init();    // 初期化関数
void read();    // センサー読み取り関数
void command_handler(char *command);
void downlink();
void record();
void dump();

/***********************************************
 * 変数宣言
 * ********************************************/
bool do_while  = true;   // while文を使うか
bool launched  = false;  // 打ち上がったか
bool reach_top = false;  // 頂点に到達したか
bool landed    = false;  // 着水したか
bool rec       = false;  // 撮影中であるか 

enum {
    READY,      // 電源投入～打ち上げ直前. FLIGHTへの移行はコマンドによる.
    FLIGHT,     // 打ち上げ直前～頂点付近
    FALL,       // 頂点付近～着水
    RECOVERY,   // 着水～回収
} phase;

//動作確認用のフラグ：1=有効, 0=無効
char f_sd;      // SDカード
char f_gps;     // GPS
bool f_lps;     // LPS22HB

// SD用
char file_name[64];
FILE *fp;

int mission_timer_reset;  // mission_timerをリセットした回数
int mission_time;         //電源投入からの経過時間
int flight_time;          //離床検知からの経過時間

float lat;          // 緯度[deg]
float lon;          // 経度[deg]
int sat;            // 捕捉衛星数
int fix;            // 位置特定品質(0だと位置特定できない)
float hdop;         // 水平精度低下率
float alt;          // 海抜高度[m]
float geoid;        // ジオイド[m]
float press;        // 気圧[hPa]
float press0;       // 
float temp;         // 温度[℃]

void main(){
    wait(10.0); //RM92AS内EEPROM書き出しまでの時間待機
    init();

    while(do_while){
        switch(phase){
            case READY: // 待機モードのときの動作をここに書く
                ground_press = press;
                break;
            case FLIGHT: // 上昇中のときの動作をここに書く
                flight_timer.start();
                
                if(!rec){
                    sig_berry1 = 1; // 撮影開始
                    rec = true;
                }
                if(flight_timer.read() > 600){
                    sig_berry1 = 0; // 撮影停止
                    rec = false;
                }
                
                if(flight_timer.read() > 6000){     // 頂点検知は打ち上げから100分後
                    if((flight_timer.read() - t) > 10){
                        if(press_LPF > temp_press){
                            is_top = 1;
                            phase = FALL;
                            sig_berry1 = 1;
                            wait(600);
                            sig_berry1 = 0;
                        }else{
                            t = flight_timer.read();
                            temp_press = press_LPF;
                        }
                    }
                }
                break;
            case FALL:  // 落下中の処理をここに書く
                if(press > ground_press){
                    phase = RECOVERY;
                    landed = true;
                }
                break;
            case RECOVERY:  // 着水後の動作をここに書く
                do_while = false;
                break:
        }
    wait(0.5);  //動作安定化のため
    }
}

void init(){
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

    lps.begin();

    rm.attach(command_handler);
    downlink_ticker.attach(downlink, 1.0f/DOWNLINK_RATE);
    read_ticker.attach(read, 1.0f/READ_RATE);

}

void read(){
    if(mission_timer.read() > 900){
        mission_time.reset();
        mission_timer_reset ++;
    }

    f_sd  = (bool)fp;

    f_gps = (bool)fix;
    lat   = gps.get_lat();
    lon   = gps.get_lon();
    sat   = gps.get_sat();
    fix   = gps.get_fix();
    hdop  = gps.get_hdop();
    alt   = gps.get_alt();
    geoid = gps.get_geoid();

    f_lps = lps.test();
    if(f_lps) {
        lps.read_press(&press);
        lps.read_temp(&temp);

        // 移動中央値を求める
        press_buf[press_count] = press;
        press_count++;
        if(press_count > 10) {
            press_count = 0;
        }
        // 配列のコピー
        float buf[10];
        for(int i = 0; i < 10; i++) {
            buf[i] = press_buf[i];
        }
        // バブルソート
        for(int i = 0; i < 9; i++) {
            for(int j = 9; j > i; j--) {
                if(buf[j] < buf[j - 1]) {
                    float temp = buf[j];
                    buf[j] = buf[j - 1];
                    buf[j - 1] = temp;
                }
            }
        }
        // 中央値
        press_median = (buf[4] + buf[5]) / 2.0f;
        // ローパスフィルタ
        press_LPF = press_median * coef + press_prev_LPF * (1 - coef);
        press_prev_LPF = press_LPF;
    }
}

void downlink(){


    char data[50];
    data[0] = ((char*)&mission_time_bits)[0];
    data[1] = ((char*)&mission_time_bits)[1];
    data[2] = ((char*)&flight_time_bits)[0];
    data[3] = ((char*)&flight_time_bits)[1];
    data[4] = phase;
    data[5] = flags1;
    data[6] = flags2;
    data[7] = ((char*)&lat)[0];
    data[8] = ((char*)&lat)[1];    //Latはそのまま4byte(32bit)
    data[9] = ((char*)&lat)[2];
    data[10] = ((char*)&lat)[3];
    data[11] = ((char*)&lon)[0];
    data[12] = ((char*)&lon)[1];   //Lonはそのまま4byte(32bit)
    data[13] = ((char*)&lon)[2];
    data[14] = ((char*)&lon)[3];
    data[15] = ((char*)&alt)[0];
    data[16] = ((char*)&alt)[1];   //Altはそのまま4byte(32bit)
    data[17] = ((char*)&alt)[2];
    data[18] = ((char*)&alt)[3];
    data[19] = ((char*)&press)[0];
    data[20] = ((char*)&press)[1];
    data[21] = ((char*)&press)[2];
    data[22] = ((char*)&press)[3];
    data[23] = ((char*)&temp)[0];
    data[24] = ((char*)&temp)[1];
    data[25] = ((char*)&temp)[2];
    data[26] = ((char*)&temp)[3];


}

void command_handler(cahr *data){

}

void record(){

    if(fp){
        char* phase_names[] = {"READY", "FLIGHT", "FALL", "RECORVERY"};
        fprintf(fp, "%.3d,%.3d,%s,", mission_time, flight_time, phase_names[phase]);
        fptintf(fp, "%d,%d,%d,%d", launched, reach_top, landed, rec);
        fptintf(fp, "%.6f,%.6f,%d,%d,%f,%f,%f", lat, lon, sat, fix, hdop, alt, geoid);
        fprintf(fp, "%.3f,%.3f", press, temp);
        fptintf(fp, "\r\n");
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
    pc.printf("");
}