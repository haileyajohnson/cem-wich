// colormap for grid cells
function getColor(fill) {
    var r, g, b;
    if (fill > 2/3) {
        var f = (fill-(2/3)) * 3;
        r = f*255;
        g = 255;
        b = 0;
    }
    else if (fill > 1/3) {
        var f = (fill-(1/3)) * 3;
        r = 0;
        g = 255;
        b = f*255;
    }
    else {
        var f = (fill) * 3;
        r = 0;
        g = f*255;
        b = 255;
    }
    return [r, g, b, 0.2];
}

function MapInterface() {
    return {
        map: null,        
        box: null,
        polyGrid: [],
        cemGrid: [],

        boundsSource: null,
        gridSource: null,
        modelSource: null,
        shorelineSource: null,

        refShoreline: null,
        cemShoreline: null,
        geeShoreline: null,

        drawingMode: false,
        editMode: false,
        draw: null,

        numCols: 100,
        numRows:  50,
        rotation: 0,

        initMap: function() {
            
            // create rotatable map
            this.map = new ol.Map({
                interactions: ol.interaction.defaults().extend([
                    new ol.interaction.DragRotateAndZoom()
                ]),
                target: 'map',
                view: new ol.View({
                    projection: "EPSG:4326",
                    center: [0, 0],
                    zoom: 2
                })
            });

            this.map.getView().on('change:rotation', () => {
                this.rotation = this.map.getView().getRotation();
                if (!this.box){
                    gridTab.setRotation();
                }
            });

            // LAYER 0: Bing Maps
            this.map.addLayer(new ol.layer.Tile({
                visible: true,
                preload: Infinity,
                source: new ol.source.BingMaps({
                    key: 'Am3Erq9Ut-VwSCA-xAUa8RoLu_jFgJwrK5zu0d81AWJkBwwOEr6DSSvzbi7b70e_',
                    imagerySet: 'aerial'
                })
            }));

            // LAYER 1: domain boundaries
            this.boundsSource = new ol.source.Vector({});  

            // craete draw function
            geometryFunction = (coordinates, geometry) => {
                var first = coordinates[0];
                var last = coordinates[1];

                var coordinates = this.getRectangleVertices(first, last);
                if (!geometry) {
                    geometry = new ol.geom.Polygon([coordinates]);
                } else {
                    geometry.setCoordinates([coordinates]);
                }
                return geometry;
            };

            // add drawing interaction
            this.draw = new ol.interaction.Draw({
                source: this.boundsSource,
                type: 'Circle',
                geometryFunction: geometryFunction
            });
            
            // When a drawing ends, save geometry
            this.boundsSource.on('addfeature', (evt) => {
                var feature = evt.feature;
                this.box = feature.getGeometry();
                gridTab.onBoxDrawn();
                this.drawGrid();
                this.toggleDrawMode();
            });
            
            this.map.addLayer(new ol.layer.Vector({
                source: this.boundsSource
            }));
            
            // LAYER 2: grid lines
            this.gridSource = new ol.source.Vector({});
            this.map.addLayer(new ol.layer.Vector({source: this.gridSource, 
                style: function(feature, resolution) {
                    return feature.get('style');
                }
            }));      
             
            // LAYER 3: CEM values
            this.modelSource = new ol.source.Vector({});
            this.map.addLayer(new ol.layer.Vector({source: this.modelSource, 
                style: function(feature, resolution) {
                    return feature.get('style');
                },
                id: "modelLayer"
            }));
            
            // add edit on click function
            this.map.on("click", (e) => {
                if (this.editMode) {
                    this.map.forEachFeatureAtPixel(e.pixel, (feature, layer) => {
                        // no-op if feature is in a different layer
                        if (layer.getId() != "modelLayer") { return; }
                        //this.editCellFeature(feature);
                        modal.open(feature);
                    });
                }
            });       

            // LAYER 4: shoreline vectors
            this.shorelineSource = new ol.source.Vector({});
            this.refShoreline = new ol.Feature({
                geometry: new ol.geom.LineString([]),
                style: new ol.style.Style({
                    stroke: new ol.style.Stroke({
                        color: 'black',
                        width: 1.2
                    })
                }),
                id: 0
            });
            this.cemShoreline = new ol.Feature({
                geometry: new ol.geom.LineString([]),
                style: new ol.style.Style({
                    stroke: new ol.style.Stroke({
                        color: 'red',
                        width: 1.2
                    })
                }),
                id: 1
            });
            this.geeShoreline = new ol.Feature({
                geometry: new ol.geom.LineString([]),
                style: new ol.style.Style({
                    stroke: new ol.style.Stroke({
                        color: 'blue',
                        width: 1.2
                    })
                }),
                id: 2
            });

            this.map.addLayer(new ol.layer.Vector({
                source: this.shorelineSource,
                style: function(feature, resolution) {
                    return feature.get('style');
                }
            }));
        },

        makeGrid: function(griddedShoreline) {
            // init grid
            this.cemGrid = [];
            for (var r = 0; r < this.numRows; r++) {
                this.cemGrid.push([]);
                for (var c = 0; c < this.numCols; c++) {
                    this.cemGrid[r].push(0);
                }
            }
            // fill in values
            for (var c = 0; c < griddedShoreline.length; c++)
            {
                var val = griddedShoreline[c];
                if (val <= 0) { continue; }
                var row = Math.floor(val);
                var sed = 1 - (val - row);
                this.cemGrid[row][c] = sed;
                for(var r = row+1; r < this.numRows; r++) {
                    this.cemGrid[r][c] = 1;
                }
            }
            return this.cemGrid;
        },

        updateDisplay: function(grid, shoreline=null) {
            // clear source
            this.modelSource.clear();
            this.cemGrid = grid;

            // make polygons
            for (var r = 0; r < this.numRows; r++)
            {
                for (var c = 0; c < this.numCols; c++) {
                    var i = this.rowColsToIndex(r, c);
                    var fill = this.cemGrid[r][c];
                    var color = getColor(fill)
                    this.modelSource.addFeature( new ol.Feature({
                        geometry: new ol.geom.Polygon([this.polyGrid[r][c]]),
                        id: i,
                        fill: fill,
                        style: new ol.style.Style({                        
                                stroke: new ol.style.Stroke({
                                    color: [255, 255, 255, 0]
                                }),
                                fill: new ol.style.Fill({
                                    color: color
                                })
                            })
                    }));
                }
            }            
            this.modelSource.refresh();

            // draw shoreline
            if (shoreline) {
                var upper_left = this.polyGrid[0][0][0];
                var lower_left = this.polyGrid[0][0][3];
                var dist_lon = upper_left[0] - lower_left[0];
                var dist_lat = upper_left[1] - lower_left[1];
                var coords = [];
                for (var i = 0; i < shoreline.length; i++)
                {
                    // get coordinates
                    var c = i;
                    var r = Math.floor(shoreline[i]);
                    var frac_full = shoreline[i] -r;
                    var lower_left = this.polyGrid[r][c][3];
                    var lower_right = this.polyGrid[r][c][2];
                    var lon = (lower_left[0] + lower_right[0])/2 + dist_lon*(1 - frac_full);
                    var lat = (lower_left[1] + lower_right[1])/2 + dist_lat*(1 - frac_full);
                    coords.push([lon, lat]);
                }
                this.cemShoreline.setGeometry(new ol.geom.LineString(coords));
                if (!this.shorelineSource.hasFeature(this.cemShoreline)) {
                    this.shorelineSource.addFeature(this.cemShoreline);
                }
                this.shorelineSource.refresh();
            }
        },

        displayShoreline: function(feature, shoreline) {
            feature.setGeometry(new ol.geom.LineString(shoreline));
            if (!this.shorelineSource.hasFeature(feature)) {
                this.shorelineSource.addFeature(feature);
            }
            this.shorelineSource.refresh();
        },

        /**
         * Update feature properties and redraw
         */
        updateFeature: function(feature, fill) {
            var id = feature.get('id');
            var rc = this.indexToRowCol(id);
            this.cemGrid[rc[0]][rc[1]] = fill;

            feature.set('fill', fill);
            this.modelSource.refresh();
        },

        /**
         * Draw grid lines on map
         */
        drawGrid: function() {
            this.gridSource.clear();

            var style = new ol.style.Style({
                stroke: new ol.style.Stroke({
                    color: [255, 0, 0, 0.8],
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
            for (var i = 0; i <= this.numRows; i++) {
                this.gridSource.addFeature(new ol.Feature({
                    geometry: new ol.geom.LineString([edge1_2[i], edge3_0[i]]),
                    style:style
                }));
            }

            for (var j = 0; j <= this.numCols; j++) {
                this.gridSource.addFeature(new ol.Feature({
                    geometry: new ol.geom.LineString([edge0_1[j], edge2_3[j]]),
                    style: style
                }));
            }

            // create matrix of polygons to map cem grid
            this.polyGrid = [];
            var top_edge = this.linspace(edge3_0[0], edge1_2[0], this.numCols);
            for (var i = 1; i <= this.numRows; i++) {
                this.polyGrid.push([]);
                var bottom_edge = this.linspace(edge3_0[i], edge1_2[i], this.numCols);
                for (var j = 0; j < this.numCols; j++) {
                    // grid polygon for individual cells in the row
                    var polyCoords = [top_edge[j], top_edge[j+1], bottom_edge[j+1], bottom_edge[j], top_edge[j]];
                    // add to polygon grid
                    this.polyGrid[i-1][j] = polyCoords;
                }
                top_edge = bottom_edge;
            }
            gridTab.onGridDrawn();
        },

        /**
         * Get coordinates of vertices of the geometry region
         * Coordinates ordered clockwise from top left
         */
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

        /**
         * draw geometry region
         */
        updateBox: function(lon1, lat1, lon2, lat2) {
            this.toggleDrawMode();
            var first = [lon1, lat1];
            var last = [lon2, lat2];

            var coordinates = this.getRectangleVertices(first, last);
            var geometry = new ol.geom.Polygon([coordinates]);
            this.boundsSource.addFeature(new ol.Feature({ geometry: geometry}));
        },

        /*********************
         * Getters and setters
         *********************/
        setNumRows: function(rows) {
            this.numRows = rows;
        },

        setNumCols: function(cols) {
            this.numCols = cols;
        },

        /**
         * Estimate cell width in meters
         */
        getColSize: function() {
            var coords = this.box.getCoordinates()[0];
            var lat1 = coords[0][1];
            var lon1 = coords[0][0];
            var lat2 = coords[1][1];
            var lon2 = coords[1][0];

            var dist_cols = this.getLatLonAsMeters(lat1, lon1, lat2, lon2);
            return dist_cols/this.numCols;
        },

        /**
         * Estimate cell length in meters
         */
        getRowSize: function() {
            var coords = this.box.getCoordinates()[0];
            var lat1 = coords[0][1];
            var lon1 = coords[0][0];
            var lat2 = coords[3][1];
            var lon2 = coords[3][0];

            var dist_rows = this.getLatLonAsMeters(lat1, lon1, lat2, lon2);
            return dist_rows/this.numRows;
        },

        /*************
         * Helpers
         *************/

        /**
         * Estimate distance between two coordinate points in meters
         */
        getLatLonAsMeters: function(lat1, lon1, lat2, lon2){ 
            var R = 6371; // Radius of earth in KM
            var dLat = lat2 * Math.PI / 180 - lat1 * Math.PI / 180;
            var dLon = lon2 * Math.PI / 180 - lon1 * Math.PI / 180;
            var a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon/2) * Math.sin(dLon/2);
            var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
            var d = R * c;
            return d * 1000; // meters
        },


         /**
          * Get N linearly spaced coordinates between start and stop
          */
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

        /**
         * allow user to draw domain on map
         */
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

        /**
         * allow user to change cell values
         */
        toggleEditMode: function() {
            this.editMode = !this.editMode;
        },      

        /**
         * convert index to row and column
         */
        indexToRowCol: function(i) {
            var r = Math.floor(i/this.numCols);
            var c = i%this.numCols;
            return [r, c];
        },

        /**
         * convert row and column to index
         */
        rowColsToIndex: function(r, c) {
            return (r*this.numCols) + c;
        },

        /**
         * reset map
         */
        clearMap: function() {
            this.boundsSource.clear();
            this.gridSource.clear();
            this.modelSource.clear();
            this.shorelineSource.clear();
            this.box = null;
            this.polyGrid = [];
            this.cemGrid = [];
        }
    }
}