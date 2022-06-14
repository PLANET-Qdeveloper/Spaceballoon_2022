#include "mbed.h"
#include "PQ_RM92A.h"

Serial pc(USBTX, USBRX, 115200);
Serial rm_serial(PA_9, PA_10, 115200);

Rm92A rm(rm_serial);

char command[1];

void downlink_handler(char *data);

void main(){
    pc.printf("PQ Space Balloon Project!!!\r\n");
    rm.attach(downlink_handler);

    while(1){
        if(pc.readable()){
            command[0] = pc.getc() + 0x80;
            es.send(command, 1);
            pc.ptintf("send:%02x\r\n", command[0]);
        }
    }
}

void downlink_handler(char *data){
    int rssi = *(int)&data[0];
    short mission_time_bits = *(short)&data[]
    char fall = 
}