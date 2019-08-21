# cem-utility

## About

## Getting started
1. The Python environment is managed using Anaconda. To set up the environment: 
    1. navigate to the project in an Anaconda command prompt
    2. `conda env create -f environment.yml` to create the environment
    3. `conda activate cem-util` to activate the environment

2. Tha Javascript distribution is managed using gulpjs. To set install gulp fuctions
    1. `npm install`

## Build
1. Build the C binary library using cmake:
    1. Navigate to `server\C`
    2. `mkdir _build`
    3. `cd _build`
    3. `cmake ..`
    4. `make`
    5. `make install`
    6. verify `bmi_cem.dll` or `bmi_cem.so` has been installed under `server\C\_build

2. Package the client-side application using gulp:  
    The application can be packaged either for production or debugging.
    * `gulp debug` places the original js files in the distribution folder
    * `gulp build` places concatenated and minified js in the distribution folder

## Run
Deploy the cem-utility with `python cem_utility.py`.
