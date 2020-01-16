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
    # mod_shoreline = globals.model[-1]
    # obs_shoreline = globals.observed[-1]
    # print(mod_shoreline)
    # print(obs_shoreline)
    # print(np.any(np.isnan(mod_shoreline)))
    # print(np.all(np.isfinite(mod_shoreline)))
    # print(np.any(np.isnan(obs_shoreline)))
    # print(np.all(np.isfinite(obs_shoreline)))

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

    # rotation = angle between modelled mode 1 and observed mode 1 (interpreted as alongshore vetor)
    cos = np.dot(pca_mod.components_[0], pca_obs.components_[0])
    cos = np.clip(cos, -1, 1)
    rotation = math.acos(cos) * (math.pi)

    # scale = ratio between variance of modelled mode 2 and variance of observed mode 2 (interpreted as cross-shore variability)
    scale = pca_mod.explained_variance_[1] / pca_obs.explained_variance_[1]

    # return tuple with rotation and scale
    return {"rotation": rotation, "scale": scale}

###
# determine similarity indicators for observed and modelled shorelines using shoreline EOF
# 1. correlation coefficients of first three modes: decribes spatial similarity of shoreline change
# 2. ratio of explained variance of first three modes: describes similarity of scale of shoreline change
# 3. similarity score, an overal descriptive index: correlation coefficient * ratio
def get_similarity_index():
    print(np.any(np.isnan(globals.model)))
    print(not np.all(np.isfinite(globals.model)))
    print(np.any(np.isnan(globals.observed)))
    print(not np.all(np.isfinite(globals.observed)))
    # modelled pca
    pca_mod = PCA(n_components=3)
    pca_mod.fit(globals.model)
    Emod = pca_mod.components_
    Lmod = pca_mod.explained_variance_
    print(Lmod)
    # observed pca
    pca_obs = PCA(n_components=3)
    pca_obs.fit(globals.observed)
    Eobs = pca_obs.components_
    Lobs = pca_obs.explained_variance_
    print(Lobs)

    r = np.empty([1, 3])
    var_ratio = np.empty([1,3])
    S = np.empty([1,3])
    # iterate modes
    for k in range(3):        
        # part 1: correlation coefficient (r_k)
        r[0][k] = np.corrcoef(Eobs[k], Emod[k])[0][1]
        # part 2: ratio of explained variance
        var_ratio[0][k] = Lmod[k]/Lobs[k]
        # part 3: similarity score (S_k)
        S[0][k] = r[0][k] * min(var_ratio[0][k], 1/var_ratio[0][k])
    
    globals.r = np.vstack((globals.r, r))
    globals.var_ratio = np.vstack((globals.var_ratio, var_ratio))
    globals.S = np.vstack((globals.S, S))

###
# get amplitude of first three modelled "wave-driven modes" in observed shorelines
def get_wave_PCs():    
    waves = PCA(n_components=3)
    waves.fit(globals.model)
    E = np.transpose(waves.components_) # column-wise eigenvectors
    return np.matmul(globals.observed, E)
