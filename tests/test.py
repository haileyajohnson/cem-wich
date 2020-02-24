
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
    nRows_vals = [26, 103, 100, 200, 50]
    nCols_vals = [28, 110, 300, 300, 100]
    filename_vals = ["test/input/shoreline_config.xlsx", "test/input/rodanthe.xlsx", "test/input/murray.xlsx", "test/input/murray2.xlsx", "test/input/canaveral.xlsx"]

    # pick test file
    f = 4
    nRows = nRows_vals[f]
    nCols = nCols_vals[f]
    filename = filename_vals[f]

    # cellWidth = 300
    # cellLength = 300
    cellWidth = 248.00815314166002
    cellLength = 132.37451295654398
    shelfSlope = 0.001
    shorefaceSlope = 0.01
    crossShoreReferencePos = 10
    shelfDepthAtReferencePos = 10.0
    minimumShelfDepthAtClosure = 10.0
    lengthTimestep = 1
    saveInterval = 1
    numTimesteps = 365

    asymmetry = .3
    stability = .3

    # create wave inputs
    # random.seed(datetime.now())
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
    import_grid = pd.read_excel(filename)
    import_grid = import_grid.values
    grid_orig = ((POINTER(c_double)) * nRows)()
    for r in range(nRows):
        grid_orig[r] = (c_double * nCols)()
        for c in range(nCols):
            #grid_orig[r][c] = import_grid[r][c]
            grid_orig[r][c] = import_grid[nRows - r - 1][c]
            
    grid_new = ((POINTER(c_double)) * nRows)()
    for r in range(nRows):
        grid_new[r] = (c_double * nCols)()
        for c in range(nCols):
            #grid_new[r][c] = import_grid[nRows - r - 1][c]
            grid_new[r][c] = import_grid[r][c]
            

    # create config objects
    input_orig = config.Config(grid = grid_orig, waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods, 
            asymmetry = -1, stability = -1, numWaveInputs = numTimesteps,
            nRows = nRows, nCols = nCols, cellWidth = cellWidth, cellLength = cellLength,
            shelfSlope = shelfSlope, shorefaceSlope = shorefaceSlope, crossShoreReferencePos = crossShoreReferencePos,
            shelfDepthAtReferencePos = shelfDepthAtReferencePos, minimumShelfDepthAtClosure = minimumShelfDepthAtClosure,
            depthOfClosure = 0, lengthTimestep = lengthTimestep, saveInterval = saveInterval, numTimesteps = numTimesteps)
            
    # create config object
    input_new = config.Config(grid = grid_new, waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
            asymmetry = -1, stability = -1, numWaveInputs = numTimesteps,
            nRows = nRows, nCols = nCols, cellWidth = cellWidth, cellLength = cellLength,
            shelfSlope = shelfSlope, shorefaceSlope = shorefaceSlope, crossShoreReferencePos = crossShoreReferencePos,
            shelfDepthAtReferencePos = shelfDepthAtReferencePos, minimumShelfDepthAtClosure = minimumShelfDepthAtClosure,
            depthOfClosure = 0, lengthTimestep = lengthTimestep, saveInterval = saveInterval, numTimesteps = numTimesteps)

    # set library paths
    #lib_path_orig = "cem_orig/_build/test_cem"
    lib_path_orig = "cem_boundary_conds/_build/test_cem"
    lib_path_new = "../server/C/_build/py_cem"

    # open libraries
    lib_orig = CDLL(lib_path_orig)
    lib_new = CDLL(lib_path_new)

    # set arg/return types
    lib_orig.run_test.argtypes = [config.Config, c_int]
    lib_orig.run_test.restype = c_int
    lib_new.run_test.argtypes = [config.Config, c_int]
    lib_new.run_test.restype = c_int

    # initialize both models
    print(lib_orig.run_test(input_orig, numTimesteps, saveInterval))
    print(lib_new.run_test(input_new, numTimesteps, saveInterval))