from geopy import distance
import pandas as pd
import os
import math
from datetime import datetime


GS1 = (40.00922334747612, -105.24741780408537)
alt1 = 1610
GS2 = (40.01198359443059, -105.24527336787577)

#check current filepath
script_dir = os.path.dirname(__file__)
results_dir = os.path.join(script_dir, 'DataFiles/')    #Make directory named 'DataFiles'
filename = 'GndStation.txt'
balloonAllData = pd.read_table(os.path.join(results_dir,filename), sep = ' ', index_col = 'Time') #, skiprows = rowsToSkip

balloonEngData = pd.read_table(os.path.join(results_dir,filename), sep = ' ', index_col = 'Time').dropna()

balloonEngData["SatCount"] = pd.to_numeric(balloonEngData["SatCount"])
balloonEngData["Latitude"] = pd.to_numeric(balloonEngData["Latitude"])
dataType = balloonEngData.dtypes
deltaData = balloonEngData[["Latitude","Longitude",'Speed','Altitude','SNR',"RSSI","millis"]].diff()
balloonGPSData = balloonEngData[["Latitude","Longitude"]]

dataLength = len(balloonEngData)
i = 0
flatDist = []
for i in range(dataLength):
    flatDist.append(distance.distance(GS1,balloonGPSData.iloc[i]).m)


height = balloonEngData["Altitude"] - alt1  #[m]

tmDist = []  #[m]
for i in range(dataLength):
    tmDist.append(math.sqrt((flatDist[i]**2)+(height[i]**2)))

seconds = balloonEngData['millis'] / 1000
difSec = seconds.diff()
speed = tmDist / difSec   #m/s
#var = 12