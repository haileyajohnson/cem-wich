
/**
 * Global variables
 */
var map;
var drawingManager;
var rectangle = null;
var polygon = null;
var poly = null;
var CLIENT_ID = '762501139172-rjf0ia3vv9edu6gg0m46aoij519khuk7.apps.googleusercontent.com';


/**
 * Helper functions
 * @param {*} rectangle 
 */
function createPolygonFromRectangle(rectangle) {  
    var coords = [
      { lat: rectangle.getBounds().getNorthEast().lat(), lng: rectangle.getBounds().getNorthEast().lng() },
      { lat: rectangle.getBounds().getNorthEast().lat(), lng: rectangle.getBounds().getSouthWest().lng() },
      { lat: rectangle.getBounds().getSouthWest().lat(), lng: rectangle.getBounds().getSouthWest().lng() },
      { lat: rectangle.getBounds().getSouthWest().lat(), lng: rectangle.getBounds().getNorthEast().lng() }
    ];

    // Construct the polygon.
    var rectPoly = new google.maps.Polygon({
        path: coords
    });
    var properties = ["strokeColor","strokeOpacity","strokeWeight","fillOpacity","fillColor"];
    //inherit rectangle properties 
    var options = {};
    properties.forEach(function(property) {
        if (rectangle.hasOwnProperty(property)) {
            options[property] = rectangle[property];
        }
    });
    rectPoly.setOptions(options);

    rectangle.setMap(null);
    rectPoly.setMap(map);
    return rectPoly;
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
    var bss = indices.map(function(i) {
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

/**
 * Main functions
 */
function areaSelected() {
    $container = $(".buttons-container");
    //$container.text = geometry.getBounds();
}

function initMap() {
    // initialize earth engine
    ee.initialize();

    // create map
    map = new google.maps.Map(document.getElementById('map'), {
        center: {lat: 0, lng: 0},
        zoom: 1,
        mapTypeId: 'satellite',   
        zoomControl: true,
        mapTypeControl: true,
        scaleControl: true,
        streetViewControl: false,
        fullscreenControl: false
    });
    // create drawing manager
    var shapeOptions = {
      fillColor: '#eedd82',
      fillOpacity: 0.2,
      strokeColor: '#eedd82',
      strokeWeight: 2,
      clickable: false,
      editable: false,
      zIndex: 1
    };
    drawingManager = new google.maps.drawing.DrawingManager({
        drawingControl: true,
        drawingControlOptions: {
          position: google.maps.ControlPosition.TOP_CENTER,
          drawingModes: ['rectangle']
        },
        rectangleOptions:shapeOptions,
        polygonOptions: shapeOptions
      });
    // allow only one shape on the map at a time
    google.maps.event.addListener(drawingManager, 'overlaycomplete', function(event) {
        if (event.type == google.maps.drawing.OverlayType.RECTANGLE) {
            if(rectangle != null)
                rectangle.setMap(null);
            rectangle = event.overlay;
            polygon = createPolygonFromRectangle(rectangle)
            areaSelected(rectangle);
            drawingManager.setDrawingMode(null);
        }
    });
    google.maps.event.addListener(drawingManager, "drawingmode_changed", function() {
        if ((drawingManager.getDrawingMode() == google.maps.drawing.OverlayType.RECTANGLE) && 
            (rectangle != null))
        rectangle.setMap(null);
    });
    drawingManager.setMap(map);
}  

function mapTransform()
{
    var coords = [];
    polygon.getPath().getArray().forEach(function(element) {
        coords.push([element.lng(), element.lat()]);
    }, this); 

    poly = ee.Geometry.Polygon(coords);

    var dataset = ee.ImageCollection('LANDSAT/LT05/C01/T1').filterBounds(poly).filterDate('1991-01-01', '1991-12-31');
    var composite = ee.Algorithms.Landsat.simpleComposite(dataset);
    
    var water_bands = ['B3', 'B5'];
    var water_threshold = 0.4;
    var water_sigma = 1;
    
    var ndwi_hist_ymax = 10;
    var ndwi = composite.normalizedDifference(water_bands);    
    
    var edge = ee.Algorithms.CannyEdgeDetector(ndwi, water_threshold, water_sigma);
    
    var ndwi_buffer = ndwi.mask(edge.focal_max(30, 'square', 'meters'));
    
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
    imageBounds = rectangle.getBounds();

    overlay = new google.maps.GroundOverlay(
        smooth,
        imageBounds);
    overlay.setMap(map);    

}


function loadApp()
{
    var onImmediateFailed = function() {
      $('.g-sign-in').removeClass('hidden');
      $('.output').text('(Log in to see the result.)');
      $('.g-sign-in .button').click(function() {
        ee.data.authenticateViaPopup(function() {
          // If the login succeeds, hide the login button and run the analysis.
          $('.g-sign-in').addClass('hidden');
          initMap();
        });
      });
    };

    // Attempt to authenticate using existing credentials.
    ee.data.authenticate(CLIENT_ID, initMap, null, null, onImmediateFailed);
}