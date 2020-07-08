from flask import Flask, render_template, send_file, request, jsonify
from werkzeug.exceptions import HTTPException
import json
app = Flask(__name__, static_folder="_dist", template_folder="client/html")
from ctypes import *
import math
import numpy as np
import ee
import os
import traceback
from io import StringIO, BytesIO
from enum import Enum
import zipfile

from server.pyfiles import *

# path to built CEM lib
import platform
lib_path = "server/C/_build/py_cem.so"
if platform.system() == "Windows":
    lib_path = "server/C/_build/py_cem"
lib = CDLL(lib_path)

# mode enum
class Mode(Enum):
    BOTH = 1
    CEM = 2
    GEE = 3

# init local vars
mode = None
numTimesteps = None
saveInterval = None
lenTimestep = None
start_year = None
current_year = None
current_date = None

############################
# request routes
############################

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
    global numTimesteps, saveInterval, lenTimestep, current_year, current_date, mode
    jsdata = request.form['input_data']
    input_data = json.loads(jsdata)
    status = 0

    # initialize vars
    current_year = start_year
    current_date = current_year
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
    globals.S = np.array([]).reshape(0, globals.max_modes)
    globals.r = np.array([]).reshape(0, globals.max_modes)
    globals.var_ratio = np.array([]).reshape(0, globals.max_modes)
    globals.wave_var = np.array([]).reshape(0, globals.max_modes)
        
    # run variables
    numTimesteps = input_data['numTimesteps']
    saveInterval = input_data['saveInterval']
    lenTimestep = input_data['lengthTimestep']

    if not mode == 3:
        # build wave inputs
        asymmetry = input_data['asymmetry']
        stability = input_data['stability']
        if asymmetry > 0:
            asymmetry = asymmetry/100
        if stability > 0:
            stability = 1 - (stability/100)
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
            asymmetry = asymmetry, stability = stability, numWaveInputs = num_wave_inputs,
            waveHeights = waveHeights, waveAngles = waveAngles, wavePeriods = wavePeriods,
            shelfSlope = input_data['shelfSlope'], shorefaceSlope = input_data['shorefaceSlope'],
            crossShoreReferencePos = 0, shelfDepthAtReferencePos = 0, minimumShelfDepthAtClosure = 0,
            depthOfClosure = input_data['depthOfClosure'], sedMobility = input_data['sedMobility'], numTimesteps = numTimesteps,
            lengthTimestep = lenTimestep, saveInterval = saveInterval)

        # init
        lib.initialize.argtypes = [config.Config]
        # lib.initialize.restype = c_int
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
    global current_year, current_date
    # update time counters
    steps = min(saveInterval, numTimesteps-timestep)
    current_date = current_year + 1 if mode == 3 else  current_date + ((steps*lenTimestep)/365)
    # init output values
    ee_grid = []
    cem_grid = []
    S = []
    r = []
    L = []
    w = -1

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
    if math.floor(current_date) > current_year:
        current_year = math.floor(current_date)
        if not mode == 2:
            # get moving window composite
            print("update: building composite")
            im = eehelpers.get_image_composite(current_year)
            if im is None:
                return throw_error("Source error: could not get image")
            # convert to grid
            print("update: generating grid")
            ee_grid = eehelpers.make_cem_grid(im)

            if ee_grid is None:
                return throw_error("Error reducing feature collection")
        # process if running in standard mode
        if mode == 1:
            process(cem_grid, ee_grid)            
            if globals.S.size > 0 and globals.wave_var.size > 0:
                S = globals.S[-1]
                S = S[~np.isnan(S)].tolist()
                print(S)
                w = globals.wave_var[-1]
                w = w[~np.isnan(w)]
                w = np.sum(w)
    
    data = {
        'message': 'Run updated',
        'grid': cem_grid.tolist(),
        'shoreline': analyses.getShoreline(cem_grid).tolist(),
        'timestep': timestep + steps,
        'results': {
            'S': S,
            'w': w
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
        im = eehelpers.get_image_composite(current_year)
        url = eehelpers.get_image_URL(im)
        final_shoreline = np.add(globals.ref_shoreline, globals.observed[-1])
        data = {
            'message': 'Run finalized',
            'ref_shoreline': globals.ref_shoreline.tolist(),
            'final_shoreline': final_shoreline.tolist(),
            'im_url': url,
            'status': 200}

        return json.dumps(data), 200
    return throw_error('Run failed to finalize')

###
# get cem grid
@app.route('/request-grid', methods = ['POST'])
def request_grid():
    global start_year
    # read request data
    jsdata = request.form['input_data']
    input_data = json.loads(jsdata)
    
    # initialize vars
    globals.nRows = input_data['nRows']
    globals.nCols = input_data['nCols']
    globals.rowSize = input_data['cellLength']
    globals.colSize = input_data['cellWidth']
    globals.polyGrid = np.asarray(input_data['polyGrid'])
    globals.geometry = input_data['geometry']
    globals.source = input_data['source']
    start_year = input_data['start']

    # get moving window composite
    print("building composite")
    im = eehelpers.get_image_composite(start_year)
    if im is None:
        return throw_error("Source error: could not get image")
    # convert to grid
    print("generating grid")
    grid = eehelpers.make_cem_grid(im)
    if grid is None:
        return throw_error("Timeout reducing feature collection")

    url = eehelpers.get_image_URL(im)

    data = {
        'message': 'Grid created',
        'grid': grid.tolist(),
        'im_url': url,
        'status': 200
    }
    return json.dumps(data), 200

    
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
        results.write("Wave-explained variance:\n")
        np.savetxt(results, globals.wave_var, delimiter=',')
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
        attachment_filename='results.zip',
        cache_timeout=0
    )

############################
# utility functions
############################

###
# process results and return to client
def process(cem_grid, ee_grid):
    # grids to shorelines
    shoreline = analyses.getShorelineChange(analyses.getShoreline(cem_grid))
    globals.model = np.vstack((globals.model, shoreline))
    shoreline = analyses.getShorelineChange(analyses.getShoreline(ee_grid))
    globals.observed = np.vstack((globals.observed, shoreline))

    # PCA analysis
    if np.shape(globals.model)[0] > 2:
        analyses.get_similarity_index()
        analyses.get_wave_PCs()

#############################
# Exception handlers
#############################

def throw_error(message):
    error = {
        'message': message,
        'status': 500
    }
    return json.dumps(error), 500

@app.errorhandler(Exception)
def handle_generic_exception(e):
    traceback.print_exc()
    return throw_error("500: Internal server error")

@app.errorhandler(HTTPException)
def handle_exception(e):
    """Return JSON instead of HTML for HTTP errors."""
    # start with the correct headers and status code from the error
    response = e.get_response()
    # replace the body with JSON
    response.data = json.dumps({
        "status": e.code,
        "message": e.description,
    })
    response.content_type = "application/json"
    return response




############################
# run app
############################
if __name__ == "__main__":
    app.run(host="localhost", port=8080)