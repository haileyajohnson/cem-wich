# The CEM-WICH

## About
The CEM-WICH (CEM Web Interface and Coupling Helper) provides a web-based graphical user interface for running the CEM (Coastline Evolution Model). The GUI serves two purposes:
1. Simplifies the generation of CEM inputs
2. Automatically compares CEM outputs to Landsat images provided by Google Earth Engine (GEE)

The purpose of the CEM-WICH it to estimate the extent to which wave climate controls local shoreline morhpology at a location of interest.

## Setting up the CEM-WICH
The following sections provide instructions for setting up and running and instance of the CEM-WICH. For instructions on use, skip to `Using the CEM-WICH`

### Getting started
1. The Python environment is managed using Anaconda. To set up the environment: 
    1. navigate to the project in an Anaconda command prompt
    2. `conda env create -f environment.yml` to create the environment
    3. `conda activate cem-wich` to activate the environment

2. Tha Javascript distribution is managed using gulpjs. To install gulp fuctions:
    1. `npm install`
    
3. The Flask server requires a Google Cloud service account to fufill Earth Engine requests.
    1. Follow instructions [here](https://cloud.google.com/iam/docs/creating-managing-service-account-keys) to create a service account
    2. Place your `private_key.json` in the parent directory of the project

### Build
1. Build the C binary library using cmake:
    1. Navigate to `server\C`
    2. `mkdir _build`
    3. `cd _build`
    3. `cmake ..`
    4. `make`
    5. `make install`
    6. verify `py_cem.dll` or `py_cem.so` has been installed under `server\C\_build`

2. Package the client-side application using gulp:  
    The application can be packaged either for production or debugging.
    * `gulp debug` places the original js files in the distribution folder
    * `gulp build` places concatenated and minified js in the distribution folder

### Run
Deploy the cem-wich with `python cem_wich.py`.

## Using the CEM-WICH
