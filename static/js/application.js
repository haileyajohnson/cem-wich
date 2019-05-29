
/**
 * Global variables
 */
var map;
var box = null;
var CLIENT_ID = '762501139172-rjf0ia3vv9edu6gg0m46aoij519khuk7.apps.googleusercontent.com';
var drawingMode = false;
var draw;
var numX = 100;
var numY = 50;
var rotation = 0;

/**
 * Helper functions
 */

function linspace(start, stop, N) {
    var difLon = stop[0] - start[0];
    var dLon = difLon/N;
    var difLat = stop[1] - start[1];
    var dLat = difLat/N;
    coords = [];
    for (var i = 1; i < N; i ++) {
        coords.push([start[0] + i*dLon, start[1] + i*dLat]);
    }
    return coords;
}

function toggleDrawMode() {
    drawingMode = !drawingMode;
    if (drawingMode) {
        $('.draw-button').addClass('selected');
        map.addInteraction(draw);
    }
    else {
        $('draw-button').removeClass('selected');
        map.removeInteraction(draw);
    }
}

function displayPhoto(photoUrl) {
    var imageLayer = new ol.layer.Image({
        source: new ol.source.ImageStatic({
            url: photoUrl,
            imageExtent: box.getExtent()
        })
    });
    map.addLayer(imageLayer);
}

function otsu(histogram) {
    var counts = ee.Array(ee.Dictionary(histogram).get('histogram'));
    var means = ee.Array(ee.Dictionary(histogram).get('bucketMeans'));
    var size = means.length().get([0]);
    var total = counts.reduce(ee.Reducer.sum(), [0]).get([0]);
    var sum = means.multiply(counts).reduce(ee.Reducer.sum(), [0]).get([0]);
    var mean = sum.divide(total);

    var indices = ee.List.sequence(1, size);

    // Compute between sum of squares, where each mean partitions the data.
    var bss = indices.map(function (i) {
        var aCounts = counts.slice(0, 0, i);
        var aCount = aCounts.reduce(ee.Reducer.sum(), [0]).get([0]);
        var aMeans = means.slice(0, 0, i);
        var aMean = aMeans.multiply(aCounts)
            .reduce(ee.Reducer.sum(), [0]).get([0])
            .divide(aCount);
        var bCount = total.subtract(aCount);
        var bMean = sum.subtract(aCount.multiply(aMean)).divide(bCount);
        return aCount.multiply(aMean.subtract(mean).pow(2)).add(
            bCount.multiply(bMean.subtract(mean).pow(2)));
    });

    //print(ui.Chart.array.values(ee.Array(bss), 0, means));

    // Return the mean value corresponding to the maximum BSS.
    return means.sort(bss).get([-1]);
};

function drawGrid() {
    
    // create grid layer
    var vectorSource = new ol.source.Vector({ wrapX: false, });

    var style = new ol.style.Style({
        stroke: new ol.style.Stroke({
          color: 'white',
          width: 0.3
        })
      });

    var coords = box.getCoordinates()[0];
    var d = [Math.abs(coords[0][0] - coords[3][0]), Math.abs(coords[0][1] - coords[1][1])];
    var xdim = 0;
    var ydim = 1;
    var size = [0, 0]; //[size in longitude, latitude]
    if (d[1] > d[0]) {
        xdim = 1;
        ydim = 0;
    }

    size[xdim] = numX;
    size[ydim] = numY;
    var dx = d[xdim]/numX;
    var dy = d[ydim]/numY;

    // vertices numbered from lower left (0) in a clockwise direction
    var edge0_1 = linspace(coords[0], coords[1], size[1]);
    var edge1_2= linspace(coords[1], coords[2], size[0]);
    var edge2_3 = linspace(coords[3], coords[2], size[1]);
    var edge3_0 = linspace(coords[0], coords[3], size[0]);

    for (var i = 0; i < size[0]-1; i++) {
        vectorSource.addFeature(new ol.Feature({geometry: new ol.geom.LineString([edge1_2[i], edge3_0[i]]), style:style}));
    }

    for (var j = 0; j < size[1]-1; j++) {
        vectorSource.addFeature(new ol.Feature({geometry: new ol.geom.LineString([edge0_1[j], edge2_3[j]]), style: style}));
    }

    // add to map
    map.addLayer(new ol.layer.Vector({source: vectorSource, 
        style: function(feature, resolution) {
            return feature.get('style');
        }
    }));
}

