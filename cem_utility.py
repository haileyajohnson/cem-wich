from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import json
import numpy as np
app = Flask(__name__, static_folder="_dist")
app.config["SECRET_KEY"] = "secret!"
socketio = SocketIO(app)

import ee
import os

from ctypes import *
from threading import Thread

class Config(Structure):
    _fields_ = [
        ("grid", POINTER(POINTER(c_float))),
        ("nRows", c_int),
        ("nCols", c_int),
        ("cellWidth", c_float),
        ("cellLength", c_float),
        ("asymmetry", c_double),
        ("stability", c_double),
        ("waveHeight", c_double),
        ("wavePeriod", c_double),
        ("shelfSlope", c_double),
        ("shorefaceSlope", c_double),
        ("numTimesteps", c_int),
        ("lengthTimestep", c_double),
        ("saveInterval", c_int)]

lib = CDLL("server/C/_build/py_cem")
source = None
year = None

def process(grid, timestep, nRows, nCols):
    ret = np.ctypeslib.as_array(grid, shape=[nRows,nCols])
    emit('results_ready', {'grid':ret.tolist(), 'time':timestep})
    print('emitted event')


@app.route("/")
def startup():
    # service_account = 'cem-ee-utility@cem-ee-utility.iam.gserviceaccount.com'    
    # credentials = ee.ServiceAccountCredentials(service_account, 'private_key.json')
    # ee.Initialize(credentials)
    distDir = []
    for filename in os.listdir('_dist'):
        if filename.endswith(".js"):
            distDir.append(filename)
        else:
            continue
    return render_template("application.html", distDir=distDir)

@socketio.on('run', namespace='/request')
def get_input_data(input_data):

    nRows = input_data['nRows']
    nCols = input_data['nCols']

    # build cell grid
    grid = ((POINTER(c_float)) * nRows)()
    for r in range(nRows):
        grid[r] = (c_float * nCols)()
        for c in range(nCols):
            grid[r][c] = input_data['grid'][r][c]
    
    numTimesteps = 1#input_data['numTimesteps']
    saveInterval = input_data['saveInterval']
    source = input_data['sourceUrl']
    year = input_data['start']

    input = Config(grid = grid, nRows = nRows,  nCols = nCols, cellWidth = input_data['cellWidth'], cellLength = input_data['cellLength'],
        asymmetry = input_data['asymmetry'], stability = input_data['stability'], waveHeight = input_data['waveHeight'],
        wavePeriod = input_data['wavePeriod'], shelfSlope = input_data['shelfSlope'], shorefaceSlope = input_data['shorefaceSlope'],
        numTimesteps = numTimesteps, lengthTimestep = input_data['lengthTimestep'], saveInterval = saveInterval)

    lib.initialize.argtypes = [Config]
    lib.initialize.restype = c_int
    status = lib.initialize(input)

    lib.update.argtypes = [c_int]
    lib.update.restype = np.ctypeslib.ndpointer(dtype=np.float64, ndim=2, shape=[nRows, nCols])#POINTER(c_double)
    i = 0
    while i < numTimesteps:
        ret = lib.update(saveInterval)
        process(ret, i, nRows, nCols)
        # t = Thread(target = process, args = (ret, i, nRows, nCols))
        # t.start()
        i+=1

    lib.finalize()


if __name__ == "__main__":
    socketio.run(app, host="localhost", port=8080)