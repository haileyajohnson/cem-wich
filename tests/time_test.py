import numpy as np
import pandas as pd
from ctypes import *
import random
import math
import psutil
import time
import sys

class Config(Structure):
    _fields_ = [
        ("grid", POINTER(POINTER(c_double))),
        ("waveHeights", POINTER(c_double)),
        ("waveAngles", POINTER(c_double)),
        ('wavePeriods', POINTER(c_double)),
        ('asymmetry', c_double),
        ('stability', c_double),
        ('numWaveInputs', c_int),
        ("nRows", c_int),
        ("nCols", c_int),
        ("cellWidth", c_double),
        ("cellLength", c_double),
        ("shelfSlope", c_double),
        ("shorefaceSlope", c_double),
		("crossShoreReferencePos", c_int),
		("shelfDepthAtReferencePos", c_double),
		("minimumShelfDepthAtClosure", c_double),
        ("lengthTimestep", c_double),
        ("numTimesteps", c_int),
        ("saveInterval", c_int)]
   
if __name__ == "__main__": 
    input("Press Enter to continue...")  
    #### open both libraries ####
    lib_path = "../server/C/_build/py_cem"
    lib_new = CDLL(lib_path)
    lib_path = "cem_orig/_build/test_cem"
    lib_orig = CDLL(lib_path)

    lib_new.initialize.argtypes = [Config]
    lib_new.initialize.restype = c_int
    lib_new.update.argtypes = [c_int]
    lib_new.update.restype = c_int
    lib_new.finalize.restype = c_int
    
    lib_orig.initialize.argtypes = [Config]
    lib_orig.initialize.restype = c_int
    lib_orig.update.argtypes = [c_int]
    lib_orig.update.restype = c_int
    lib_orig.finalize.restype = c_int

    #### create basic input variables ####

    cellWidth = 300
    cellLength = 300
    shelfSlope = 0.001
    shorefaceSlope = 0.01
    crossShoreReferencePos = 10
    shelfDepthAtReferencePos = 10.0
    minimumShelfDepthAtClosure = 10.0
    lengthTimestep = 1
    saveInterval = 1

    #### test1: test-specific vars ####

    nRows_vals = [26, 103, 100]
    nCols_vals = [28, 110, 300]
    filename_vals = ["test/input/shoreline_config.xlsx", "test/input/rodanthe.xlsx", "test/input/murray.xlsx"]
    numTimesteps_vals= [10, 100, 1000, 10000]

    #### get process ####
    process = psutil.Process()

    #### iterate grid sizes ####
    for i in range(1):
        nRows = nRows_vals[i]
        nCols = nCols_vals[i]
        filename = filename_vals[i]

        # create grid input
        import_grid = pd.read_excel(filename)
        import_grid = import_grid.values
        grid_orig = ((POINTER(c_double)) * nRows)()                
        grid_new = ((POINTER(c_double)) * nRows)()
        for r in range(nRows):
            grid_orig[r] = (c_double * nCols)()
            grid_new[r] = (c_double * nCols)()
            for c in range(nCols):
                grid_orig[r][c] = import_grid[r][c]
                grid_new[r][c] = import_grid[nRows - r - 1][c]

        #### iterate number of timessteps ####
        for j in range(4):
            numTimesteps = numTimesteps_vals[j]
            waveHeights = (c_double * numTimesteps)()
            waveAngles = (c_double * numTimesteps)()
            wavePeriods = (c_double * numTimesteps)()

            for t in range(numTimesteps):
                waveHeights[t] = random.random() + 1 # random wave height 1 to 2 meters
                waveAngles[t] = random.random() * (math.pi/4)  # wave angle based 0 to 45
                wavePeriods[t] = (random.random()*10) + 5 # random wave period 5 to 15 seconds     

            # run each test 10 times
            for k in range(10):
                asymmetry = random.random() * 0.4 + 0.3
                stability = random.random() * 0.4 + 0.3

                # create wave inputs
                random.seed(5)

                # vars to save times + mems
                time_orig = 0
                time_new = 0
                mem_orig = 0
                mem_new = 0

                for t in range(numTimesteps):
                    if random.random() >= stability:
                        waveAngles[t] = waveAngles[t] + (math.pi/4)
                    if random.random() >= asymmetry:
                        waveAngles[t] = waveAngles[t] * -1

                # create config object
                input_orig = Config(grid = grid_orig, waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
                        asymmetry = -1, stability = -1, numWaveInputs = numTimesteps,
                        nRows = nRows, nCols = nCols, cellWidth = cellWidth, cellLength = cellLength,
                        shelfSlope = shelfSlope, shorefaceSlope = shorefaceSlope, crossShoreReferencePos = crossShoreReferencePos,
                        shelfDepthAtReferencePos = shelfDepthAtReferencePos, minimumShelfDepthAtClosure = minimumShelfDepthAtClosure,
                        lengthTimestep = lengthTimestep, saveInterval = saveInterval, numTimesteps = numTimesteps)
                input_new = Config(grid = grid_new, waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
                        asymmetry = -1, stability = -1, numWaveInputs = numTimesteps,
                        nRows = nRows, nCols = nCols, cellWidth = cellWidth, cellLength = cellLength,
                        shelfSlope = shelfSlope, shorefaceSlope = shorefaceSlope, crossShoreReferencePos = crossShoreReferencePos,
                        shelfDepthAtReferencePos = shelfDepthAtReferencePos, minimumShelfDepthAtClosure = minimumShelfDepthAtClosure,
                        lengthTimestep = lengthTimestep, saveInterval = saveInterval, numTimesteps = numTimesteps)


                #### Run new ####
                start = time.time()
                mem = process.memory_info().rss
                temp = lib_new.initialize(input_new)
                i = 0
                while i < numTimesteps:
                    temp = lib_new.update(saveInterval)
                    i+=saveInterval
                temp = lib_new.finalize()
                end = time.time()
                time_new = time_new + (end-start)
                mem_new = mem_new + (process.memory_info().rss - mem)

                #### Run original ####
                start = time.time()
                mem = process.memory_info().rss
                temp = lib_orig.initialize(input_orig)
                i = 0
                while i < numTimesteps:
                    temp = lib_orig.update(saveInterval)
                    i+=saveInterval
                temp = lib_orig.finalize()
                end = time.time()
                time_orig = time_orig + (end-start)
                mem_orig = mem_orig + (process.memory_info().rss - mem)

            print("Size: " + str(nCols) + "   t: " + str(numTimesteps) + "\n")
            print("Orig\ntime: " + str(time_orig/10) + " mem: " + str(mem_orig/10))
            print("New\ntime: " + str(time_new/10) + " mem: " + str(mem_new/10))
    sys.exit(0)