import ee
import numpy as np
import math
from skimage import filters
from scipy.ndimage import gaussian_filter1d
import cv2

from server.pyfiles import globals

###
# source definition enum
sources = [
{
    'start': 1985,
    'end': 2011,
    'bands': ['B2', 'B4'],
    'url':"LANDSAT/LT05/C01/T1"
},
{
    'start': 1999,
    'end': 9999,
    'bands': ['B2', 'B4'],
    'url':"LANDSAT/LE07/C01/T1"
},
{
    'start': 2014,
    'end': 9999,
    'bands': ['B3', 'B5'],
    'url':"LANDSAT/LC08/C01/T1"
}]

proj = None
latlon_proj = None

def init_projections():
    global proj, latlon_proj
    # World Mercator projection
    proj = ee.Projection('PROJCS[WGS 84 / World Mercator", \
        GEOGCS["WGS 84",\
            DATUM["WGS_1984",\
                SPHEROID["WGS 84",6378137,298.257223563,\
                    AUTHORITY["EPSG","7030"]],\
                AUTHORITY["EPSG","6326"]],\
            PRIMEM["Greenwich",0,\
                AUTHORITY["EPSG","8901"]],\
            UNIT["degree",0.0174532925199433,\
                AUTHORITY["EPSG","9122"]],\
            AUTHORITY["EPSG","4326"]],\
        PROJECTION["Mercator_1SP"],\
        PARAMETER["central_meridian",0],\
        PARAMETER["scale_factor",1],\
        PARAMETER["false_easting",0],\
        PARAMETER["false_northing",0],\
        UNIT["metre",1,\
            AUTHORITY["EPSG","9001"]],\
        AXIS["Easting",EAST],\
        AXIS["Northing",NORTH],\
        AUTHORITY["EPSG","3395"]]')

    latlon_proj = ee.Projection('GEOGCS["WGS 84",\
        DATUM["WGS_1984",\
            SPHEROID["WGS 84",6378137,298.257223563,\
                AUTHORITY["EPSG","7030"]],\
            AUTHORITY["EPSG","6326"]],\
        PRIMEM["Greenwich",0,\
            AUTHORITY["EPSG","8901"]],\
        UNIT["degree",0.0174532925199433,\
            AUTHORITY["EPSG","9122"]],\
        AUTHORITY["EPSG","4326"]]')

###
# find shoreline and return as coordinates
def get_shoreline(year):
    
    # create geometry for bounds    
    poly = ee.Geometry.Polygon(globals.geometry[0])

    # get ndwi image composite
    ndwi = _get_image_composite(year, poly)
    
    if ndwi is None:
        return throw_error("Source error: could not get image")

    # use Canny edge detection to estimate shoreline and create buffer
    shoreline_estimate = ee.Algorithms.CannyEdgeDetector(ndwi, 0.7, 1)
    ndwi_buffer = ndwi.mask(shoreline_estimate.focal_max(30, 'square', 'meters'))

    # Otsu on shoreline buffer area
    values = ndwi_buffer.reduceRegion(reducer = ee.Reducer.toList(), geometry = poly, scale = 30, bestEffort = True).getInfo()
    water = ndwi.gt(filters.threshold_otsu(np.array(values['nd'])))

    # remove noise and small features
    minConnectivity = 50
    connectCount = water.connectedPixelCount(minConnectivity, True)

    # create mask
    land_pix = water.eq(0) and connectCount.lt(minConnectivity)
    water_pix = (water.eq(1) and connectCount.lt(minConnectivity)).multiply(-1)
    mask = water.add(land_pix).add(water_pix).Not().clip(poly)
       
    # export rotated data and pixel coordinates as np arrays
    im = mask.addBands(ee.Image.pixelLonLat())
    im_data = im.reduceRegion(reducer = ee.Reducer.toList(), geometry = poly, scale = 30, bestEffort = True).getInfo()
    data = np.array(im_data['nd'])
    x = np.array(im_data['longitude'])
    y = np.array(im_data['latitude'])

    # reshape unrotated data in 2D
    uniqueY = np.unique(y).tolist() # get number of unique coords
    uniqueX = np.unique(x).tolist()
    nRows = len(uniqueY)
    nCols = len(uniqueX)
    grid = np.empty([nRows, nCols])
    grid[:, :] = np.nan
    for i in range(len(data)):
        r = uniqueY.index(y[i])
        c = uniqueX.index(x[i])
        grid[r, c] = data[i]

    shoreline_i = _contour_trace(grid)
    if shoreline_i is None or len(shoreline_i) == 0:
        return None, None

    # shoreline indices to lat lon
    shoreline = []
    for i in range(len(shoreline_i)):
        r = shoreline_i[i][0]
        c = shoreline_i[i][1]
        shoreline.append([uniqueX[c], uniqueY[r]])
    shoreline = np.array(shoreline)

    # smooth shoreline
    shoreline = gaussian_filter1d(shoreline, 3, axis=0)
    
    # get lon/lat distance from top left corner
    offset = np.array(globals.geometry[0][0])
    rel_shoreline = shoreline - offset
    
    # get shoreline grid values
    gridded_shoreline = _alongshore_average(rel_shoreline)

    return shoreline, np.nan_to_num(gridded_shoreline)

