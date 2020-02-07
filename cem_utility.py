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

# path to built CEM lib
import platform
lib_path = "server/C/_build/py_cem.so"
if platform.system() == "Windows":
    lib_path = "server/C/_build/py_cem"
lib = CDLL(lib_path)

numTimesteps = None
saveInterval = None
lenTimestep = None
start_year = None
current_year = None

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
    global numTimesteps, saveInterval, lenTimestep, start_year, current_year, mode
    jsdata = request.form['input_data']
    input_data = json.loads(jsdata)
    print("run cem")
    status = 0
    # initialize global variables
    globals.nRows = input_data['nRows']
    globals.nCols = input_data['nCols']
    globals.rowSize = input_data['cellLength']
    globals.colSize = input_data['cellWidth']
    globals.polyGrid = np.asarray(input_data['polyGrid'])
    globals.geometry = input_data['geometry']
    globals.source = input_data['source']
    start_year = input_data['start']
    current_year = start_year
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
        
    # run variables
    numTimesteps = input_data['numTimesteps']
    saveInterval = input_data['saveInterval']
    lengthTimestep = input_data['lengthTimestep']

    if not mode == 3:
        # build wave inputs
        H = input_data['waveHeights']
        T = input_data['wavePeriods']
        theta = input_data['waveAngles']
        num_wave_inputs = len(H)
        if not len(T) == num_wave_inputs or not len(theta) == num_wave_inputs:
            return throw_error("Length of wave inputs do not match") 
        waveHeights = (c_double * num_wave_inputs)()
        wavePeriods = (c_double * num_wave_inputs)()
        waveAngles = (c_double * num_wave_inputs)()
        for i in range(num_wave_inputs):
            waveHeights[i] = H[i]
        for i in range(num_wave_inputs):
            wavePeriods[i] = T[i]
        for i in range(num_wave_inputs):
            waveAngles[i] = theta[i]

        # config object
        input = config.Config(grid = grid, nRows = globals.nRows,  nCols = globals.nCols, cellWidth = globals.colSize, cellLength = globals.rowSize,
            asymmetry = input_data['asymmetry'], stability = input_data['stability'], numWaveInputs = num_wave_inputs,
            waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
            shelfSlope = input_data['shelfSlope'], shorefaceSlope = input_data['shorefaceSlope'],
            crossShoreReferencePos = 0, shelfDepthAtReferencePos = 0, minimumShelfDepthAtClosure = 0,
            depthOfClosure = input_data['depthOfClosure'], numTimesteps = numTimesteps,
            lengthTimestep = lengthTimestep, saveInterval = saveInterval)

        # init
        lib.initialize.argtypes = [config.Config]
        lib.initialize.restype = c_int
        lib.update.argtypes = [c_int]
        lib.update.restype = POINTER(c_double)
        lib.finalize.restype = c_int

        status = lib.initialize(input)

    # return response
    if status == 0:
        return json.dumps({'message': 'Run initialized', 'status': 200})   
    return throw_error("Run failed to initialize")

###
# update
@app.route('/update/<int:timestep>', methods = ['GET'])
def update(timestep):
    global current_year
    # update time counters
    steps = min(saveInterval, numTimesteps-timestep)
    end_timestep = timestep + 365 if mode == 3 else timestep + steps
    year = current_year + math.floor(end_timestep/365)
    # init values
    ee_grid = []
    cem_grid = []
    sp_pca = []
    t_pca = []
    # run CEM if not in GEE only mode
    if not mode == 3:
        try:
            out = lib.update(steps)
        except:
            print("Error on run update")
            return throw_error("Error on run update")   

        cem_grid = np.ctypeslib.as_array(out, shape=[globals.nRows, globals.nCols])
        if np.any(np.isnan(cem_grid)) or not np.all(np.isfinite(cem_grid)):
            return throw_error("CEM returned NaN or Inf value")

    # update satellite shoreline
    if year > current_year:
        print("mode: " + str(mode))
        current_year = year
        if not mode == 2:
            ee_grid = get_sat_grid()
        # process if running in standard mode
        if mode == 1:
            prc = process(cem_grid, ee_grid)
            sp_pca = prc[0]
            t_pca = prc[1]            
            if not np.isfinite(sp_pca['rotation']) or not np.isfinite(sp_pca['scale']):
                return throw_error("Spatial PCA returned NaN or Inf value")
            if np.any(np.isnan(t_pca)) or not np.all(np.isfinite(t_pca)):
                return throw_error("Temporal PCA returned NaN or Inf value")
    
    data = {
        'message': 'Run updated',
        'grid': cem_grid.tolist(),
        'timestep': end_timestep,
        'results': {
            'sp_pca': sp_pca,
            't_pca': t_pca
        },
        'status': 200
    }
    return json.dumps(data), 200

### 
# finalize
@app.route('/finalize', methods = ['GET'])
def finalize():
    status = 0
    if not mode == 3:
        status = lib.finalize()
    if status == 0:
        return json.dumps({'message': 'Run finalized', 'status': 200}), 200
    return throw_error('Run failed to finalize')
    
###
# get remote-sensed shoreline for supplied date
def get_sat_grid():
    # get moving window composite
    print("get im")
    im = eehelpers.get_image_composite(current_year)
    # convert to grid
    print("convert im")
    return eehelpers.make_cem_grid(im)

###
# process results and return to client
def process(cem_grid, ee_grid):
    # grids to shorelines
    shoreline = analyses.getShorelineChange(analyses.getShoreline(cem_grid))
    globals.model = np.vstack((globals.model, shoreline))
    shorline = analyses.getShorelineChange(analyses.getShoreline(ee_grid))
    globals.observed = np.vstack((globals.observed, shoreline))

    # spatial PCA
    sp_pca = analyses.get_spatial_pca()
    # temporal PCA
    t_pca = []
    if np.shape(globals.model)[0] > 2:
        analyses.get_similarity_index()
        t_pca = globals.S[-1].tolist()

    return sp_pca, t_pca

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
    if mode == 1 and np.shape(globals.model)[0] > 2 and np.shape(globals.observed)[0] > 2:
        PCs = analyses.get_wave_PCs()
        results.write("Wave-driven modes:\n")
        np.savetxt(results, PCs, delimiter=',')
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
        if mode == 1:
            z.writestr('results.txt', results.getvalue())

    buff.seek(0)
    return send_file(
        buff,
        mimetype='application/zip',
        as_attachment=True,
        attachment_filename='results.zip'
    )

@app.errorhandler(500)
def throw_error(message):
    error = {
        'message': message,
        'status': 500
    }
    return json.dumps(error), 500


if __name__ == "__main__":
    app.run(host="localhost", port=8080)