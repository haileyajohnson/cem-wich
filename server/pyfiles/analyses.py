import numpy as np
import math
from sklearn.decomposition import PCA

from server.pyfiles import globals

###
# return array of shoreline positions
def getShoreline(grid):
    shoreline = np.empty(globals.nCols)   
    for c in range(globals.nCols): 
        for r in range(globals.nRows):
            if grid[r][c] > 0:
                shoreline[c] = r + (1-grid[r][c])
                break
    return shoreline

##
# return array of shoreline positions relative to reference shoreline
def getShorelineChange(shoreline):
    return np.subtract(shoreline, globals.ref_shoreline)

###
# determine similarity indicators for observed and modelled shorelines using shoreline EOF
# 1. correlation coefficients of first three modes: decribes spatial similarity of shoreline change
# 2. ratio of explained variance of first three modes: describes similarity of scale of shoreline change
# 3. similarity score, an overal descriptive index: correlation coefficient * ratio
def get_similarity_index():

    # modeled pca
    pca = PCA(n_components=.99, svd_solver='full')
    pca.fit(globals.model)
    Emod = pca.components_
    Lmod = pca.explained_variance_
    nModes = min(Lmod.size, globals.max_modes) 
    print(Lmod)
    # observed pca
    pca.fit(globals.observed)
    Eobs = pca.components_
    Lobs = pca.explained_variance_
    print(Lobs)

    r = np.empty([1, globals.max_modes])
    r[:] = np.nan
    var_ratio = np.empty([1, globals.max_modes])
    var_ratio[:] = np.nan
    S = np.empty([1, globals.max_modes])
    S[:] = np.nan

    # iterate modes
    for k in range(nModes):        
        # part 1: correlation coefficient (r_k)
        r[0][k] = np.corrcoef(Eobs[k], Emod[k])[0][1]
        # part 2: ratio of explained variance
        var_ratio[0][k] = 0 if Lmod[k] == 0 or Lobs[k] == 0 else Lobs[k]/Lmod[k]
        # part 3: similarity score (S_k)
        S[0][k] = r[0][k] * min(var_ratio[0][k], 1/var_ratio[0][k])
    
    globals.r = np.vstack((globals.r, r))
    globals.var_ratio = np.vstack((globals.var_ratio, var_ratio))
    globals.S = np.vstack((globals.S, S))

###
# project "wave-driven modes" on observed shoreline date
def get_wave_PCs():    
    pca = PCA(n_components=.99, svd_solver='full')
    pca.fit(globals.model)
    nModes = min(pca.explained_variance_.size, globals.max_modes)
    E = np.transpose(pca.components_) # column-wise eigenvectors
    total_var = np.var(globals.observed)
    # get amplitude of wave-driven modes in observed data
    C = np.matmul(globals.observed, E)
    # record percent explained variance of observed data
    C_var = np.var(C, axis = 0)
    wave_var = np.empty([1, globals.max_modes])
    wave_var[:] = np.nan
    for k in range(nModes):
        wave_var[0][k] = C_var[k]

    globals.wave_var = np.vstack((globals.wave_var, wave_var))
    return C