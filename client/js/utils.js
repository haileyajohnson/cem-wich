var pi = Math.PI;

/**
 * Convert radians to degrees
 */
function rad_to_deg(radians) {
    return radians * (180/pi);
}

/**
 * Convert degrees to radians
 */
function deg_to_rad(degrees) {
    return degrees * (pi/180)
}

/**
 * Safe integer parsing
 */
function tryParseInt(input, fallback) {
    try {
        return parseInt(input);
    }
    catch (err) {
        showErrorMessage("Could not parse " + input + " as type: float");
        return fallback;
    }
}

/**
 * Safe float parsing
 */
function tryParseFloat(input, fallback) {
    try {
        return parseFloat(input);
    }
    catch (err) {
        showErrorMessage("Could not parse " + input + " as type: float");
        return fallback;
    }
}

/**
 *  ensure all required fields are present in payload
 */
function validateData(data) {
    if (!data.grid || data.grid.length == 0) { return -1; }
    if (!data.cellWidth || data.cellWidth <= 0) { return -1; }
    if (!data.cellLength || data.cellLength <= 0) { return -1; }
    if (!data.nRows || data.nRows <= 0) { return -1; }
    if (!data.nCols || data.nCols <= 0) { return -1; }
    if (!data.source) { return -1; }
    if (!data.start) { return -1; }
    if (!data.geometry || data.geometry == 0) { return -1; }
    if (!data.polyGrid || data.polyGrid.length == 0) { return -1; }
    if (!data.waveHeights) { return -1; }
    if (!data.wavePeriods) { return -1; }
    if (!data.shelfSlope) { return -1; }
    if (!data.shorefaceSlope) { return -1; }
    if (!data.crossShoreRef) { return -1; }
    if (!data.refDepth) { return -1; }
    if (!data.minClosureDepth) { return -1; }
    if (!data.numTimesteps || data.numTimesteps <= 0) { return -1; }
    if (!data.lengthTimestep || data.lengthTimestep <= 0) { return -1; }
    if (!data.saveInterval || data.saveInterval <=0) { return -1; }
    // success
    return 0;
}

/**
 * enable JQuery component
 */
$.prototype.enable = function () {
    $.each(this, function (index, el) {
        $(el).removeAttr('disabled');
    });
}

/**
 * disable JQuery component
 */
$.prototype.disable = function () {
    $.each(this, function (index, el) {
        $(el).attr('disabled', 'disabled');
    });
}