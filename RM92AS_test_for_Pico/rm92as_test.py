from machine import UART

rm92as = UART(1, baudrate=115200, rx=5, tx=4)

while True:
    