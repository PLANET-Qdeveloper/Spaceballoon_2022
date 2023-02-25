'''
mongolia balloon main program
'''

from micropython import const
from machine import Pin, SPI, UART, I2C, reset, Timer, lightsleep
from utime import ticks_ms
import time, os
import sys
import sdcard

import bme280
from PQ_LPS22HB import LPS22HB #気圧センサ
from PQ_RM92A import RM92A #無線機
from micropyGPS import MicropyGPS
#BMEのライブラリ系誰かよろ　頂点検知のやつはlpsとbmeをorで結びつけるんやろ？


'''
SD関係
'''
cs = Pin(17, Pin.OUT)
spi = SPI(0, baudrate=32000000, sck=Pin(18), mosi=Pin(19), miso=Pin(16))
sd = sdcard.SDCard(spi, cs)
os.mount(sd, '/sd')
file_index = 1
file_name = '/sd/PQ_AVIONICS'+str(file_index)+'.csv'
init_sd_time = 0
while True:
    try:
        file = open(file_name, 'r')
    except OSError: # 新しい番号であればエラーに拾われる
        file = open(file_name, 'w')
        init_sd_time = ticks_ms()
        break
    if file:    # 同じ番号が存在する場合引っかかる
        file.close()    # 一旦古いファイルなので閉じる
        file_index += 1
        file_name = '/sd/PQ_AVIONICS'+str(file_index)+'.csv'

file.write("time,pressure,temperature,lat,lon,alt\r\n")


'''
定数宣言
'''
SIGNAL_TIMING = const(1000)  
T_BURN = const(6000)  #Engine burning time
T_APOGEE = const(25790) #頂点検知
T_HEATING = const(5000) #ニクロム線加熱時間(使わんけど)
T_RECOVERY = const(120610)
T_RELAY_OFF = const(3000000)


'''
通信関係
'''
# UART通信
rm_uart= UART(0, baudrate=115200, tx=Pin(0), rx=Pin(1))
rm = RM92A(rm_uart)

gps_uart = UART(1, baudrate=9600, tx=Pin(8), rx=Pin(9))
gps = MicropyGPS()

i2ci =  I2C(0, scl=Pin(21), sda=Pin(20))
lps = LPS22HB(i2ci)
bme=bme280.BME280(i2c=i2ci)
led = Pin(12, Pin.OUT)
mos = Pin(4, Pin.OUT)

'''
Timerオブジェクト(周期処理用)
'''
downlink_timer = Timer()
record_timer = Timer()
read_timer = Timer()

signal_timing = 1000
init_mission_time = ticks_ms()
irq_called_time = ticks_ms()

'''
変数宣言
'''
temp = []
pres = []

PRESS = 0
TEMP = 0
lat = 0
lon = 0
alt = 0

flight_time = 0
flight_time_A =0
flight_time_B =0
init_time = 0

def read(t):
    global PRESS,TEMP,lat,lon,alt    
    try:
        t1,p1,h1 = bme.read_compensated_data()
    except OSError: 
        t1 = p1 = h1 = 0
    p1 = p1 // 256
    pi1 = p1 / 100
    ti1 = t1 / 100
    
    pres.append(pi1)
    temp.append(ti1)
    
    try:
        press = lps.read_pressure()
        temperature = lps.read_temperature()
    except OSError: 
        press = temperature = 0
        
    pres.append(press)
    temp.append(temperature)
    
    pres.sort()
    temp.sort()

    PRESS = pres[-1]
    TEMP = temp[-1]

    pres.clear()
    temp.clear()
    
    len = gps_uart.any()
    if len > 0:
        b = gps_uart.read(len)
        for x in b:
            if 10 <= x <=126:
                status = gps.update(chr(x))
                if status:
                    lat = gps.latitude[0] + gps.latitude[1]/60
                    lon = gps.longitude[0] + gps.longitude[1]/60
                    alt = gps.altitude
read_timer.init(period=500, callback=read)


def downlink(t):
    global flight_time_A, flight_time_B, PRESS, lat, lon, alt
    PRESS1 = PRESS * 10
    TEMP1 = TEMP * 100
    LAT = lat
    LON = lon
    ALT = alt
    
    #気温・気圧・高度は256進数で、緯度・経度は整数部分と小数部分(2桁ずつ)に分けて送信
    
    press1 = int(PRESS1 // 256)
    press2 = int(PRESS1 % 256)
    
    temp1 = int(TEMP1 // 256)
    temp2 = int(TEMP1 % 256)
    
    lat1 = int(LAT)
    lat2 = int((LAT - lat1) * 100)
    lat3 = int(((LAT - lat1) * 100 - lat2) * 100)
    
    lon1 = int(LON)
    lon2 = int((LON - lon1) * 100)
    lon3 = int(((LON - lon1) * 100 - lon2) * 100)
    
    alt1 = int(ALT // 256)
    alt2 = int(ALT % 256)
    
    print(press1,press2,temp1,temp2,lat1,lat2,lat3,lon1,lon2,lon3,alt1,alt2)
    
    send_data = bytearray(21)
    send_data[0] = 0x24
    send_data[1] = press1
    send_data[2] = press2
    send_data[3] = temp1
    send_data[4] = temp2
    send_data[5] = lat1
    send_data[6] = lat2
    send_data[7] = lat3
    send_data[8] = lon1
    send_data[9] = lon2
    send_data[10] = lon3
    send_data[11] = alt1
    send_data[12] = alt2
    send_data[13] = 13
    send_data[14] = 14

    rm.send(0xFFFF, send_data)
    print("a")
    
downlink_timer.init(period=5000, callback=downlink)

def record(t):
    global PRESS, TEMP, lat, lon, alt, flight_time, init_sd_time, file
    file.write("%d,%f,%f,%f,%f,%f\r\n"%(flight_time, PRESS, TEMP, lat, lon, alt))
    if (ticks_ms() - init_sd_time > 10000):    # 10秒ごとにclose()して保存する
        file.close()
        file = open(file_name, "a")
        init_sd_time = ticks_ms()
record_timer.init(period=1000, callback=record)

def main():
    global flight_time_A, flight_time_B, flight_time
    while True:
         lightsleep(100)
         led.value(1)
         lightsleep(100)
         led.value(0)
         flight_time = (ticks_ms() - init_time)/1000
         flight_time_A = int(flight_time//256)
         flight_time_B = int(flight_time%256)
         
         '''
         temperature2 = lps.read_temperature()
         print(temperature2)
        if temperature2 < 25:
            mos.value(1)
            time.sleep(5)
        else:
            mos.value(0)
            time.sleep(5)
        '''
if __name__=='__main__':
    main()
