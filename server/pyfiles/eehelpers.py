import ee
import numpy as np
from skimage import filters
import datetime

from server.pyfiles import globals

###
# make CEM-formatted grid from sat image
def make_cem_grid(im):       
    features = []
    for r in range(globals.nRows):
        for c in range(globals.nCols):
            features.append(ee.Feature(ee.Geometry.Polygon(globals.polyGrid[r][c].tolist())))

    fc = ee.FeatureCollection(features)

    eeGrid = np.empty((globals.nRows, globals.nCols), float)
            
    # Reduce features to mean value
    maxTries = 8
    numTries = 0
    while numTries < maxTries:
        try:
            dict = im.reduceRegions(reducer = ee.Reducer.mean(), collection = fc, scale = 30)
            info = dict.getInfo().get('features')
            break
        except:
            numTries = numTries + 1
            print("Could not get info from EE server. Trying again...")
            if numTries == maxTries:
                print("Max tries exceeded: could not get info from EE server")
                return eeGrid

    i = 0
    for r in range(globals.nRows):
        for c in range(globals.nCols):
            feature = info[i]
            fill = feature['properties']['mean']
            eeGrid[r][c] = fill
            i += 1
    return eeGrid

###
# create water mask from 365 day TOA composite 
def get_image_composite(date):
    # build request
    poly = ee.Geometry.Polygon(globals.geometry[0])
    start_date = (date - datetime.timedelta(6*365/12)).strftime('%Y-%m-%d')
    end_date = (date + datetime.timedelta(6*365/12)).strftime('%Y-%m-%d')
    url = _get_source_url(date)

    # get composite
    collection = ee.ImageCollection(url).filterBounds(poly).filterDate(start_date, end_date) 
    # TODO: fine tune temporal resolution on ee composites
    composite = ee.Algorithms.Landsat.simpleComposite(collection)

    # otsu
    water_bands = _get_source_bands()
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
    gaussian = ee.Kernel.gaussian(3)            
    smooth = mask.convolve(gaussian).clip(poly)
    return smooth
    
###
# build URL to request image
def _get_source_url(date):
    # update source if necessary        
    if globals.source == "LS5" and date.year >= 2012:
        return "LANDSAT/LE07/C01/T1"
    if (globals.source == "LS5"):
        return "LANDSAT/LT05/C01/T1"
    if (globals.source == "LS7"):
        return "LANDSAT/LE07/C01/T1"
    return "LANDSAT/LC08/C01/T1"

###
# return NWDI source bands for current source
def _get_source_bands():    
    if (globals.source == "LS8"):
        return ["B3", "B5"]
    else:
        return ["B2", "B4"]
