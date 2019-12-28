
import numpy as np
import pandas as pd
from ctypes import *
import random
import math
from datetime import datetime

class Config(Structure):
    _fields_ = [
        ("grid", POINTER(POINTER(c_float))),
        ("waveHeights", POINTER(c_float)),
        ("waveAngles", POINTER(c_float)),
        ('wavePeriods', POINTER(c_float)),
        ("nRows", c_int),
        ("nCols", c_int),
        ("cellWidth", c_float),
        ("cellLength", c_float),
        ("shelfSlope", c_float),
        ("shorefaceSlope", c_float),
		("crossShoreReferencePos", c_int),
		("shelfDepthAtReferencePos", c_float),
		("minimumShelfDepthAtClosure", c_float),
        ("lengthTimestep", c_float),
        ("saveInterval", c_int)]
        
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
    saveInterval = 1
    numTimesteps = 365

    asymmetry = .7
    stability = .4

    # create wave inputs
    # random.seed(datetime.now())
    random.seed(5)
    waveHeights = (c_float * numTimesteps)()
    waveAngles = (c_float * numTimesteps)()
    wavePeriods = (c_float * numTimesteps)()
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
    grid_orig = ((POINTER(c_float)) * nRows)()
    for r in range(nRows):
        grid[r] = (c_float * nCols)()
        for c in range(nCols):
            grid[r][c] = import_grid[r][c]
            
    grid_new = ((POINTER(c_float)) * nRows)()
    for r in range(nRows):
        grid[r] = (c_float * nCols)()
        for c in range(nCols):
            grid[r][c] = import_grid[nRows - r - 1][c]

    # create config objects
    input_orig = Config(grid = grid_orig, waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
            nRows = nRows, nCols = nCols, cellWidth = cellWidth, cellLength = cellLength,
            shelfSlope = shelfSlope, shorefaceSlope = shorefaceSlope, crossShoreReferencePos = crossShoreReferencePos,
            shelfDepthAtReferencePos = shelfDepthAtReferencePos, minimumShelfDepthAtClosure = minimumShelfDepthAtClosure,
            lengthTimestep = lengthTimestep, saveInterval = saveInterval)
            
    # create config object
    input_new = Config(grid = grid_new, waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
            nRows = nRows, nCols = nCols, cellWidth = cellWidth, cellLength = cellLength,
            shelfSlope = shelfSlope, shorefaceSlope = shorefaceSlope, crossShoreReferencePos = crossShoreReferencePos,
            shelfDepthAtReferencePos = shelfDepthAtReferencePos, minimumShelfDepthAtClosure = minimumShelfDepthAtClosure,
            lengthTimestep = lengthTimestep, saveInterval = saveInterval)

    # set library paths
    #lib_path_orig = "cem_orig/_build/test_cem"
    lib_path_orig = "cem_boundary_conds/_build/test_cem"
    lib_path_new = "../server/C/_build/py_cem"

    # open libraries
    lib_orig = CDLL(lib_path_orig)
    lib_new = CDLL(lib_path_new)

    # set arg/return types
    lib_orig.run_test.argtypes = [Config, c_int]
    lib_orig.run_test.restype = c_int
    lib_new.run_test.argtypes = [Config, c_int]
    lib_new.run_test.restype = c_int

    # initialize both models
    print(lib_orig.run_test(input_orig, numTimesteps, saveInterval))
    print(lib_new.run_test(input_new, numTimesteps, saveInterval))