#pragma once
/*
*   ES920LR ライブラリ　ver0.2.2
*   RSSI値、アドレス付与はまだ未対応です。
*   データ形式はbinaryです。asciiには未対応です。
*   設定の変更はあらかじめ行うという形式です。
*/
 
#include "mbed.h"
 
const int ES920LR_BUFF_SIZE = 70;
const int ES920LR_FUNC_NUMBER = 256;
const int ES920LR_DATA_INTERVAL = 10000;//[us]
 
/**
@brief 920MHz帯無線機ES920LRを簡単に使えるクラス
*/
 
 
class ES920LR {
 
public:
    
    typedef enum END_COMMAND {
        endl = 1,
        asciiEnd = 2
    }end_command;
 
    
    //ノード種別設定
    typedef enum NODE {
        Coordinator = 1,
        EndDevice = 2
    }nodeNumber;
 
    //帯域幅
    typedef enum BAND_WIDTH {
        _62_5kHz = 3,
        _125kHz = 4,
        _250kHz = 5,
        _500kHz = 6
    }band_width;
 
    //転送モード選択
    typedef enum TRANSMODE {
        Payload = 1,
        Frame = 2
    }transmode;
 
    typedef enum ON_OFF {
        OFF = 2,
        ON = 1
    }on_off;
 
    typedef enum Operation {
        Configuration = 1,
        Operation = 2
    }operation;
    
 
    typedef enum es920lr_buff_classify {
        BUFF_A = 0,
        BUFF_B = 1
    }ES920LR_Buff_Classify;
 
    //解析方法に関する変数
    const int WAIT_TIME_US = 1000;
 
    typedef enum {
        DATA_STRING = 0,        //データの文字列
        RESPONSE_STRING = 1,    //OKかNGのレスポンスの文字列
        NORMAL_STRING = 2,
        OTHER_STRING = 3,
        RECEIVE_ID = 4,
        INTERACTIVE_STRING = 5
    }ES920LR_Analyze_Selector;
 
 
public:
    /*ES920LR(RawSerial &serial, const int baud = 115200){
        AB = BUFF_A;
        ser = &serial;
        _baud = baud;
        ser->baud(_baud);
        init();
        ser->attach(this, &ES920LR::es920lrHandler, Serial::RxIrq);//受信割り込み設定
        _es920lr_timer.start();//タイマースタート
        printf("hello world\r\n");
    }*/
    ES920LR(RawSerial &serial, RawSerial &pc, const int baud = 115200 ){
        AB = BUFF_A;
        ser = &serial;
        _pc = &pc;
        _baud = baud;
        ser->baud(_baud);
        init();
        _es920lr_timer.start();//タイマースタート
        printf("hello world\r\n");
        wait(2.0f);
        ser->attach(this, &ES920LR::es920lrHandler, Serial::RxIrq);//受信割り込み設定
    }
private:
 
    RawSerial * ser;
    RawSerial * _pc;
    Timer _es920lr_timer;
    int _baud;
 
 
    //バッファ関連の変数
    char buff_A[ES920LR_BUFF_SIZE];
    char buff_B[ES920LR_BUFF_SIZE];
    char* readBuffAddr;
    char* writeBuffAddr;
    ES920LR_Buff_Classify AB;
 
    int es920lr_dataLength;
 
    union ES920LR_CAST {
        double d;
        float f;
        long l;
        int i;
        short s;
        char c;
        char u[8];
    };
    union ES920LR_CAST es920lr_cast;
 
 
    int sendDataSize;
    void(*p_callFunc[256])(void);
    char sendBuff[52];
    
    nodeNumber _nodeNumber;//ノード種別
    band_width _band_width;//帯域幅
    int _diffusivity;//拡散率
    int _ch;//チャンネル
    ON_OFF _ack;//ack受信設定
    ON_OFF _rcvid;//レシーブID表示設定
    transmode _transmode;//転送モード
    operation _operation;//起動モード
    int _baudrate;//UART速度
    int _power;//出力
    int _sendtime;//自動送信時間
    char _senddata[51];//自動送信データ
    uint16_t _panid;//PANネットワークアドレス
    uint16_t _ownid;//自ノードのネットワークアドレス
    uint16_t _dstid;//送信先ノードのネットワークアドレス
    ON_OFF _rssiAdd;//RSSI値を付加するかどうか
    
    int sendIndex;//送信バッファの現在のインデックス,1以上51以下
    int responseFlag;//送信後のレスポンスを受け取るかどうか
 
