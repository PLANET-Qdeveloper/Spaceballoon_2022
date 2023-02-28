#モンゴル打ち上げの分離とリーフィング用のプログラム

'''
ライブラリ
'''

from machine import I2C, Pin, Timer, UART
from utime import ticks_ms, sleep

from PQ_RM92A import RM92A
from bme280 import BME280
from PQ_LPS22HB import LPS22HB


'''
定数宣言
'''

signal_timing = 1000
delay = 5
pre0 = 1013  #海面気圧。[hPa]で入力。
alti = 2000  #目標開始高度。[m]で入力。


'''
通信関係
'''

# UART通信
rm_uart= UART(0, baudrate=115200, tx=Pin(0), rx=Pin(1))

#I2Cのピン設定
lps_i2c = I2C(0, sda=Pin(12), scl=Pin(13),freq=400000)
bme_i2c = I2C(1, sda=Pin(6), scl=Pin(7),freq=400000)  


'''
クラスからインスタンスを生成
'''

#リストのインスタンス
pres = [] 
temp = []

#ピンのインスタンス
Aphase = Pin(18, Pin.OUT)
Aenable = Pin(19, Pin.OUT)
Bphase = Pin(20, Pin.OUT)
Benable = Pin(21, Pin.OUT)


#ピンのインスタンス
p2 = Pin(2, Pin.IN)  #irq用
IN1 = Pin(16, Pin.OUT)
IN2 = Pin(18, Pin.OUT)
nich = Pin(27, Pin.OUT)  #ニクロム線用

IN1.value(0)
IN2.value(0)
nich.value(0)


'''
Timerオブジェクト(周期処理用)
'''

irq_called_time = ticks_ms()
data_timer = Timer()

'''
変数宣言
'''

rm = RM92A(rm_uart)
lps = LPS22HB(lps_i2c)
bme = BME280(i2c=bme_i2c)

phase = 0
command = 48

pre_last = Mpress

rock = False

'''
関数
'''

#ニクロム線加熱用
def heat():

    nich.value(1)
    sleep(10)
    nich.value(0)


#分離機構作動
def function():
    
    IN2.value(0)
    IN1.value(1)
    sleep(0.5)

    IN1.value(0)
    IN2.value(1)
    sleep(10)
   
    IN1.value(1)
    IN2.value(1)
    sleep(3)

    IN1.value(0)
    IN2.value(0)


#アップリンク
def command_handler(p2):
    global block_irq, irq_called_time, phase, command

    rx_buf = bytearray(10)

    if (ticks_ms() - irq_called_time) > signal_timing:
        block_irq = False
    if not block_irq:
        rm_uart.readinto(rx_buf, 4)
        command = rx_buf[0]
        if command == 48:    
            if phase == 1:
                phase = 0
        elif command == 49:   
            if phase == 0:
                phase = 1
                
        irq_called_time = ticks_ms()
        block_irq = True
irq_obj = p2.irq(handler=command_handler, trigger=(Pin.IRQ_FALLING | Pin.IRQ_RISING)) 


def rotateCw():
    Aenable.value(1)
    Benable.value(1)

    for j in range(15):

        for i in range(8):
            Aphase.value(1)
            sleep(0.005)
            Bphase.value(1)
            sleep(0.005)
            Aphase.value(0)
            sleep(0.005)
            Bphase.value(0)
            sleep(0.005)    

        sleep(delay)   

    Aenable.value(0)
    Benable.value(0)
        


def data():
    global t1, p1, h1, pi1, ti1, press, temperature, Mpress, Mtemp
    try:
        t1,p1,h1 = bme.read_compensated_data() 

    except OSError: 
        t1=p1=h1=0 

    p1 = p1 // 256 #整数値に変換
    pi1 = p1 / 100 #hPaの形に修正
    ti1 = t1/100 

    pres.append(pi1)
    temp.append(ti1) 
    
    try:
        press = lps.read_pressure()
        temperature = lps.read_temperature()

    except OSError: 
        press=temperature=0

    pres.append(press)
    temp.append(temperature)
    
    pres.sort() 
    temp.sort()

    Mpress = pres[-1]
    Mtemp = temp[-1]

    pres.clear() 
    temp.clear()
data_timer.init(period=500, callback=data)


def altitude(pre, tem):
    T = tem + 273.15
    nume = (pre0/pre)**(1/5.257) - 1

    alti = nume*T/0.0065

    return alti


def main():
    global phase, Mpress, Mtemp, pre_now, pre_last, tem_now, rock
    while True:
        pre_now = Mpress
        tem_now = Mtemp
        if altitude(pre_now,tem_now) >= 10000:
            rock = True
        if pre_now > pre_last:
            if altitude(pre_now, tem_now) <= 2000:
                if rock:
                    rotateCw()

        if phase == 1:
            heat()
            sleep(13)
            function()

            phase = 2


        pre_last = pre_now
        sleep(1)


if __name__=='__main__':
    main()
