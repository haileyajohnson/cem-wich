from flask import Flask, render_template, request, jsonify
import json
app = Flask(__name__, static_folder="_dist")

import ee
import os

from ctypes import *

def configFactory(nRows, nCols):
    class Config(Structure):
        _fields_ = [
            ("grid", (c_float * nCols) * nRows),
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
    return Config

lib = CDLL("server/C/_build/py_cem")

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

@app.route('/senddata', methods = ['POST'])
def get_input_data():
    jsdata = request.form['input_data']
    input_data = json.loads(jsdata)

    nRows = input_data['nRows']
    nCols = input_data['nCols']

    Config = configFactory(nRows, nCols)
    input = Config(nRows = nRows,  nCols = nCols, cellWidth = input_data['cellWidth'], cellLength = input_data['cellLength'],
        asymmetry = input_data['asymmetry'], stability = input_data['stability'], waveHeight = input_data['waveHeight'],
        wavePeriod = input_data['wavePeriod'], shelfSlope = input_data['shelfSlope'], shorefaceSlope = input_data['shorefaceSlope'],
        numTimesteps = input_data['numTimesteps'], lengthTimestep = input_data['lengthTimestep'], saveInterval = input_data['saveInterval'])

    # for r in range(nRows):
    #     for c in range(nCols):
    #         input.grid[r][c] = 0

    print(input.nRows)

    lib.initialize.argtypes = [POINTER(Config)]
    lib.initialize.restype = c_int
    status = lib.initialize(byref(input))

    return json.dumps({'success': True}), 200, {'ContentType':'applicaiton/json'}


if __name__ == "__main__":
    app.run(host="localhost", port=8080)