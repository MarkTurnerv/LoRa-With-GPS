# -*- coding: utf-8 -*-
"""
Created on Fri Jul 12 10:09:00 2024

@author: Mark Turner

Purpose: read in data (intended for GPS sent over LoRa) from serial port
and write to csv file

Note: File never closes serial line, so console must be closed between runs to force
        serial to disconnect. Also disconnect serial port from Arduino so line is not busy
"""
import serial
import serial.tools.list_ports
#import csv
import os
import matplotlib  #Use to move figure to upper right corner
import matplotlib.pyplot as plt #import matplotlib library
import pandas as pd

plt.ion() #Tell matplotlib you want interactive mode to plot live data
#List available serial ports and have user input comport to use
ports = list(serial.tools.list_ports.comports())
print("Available com ports: ")
for p in ports:
    print(p)
    
ArduinoPort = input("Enter com port: ")

ser = serial.Serial(ArduinoPort,timeout=10) #open serial port with 10 sec timout

#check current filepath
script_dir = os.path.dirname(__file__)
results_dir = os.path.join(script_dir, 'DataFiles/')    #Make directory named 'DataFiles'
os.makedirs(results_dir, exist_ok=True)

#Initial filename
newFilename = False
filenameText = 'LoRaGPSdata'
filenumber = 1
#Check if initial filename is already in use; if so increase filenumber
while(newFilename != True):
    filename = filenameText + str(filenumber) + '.txt'
    if (os.path.isfile(results_dir+'/'+filename)):
        filenumber = filenumber + 1
    else:
        newFilename = True
f = open(os.path.join(results_dir,filename),'a')
header = "Date Time SatCount Latitude Longitude Speed Altitude\n"
f.write(header)
f.close()
'''        
global moveBool
def move_figure(f, x, y):
    """Move figure's upper left corner to pixel (x, y)"""
    backend = matplotlib.get_backend()
    if backend == 'TkAgg':
        f.canvas.manager.window.wm_geometry("+%d+%d" % (x, y))
    elif backend == 'WXAgg':
        f.canvas.manager.window.SetPosition((x, y))
    else:
        # This works for QT and GTK
        # You can also use window.setGeometry
        plt.gcf().canvas.manager.window.move(int(x), y)
        '''
#i=0
#skipRow = [True]    #don't skip first row (header)
#rowsToSkip = []
#Write to txt file
while True:
    f = open(os.path.join(results_dir,filename),'a')
    GPSbytes = ser.readline()   #read serial line
    GPSstring = GPSbytes.decode()   #convert bytes to string (with formatting)
    #GPSstring = GPSstring.replace('\r','')
    GPSstring = GPSstring.replace(' SC:',' ')
    GPSstring = GPSstring.replace(' lat:',' ')
    GPSstring = GPSstring.replace(' lon:',' ')
    GPSstring = GPSstring.replace(' Sp:',' ')
    GPSstring = GPSstring.replace(' Alt:',' ')
    '''checkNumeric = GPSstring.split()
    if i>0:
        skipRow.append(checkNumeric[2].isnumeric())
    if i == 0: i = 1
    if False in skipRow:
        for x in range(0, len(skipRow)):
            if not x in rowsToSkip and skipRow[x] == False:
                rowsToSkip.append(x)
    '''
    f.write(GPSstring)
    f.close()
   
    data = pd.read_table(os.path.join(results_dir,filename), sep = ' ', index_col = 'Time') #, skiprows = rowsToSkip
    data.drop(["for"], inplace = True, errors = 'ignore')
    data.drop(["not"], inplace = True, errors = 'ignore')
    plt.plot(data['Altitude'])
    
    
    
'''    
filenamepath = str(os.path.join(results_dir,filename))
while True:
    with open(filenamepath, mode='a') as output_file:
        output_writer = csv.writer(output_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
'''
