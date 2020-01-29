from ctypes import *

# configuration object passed to CEM lib
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
        ("depthOfClosure", c_double),
        ("lengthTimestep", c_double),
        ("numTimesteps", c_int),
        ("saveInterval", c_int)]