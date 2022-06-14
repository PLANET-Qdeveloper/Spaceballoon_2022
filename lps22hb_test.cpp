#include "mbed.h"
#include "PQ_LPS22HB.h"

I2C i2c(p28, p27);
float press;
Serial pc(USBTX, USBRX, 115200);

void init();

LPS22HB lps(i2c, LPS22HB::SA0_LOW); // 気圧，温度


int main(){
    init();
    while(1){
        lps.read_press(&press);
        
        pc.printf("%f[hPa]\r\n", press);    //1010[hPa]
        
        wait(0.5);
    }
}

void init(){
    lps.begin();
    }
