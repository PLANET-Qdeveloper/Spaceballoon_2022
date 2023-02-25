# ２次元リアルタイムグラフの雛形

# Library
import numpy as np # プロットするデータ配列を作成するため
import matplotlib.pyplot as plt # グラフ作成のため
import random
import serial
import time

comport = serial.Serial('COM13', baudrate=115200, parity=serial.PARITY_NONE)

# params
a = []
b = []
p = []
q = []
sleepTime = 0.1  # １フレーム表示する時間[s]
i = 0
lon = 130
lat = 33
fig = plt.figure()
alt = 0

press = fig.add_subplot(2, 2, 1)
gps = fig.add_subplot(2, 2, 2)

"""
Height = fig.add_subplot(2,2,3)
"""

# plotting
while True:

    recv_data = comport.readline().split(b",")
    pr = float(recv_data[0])
    t = float(recv_data[1])
    lat = float(recv_data[2])
    lon = float(recv_data[3])
    alt = float(recv_data[4])

    press = fig.add_subplot(2, 2, 1)
    gps = fig.add_subplot(2, 2, 2)

    '''
    i = i + 1 # フレーム回数分グラフを更新
    x = 1023 - i - random.uniform(0,10)
    '''
    a.append(pr)# プロットするデータを作成
    b.append(t)

    # P = float(recv_data[2])
    # Q = float(recv_data[3])
    p.append(lat)
    q.append(lon)
    
    fig.text(0.1, 0.35, "PRESSURE")
    fig.text(0.1, 0.3, pr)
    fig.text(0.1, 0.25, "TEMP")
    fig.text(0.1, 0.2, t)
    fig.text(0.6, 0.35, "LAT")
    fig.text(0.6, 0.3, lat)
    fig.text(0.6, 0.25, "LON")
    fig.text(0.6, 0.2, lon)
    fig.text(0.3, 0.25, "ALT")
    fig.text(0.3, 0.2, alt)

    # plt.plot(b,a) # データをプロット
    # plt.ylim(1000, 1020)

    press.plot(a, color="blue")
    gps.plot(q,p, color="blue")

    plt.draw() # グラフを画面に表示開始
    plt.pause(sleepTime) # SleepTime時間だけ表示を継続
    plt.clf() # プロットした点を消してグラフを初期化

    
