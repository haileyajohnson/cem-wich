from flask import Flask, render_template, send_file, request, jsonify
import json
app = Flask(__name__, static_folder="_dist")
from ctypes import *
import math
import numpy as np
import multiprocessing as mp
import ee
import os
from io import StringIO, BytesIO
from enum import Enum
import zipfile

from server.pyfiles import *

# mode application is running in
mode = None
class Mode(Enum):
    BOTH = 1
    CEM = 2
    GEE = 3

# configuration information passed to CEM lib
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

# path to built CEM lib
import platform
lib_path = "server/C/_build/py_cem.so"
if platform.system() == "Windows":
    lib_path = "server/C/_build/py_cem"
lib = CDLL(lib_path)

numTimesteps = None
saveInterval = None

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
@app.route('/initialize', methods = ['POST'])
def initialize():    
    jsdata = request.form['input_data']
    input_data = json.loads(jsdata)
    print("run cem")
    # initialize global variables
    globals.nRows = input_data['nRows']
    globals.nCols = input_data['nCols']
    globals.rowSize = input_data['cellLength']
    globals.colSize = input_data['cellWidth']
    globals.polyGrid = np.asarray(input_data['polyGrid'])
    globals.geometry = input_data['geometry']
    globals.source = input_data['source']
    year = input_data['start']
    mode = input_data['mode']

    # build cell grid
    input_grid = input_data['grid']
    grid = ((POINTER(c_double)) * globals.nRows)()
    for r in range(globals.nRows):
        grid[r] = (c_double * globals.nCols)()
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
    waveHeights = (c_double * len(H))()
    wavePeriods = (c_double * len(T))()
    waveAngles = (c_double * len(theta))()
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
    lib.update.restype = POINTER(c_double)

    # multiprocess satellite-based shoreline detections
    # pool = mp.Pool()
    
    # sat_results = pool.apply_async(get_sat_data, args=(year))
    # end pool
    # pool.close()
    # pool.join()
    resp = jsonify({'message': 'CEM initialized'})
    resp.status_code = 200
    return resp

###
# update
@app.route('/update/<int:timestep>', methods = ['GET'])
def update(timestep):
    steps = min(saveInterval, numTimesteps-timestep)
    out = lib.update(steps)    
    grid = np.ctypeslib.as_array(out, shape=[globals.nRows, globals.nCols])
    resp = jsonify({'message': 'CEM updated', 'grid': grid, 'timestep': timestep+steps})
    resp.status_code = 200
    return resp

### 
# finalize
def finalize():
    # finalize
    lib.finalize()
    finalize()
    resp = jsonify({'message': 'CEM finalized'})
    resp.status_code = 200
    return resp


###
# run cem for specified timesteps and format output
def run_cem(interval):        
    
###
# get remote-sensed shoreline for supplied date
def get_sat_data(year):
    # get moving window composite
    print("get im")
    im = eehelpers.get_image_composite(year)
    # convert to grid
    print("convert im")
    return eehelpers.make_cem_grid(im)

###
# process results and return to client
def process(mod_grid, eeGrid, timestep):
    print("processing results: " + str(timestep))
    # grid to shoreline
    shoreline = analyses.getShoreline(mod_grid)
    shoreline = analyses.getShorelineChange(analyses.getShoreline(mod_grid))
    globals.model = np.vstack((globals.model, shoreline))
    shoreline = globals.ref_shoreline #analyses.getShorelineChange(analyses.getShoreline(eeGrid))
    globals.observed = np.vstack((globals.observed, shoreline))

    # spatial PCA
    # np.savetxt('tests/test.out', ret, delimiter=',')
    sp_pca = analyses.get_spatial_pca() if mode == 1 else []
    # temporal PCA
    t_pca = []
    if mode == 1 and np.shape(globals.model)[0] > 2:
        analyses.get_similarity_index()
        t_pca = globals.S[-1].tolist()
    return {'grid':mod_grid.tolist(), 'time':timestep, 'sp_pca': sp_pca, 't_pca': t_pca }

###
# export zip file of data
@app.route('/download-zip')
def export_zip():
    # reference shoreline
    ref_shoreline = StringIO()
    np.savetxt(ref_shoreline, globals.ref_shoreline, delimiter=',')

    # cem_shorelines.txt
    if not mode == 3:
        cem_shorelines = StringIO()    
        np.savetxt(cem_shorelines, globals.model, delimiter=',')

    # gee_shorelines.txt
    if not mode == 2:
        gee_shorelines = StringIO()
        np.savetxt(gee_shorelines, globals.observed, delimiter=',')
    
    # results.txt
    results = StringIO()
    if not mode == 3:
        PCs = analyses.get_wave_PCs()
        results.write("Wave-drive modes:\n")
        np.savetxt(results, PCs, delimiter=',')
    if mode == 1:
        results.write("Corrcoeffs:\n")
        np.savetxt(results, globals.r, delimiter=',')
        results.write("\n\n\nVariance ratios:\n")
        np.savetxt(results, globals.var_ratio, delimiter=',')
        results.write("\n\n\nSimlarity scores:\n")
        np.savetxt(results, globals.S, delimiter=',')

    # create zip file in memory
    buff = BytesIO()
    with zipfile.ZipFile(buff, mode='w') as z:
        z.writestr('ref_shoreline.txt', ref_shoreline.getvalue())
        if not mode == 3:
            z.writestr('cem_shorelines.txt', cem_shorelines.getvalue())
        if not mode == 2:
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
    app.run(host="localhost", port=8080)