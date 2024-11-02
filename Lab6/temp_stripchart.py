# Authors:
#   Kerem Oktay
#   Idil Bil
#
# Functionality:
#   Draws a stripchart from the temperature data flowing in serial comm.
# 
# Note:
#   Some parts of this code is taken from serial_in and stripchart_sinewave
#   codes provided on the course page.

import time
import serial
import serial.tools.list_ports
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import sys, time, math

temp_low = -45  # lowest temperature is actually -40, 5 units of margin given for the graph
temp_high = 105 # highest temperature is actually 100, 5 units of margin given for the graph
xsize = 100     # time axis size

selected_color = 'purple' # can change the color of the graph

# configure the serial port
try: 
    ser = serial.Serial(
        port = 'COM8', # Change as needed
        baudrate = 115200,
        parity = serial.PARITY_NONE,
        stopbits = serial.STOPBITS_TWO,
        bytesize = serial.EIGHTBITS
    )
    ser.isOpen()
except:
    portlist = list(serial.tools.list_ports.comports())
    print('Available serial ports:')
    for item in portlist:
        print(item[0])
    exit()

def temp_data_read():
    t = temp_data_read.t
    while True:
        t += 1
        temp_value_float = float(ser.readline())  # temp_value_float has the temperature reading as a float number
        yield t, temp_value_float

def run(data):
    # update the data
    t,y = data
    if t>-1:
        xdata.append(t)
        ydata.append(y)
        if t>xsize: # Scroll to the left.
            ax.set_xlim(t-xsize, t)
        line.set_data(xdata, ydata)

    return line,
    
def on_close_figure(event):
    sys.exit(0)

temp_data_read.t = -1
fig = plt.figure()
fig.canvas.mpl_connect('close_event', on_close_figure)
ax = fig.add_subplot(111)
line, = ax.plot([], [], lw=2, color=selected_color)
ax.set_ylim(temp_low, temp_high)
ax.set_xlim(0, xsize)
ax.grid()
xdata, ydata = [], []

# Important: Although blit=True makes graphing faster, we need blit=False to prevent
# spurious lines to appear when resizing the stripchart.
ani = animation.FuncAnimation(fig, run, temp_data_read, blit=False, interval=50, repeat=False)
plt.show()
 


        
