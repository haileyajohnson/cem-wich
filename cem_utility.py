from flask import Flask, render_template, send_file
from flask_socketio import SocketIO, emit
app = Flask(__name__, static_folder="_dist")
app.config["SECRET_KEY"] = "secret!"
socketio = SocketIO(app)
from ctypes import *
import math
import numpy as np
import multiprocessing as mp
import ee
import os
import datetime
from io import StringIO
import zipfile

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
def run(input_data):    
    print("run cem")
    # initialize global variables
    globals.nRows = input_data['nRows']
    globals.nCols = input_data['nCols']
    globals.rowSize = input_data['cellLength']
    globals.colSize = input_data['cellWidth']
    globals.polyGrid = np.asarray(input_data['polyGrid'])
    globals.geometry = input_data['geometry']
    globals.source = input_data['source']
    date = datetime.datetime.strptime(input_data['date'], '%Y-%m-%d')

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

    # initialize exported variables
    globals.S = np.array([]).reshape(0, 3)
    globals.r = np.array([]).reshape(0, 3)
    globals.var_ratio = np.array([]).reshape(0, 3)

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
    numTimesteps = 10 #input_data['numTimesteps']
    saveInterval = input_data['saveInterval']
    lengthTimestep = input_data['lengthTimestep']

    # config object
    input = Config(grid = grid, nRows = globals.nRows,  nCols = globals.nCols, cellWidth = globals.colSize, cellLength = globals.rowSize,
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
        i+=saveInterval
        # run cem synchronously    
        mod_results = run_cem(steps)
        # get remote-sense shoreline ansynchronously
        date += datetime.timedelta(days=lengthTimestep*steps)
        # sat_results = pool.apply_async(get_sat_data, args=(date))
        print("get sat results")
        sat_results = get_sat_data(date)
        process(mod_results, sat_results, i)
    # end pool
    # pool.close()
    # pool.join()

    # finalize
    lib.finalize()
    finalize()

###
# return results
@socketio.on('export', namespace='/request')
def export_results():
    PC_theoretical = analyses.get_wave_PCs()

###
# run cem for specified timesteps and format output
def run_cem(interval):        
    out = lib.update(interval)    
    return np.ctypeslib.as_array(out, shape=[globals.nRows, globals.nCols])
    
###
# get remote-sensed shoreline for supplied date
def get_sat_data(date):
    # get moving window composite
    print("get im")
    im = eehelpers.get_image_composite(date)
    # convert to grid
    print("convert im")
    return eehelpers.make_cem_grid(im)

###
# process results and return to client
def process(mod_grid, eeGrid, timestep):
    print("processing results: " + str(timestep))
    # grid to shoreline
    shoreline = analyses.getShorelineChange(analyses.getShoreline(mod_grid))
    globals.model = np.vstack((globals.model, shoreline))
    shoreline = analyses.getShorelineChange(analyses.getShoreline(eeGrid))
    globals.observed = np.vstack((globals.observed, shoreline))

    # spatial PCA
    # np.savetxt('tests/test.out', ret, delimiter=',')
    sp_pca = analyses.get_spatial_pca()
    # temporal PCA
    t_pca = []
    if np.shape(globals.model)[0] > 2:
        analyses.get_similarity_index()
        t_pca = globals.S[-1].tolist()
    emit('results_ready',
    {'grid':mod_grid.tolist(), 'time':timestep, 'sp_pca': sp_pca, 't_pca': t_pca },
    namespace='/request')
    print('emitted event')

###
# notify client when model completes
def finalize():
    emit('model_complete', {}, namespace='/request')

###
# export zip file of data
@app.route('/download-zip')
def export_zip():
    # cem_shorelines.txt
    cem_shorelines = StringIO()    
    np.savetxt(cem_shorelines, globals.model, delimiter=',')

    # gee_shorelines.txt
    gee_shorelines = StringIO()
    np.savetxt(gee_shorelines, globals.observed, delimiter=',')
    
    # results.txt
    PCs = analyses.get_wave_PCs()
    results = StringIO()
    results.write("Corrcoeffs:\n")
    np.savetxt(results, globals.r, delimiter=',')
    results.write("\n\n\nVariance ratios:\n")
    np.savetxt(results, globals.var_ratio, delimiter=',')
    results.write("\n\n\nSimlarity scores:\n")
    np.savetxt(results, globals.S, delimiter=',')
    results.write("Wave-drive modes:\n")
    np.savetxt(results, PCs, delimiter=',')

    # create zip file in memory
    buff = StringIO()
    with zipfile.ZipFile(buff, mode='w') as z:
        z.writestr('cem_shorelines.txt', cem_shorelines.getvalue())
        z.writestr('gee_shorelines.txt', gee_shorelines.getvalue())
        z.writestr('results.txt', results.getvalue())

    buff.seek(0)
    return send_file(
        buff,
        mimetype='application/zip',
        as_attachment=True,
        attachment_filename='results.zip'
    )


if __name__ == "__main__":
    socketio.run(app, host="localhost", port=8080)