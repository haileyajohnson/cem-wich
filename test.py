from ctypes import *

lib = CDLL("server/C/_build/bmi_cem")

result = lib.testFunc(2, 3)

print(result)