/**
 * Main functions
 */

function initMap() {
    // initialize earth engine
    ee.initialize();

    var vectorSource = new ol.source.Vector({ wrapX: false, 
        projection: "EPSG:4326", });
    // create rotatable map
    map = new ol.Map({
        interactions: ol.interaction.defaults().extend([
            new ol.interaction.DragRotateAndZoom()
        ]),
        target: 'map',
        view: new ol.View({
            projection: "EPSG:4326",
            center: [-75.5, 35.25],
            zoom:6
            // center: [0, 0],
            // zoom: 2
        })
    });

    // add Bing Maps layer
    var bingLayer = new ol.layer.Tile({
        visible: true,
        preload: Infinity,
        source: new ol.source.BingMaps({
            key: 'Am3Erq9Ut-VwSCA-xAUa8RoLu_jFgJwrK5zu0d81AWJkBwwOEr6DSSvzbi7b70e_',
            imagerySet: 'aerial'
        })
    });
    map.addLayer(bingLayer);
    map.addLayer(new ol.layer.Vector({
        source: vectorSource
    }));

    geometryFunction = function (coordinates, geometry) {
        var first = coordinates[0];
        var last = coordinates[1];
        // find distance between corners
        var dx = last[0] - first[0];
        var dy = last[1] - first[1];
        var dist = Math.sqrt(Math.pow(dx, 2) + Math.pow(dy, 2))
        // rotate to view parallel 
        rotation = map.getView().getRotation(); // roation of map view
        var theta = Math.atan2(dy, dx); // angle between vertices in coordinate plane
        var w = dist * Math.cos(theta - rotation); // width of rotated box
        var l = dist * Math.sin(theta - rotation); // length of rotated box
        var left = [first[0] - l * Math.sin(rotation), first[1] + l * Math.cos(rotation)];
        var right = [first[0] + w * Math.cos(rotation), first[1] + w * Math.sin(rotation)];
        var newCoordinates = [first, left, last, right];
        newCoordinates.push(newCoordinates[0].slice());
        if (!geometry) {
            geometry = new ol.geom.Polygon([newCoordinates]);
        } else {
            geometry.setCoordinates([newCoordinates]);
        }
        return geometry;
    };

    // add drawing interaction
    draw = new ol.interaction.Draw({
        source: vectorSource,
        type: 'Circle',
        geometryFunction: geometryFunction
    });
    // On draw start for polygon, remove previous
    draw.on('drawstart', function (e) {
        vectorSource.clear();
    });
    // When a drawing ends, save geometry
    vectorSource.on('addfeature', function (evt) {
        var feature = evt.feature;
        box = feature.getGeometry();
        drawGrid();
    });

}

function mapTransform() {
    var poly = new ee.Geometry.Polygon(box.getCoordinates()[0]);

    var dataset = ee.ImageCollection('LANDSAT/LT05/C01/T1').filterBounds(poly).filterDate('1991-01-01', '1991-12-31');
    var composite = ee.Algorithms.Landsat.simpleComposite(dataset);

    var water_bands = ['B3', 'B5'];
    var water_threshold = 0.4;
    var water_sigma = 1;

    var ndwi_hist_ymax = 10;
    var ndwi = composite.normalizedDifference(water_bands);

    var values = ndwi.reduceRegion({
        reducer: ee.Reducer.histogram(),
        geometry: poly,
        scale: 30,
        bestEffort: true
    });

    var water = ndwi.gt(otsu(values.get('nd')));
    var water_edge = ee.Algorithms.CannyEdgeDetector(water, 0.5, 0);

    var gaussian = ee.Kernel.gaussian({
        radius: 5
    });

    var smooth = water_edge.convolve(gaussian);

    smooth.getThumbURL({ dimensions: [800, 800], region: poly.toGeoJSONString() }, displayPhoto);
}


function loadApp() {
    var onImmediateFailed = function () {
        $('.g-sign-in').removeClass('hidden');
        $('.output').text('(Log in to see the result.)');
        $('.g-sign-in .button').click(function () {
            ee.data.authenticateViaPopup(function () {
                // If the login succeeds, hide the login button and run the analysis.
                $('.g-sign-in').addClass('hidden');
                initMap();
            });
        });
    };

    // Attempt to authenticate using existing credentials.
    ee.data.authenticate(CLIENT_ID, initMap, null, null, onImmediateFailed);
}