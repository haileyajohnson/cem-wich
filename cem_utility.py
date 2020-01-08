from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
app = Flask(__name__, static_folder="_dist")
app.config["SECRET_KEY"] = "secret!"
socketio = SocketIO(app)
from ctypes import *
import json
import math
import numpy as np
import multiprocessing as mp
import ee
import os
import datetime

from server.pyfiles import *

# configuration information passed to CEM lib
class Config(Structure):
    _fields_ = [
        ("grid", POINTER(POINTER(c_float))),
        ("waveHeights", POINTER(c_float)),
        ("waveAngles", POINTER(c_float)),
        ('wavePeriods', POINTER(c_float)),
        ('asymmetry', c_float),
        ('stability', c_float),
        ('numWaveInputs', c_int),
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
        ("numTimesteps", c_int),
        ("saveInterval", c_int)]

# path to built CEM lib
import platform
lib_path = "server/C/_build/py_cem.so"
if platform.system() == "Windows":
    lib_path = "server/C/_build/py_cem"
lib = CDLL(lib_path)

###
# start application
@app.route("/")
def startup():
    service_account = 'cem-ee-utility@cem-ee-utility.iam.gserviceaccount.com'    
    credentials = ee.ServiceAccountCredentials(service_account, '../private_key.json')
    ee.Initialize(credentials)
    distDir = []
    for filename in os.listdir('_dist'):
        if filename.endswith(".js"):
            distDir.append(filename)
        else:
            continue
    return render_template("application.html", distDir=distDir)
    
###
# run CEM
@socketio.on('run', namespace='/request')
def run_cem(input_data):
    # initialize global variables
    globals.nRows = input_data['nRows']
    globals.nCols = input_data['nCols']
    globals.polyGrid = np.asarray(input_data['polyGrid'])
    globals.geometry = input_data['geometry']
    globals.source = input_data['source']
    globals.date = datetime.datetime.strptime(input_data['date'], '%Y-%m-%d')

    # build cell grid
    input_grid = input_data['grid']
    grid = ((POINTER(c_float)) * globals.nRows)()
    for r in range(globals.nRows):
        grid[r] = (c_float * globals.nCols)()
        for c in range(globals.nCols):
            grid[r][c] = input_grid[r][c]

    # initialize reference shoreline and shoreline matrices    
    globals.ref_shoreline = analyses.getShoreline(input_grid)
    globals.model = np.array([]).reshape(0, globals.nCols)
    globals.observed = np.array([]).reshape(0, globals.nCols)

    # build wave inputs
    H = input_data['waveHeights']
    T = input_data['wavePeriods']
    theta = input_data['waveAngles']
    waveHeights = (c_float * len(H))()
    wavePeriods = (c_float * len(T))()
    waveAngles = (c_float * len(theta))()
    for i in range(len(H)):
        waveHeights[i] = H[i]
    for i in range(len(T)):
        wavePeriods[i] = T[i]
    for i in range(len(theta)):
        waveAngles[i] = theta[i]
    
    # run variables
    numTimesteps = input_data['numTimesteps']
    saveInterval = input_data['saveInterval']
    lengthTimestep = input_data['lengthTimestep']

    # config object
    input = Config(grid = grid, nRows = globals.nRows,  nCols = globals.nCols, cellWidth = input_data['cellWidth'], cellLength = input_data['cellLength'],
        asymmetry = input_data['asymmetry'], stability = input_data['stability'], numWaveInputs = len(H),
        waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
        shelfSlope = input_data['shelfSlope'], shorefaceSlope = input_data['shorefaceSlope'],
        crossShoreReferencePos = input_data['crossShoreRef'], shelfDepthAtReferencePos = input_data['refDepth'],
        minimumShelfDepthAtClosure = input_data['minClosureDepth'], numTimesteps = numTimesteps,
        lengthTimestep = lengthTimestep, saveInterval = saveInterval)

    # init
    lib.initialize.argtypes = [Config]
    lib.initialize.restype = c_int
    print(lib.initialize(input))

    # update
    lib.update.argtypes = [c_int]
    lib.update.restype = POINTER(c_float)
    i = 0
    # multiprocessing
    # pool = mp.Pool()
    while i < numTimesteps:
        steps = min(saveInterval, numTimesteps-i)        
        out = lib.update(steps)
        i+=saveInterval
        globals.date += datetime.timedelta(days=lengthTimestep*steps)
        process(out, i, steps)
        # pool.apply_async(process, args=(ret, i))
    # end pool
    # pool.close()
    # pool.join()

    # finalize
    lib.finalize()
    finalize()

###
# process results and return to client
def process(output, timestep, steps):
    # reshape cem outputs
    grid = np.ctypeslib.as_array(output, shape=[globals.nRows, globals.nCols])
    shoreline = analyses.getShorelineChange(analyses.getShoreline(grid))
    globals.model = np.vstack(globals.model, shorelines)

    # get moving window composite
    print('get im')
    im = eehelpers.get_image_composite()
    print('convert im')
    # convert to grid
    eeGrid = eehelpers.make_cem_grid(im)
    # get & save shoreline pos.    
    shorelines = analyses.getShorelineChange(analyses.getShoreline(eeGrid))
    globals.observed = np.vstack(globals.observed, shorelines)

    # spatial PCA
    # np.savetxt('tests/test.out', ret, delimiter=',')
    sp_pca = analyses.get_spatial_pca()
    # temporal PCA
    t_pca = analyses.get_tempora_pca()
    emit('results_ready',
    {'grid':ret.tolist(), 'time':timestep, 'sp_pca': sp_pca, 't_pca': t_pca.tolist() },
    namespace='/request')
    print('emitted event')

###
# notify client when model completes
def finalize():
    emit('model_complete', {}, namespace='/request')

if __name__ == "__main__":
    socketio.run(app, host="localhost", port=8080)