from geopy.distance import lonlat, distance
import pandas as pd
import os


GS1 = (40.00928583498405, -105.24752868245703)
alt1 = 1610
GS2 = (40.01198359443059, -105.24527336787577)

#check current filepath
script_dir = os.path.dirname(__file__)
results_dir = os.path.join(script_dir, 'DataFiles/')    #Make directory named 'DataFiles'
filename = 'GndStation.txt'
balloonAllData = pd.read_table(os.path.join(results_dir,filename), sep = ' ', index_col = 'Time') #, skiprows = rowsToSkip

balloonGPSData = pd.read_table(os.path.join(results_dir,filename), sep = ' ', index_col = 'Time').dropna()


balloonGPSData["Latitude"] = pd.to_numeric(balloonGPSData["Latitude"])
dataType = balloonGPSData.dtypes
deltaData = balloonGPSData[["Latitude","Longitude",'Speed','Altitude','SNR',"RSSI","millis"]].diff()
#deltaData = deltaData.reset_index(drop=True)
#deltaData = deltaData.diff()




var = 12