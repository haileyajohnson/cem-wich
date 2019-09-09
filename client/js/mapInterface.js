function MapInterface() {
    return {
        map: null,        
        box: null,
        polyGrid: [],
        cemGrid: [],

        shorelineSource: null,
        boundsSource: null,
        gridSource: null,
        modelSource: null,
        imLayer: null,

        shorelineFeaturesInExtent: [],
        transectsInExtent: [],

        drawingMode: false,
        draw: null,
        numCols: 100,
        numRows:  50,
        rotation: 0,

        linspace: function(start, stop, N) {
            var difLon = stop[0] - start[0];
            var dLon = difLon/N;
            var difLat = stop[1] - start[1];
            var dLat = difLat/N;
            coords = [start];
            for (var i = 1; i < N; i ++) {
                coords.push([start[0] + i*dLon, start[1] + i*dLat]);
            }
            coords.push(stop);
            return coords;
        },

        toggleDrawMode: function() {
            this.drawingMode = !this.drawingMode;
            if (this.drawingMode) {
                $('.draw-button').addClass('selected');
                this.boundsSource.clear();
                this.gridSource.clear();
                this.box = null;
                this.map.addInteraction(this.draw);
            }
            else {
                $('draw-button').removeClass('selected');
                this.map.removeInteraction(this.draw);
            }
        },

        displayPhoto: function(photoUrl) {
            this.imLayer = new ol.layer.Image({
                source: new ol.source.ImageStatic({
                    url: photoUrl,
                    imageExtent: this.box.getExtent()
                })
            });
            this.imLayer.setZIndex(2);
            this.map.addLayer(this.imLayer);
            allowClear();
        },

        otsu: function(histogram) {
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

            // Return the mean value corresponding to the maximum BSS.
            return means.sort(bss).get([-1]);
        },

        getTransects: function () {
            //var features = this.shorelineSource.getFeatures();
            var tempSource= new ol.source.Vector();
            var features = [];

            this.shorelineSource.forEachFeatureIntersectingExtent(this.box.getExtent(), (f) => {
                features.push(f);
            });

            tempSource.addFeatures(features);
            
            var shorelineLayer = new ol.layer.Vector({
                source: tempSource
            });
            this.map.addLayer(shorelineLayer);
        },

        drawGrid: function() {
            this.gridSource.clear();

            var style = new ol.style.Style({
                stroke: new ol.style.Stroke({
                color: 'blue',
                width: 0.3
                })
            });

            var coords = this.box.getCoordinates()[0];
            // vertices numbered from top left (0) in a clockwise direction
            var edge0_1 = this.linspace(coords[0], coords[1], this.numCols);
            var edge1_2= this.linspace(coords[1], coords[2], this.numRows);
            var edge2_3 = this.linspace(coords[3], coords[2], this.numCols);
            var edge3_0 = this.linspace(coords[0], coords[3], this.numRows);

            // add each line feature
            for (var i = 0; i < edge1_2.length; i++) {
                this.gridSource.addFeature(new ol.Feature({
                    geometry: new ol.geom.LineString([edge1_2[i], edge3_0[i]]),
                    style:style
                }));
            }

            for (var j = 0; j < edge0_1.length; j++) {
                this.gridSource.addFeature(new ol.Feature({
                    geometry: new ol.geom.LineString([edge0_1[j], edge2_3[j]]),
                    style: style
                }));
            }

            // add to map
            var vectorLayer = new ol.layer.Vector({source: this.gridSource, 
                style: function(feature, resolution) {
                    return feature.get('style');
                }
            });
            vectorLayer.setZIndex(3);
            this.map.addLayer(vectorLayer);

            // create matrix of polygons to map cem grid
            this.polyGrid = [];
            for (var i = 0; i < edge1_2.length - 1; i++) {
                this.polyGrid.push([]);
                var top_edge = this.linspace(edge1_2[i], edge3_0[i], this.numCols);
                var bottom_edge = this.linspace(edge1_2[i+1], edge3_0[i+1], this.numCols);
                for (var j = 0; j < top_edge.length - 1; j++) {
                    // grid polygon for individual cells in the row
                    var polyCoords = [top_edge[j], top_edge[j+1], bottom_edge[j+1], bottom_edge[j], top_edge[j]];
                    // add to polygon grid
                    this.polyGrid[i][j] = polyCoords;
                }
            }
        },

        initMap: function() {
            // initialize earth engine
            ee.initialize();

            // create rotatable map
            this.map = new ol.Map({
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

            this.map.getView().on('change:rotation', () => {
                this.rotation = this.map.getView().getRotation();
                onRotationChange(this.rotation);
            });
            
            // create shoreline source
            this.shorelineSource = new ol.source.Vector({
                url: '_dist/extern/resources/10m_coastline.kml',
                format: new ol.format.KML()
            });
            var shorelineLayer = new ol.layer.Vector({
                source: this.shorelineSource
            });
            this.map.addLayer(shorelineLayer);

            // create box source
            this.boundsSource = new ol.source.Vector({});           
            
            // create grid source
            this.gridSource = new ol.source.Vector({});      
             
            // create CEM source
            this.modelSource = new ol.source.Vector({});   

            // create Bing Maps layer
            var bingLayer = new ol.layer.Tile({
                visible: true,
                preload: Infinity,
                source: new ol.source.BingMaps({
                    key: 'Am3Erq9Ut-VwSCA-xAUa8RoLu_jFgJwrK5zu0d81AWJkBwwOEr6DSSvzbi7b70e_',
                    imagerySet: 'aerial'
                })
            });
            this.map.addLayer(bingLayer);

            // craete draw function
            var that = this;
            geometryFunction = function (coordinates, geometry) {
                var first = coordinates[0];
                var last = coordinates[1];
                var coordinates = that.getRectangleVertices(first, last);                
                if (!geometry) {
                    geometry = new ol.geom.Polygon([coordinates]);
                } else {
                    geometry.setCoordinates([coordinates]);
                }
                return geometry;
            };

            // add drawing interaction
            this.draw = new ol.interaction.Draw({
                source: that.boundsSource,
                type: 'Circle',
                geometryFunction: geometryFunction
            });
            
            // When a drawing ends, save geometry
            this.boundsSource.on('addfeature', (evt) => {
                var feature = evt.feature;
                that.box = feature.getGeometry();
                onBoxChange();
                this.drawGrid();
                this.getTransects();
                this.toggleDrawMode();
            });
            
            // add layer
            this.map.addLayer(new ol.layer.Vector({
                source: this.boundsSource
            }));
        },

        getRectangleVertices: function(first, last) {
            // find distance between corners
            var dLon = last[0] - first[0];
            var dLat = last[1] - first[1];
            var dist = Math.sqrt(Math.pow(dLon, 2) + Math.pow(dLat, 2))
            // rotate to view parallel 
            var theta = Math.atan2(dLat, dLon); // angle between vertices in coordinate plane
            var w = dist * Math.cos(theta - this.rotation); // width of rotated box
            var l = dist * Math.sin(theta - this.rotation); // length of rotated box
            var up = [first[0] - l * Math.sin(this.rotation), first[1] + l * Math.cos(this.rotation)];
            var over = [first[0] + w * Math.cos(this.rotation), first[1] + w * Math.sin(this.rotation)];
            // order coordinates clockwise from top left
            var coordinates;
            if (((dLon*Math.cos(-this.rotation))-(dLat*Math.sin(-this.rotation))) > 0) { // last is right of first
                 if ((dLon*Math.sin(-this.rotation) + dLat*Math.cos(-this.rotation)) > 0) { //last is higher than first
                     coordinates = [up, last, over, first];
                 } else { // last is lower than first
                     coordinates = [first, over, last, up];
                 }
             } else { // last is left of first
                 if ((dLon*Math.sin(-this.rotation) + dLat*Math.cos(-this.rotation)) > 0) { //last is higher than first
                     coordinates = [last, up, first, over];
                 } else { // last is lower than first
                     coordinates = [over, first, up, last];
                 }
             }
            coordinates.push(coordinates[0].slice());
            return coordinates
        },

        updateBox: function(lon1, lat1, lon2, lat2) {
            this.toggleDrawMode();
            var coordinates = this.getRectangleVertices([lon1, lat1], [lon2, lat2])
            var geometry = new ol.geom.Polygon([coordinates]);
            this.boundsSource.addFeature(new ol.Feature({ geometry: geometry}));
        },

        mapTransform: function() {
            var poly = new ee.Geometry.Polygon(this.box.getCoordinates()[0]);

            var dataset = ee.ImageCollection('LANDSAT/LE07/C01/T1').filterBounds(poly).filterDate('1999-01-01', '1999-12-31');
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

            var water = ndwi.gt(this.otsu(values.get('nd')));
            // var water_edge = ee.Algorithms.CannyEdgeDetector(water, 0.5, 0);

            // var gaussian = ee.Kernel.gaussian({
            //     radius: 5
            // });

            // var smooth = water_edge.convolve(gaussian);
            // var smooth = smooth.clip(poly);

            var smooth = water.clip(poly);

            // show on map
            smooth.getThumbURL({ dimensions: [800, 800], region: poly.toGeoJSONString() }, (url) => { 
                this.displayPhoto(url);
                this.createGrid(smooth);
            });
        },

        /**
         * convert Otsu image into CEM input grid
         * size numRows X numCols
         */
        createGrid: function(image) {
            this.cemGrid = [];
            var features = [];
            for (r = 0; r < this.numRows; r++) {
                this.cemGrid.push([]);
                for (c = 0; c < this.numCols; c++) {
                    // create polygon feature for each cell                    
                    features.push(new ee.Feature(new ee.Geometry.Polygon(this.polyGrid[r][c])));
                }
            }

            var fc = new ee.FeatureCollection(features);
                    
            // Reduce the region. The region parameter is the Feature geometry.
            var dict = image.reduceRegions({
                reducer: ee.Reducer.mean(),
                collection: fc,
                scale: 30
            });

            var style = new ol.style.Style({
                stroke: new ol.style.Stroke({
                    color: 'blue',
                    width: 0.3
                    }),
                fill: new ol.style.Fill({
                    color: 'rgba(0, 0, 255, 0.1)'
                })
            });
            var cemGrid = dict.getInfo();

            // display model grid
            for (var i = 0; i < cemGrid.features.length; i++) {
                var feature = cemGrid.features[i];
                var coords = feature.geometry.coordinates[0];
                var value = 1 - feature.properties.mean;
                // scale polygons to show percent fill
                var scale = [value*(coords[0][0] - coords[3][0]), value*(coords[0][1] - coords[3][1])];
                coords[0][0] = coords[3][0] + scale[0];
                coords[0][1] = coords[3][1] + scale[1];
                coords[4] = coords[0];
                coords[1][0] = coords[2][0] + scale[0];
                coords[1][1] = coords[2][1] + scale[1];
                this.modelSource.addFeature( new ol.Feature({
                    geometry: new ol.geom.Polygon([coords]),
                    style: style
                }))
            }

            // add to map
            var vectorLayer = new ol.layer.Vector({source: this.modelSource});
            vectorLayer.setZIndex(4);
            this.map.addLayer(vectorLayer);
        },

        /**
         * Getters and setters
         */

        setNumRows: function(rows) {
            this.numRows = rows;
        },

        setNumCols: function(cols) {
            this.numCols = cols;
        },

        /**
         * reset map
         */
        clearMap: function() {
            this.boundsSource.clear();
            this.gridSource.clear();
            this.modelSource.clear();
            if (this.imLayer) { this.map.removeLayer(this.imLayer); }
            this.box = null;
        }

    }
}