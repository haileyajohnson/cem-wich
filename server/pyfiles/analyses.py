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
            if grid[r][c] > 0.1:
                shoreline[c] = r + (1-grid[r][c])
                break
    return shoreline

##
# return array of shoreline positions relative to reference shoreline
def getShorelineChange(shoreline):
    return np.subtract(shoreline, globals.ref_shoreline)

###
# get difference in shoreline orientation and cross-shore variability using 2D spatial PCA
def get_spatial_pca():
    # columnwise positions
    cols = np.multiply(range(globals.nCols), globals.colSize).reshape([globals.nCols, 1])
    # row-wise positions
    rows_mod = np.multiply(globals.model[-1], globals.rowSize).reshape([globals.nCols, 1])
    rows_obs = np.multiply(globals.observed[-1], globals.rowSize).reshape([globals.nCols, 1])
    mod = np.concatenate((cols, rows_mod), axis=1)
    obs = np.concatenate((cols, rows_obs), axis=1)

    pca_mod = PCA(n_components=2)
    pca_mod.fit(mod)
    pca_obs = PCA(n_components=2)
    pca_obs.fit(obs)

    # rotation = angle between modeled mode 1 and observed mode 1 (interpreted as alongshore vector)
    cos = np.dot(pca_mod.components_[0], pca_obs.components_[0])
    cos = np.clip(cos, -1, 1)
    rotation = math.acos(cos) * (math.pi)

    # scale = ratio between variance of modeled mode 2 and variance of observed mode 2 (interpreted as cross-shore variability)
    var_mod = pca_mod.explained_variance_[1]
    var_obs = pca_obs.explained_variance_[1]
    scale = 0 if var_mod == 0 or var_obs == 0 else var_obs / var_mod

    # return tuple with rotation and scale
    return {"rotation": rotation, "scale": scale}

###
# determine similarity indicators for observed and modelled shorelines using shoreline EOF
# 1. correlation coefficients of first three modes: decribes spatial similarity of shoreline change
# 2. ratio of explained variance of first three modes: describes similarity of scale of shoreline change
# 3. similarity score, an overal descriptive index: correlation coefficient * ratio
def get_similarity_index():

    # modeled pca
    pca = PCA(n_components=.99, solver='full')
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
    var_ratio = np.empty([1, globals.max_modes])
    S = np.empty([1, globals.max_modes])
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
# get amplitude of first three modeled "wave-driven modes" in observed shorelines
def get_wave_PCs():    
    waves = PCA(n_components=.99, solver='full')
    waves.fit(globals.model)
    E = np.transpose(waves.components_) # column-wise eigenvectors
    return np.matmul(globals.observed, E)
