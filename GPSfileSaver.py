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
import csv
import os

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

#Write to txt file
while True:
    f = open(os.path.join(results_dir,filename),'a')
    GPSbytes = ser.readline()   #read serial line
    GPSstring = GPSbytes.decode()   #convert bytes to string (with formatting)
    #GPSstring = GPSstring.replace('\r','')
    f.write(GPSstring)
    f.close()
'''    
filenamepath = str(os.path.join(results_dir,filename))
while True:
    with open(filenamepath, mode='a') as output_file:
        output_writer = csv.writer(output_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
'''
