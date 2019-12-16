from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import json
import math
import numpy as np
from skimage import filters
from sklearn.decomposition import PCA
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

# globals
source = None
polyGrid = None
geometry = None
nRows = None
nCols = None
year = 0

def process(grid, timestep):
    ret = np.ctypeslib.as_array(grid, shape=[nRows,nCols])
    print('get im')
    # get satellite composite
    im = get_image_composite()
    print('convert im')
    # convert to grid
    eeGrid = make_cem_grid(im)
    print('get dif')
    # flat difference
    dif = get_flat_difference(grid, eeGrid)
    # spatial PCA
    sp_pca = get_spatial_pca(grid, eeGrid)
    # temporal PCA
    # TODO
    emit('results_ready', {'grid':ret.tolist(), 'time':timestep, 'dif': dif})
    print('emitted event')

def finalize():
    emit('model_complete', {})


@app.route("/")
def startup():
    service_account = 'cem-ee-utility@cem-ee-utility.iam.gserviceaccount.com'    
    credentials = ee.ServiceAccountCredentials(service_account, 'private_key.json')
    ee.Initialize(credentials)
    distDir = []
    for filename in os.listdir('_dist'):
        if filename.endswith(".js"):
            distDir.append(filename)
        else:
            continue
    return render_template("application.html", distDir=distDir)

@socketio.on('submit', namespace='/request')
def get_grid_info(grid_info):
    global nRows, nCols, polyGrid, geometry, source, year
    nRows = grid_info['nRows']
    nCols = grid_info['nCols']
    polyGrid = np.asarray(grid_info['polyGrid'])
    geometry = grid_info['geometry']
    source = grid_info['source']
    year = grid_info['year']


@socketio.on('run', namespace='/request')
def get_input_data(input_data):
    global year
    # build cell grid
    grid = ((POINTER(c_float)) * nRows)()
    for r in range(nRows):
        grid[r] = (c_float * nCols)()
        for c in range(nCols):
            grid[r][c] = input_data['grid'][r][c]
    
    numTimesteps = 1#input_data['numTimesteps']
    saveInterval = input_data['saveInterval']
    lengthTimestep = input_data['lengthTimestep']

    input = Config(grid = grid, nRows = nRows,  nCols = nCols, cellWidth = input_data['cellWidth'], cellLength = input_data['cellLength'],
        asymmetry = input_data['asymmetry'], stability = input_data['stability'], waveHeight = input_data['waveHeight'],
        wavePeriod = input_data['wavePeriod'], shelfSlope = input_data['shelfSlope'], shorefaceSlope = input_data['shorefaceSlope'],
        numTimesteps = numTimesteps, lengthTimestep = lengthTimestep, saveInterval = saveInterval)

    lib.initialize.argtypes = [Config]
    lib.initialize.restype = c_int
    status = lib.initialize(input)

    lib.update.argtypes = [c_int]
    lib.update.restype = np.ctypeslib.ndpointer(dtype=np.float64, ndim=2, shape=[nRows, nCols])#POINTER(c_double)
    i = 0
    # TODO multiprocessing
    while i < numTimesteps:
        ret = lib.update(saveInterval)
        i+=saveInterval
        year+=lengthTimestep/365
        process(ret, i)

    lib.finalize()
    finalize()

def get_image_composite():
    # build request
    url = get_source_url()
    poly = ee.Geometry.Polygon(geometry[0])
    start_date = str(math.floor(year)) +  "-01-01"
    end_date = str(math.floor(year)) + "-12-01"
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
    land_pix = water.eq(0) and connectCount.lt(minConnectivity)
    water_pix = (water.eq(1) and connectCount.lt(minConnectivity)).multiply(-1)
    mask = water.add(land_pix).add(water_pix).Not()

    gaussian = ee.Kernel.gaussian(1)
            
    smooth = mask.convolve(gaussian).clip(poly)
    return smooth

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

def get_flat_difference(grid, eeGrid):
    dif = 0
    for r in range(nRows):
        for c in range(nCols):
            dif += abs(eeGrid[r][c] - grid[r][c])
    return dif/(nRows*nCols)

def get_spatial_pca(grid, eeGrid):
    return

# TODO
def get_temporal_pca(grid, eeGrid):
    return

def get_source_url():
    global source, year
    if source == "LS5" and year >= 2012:
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