    void init() {
        _nodeNumber = Coordinator;
        _band_width = _125kHz;
        _diffusivity = 7;//拡散率
        _ch = 1;//チャンネル
        _ack = OFF;//ack受信設定
        _rcvid = OFF;//レシーブID表示設定
        _transmode = Payload;//転送モード
        _operation = Operation;//起動モード
        _baudrate = 115200;//UART速度
        _rssiAdd = OFF;
        _power = 13;
        _sendtime = 0;
        memset(sendBuff, '\0', 51);//送信バッファの初期化
        _panid = 0x0001;
        _ownid = 0x0001;
        _dstid = 0x0000;
 
        memset(buff_A, '\0', ES920LR_BUFF_SIZE);//受信バッファの初期化
        memset(buff_B, '\0', ES920LR_BUFF_SIZE);
        readBuffAddr = buff_B;
        writeBuffAddr = buff_A;
        sendDataSize = 0;
        memset(data, '\0', 50);//受信データ配列の初期化
        for (int i = 0; i < 256; ++i) {//関数ポインタにダミー関数を設定
            p_callFunc[i] = NULL;//&this->dummyFunc;
        }
        es920lr_dataLength = 0;
        sendIndex = 1;
        memset(rssi, '\0', 5);
        memset(rcvPANID, '\0', 5);
        memset(rcvADDR, '\0', 5);
        header = 0;
        responseFlag = 0;
        //ser->printf("start\r\n");
        ser->baud(115200);
        wait(0.5f);
 
    }
 
public:
 
    char data[51];
    char rssi[5];//最新の電波強度
    char rcvPANID[5];
    char rcvADDR[5];
    char header;//最新のデータのヘッダー
 
 
 
    //送信バッファを送信する
    void send() {
        //_pc->printf("index:%d\r\n", sendIndex);
        sendBuff[0] = (uint8_t)(sendIndex - 1);//sendIndexは一つ大きいので
        sendBuff[sendIndex + 1] = '\0';//printf用に最後にnullを追加
        responseFlag = 1;
        for (int i = 0; i <= sendIndex; ++i) {
            ser->putc(sendBuff[i]);
        }
        //ser->printf("%s", sendBuff);//putcにすべきだった
        _pc->printf("index: %d\r\n", sendIndex);
        _pc->printf("send: ");
        for (int i = 0; i < sendIndex; ++i) {
            _pc->printf("%02X ", sendBuff[i]);
        }
        _pc->printf("\r\n");
        
 
        //初期化
        sendIndex = 1;
        memset(sendBuff, '\0', 52);
    }
 
    //送信バッファに追加
    void Write(char arg) {
        if (sendIndex < 51) {//気持ち悪いが、index==51が最大
            es920lr_cast.c = arg;
            sendBuff[sendIndex] = es920lr_cast.u[0];//sendIndexは初期値1なので、＋１は必要ない
            sendIndex += 1;
        }
    }
    void Write(unsigned char arg) {
        if (sendIndex < 51) {//気持ち悪いが、index==51が最大
            es920lr_cast.c = arg;
            sendBuff[sendIndex] = es920lr_cast.u[0];//sendIndexは初期値1なので、＋１は必要ない
            sendIndex += 1;
        }
    }
    void Write(short arg) {
        if (sendIndex < 50) {
            es920lr_cast.s = arg;
            for (int i = 0; i < 2; ++i) {
                sendBuff[sendIndex] = es920lr_cast.u[i];
                ++sendIndex;
            }
        }
    }
    void Write(unsigned short arg) {
        if (sendIndex < 50) {
            es920lr_cast.s = arg;
            for (int i = 0; i < 2; ++i) {
                sendBuff[sendIndex] = es920lr_cast.u[i];
                ++sendIndex;
            }
        }
    }
    void Write(int arg) {
        if (sendIndex < 48) {
            es920lr_cast.i = arg;
            for (int i = 0; i < 4; ++i) {
                sendBuff[sendIndex] = es920lr_cast.u[i];
                ++sendIndex;
            }
        }
    }
    void Write(unsigned int arg) {
        if (sendIndex < 48) {
            es920lr_cast.i = arg;
            for (int i = 0; i < 4; ++i) {
                sendBuff[sendIndex] = es920lr_cast.u[i];
                ++sendIndex;
            }
        }
    }
    void Write(float arg) {
        if (sendIndex < 48) {
            es920lr_cast.f = arg;
            for (int i = 0; i < 4; ++i) {
                sendBuff[sendIndex] = es920lr_cast.u[i];
                ++sendIndex;
            }
        }
    }
    void Write(double arg) {
        if (sendIndex < 44) {
            es920lr_cast.d = arg;
            for (int i = 0; i < 8; ++i) {
                sendBuff[sendIndex] = es920lr_cast.u[i];
                ++sendIndex;
            }
        }
    }
    void Write(long arg) {
        if (sendIndex < 44) {
            es920lr_cast.l = arg;
            for (int i = 0; i < 8; ++i) {
                sendBuff[sendIndex] = es920lr_cast.u[i];
                ++sendIndex;
            }
        }
    }
 
