import numpy as np
from sklearn.decomposition import PCA

from server.pyfiles import globals

###
# return array of shoreline positions
def getShoreline(grid):
    shoreline = np.empty(1, globals.nCols)    
    for r in range(globals.nRows):
        for c in range(globals.nCols):
            if grid[r][c] > 0:
                shoreline[c] = r + (1-grid[r][c])
                break
    return shoreline

##
# return array of shoreline positions relative to reference shoreline
def getShorelineChange(shoreline):
    return np.subtract(shoreline - globals.ref_shoreline)

###
# get avg. cell-wise difference
def get_flat_difference(grid):
    dif = 0
    for r in range(globals.nRows):
        for c in range(globals.nCols):
            dif += abs(globals.eeGrid[r][c] - grid[r][c])
    return dif/(globals.nRows*globals.nCols)

###
# get difference in eigenvectors
def get_spatial_pca():
    print(np.any(np.isnan(grid)))
    print(np.all(np.isfinite(grid)))
    print(np.any(np.isnan(globals.eeGrid)))
    print(np.all(np.isfinite(globals.eeGrid)))
    # cross-shore positions
    rows = np.arange(globals.nCols)
    rows = np.repeat(rows, globals.nRows)
    # alongshore positions
    cols = np.array([np.arange(globals.nCols)])
    cols = np.repeat(cols, globals.nRows, axis=0).flatten()

    # modelled sediment fill
    grid_fill = grid.flatten()
    data1 = np.array([rows, cols, grid_fill])
    pca_cem = PCA(n_components=3)
    pca_cem.fit(np.transpose(data1))
    print(pca_cem.components_)
    print(pca_cem.explained_variance_ratio_)
    # multiply eigenvectors by eigenvalues
    m1 = np.multiply(np.transpose(pca_cem.explained_variance_ratio_), pca_cem.components_)

    # observed sediment fill
    ee_fill = globals.eeGrid.flatten()
    data2 = np.array([rows, cols, ee_fill])
    pca_ee = PCA(n_components=3)
    pca_ee.fit(np.transpose(data2))
    print(pca_ee.components_)
    print(pca_ee.explained_variance_ratio_)
    # multiply eigenvectors by eigenvalues
    m2 = np.multiply(np.transpose(pca_ee.explained_variance_ratio_), pca_ee.components_)    

    # return magnitude of difference vectors between first two modes
    dif = np.subtract(m1, m2)
    mag_dif = np.linalg.norm(dif, axis=1)
    return {}

# TODO
def get_pca(obs, mod):
    return np.array([])