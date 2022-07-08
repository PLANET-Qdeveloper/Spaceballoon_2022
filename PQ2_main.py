from machine import Pin, UART, I2C, reset, Timer, lightsleep
from utime import ticks_ms
import time

from PQ_LPS22HB import LPS22HB
from PQ_RM92 import RM92A
#from PQ_GPS import GPS

block_flug = False
signal_timing = 2000
irq_called_time = time.ticks_ms() 

rm_uart = UART(0, baudrate=115200, tx=Pin(0), rx=Pin(1))
gps_uart = UART(1, baudrate=115200, tx=Pin(4), rx=Pin(5))
i2c = I2C(0, scl=Pin(21), sda=Pin(20))

rm = RM92A(rm_uart)
#gps = GPS(gps_uart)
lps = LPS22HB(i2c)

p2 = Pin(2, Pin.IN) # irq用のピン

# 定数
T_BURN = 3300
T_SEP = 12200
T_HEATING = 9000
T_RECOVERY = 300000

# 変数整理
burning = False
detect_peak = False
flight_pin = Pin(26, Pin.IN)
sep_pin = Pin(27, Pin.OUT)
sep_pin.value(0)
phase = 0
mission_timer_reset = 0
init_mission_time = ticks_ms()
mission_time = 0
mission_time_int = 0
init_flight_time = 0
flight_time = 0
flight_time_int = 0
init_sep_time = 0
sep_time = 0

peak_count = 0

press_index = 0
press_buf = [0]*10
pressure = prev_press = ground_press = tempareture = 0
lat = lon = alt = 0

landed = False
apogee = False
separated = False

led = Pin(25, Pin.OUT)

# Timerオブジェクト(周期処理用)
peak_detection_timer = Timer()
read_timer = Timer()
downlink_timer = Timer()

def get_smoothed_press():
    global press_index, press_buf
    #press_buf[press_index] = lps.read_pressure()
    press_buf[press_index] = 1010
    press_index += 1
    if press_index == 9:
        press_index = 0
        for i in range(10):
            for j in range(10-i-1):
                if press_buf[j] > press_buf[j+1]:   # 左のほうが大きい場合
                    press_buf[j], press_buf[j+1] = press_buf[j+1], press_buf[j] # 前後入れ替え
    press_median = (press_buf[4]+press_buf[5])/2
    # LPFはいらないかも
    return press_median

def peak_detection(t):
    global prev_press, pressure
    global peak_count, apogee
    if pressure > prev_press:
        peak_count += 1
    else:
        peak_count = 0
    prev_press = pressure
    if peak_count == 5:
        apogee = True

def read():
    global phase
    global mission_time, mission_timer_reset, mission_time_int, init_mission_time, init_flight_time, flight_time_int, flight_time, sep_time, init_sep_time
    global pressure, temperature
    global lat, lon, alt
    
    # 時間関係
    mission_time = ticks_ms() - init_mission_time
    mission_time_int = int(mission_time/1000)    # 小数点以下は切り捨て
    if mission_time_int > 180:
        init_mission_time = ticks_ms()
        mission_timer_reset += 1
    if phase == 2:
        flight_time = ticks_ms() - init_flight_time
        flight_time_int = int(flight_time/1000)
    if phase == 3:
        sep_time = ticks_ms() - init_sep_time
    
    # センサーの値を取得
    pressure = get_smoothed_press()  # medianをとってくる
    #temperature = lps.getTemperature()
    temperature = 28
    #lat = gps.getLat()
    #lon = gps.getLon()
    #alt = gps.getAlt()

def debug():
    print('------------------------------------------------------------------')
    print(phase, flight_pin.value(), sep_pin.value())
    print(mission_timer_reset, mission_time, flight_time, sep_time)
    print(pressure, temperature)
    print(lat, lon, alt)
    
    