    /**
    @bref データ処理関数を設定
    @param *funcAddr 関数ポインタ、void型引数無し関数のみ設定可
    @param header 関数を選択する0~255のヘッダー
    */
    void attach(void(*funcAddr)(void), unsigned char myHeader) {
        p_callFunc[myHeader] = funcAddr;
        return;
    }
 
    int asciiToNumber(char c) {
        //printf("%c ", c);
        int temp = (int)c;
        if (temp >= 65) {//A ~ F
            return (temp - 55);
        }
        else {//0 ~ 9
            return (temp - 48);
        }
    }
 
    //RSSI値を数値に変換する
    int calcRSSI() {
        return asciiToNumber(rssi[0]) * 4096 + asciiToNumber(rssi[1]) * 256 + asciiToNumber(rssi[2]) * 16 + asciiToNumber(rssi[3]);
    }
 
    void dummyFunc() {
        printf("ATTACH NO FUNCTION\r\n");
    }
 
    short toShort(char *array) {
        //es920lr_cast.s = ((uint16_t)array[0] << 8) | (uint16_t)array[1];
        es920lr_cast.u[0] = array[0];
        es920lr_cast.u[1] = array[1];
        return es920lr_cast.s;
    }
    short toShort(int i) {
        return toShort(&data[i]);
    }
 
    int toInt(char *array) {
        //es920lr_cast.i = ((uint32_t)array[0] << 24) | ((uint32_t)array[1] << 16) | ((uint32_t)array[2] << 8) | (uint32_t)array[3];
        es920lr_cast.u[0] = array[0];
        es920lr_cast.u[1] = array[1];
        es920lr_cast.u[2] = array[2];
        es920lr_cast.u[3] = array[3];
        return es920lr_cast.i;
    }
    int toInt(int i) {
        return toInt(&data[i]);
    }
 
    float toFloat(char *array) {
        es920lr_cast.u[0] = array[0];
        es920lr_cast.u[1] = array[1];
        es920lr_cast.u[2] = array[2];
        es920lr_cast.u[3] = array[3];
        return es920lr_cast.f;
    }
    float toFloat(int i) {
        return toFloat(&data[i]);
    }
 
    double toDouble(char *array) {
        for (int i = 0; i< 8; ++i) {
            es920lr_cast.u[i] = array[i];
        }
        return es920lr_cast.d;
    }
    double toDouble(int i) {
        return toDouble(&data[i]);
    }
 
    long toLong(char *array) {
        for (int i = 0; i < 8; ++i) {
            es920lr_cast.u[i] = array[i];
        }
        return es920lr_cast.l;
    }
    long toLong(int i) {
        return toLong(&data[i]);
    }
 
private:
    //1文字受信するたびに呼ばれ、データを受信バッファに入れる
    void es920lrHandler() {
 
        static int index = -1;//バッファのイテレータとなる
        static char c = 0;//受信したデータを一旦入れる
        static int firstFlag = 1;//最初の1byteなら1
        static int dataSize = 0;
        static int callTime[2] = {0, 0};
        
        c = ser->getc();//1byte受信
 
        //_pc->printf("%02X ", c);
 
        callTime[1] = _es920lr_timer.read_us();//現在の時刻を取得
        if (abs(callTime[1] - callTime[0]) > ES920LR_DATA_INTERVAL) {//前の呼び出しとのΔtから新しいデータか判断
            firstFlag = 1;
        }
        callTime[0] = callTime[1];//時刻の更新
 
        //バイナリモードを想定
        if (firstFlag) {
            dataSize = (unsigned char)c;
            firstFlag = 0;
            index = 0;
            return;
        }
        else {
            writeBuffAddr[index] = c;
    
            if (index == (dataSize - 1)) {//1パケット終わり
                firstFlag = 1;
                writeBuffAddr[index + 1] = '\0';//終端文字追加
                //printf("%s\r\n", writeBuffAddr);
                /*printf("wbuf: ");
                for (int i = 0; i < 49; ++i) {
                    printf("%02X ", writeBuffAddr[i]);
                }
                printf("\r\n");
                */
 
 
                if (AB == BUFF_A) {//現在バッファAに書き込み中
                    readBuffAddr = buff_A;
                    writeBuffAddr = buff_B;
                    AB = BUFF_B;
                }
                else {//現在バッファBに書き込み中
                    readBuffAddr = buff_B;
                    writeBuffAddr = buff_A;
                    AB = BUFF_A;
                }
 
                if (!responseFlag) {//レスポンスではない
 
                    //es920lr_debug();
                    data_analyze(dataSize);//data配列をつくって、rssi値などを更新
 
                    //デバッグ
                    //printf("%s ", rssi);
                    for (int i = 0; i < 49; ++i) {
                        _pc->printf("%02X ", data[i]);
                    }
                    _pc->printf("\r\n");
                    _pc->printf("header: %02X\r\n", header);
                    _pc->printf("size: %d\r\n", dataSize);
 
                    //ユーザー関数呼び出し
                    if (p_callFunc[(uint8_t)header] != NULL) {
                        (*p_callFunc[(uint8_t)header])();
                    }
                }
                else {//レスポンス
                    responseFlag = 0;
                    //デバッグ
                    _pc->printf("res %s\r\n", readBuffAddr);
                }
            }
 
            ++index;//カウントをインクリメント
            return;
        }
    }
 
