#ライブラリ
from machine import Pin, I2C, SPI, PWM, Timer
from utime import sleep, ticks_ms
import os

import sdcard
from mpu9250 import MPU9250
from mpu6500 import MPU6500, SF_G, SF_DEG_S


#ピン設定
pwm = PWM(Pin(18)) #サーボモーター
pwm.freq(50)


#通信関係
i2c = I2C(1,scl=Pin(27), sda=Pin(26), freq=100000)   #９軸センサ
mpu6500 = MPU6500(i2c, accel_sf=SF_G, gyro_sf=SF_DEG_S)
sensor = MPU9250(i2c, mpu6500=mpu6500)

cs = Pin(5, Pin.OUT)    #SDカード
spi = SPI(0, baudrate=32000000, sck=Pin(2), mosi=Pin(3), miso=Pin(4))

# Timerオブジェクト
record_timer = Timer()
init_sd_time = ticks_ms()
init_mission_time = ticks_ms()


#SDカード関連
sd = sdcard.SDCard(spi, cs)
os.mount(sd, '/sd')
file_index = 1
file_name = '/sd/PQ_SpaceBalloon_MNG_2023'+str(file_index)+'.txt'
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
        file_name = '/sd/PQ_SpaceBalloon_MNG_2023'+str(file_index)+'.txt'

file.write("mission_time,gyro_x,gyro_y,gyro_z,n,degree,acction,\r\n")


#変数設定
t1 = 60

n = 0
m = 0
degree = 90
acction = 0
gyro_x, gyro_y, gyro_z = sensor.gyro
mission_time = ticks_ms() - init_mission_time

#キャリブレーション用
original_gyro_x = 0
original_gyro_y = 0
original_gyro_z = 0


#関数設定
def record(t):
    global file, init_sd_time, mission_time, gyro_x, gyro_y, gyro_z, n, degree, acction
    file.write("%f,%f,%f,%f,%f,%d,%d"%(mission_time, gyro_x, gyro_y, gyro_z, n, degree, acction))
    if (ticks_ms() - init_sd_time > 1000):    # 1秒ごとにclose()して保存する
        file.close()
        file = open(file_name, "a")
        init_sd_time = ticks_ms()
record_timer.init(period=100, callback=record)


def servo_value(vari):
    degree = (vari + 9) * 10
    return int((degree * 9.5 / 180 + 2.5) * 65535 / 100)


def main():
    global gyro_x, gyro_y, gyro_z, n, m, acction, t1

    while True:
        gyro_x = sensor.gyro[0] - original_gyro_x
        gyro_y = sensor.gyro[1] - original_gyro_y
        gyro_z = sensor.gyro[2] - original_gyro_z

        if gyro_z > t1:
            if n <= 3:
                n += 1
                acction = 2
                
            else:
                for i in range(n*10):
                    n -= 0.1
                    sleep(0.5)
                acction = 4
                sleep(1)
                
                    

        if gyro_z < -t1:
            if n >= -3:
                n -= 1
                acction = -2
                
            else:
                for i in range((-n)*10):
                    n += 0.1
                    sleep(0.5)
                acction = -4
                sleep(1)
        
        if -t1 <= gyro_z <= t1:
            if n > 0:
                n -= 0.1
                sleep(0.5)
                acction = 1
                
            if n < 0:
                n += 0.1
                sleep(0.5)
                acction = -1

        pwm.duty_u16(servo_value(n))
        sleep(0.5)


if __name__=='__main__':
    main()