###
# create water mask from 365 day TOA composite 
def _get_image_composite(year, poly):
    # build request
    start_date = str(year) + "-01-01"
    end_date = str(year) + "-12-31"
    source = _get_source(year)

    if source is None:
        return None

    # get composite
    print("building composite")
    collection = ee.ImageCollection(source['url']).filterBounds(poly).filterDate(start_date, end_date) 
    collection = collection.map(_im_resample)
    composite = collection.reduce(ee.Reducer.percentile([20])).rename(ee.Image(collection.first()).bandNames())

    # convert to ndwi
    water_bands = source['bands']
    ndwi = composite.normalizedDifference(water_bands)
    return ndwi

###
# get outer shoreline position indices
def _contour_trace(grid):
    m, n = grid.shape
    # set up search based on rotation
    rot = math.degrees(globals.rotation)
    if rot >= -45 and rot <= 45:
        search_dir = [0,1] # right
        start = [m-1, 0] # top left
        second_dir = [-1,0] # down
    elif rot < -45 and rot > -135:
        search_dir = [-1,0] # down
        start = [m-1, n-1] # top right
        second_dir = [0,-1] # left
    elif rot > 45 and rot < 135:
        search_dir = [1,0] # up
        start = [0, 0] # bottom left
        second_dir = [0, 1] # right
    else:
        search_dir = [0,-1] # left
        start = [0, n-1] # bottom right
        second_dir = [1, 0] # up
    search_dir = np.array(search_dir)
    start = np.array(start)
    second_dir = np.array(second_dir)

    # find starting cell
    cur = start
    count = 1
    while True:
        val = grid[cur[0], cur[1]]
        if np.isnan(val):
            cur = np.add(cur, search_dir)
            continue
        if val > 0:
            break
        cur = np.add(start, np.multiply(second_dir, count))
        count = count + 1

    shoreline = []
    # trace contour with Moore neighborhood tracing
    backtrack = np.multiply(search_dir, -1)
    while True:
        shoreline.append(cur)
        # backtrack
        temp = (cur + np.array([round(backtrack[0]), round(backtrack[1])])).astype(int)
        # 45 clockwise rotation matrix
        theta = math.radians(45)
        rot = np.array([[math.cos(theta), -math.sin(theta)],[math.sin(theta), math.cos(theta)]])
        dir = backtrack
        #print(cur)
        #print(dir)
        while True:
            # rotate 45
            dir = np.transpose(np.matmul(rot, np.transpose(dir)))
            #print(dir)
            next = (cur + np.array([round(dir[0]), round(dir[1])])).astype(int)
            
            # stop criterion
            if np.isnan(grid[next[0], next[1]]) and len(shoreline) > 2:
                cur = None
                break
            if next[0] < 0 or next[0] >= m or next[1] < 0 or next[1] >= n:
                cur = None
                break
            
            backtrack = temp - next
            temp = next
            #print(temp)
            if _is_land(grid, temp):
                # throw error if looping back
                if _in_shoreline(shoreline, temp):
                    return None
                cur = temp
                break
        if cur is None:
            break
    
    return shoreline

###
# get column-wise averages of shorelines positions
def _alongshore_average(relative_shoreline):
    rot = np.array([[math.cos(globals.rotation), math.sin(globals.rotation)],[-math.sin(globals.rotation), math.cos(globals.rotation)]])
    relative_shoreline = np.transpose(np.matmul(rot, np.transpose(relative_shoreline)))
    rot_bounds = np.transpose(np.matmul(rot, np.transpose(np.array(globals.geometry[0]))))

    m, n = relative_shoreline.shape
    ySize = abs(rot_bounds[0][1] - rot_bounds[3][1])/globals.nRows
    xSize = abs(rot_bounds[1][0] - rot_bounds[0][0])/globals.nCols

    # convert to row/col values
    relative_shoreline = np.divide(relative_shoreline, np.array([xSize, ySize]))
    sums = np.zeros((2, globals.nCols))
    for i in range(m):
        y = abs(relative_shoreline[i, 1])
        x = abs(relative_shoreline[i, 0])
        if x <= 0 or x >= globals.nCols or y < 0 or y >= globals.nRows:
            continue
        c =  int(x)
        sums[0, c] = sums[0, c] + (x*y)
        sums[1, c] = sums[1, c] + x

    # cross-shore centroids
    avg_shoreline = np.divide(sums[0, :], sums[1, :])
    return avg_shoreline

###
# build URL to request image
def _get_source(year):
    if (globals.source < 0):
        for i in range(len(sources)-1, -1, -1):
            if (year >= sources[i]['start']):
                return sources[i]

    elif (globals.source < len(sources)):
        return sources[globals.source]
    return None

##
# function to resample landsat image
def _im_resample (image):
  return ee.Algorithms.Landsat.TOA(image).resample('bicubic')

##
# check if image pixel is land
def _is_land (image, pixel):
    m, n = image.shape
    if pixel[0] < 0 or pixel[0] >= m:
        return False
    if pixel[1] < 0 or pixel[1] >= n:
        return False
    return image[pixel[0], pixel[1]] > 0

##
# check if pixel is already counted as shoreline
def _in_shoreline(shoreline, pixel):
    for i in range(len(shoreline)):
        val = shoreline[i]
        if val[0] == pixel[0] and val[1] == pixel[1]:
            return True
    return False
    
