#include "mbed.h"
#include "PQ_RM92A.h"

unsigned char rx_buf[256];


RM92A::RM92A(Serial &serial){
    _serial = &serial;
    _serial->attach(callback(this, &RM92A::receive), Serial::RXIrq);   // 割り込みを設定
    memset(tx_buf, '\0', 228);  // 送信バッファの初期化
    memset(rx_buf, '\0', 228);  // 受信バッファの初期化
    index = 0;
    flag = 0;
    response = false;
}

/**********************************************************************************
 * 
 * |@|@|Len|adress1|adress2|data|0xAA|
 * dataは228byteまで.
 * 
 * *******************************************************************************/
unsigned char tx_buf[128];
void RM92A::send(unsigned short adress, unsigned char *data, int size)
{
    tx_buf[0] = "@";
    tx_buf[1] = "@";
    tx_buf[2] = size;
    tx_buf[3] = (unsigned char)((adress >> 8) & 0xFF);  // 0xFF = (1111 1111)
    tx_buf[4] = (unsigned char)((adress >> 0) & 0xFF);
    strcpy(&tx_buf[5],data,size);
    tx_buf[size+5] = 0xAA;
    int buf_size = size + 6;

    _serial->write(tx_buf, buf_size);
}

void RM92A::attach(void(*func_ptr)(char*)){
    func = func_ptr;
}

void RM92A::receive()
{
    while(_serial->available()>0) {
        data = _serial->read();
        
    }
    char *split;

    split = strtok(rx_buf, ",\r\n");


    /**************************************************
    if(flag == 0) {
        rx_size = _serial->getc();
        memset(rx_buf, '\0', 228);
        index = 0;
        flag = 1;
    }
    if(flag == 1) {
        rx_buf[index] = _serial->getc();
        if(index == rx_size - 1) {
            if(!response) {
                if(func != NULL) {
                    (*func)(rx_buf);
                }
            } else {
                response = false;
            }
            flag = 0;
        } else {
            index ++;
        }
    }
    *************************************************/
 
}