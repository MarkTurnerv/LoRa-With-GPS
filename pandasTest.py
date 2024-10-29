# -*- coding: utf-8 -*-
"""
Created on Thu Jul 18 13:13:19 2024

@author: markt
"""

import pandas as pd
import os

#check current filepath
script_dir = os.path.dirname(__file__)
results_dir = os.path.join(script_dir, 'DataFiles/')
filename = "LoRaGPSdata6.txt"

data = pd.read_table(os.path.join(results_dir,filename), sep = ',')