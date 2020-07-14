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
# Compare PCA of modeled and observed shoreline change
# Compute:
# 1. percent variance explained by "wave-driven" modes
# 2. similarity index of first k modes
#   a. correlation coefficients of first k modes: decribes spatial similarity of shoreline change
#   b. ratio of explained variance of first k modes: describes similarity of scale of shoreline change
def compare_PCA():

    # PCA for modeled data
    pca = PCA(n_components=.99, svd_solver='full')
    pca.fit(globals.model)
    E_mod = pca.components_
    L_mod = pca.explained_variance_
    nModes = min(L_mod.size, globals.max_modes)

    # Project "wave-driven" modes onto observed data
    E_waves = np.transpose(E_mod) # column-wise eigenvectors
    C = np.matmul(globals.observed, E_waves)

    # get total variance of observed data
    total_var = np.sum(np.diagonal(np.cov(globals.observed)))

    # record percent variance of observed data esplained by wave modes
    wave_var = np.empty([1, globals.max_modes])
    wave_var[:] = np.nan
    for k in range(nModes):
        wave_var[0][k] = np.var(C[:,k])/total_var
    globals.wave_var = np.vstack((globals.wave_var, wave_var))

    # PCA for observed data
    pca = PCA(n_components=nModes, svd_solver='full')
    pca.fit(globals.observed)
    E_obs = pca.components_
    L_obs = pca.explained_variance_

    # initialize similarity index arrays
    r = np.empty([1, globals.max_modes])
    r[:] = np.nan
    var_ratio = np.empty([1, globals.max_modes])
    var_ratio[:] = np.nan
    S = np.empty([1, globals.max_modes])
    S[:] = np.nan

    # fill in values
    for k in range(nModes):        
        # part 1: correlation coefficient (r_k)
        r[0][k] = np.corrcoef(E_obs[k], E_mod[k])[0][1]
        # part 2: ratio of explained variance
        var_ratio[0][k] = 0 if L_mod[k] == 0 or L_obs[k] == 0 else min(L_obs[k], L_mod[k])/max(L_obs[k], L_mod[k])
        # part 3: similarity score (S_k)
        S[0][k] = r[0][k] * var_ratio[0][k]
    
    globals.r = np.vstack((globals.r, r))
    globals.var_ratio = np.vstack((globals.var_ratio, var_ratio))
    globals.S = np.vstack((globals.S, S))