def downlink(t):
    debug()
    global phase
    global flight_pin, sep_pin
    global mission_timer_reset, mission_time_int, flight_time_int 
    global burning, apogee, separated, landed
    global pressure, temperature, lat, lon, alt

    flags = 0
    flags |= flight_pin.value() << 7 
    flags |= burning << 6
    flags |= apogee << 5
    flags |= separated << 4
    flags |= sep_pin.value() << 3
    flags |= landed << 2
    flags |= 0 << 1
    flags |= 0 << 0 
    
    press_int = int(pressure*100)  # 下2桁までを繰り上げして型変換
    press_bits_A = press_int >> 16 & 0xff
    press_bits_B = press_int >> 8  & 0xff
    press_bits_C = press_int >> 0  & 0xff

    temp_int = int(temperature) # 小数点以下は切り捨て
    temp_bits = temp_int >> 0 & 0xff

    lat_int = int(lat*10000)    # 下4桁までを繰り上げ
    lat_bits_A = lat_int >> 16 & 0xff 
    lat_bits_B = lat_int >> 8  & 0xff
    lat_bits_C = lat_int >> 0  & 0xff
    lon_int = int(lon*10000)    # 下4桁までを繰り上げ
    lon_bits_A = lon_int >> 16 & 0xff 
    lon_bits_B = lon_int >> 8  & 0xff
    lon_bits_C = lon_int >> 0  & 0xff
    
    send_data = bytearray(16)
    send_data[0] = 0x44   # Header
    send_data[1] = mission_timer_reset
    send_data[2] = mission_time_int
    send_data[3] = flight_time_int
    send_data[4] = phase
    send_data[5] = flags
    send_data[6] = press_bits_A
    send_data[7] = press_bits_B
    send_data[8] = press_bits_C
    send_data[9] = temp_bits 
    send_data[10] = lat_bits_A
    send_data[11] = lat_bits_B
    send_data[12] = lat_bits_C
    send_data[13] = lon_bits_A
    send_data[14] = lon_bits_B
    send_data[15] = lon_bits_C
    
    rm.send(0xFFFF, send_data)
    
downlink_timer.init(period=1000, callback=downlink)
    
def command_handler(p2):
    global block_irq
    global irq_called_time
    global signal_timing
    rx_buf = bytearray(4)
    if (time.ticks_ms() - irq_called_time) > signal_timing:
        block_irq = False
    if not block_irq:
        rm_uart.readinto(rx_buf, 4)
        command = rx_buf[1]
        if command == 0xB0:     # READY->SAFETY
            if phase == 1: phase = 0
        elif command == 0xB1:   # SAFETY->READY
            if phase == 0: phase = 1
        elif command == 0xB2:   # READY->FLIGHT
            if phase == 1: phase = 2
        elif command == 0xB3:   # SEP
            if (burning == False & phase == 2): phase = 3
        elif command == 0xB4:   # EMERGENCY
            if (phase >= 1 & phase <= 3): phase = 5
        elif command == 0xB5:   # RESET
            reset()
        irq_called_time = time.ticks_ms()
        block_irq = True

irq_obj = p2.irq(handler=command_handler, trigger=(Pin.IRQ_FALLING | Pin.IRQ_RISING))



def main():
    global phase
    global flight_pin, sep_pin
    global init_flight_time, init_sep_time
    global burning, apogee, separated, landed
    
    while True:
        read()
        
        if phase == 0:  # SAFETYモード
            phase = 1
            
        elif phase == 1:    # READYモード     
            if flight_pin.value() == 1:
                burning = True
                init_flight_time = ticks_ms()
                phase = 2
        
        elif phase == 2:    # FLIGHTモード
            if flight_time > T_BURN:
                if burning == True:
                    burning = False
                    peak_detection_timer.init(period=100, callback=peak_detection)
            if (not burning) and (apogee or (flight_time > T_SEP)):
                phase = 3
                peak_detection_timer.deinit()
        
        elif phase == 3:   # SEPモード
            sep_pin.value(1)
            if not separated:
                init_sep_time = ticks_ms()
                separated = True
            else:
                if sep_time > T_HEATING:
                    sep_pin.value(0)
                    phase = 4
           
        elif phase == 4:   # RECOVERY
            if not landed:
                if (pressure > ground_press) or (flight_time > T_RECOVERY):
                    #relay = 0
                    landed = True
            else:
                lightsleep(1000)
            
        else:   # EMERGENCY
            sep_pin.value(0)
            print("EMERGENCY!!!!")
            lightsleep(1000)
        lightsleep(10)

if __name__ == '__main__':
    main()
