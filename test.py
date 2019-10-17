from ctypes import *

lib = CDLL("server/C/_build/py_cem")
#lib = CDLL("server/C/out/build/x86-Debug/Debug/py_cem")

print(lib.testFunc(2, 3))

lib.initialize("../../test/inlet_test.xlsx")