var pi = Math.PI;

function rad_to_deg(radians) {
    return radians * (180/pi);
}

function deg_to_rad(degrees) {
    return degrees * (pi/180)
}

function tryParseInt(input, fallback) {
    try {
        return parseInt(input);
    }
    catch (err) {
        displayInputErrors();
        return fallback;
    }
}

function tryParseFloat(input, fallback) {
    try {
        return parseFloat(input);
    }
    catch (err) {
        displayInputErrors();
        return fallback;
    }
}

function displayInputErrors() {
}

$.prototype.enable = function () {
    $.each(this, function (index, el) {
        $(el).removeAttr('disabled');
    });
}

$.prototype.disable = function () {
    $.each(this, function (index, el) {
        $(el).attr('disabled', 'disabled');
    });
}