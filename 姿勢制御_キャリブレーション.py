#ライブラリ
from machine import Pin, I2C, SPI, PWM, Timer
from utime import sleep, ticks_ms
import os

from mpu9250 import MPU9250
from mpu6500 import MPU6500, SF_G, SF_DEG_S


#通信関係
i2c = I2C(1,scl=Pin(27), sda=Pin(26), freq=100000)   #９軸センサ
mpu6500 = MPU6500(i2c, accel_sf=SF_G, gyro_sf=SF_DEG_S)
sensor = MPU9250(i2c, mpu6500=mpu6500)


#変数設定
gyro_x, gyro_y, gyro_z = sensor.gyro

total_gyro_x = total_gyro_y = total_gyro_z = 0
original_gyro_x = original_gyro_y = original_gyro_z = 0


#関数設定
def main():
    global gyro_x, gyro_y, gyro_z, total_gyro_x, total_gyro_y, total_gyro_z,
    global original_gyro_x, original_gyro_y, original_gyro_z


    while True:
        for i in range(10):
            gyro_x = sensor.gyro[0]
            gyro_y = sensor.gyro[1]
            gyro_z = sensor.gyro[2]
        
            total_gyro_x += gyro_x
            total_gyro_y += gyro_y
            total_gyro_z += gyro_z
        
        original_gyro_x = total_gyro_x / 10
        original_gyro_y = total_gyro_y / 10
        original_gyro_z = total_gyro_z / 10
        
        print('original_gyro_x = {}, original_gyro_y = {}, original_gyro_z = {}'.format(original_gyro_x, original_gyro_y, original_gyro_z))
        

if __name__=='__main__':
    main()