    //デバッグ用
    void es920lr_debug() {
        printf("%s\r\n", readBuffAddr);
    }
 
    //シリアル通信の通信速度変更　※設定を変更するわけではない
    void baud(int baudrate) {
        ser->baud(baudrate);
    }
 
 
 
    /**
    @bref データを取得します
    */
    void data_analyze(int length) {
        static char tempStr[70] = {};
        
        for (int i = 0; i < length; ++i)    tempStr[i] = readBuffAddr[i];
 
        memset(data, 0, 50);
        if (_rcvid == ON) {//相手ノードネットワークアドレス付与設定
            if (_rssiAdd == OFF) {
                //AAAABBBB!!!!...!\0
                // |   |    |     +- <CR><LF>文字列終了
                // |   |    +------- データ
                // |   +------------ 送信元アドレス
                // +---------------- 送信元PAN ID
                for (int i = 0; i < 4; ++i) {
                    rcvPANID[i] = tempStr[i];
                    rcvADDR[4 + i] = tempStr[4 + i];
                }
                header = tempStr[8];
                //strncpy(data, &tempStr[9], length - 9);
                for (int i = 0; i < length - 9; ++i) {
                    data[i] = tempStr[9 + i];
                }
                return;
            }
            else {
                //1111AAAABBBB!!!!...!\0
                // |    |   |    |    +-- <CR><LF>文字列終了
                // |    |   |    +------- データ
                // |    |   +------------ 送信元アドレス
                // |    +---------------- 送信元PAN ID
                // +--------------------- RSSI値
                for (int i = 0; i < 4; ++i) {
                    rssi[i] = tempStr[i];
                    rcvPANID[4 + i] = tempStr[4 + i];
                    rcvADDR[8 + i] = tempStr[8 + i];
                }
                header = tempStr[12];
                //strncpy(data, &tempStr[13], length - 13);
                for (int i = 0; i < length - 13; ++i) {
                    data[i] = tempStr[13 + i];
                }
            }
        }
        else {//相手ノードネットワークアドレスなし
            if (_rssiAdd == OFF) {
                //!!!!...!\0
                header = tempStr[0];
                //strncpy(data, &tempStr[1], length-1);
                for (int i = 0; i < length - 1; ++i) {
                    data[i] = tempStr[1 + i];
                }
                return;
            }
            else {//RSSI値付与
                  //1111!!!!...!\0
                  // |    |     +-- <CR><LF>文字列終了
                  // |    +-------- データ
                  // +------------- RSSI値
                for (int i = 0; i < 4; ++i) {
                    rssi[i] = tempStr[i];
                }
                //strncpy(data, &tempStr[5], length- 5);
                for (int i = 0; i < length - 5; ++i) {
                    data[i] = tempStr[5 + i];
                }
            }
        }
 
    }
 
    
 
};
 
 
//<<演算子オーバーロード
ES920LR& operator<<(ES920LR& es, const char& arg) {
    es.Write(arg);
    return es;
}
 
ES920LR& operator<<(ES920LR& es, const unsigned char& arg) {
    es.Write(arg);
    return es;
}
 
ES920LR& operator<<(ES920LR& es, const short& arg) {
    es.Write(arg);
    return es;
}
 
ES920LR& operator<<(ES920LR& es, const unsigned short& arg) {
    es.Write(arg);
    return es;
}
 
ES920LR& operator<<(ES920LR& es, const int& arg) {
    es.Write(arg);
    return es;
}
 
ES920LR& operator<<(ES920LR& es, const unsigned int& arg) {
    es.Write(arg);
    return es;
}
 
ES920LR& operator<<(ES920LR& es, const long& arg) {
    es.Write(arg);
    return es;
}
ES920LR& operator<<(ES920LR& es, const float& arg) {
    es.Write(arg);
    return es;
}
 
ES920LR& operator<<(ES920LR& es, const double& arg) {
    es.Write(arg);
    return es;
}
 
void operator<<(ES920LR& es, const ES920LR::END_COMMAND cmd) {
 
    switch (cmd)
    {
        case ES920LR::endl:
            es.send();
            break;
        case ES920LR::asciiEnd:
            es.Write('\r');
            es.Write('\n');
            es.send();
            break;
    }
 
}