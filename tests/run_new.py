import numpy as np
import pandas as pd
from ctypes import *
import random
import math
from datetime import datetime

import sys
sys.path.append('..')
from server.pyfiles import config
   
if __name__ == "__main__": 
    # wait for vs debugger
    input("Press Enter to continue...")  
    # create basic input variables
    nRows = 26
    nCols = 28
    cellWidth = 300
    cellLength = 300
    shelfSlope = 0.001
    shorefaceSlope = 0.01
    crossShoreReferencePos = 10
    shelfDepthAtReferencePos = 10.0
    minimumShelfDepthAtClosure = 10.0
    lengthTimestep = 1
    saveInterval = 365
    numTimesteps = 3650

    asymmetry = .7
    #stability = .3
    stability = .4

    # create wave inputs
    random.seed(5)
    waveHeights = (c_double * numTimesteps)()
    waveAngles = (c_double * numTimesteps)()
    wavePeriods = (c_double * numTimesteps)()
    for i in range(numTimesteps):
        waveHeights[i] = random.random() + 1 # random wave height 1 to 2 meters
        angle = random.random() * (math.pi/4)
        if random.random() >= stability:
            angle = angle + (math.pi/4)
        if random.random() >= asymmetry:
            angle = angle * -1
        waveAngles[i] = angle # wave angle based on A + U distributions
        wavePeriods[i] = (random.random()*10) + 5 # random wave period 5 to 15 seconds

    # create grid input
    import_grid = pd.read_excel("test/input/shoreline_config.xlsx")
    import_grid = import_grid.values
    grid = ((POINTER(c_double)) * nRows)()
    for r in range(nRows):
        grid[r] = (c_double * nCols)()
        for c in range(nCols):
            grid[r][c] = import_grid[nRows - r - 1][c]

    # create config object
    input = config.Config(grid = grid, waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
            asymmetry = -1, stability = -1, numWaveInputs = numTimesteps,
            nRows = nRows, nCols = nCols, cellWidth = cellWidth, cellLength = cellLength,
            shelfSlope = shelfSlope, shorefaceSlope = shorefaceSlope, crossShoreReferencePos = crossShoreReferencePos,
            shelfDepthAtReferencePos = shelfDepthAtReferencePos, minimumShelfDepthAtClosure = minimumShelfDepthAtClosure,
            depthOfClosure = 0, lengthTimestep = lengthTimestep, saveInterval = saveInterval, numTimesteps = numTimesteps)

    # set library path
    lib_path = "../server/C/_build/py_cem"

    # open library
    lib = CDLL(lib_path)

    # set arg/return types
    lib.run_test.argtypes = [config.Config, c_int]
    lib.run_test.restype = c_int

    # initialize both models
    print(lib.run_test(input, numTimesteps, saveInterval))