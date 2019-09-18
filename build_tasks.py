import subprocess
import sys
import os

SHELL = False

# compile C files
def compile_C(WIN_OS):
    if WIN_OS:
        src_dir = "server/C" # set your source dir here
        build_dir = src_dir + "/_build" # set your build dir here

        result = []
       
        # get installation path of Visual Studio 2017
        process = subprocess.Popen('"%s"& set' % "VsDevCmd.bat",
                                stdout=subprocess.PIPE, shell=True)
        (out, err) = process.communicate()
        for line in out.split("\n"):
            if '=' not in line:
                continue
            line = line.strip()
            keye, value = line.split('=', 1)
            result[key] = value
        print(result)
    else:
        subprocess.check_call(["cmake", ".."], shell=SHELL)
        subprocess.checkcall(["make"], shell=SHELL)

# build client side application
def build_client(DEBUG):
    cmd = "debug" if DEBUG else "build"
    subprocess.check_call(["gulp", cmd], shell=SHELL)


# build client side application

if __name__ == "__main__":
    DEBUG = False
    C_ONLY = False
    WIN_OS = False
    
    if sys.platform.startswith("win"):
        WIN_OS = True
        SHELL = True

    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg == "-C":
            C_ONLY = True
        elif arg == "-D":
            DEBUG = True
    
    compile_C(WIN_OS)

    if not C_ONLY:
        build_client(DEBUG)