from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import json
import math
import numpy as np
from skimage import filters
from sklearn.decomposition import PCA
import multiprocessing as mp
app = Flask(__name__, static_folder="_dist")
app.config["SECRET_KEY"] = "secret!"
socketio = SocketIO(app)

import ee
import os

from ctypes import *
from threading import Thread

# configuration information passed to CEM lib
class Config(Structure):
    _fields_ = [
        ("grid", POINTER(POINTER(c_float))),
        ("waveHeights", POINTER(c_float)),
        ("waveAngles", POINTER(c_float)),
        ('wavePeriods', POINTER(c_float)),
        ('asymmetry', c_float),
        ('stability', c_float),
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

# globals
source = None
polyGrid = None
geometry = None
nRows = None
nCols = None
cur_year = 0
composite_year = -1
eeGrid = None


###
# process results and return to client
def process(grid, timestep):
    global eeGrid
    ret = np.ctypeslib.as_array(grid, shape=[nRows,nCols])
    # get satellite composite if year is different than composite year
    if math.floor(composite_year) != math.floor(cur_year):
        print('get im')
        im = get_image_composite()
        print('convert im')
        # convert to grid
        eeGrid = make_cem_grid(im)
    print('get dif')
    # flat difference
    dif = get_flat_difference(ret, eeGrid)
    # spatial PCA
    np.savetxt('tests/test.out', ret, delimiter=',')
    sp_pca = get_spatial_pca(ret, eeGrid)
    # temporal PCA
    # TODO
    emit('results_ready',
    {'grid':ret.tolist(), 'time':timestep, 'dif': dif, 'sp_pca': sp_pca.tolist()},
    namespace='/request')
    print('emitted event')

###
# notify client when model completes
def finalize():
    emit('model_complete', {}, namespace='/request')

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
def get_input_data(input_data):
    global nRows, nCols, polyGrid, geometry, source, cur_year
    nRows = input_data['nRows']
    nCols = input_data['nCols']
    polyGrid = np.asarray(input_data['polyGrid'])
    geometry = input_data['geometry']
    source = input_data['source']
    cur_year = input_data['year']

    # build cell grid
    input_grid = input_data['grid']
    grid = ((POINTER(c_float)) * nRows)()
    for r in range(nRows):
        grid[r] = (c_float * nCols)()
        for c in range(nCols):
            grid[r][c] = input_grid[r][c]

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
    
    numTimesteps = input_data['numTimesteps']
    saveInterval = input_data['saveInterval']
    lengthTimestep = input_data['lengthTimestep']

    # config object
    input = Config(grid = grid, nRows = nRows,  nCols = nCols, cellWidth = input_data['cellWidth'], cellLength = input_data['cellLength'],
        asymmetry = input_data['asymmetry'], stability = input_data['stability'], 
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
        ret = lib.update(steps)
        i+=saveInterval
        cur_year += (lengthTimestep*steps)/365
        process(ret, i)
        # pool.apply_async(process, args=(ret, i))
    # end pool
    # pool.close()
    # pool.join()
    # finalize
    lib.finalize()
    finalize()

def get_image_composite():
    global composite_year
    composite_year = cur_year
    # build request
    url = get_source_url()
    poly = ee.Geometry.Polygon(geometry[0])
    start_date = str(math.floor(cur_year)) +  "-01-01"
    end_date = str(math.floor(cur_year)) + "-12-01"

    # get composite
    collection = ee.ImageCollection(url).filterBounds(poly).filterDate(start_date, end_date) 
    # TODO: fine tune temporal resolution on ee composites
    composite = ee.Algorithms.Landsat.simpleComposite(collection)

    # otsu
    water_bands = get_source_bands()
    ndwi = composite.normalizedDifference(water_bands)
    values = ndwi.reduceRegion(reducer = ee.Reducer.toList(), geometry = poly, scale = 10, bestEffort = True).getInfo()
    water = ndwi.gt(filters.threshold_otsu(np.array(values['nd'])))
    minConnectivity = 50
    connectCount = water.connectedPixelCount(minConnectivity, True)

    # create mask
    land_pix = water.eq(0) and connectCount.lt(minConnectivity)
    water_pix = (water.eq(1) and connectCount.lt(minConnectivity)).multiply(-1)
    mask = water.add(land_pix).add(water_pix).Not()

    # smooth
    gaussian = ee.Kernel.gaussian(1)            
    smooth = mask.convolve(gaussian).clip(poly)
    return smooth

###
# make CEM-formatted grid from sat image
def make_cem_grid(im):
    global polyGrid            
    features = []
    for r in range(nRows):
        for c in range(nCols):
            features.append(ee.Feature(ee.Geometry.Polygon(polyGrid[r][c].tolist())))

    fc = ee.FeatureCollection(features)
            
    # Reduce the region. The region parameter is the Feature geometry.
    dict = im.reduceRegions(reducer = ee.Reducer.mean(), collection = fc, scale = 30)

    info = dict.getInfo().get('features')

    eeGrid = np.empty((nRows, nCols), float)
    i = 0
    for r in range(nRows):
        for c in range(nCols):
            feature = info[i]
            fill = feature['properties']['mean']
            eeGrid[r][c] = fill
            i += 1
    return eeGrid

###
# get avg. cell-wise difference
def get_flat_difference(grid, eeGrid):
    dif = 0
    for r in range(nRows):
        for c in range(nCols):
            dif += abs(eeGrid[r][c] - grid[r][c])
    return dif/(nRows*nCols)

###
# get difference in eigenvectors
def get_spatial_pca(grid, eeGrid):
    print(np.any(np.isnan(grid)))
    print(np.all(np.isfinite(grid)))
    print(np.any(np.isnan(eeGrid)))
    print(np.all(np.isfinite(eeGrid)))
    # cross-shore positions
    rows = np.arange(nCols)
    rows = np.repeat(rows, nRows)
    # alongshore positions
    cols = np.array([np.arange(nCols)])
    cols = np.repeat(cols, nRows, axis=0).flatten()

    # modelled sediment fill
    grid_fill = grid.flatten()
    data1 = np.array([rows, cols, grid_fill])
    pca_cem = PCA(n_components=3)
    pca_cem.fit(np.transpose(data1))
    print(pca_cem.components_)
    print(pca_cem.explained_variance_ratio_)
    # multiply eigenvectors by eigenvalues
    m1 = np.multiply(np.transpose(pca_cem.explained_variance_ratio_), pca_cem.components_)

    # observed sediment fill
    ee_fill = eeGrid.flatten()
    data2 = np.array([rows, cols, ee_fill])
    pca_ee = PCA(n_components=3)
    pca_ee.fit(np.transpose(data2))
    print(pca_ee.components_)
    print(pca_ee.explained_variance_ratio_)
    # multiply eigenvectors by eigenvalues
    m2 = np.multiply(np.transpose(pca_ee.explained_variance_ratio_), pca_ee.components_)    

    # return magnitude of difference vectors between first two modes
    dif = np.subtract(m1, m2)
    mag_dif = np.linalg.norm(dif, axis=1)
    return mag_dif

# TODO
def get_temporal_pca(grid, eeGrid):
    return

###
# build URL to request image
def get_source_url():
    global source, cur_year
    if source == "LS5" and cur_year >= 2012:
        source = "LS7"
    
    if (source == "LS5"):
        return "LANDSAT/LT05/C01/T1"
    elif (source == "LS7"):
        return "LANDSAT/LE07/C01/T1"
    else:
        return "LANDSAT/LC08/C01/T1"

def get_source_bands():    
    if (source == "LS8"):
        return ["B3", "B5"]
    else:
        return ["B2", "B4"]

if __name__ == "__main__":
    socketio.run(app, host="localhost", port=8080)