
/**
 * Global variables
 */
// config vars
var configJSON;
const CLIENT_ID = '762501139172-rjf0ia3vv9edu6gg0m46aoij519khuk7.apps.googleusercontent.com';

// application interface objects
var mapInterface;
var modelInterface;


// views
var selectedTab;
var gridTab;
var wavTab;
var controlTab;
var runTab;
var modal;

// jQuery
var $saveButton;
var $loadButton;

function loadApp() {
    // Attempt to authenticate using existing credentials.
    ee.data.authenticate(CLIENT_ID, initialize, null, null, googleSignIn);
}

function initialize() {
    configJSON = {};

    // create applicaiton interfaces
    mapInterface = MapInterface();
    mapInterface.initMap();
    
    // create tabs
    gridTab = GridTab();
    gridTab.init();
    waveTab = WaveTab();
    waveTab.init();
    controlTab = ControlsTab()
    controlTab.init();
    runTab = RunTab();
    runTab.init();
    
    // create modal dialog window
    modal = ModalInterface();
    modal.init();

    // init buttons
    $saveButton = $(".save-button");
    $loadButton = $(".load-button");

    // open grid tab
    onTabChange(gridTab);
}

function googleSignIn() {
    $('.g-sign-in').removeClass('hidden');
    $('.output').text('(Log in to see the result.)');
    $('.g-sign-in .button').click(function () {
        ee.data.authenticateViaPopup(function () {
            // If the login succeeds, hide the login button and run the analysis.
            $('.g-sign-in').addClass('hidden');
            initialize();
        });
    });
}

/**
 *  Event listeners 
 */
function onTabChange(newTab){
    if (selectedTab) {
        selectedTab.$tab.removeAttr("selected");
        selectedTab.$elem.hide();
    }
    newTab.$tab.attr("selected", "selected");
    newTab.$elem.show();
    selectedTab = newTab;
}

function onSave() {
    updateJSON();
    saveConfig();
}

function onLoad() {
    $('#file-input').trigger('click');
}


function onRun() {
    // create payload
    var input_data = {
        grid: mapInterface.cemGrid,
        nRows: mapInterface.numRows,
        nCols: mapInterface.numCols,
        cellWidth: mapInterface.getCellWidth(),
        cellLength: mapInterface.getCellLength(),
        asymmetry: parseFloat(this.waveTab.a_val),
        stability: parseFloat(this.waveTab.u_val),
        waveHeight: parseFloat(this.waveTab.wave_height),
        wavePeriod: parseFloat(this.waveTab.wave_period),
        shelfSlope: parseFloat(this.controlTab.shelf_slope),
        shorefaceSlope: parseFloat(this.controlTab.shoreface_slope),
        numTimesteps: parseInt(Math.round((this.controlTab.end_year - this.mapInterface.source.year) * (365/this.controlTab.length_timestep))),
        lengthTimestep: parseFloat(this.controlTab.length_timestep),
        saveInterval: parseInt(this.controlTab.save_interval),
        sourceUrl: mapInterface.source.url,
        start: mapInterface.source.year
    }
    // ensure necessary values are present and valid
    var status = validateData(input_data);
    if (status == 0) {
        // pass to python
        $.post('/senddata', {
            type: "json",
            input_data: JSON.stringify(input_data),
            success: () => { console.log("success!")}
        });
    }
}

function onModelComplete() {

}

/**
 * Save/load functions
 */

function updateJSON() {
    // save view settings
    var viewSettings = {
        center: mapInterface.map.getView().getCenter(),
        zoom: mapInterface.map.getView().getZoom()
    }
    configJSON.viewSettings = viewSettings;

    // save grid inputs
    var coords = mapInterface.box ? mapInterface.box.getCoordinates()[0] : null;
    var gridConfig = {
        rotation: mapInterface.rotation,
        numRows: mapInterface.numRows,
        numCols: mapInterface.numCols,
        grid: mapInterface.cemGrid
    };
    if (coords) {
        gridConfig.points = [coords[0], coords[2]];
    }
    configJSON.gridConfig = gridConfig;

    // save wave inputs

    // save config inputs
}

function importJSON(newContent) {
    // clear map
    mapInterface.clearMap();
    // load config file
    configJSON = JSON.parse(newContent);

    // load map view settings
    mapInterface.map.getView().setCenter(configJSON.viewSettings.center);
    mapInterface.map.getView().setZoom(configJSON.viewSettings.zoom);

    // load grid inputs
    var gridConfig = configJSON.gridConfig;
    mapInterface.setNumRows(gridConfig.numRows);
    mapInterface.setNumCols(gridConfig.numCols);
    mapInterface.map.getView().setRotation(gridConfig.rotation);  
    mapInterface.cemGrid = gridConfig.grid;
    if (gridConfig.points) {
        var points = gridConfig.points;
        mapInterface.updateBox(points[0][0], points[0][1], points[1][0], points[1][1]);
    }
    gridTab.setAllValues();

    // load wave inpts

    // load config inputs
}

function saveConfig() {    
    var a = document.createElement("a");
    var file = new Blob([JSON.stringify(configJSON)], {type: 'application/octet-stream'});
    a.href = URL.createObjectURL(file);
    a.download = "config.json";
    a.click();
}

 function loadConfig() {
    // getting a hold of the file reference
    var file = this.event.target.files[0];
    if (!file) { return; }
    
    // setting up the reader
    var reader = new FileReader();
    reader.readAsText(file,'UTF-8');

    reader.onload = (readerEvent) => {
        var content = readerEvent.target.result;
        importJSON(content)
    };
    this.event.target.value = '';
 }

/**
 * Grid tab view
 */
function GridTab() {
    return {
        $elem: $(".grid-tab"),
        $tab: $("#grid-tab"),
        
        $rotation: $("input[name=rotation]"),
        
        $numRows: $("input[name=numRows]"),       
        $numCols: $("input[name=numCols]"),

        $lon1: $("input[name=lon1]"),
        $lat1: $("input[name=lat1]"),
        $lon2: $("input[name=lon2]"),
        $lat2: $("input[name=lat2]"),
        $coords: $(".coord"),

        $drawButton: $(".draw-button"),
        $submitButton: $(".submit-button"),
        $clearButton: $(".clear-button"),
        $editButton: $(".edit-button"),

        init: function() {
            // add input listeners
            this.$tab.click(() => { onTabChange(this); });
            this.$rotation.change(() => { this.onRotationChange(); });
            this.$numRows.change(() => { this.onNumRowsChange(); });
            this.$numCols.change(() => { this.onNumColsChange(); });
            this.$coords.change(() => { this.onCoordsChange(); });

            // add button listeners
            this.$drawButton.click(() => { this.onDrawClicked(); });
            this.$submitButton.click(() => { this.onSubmitClicked(); });
            this.$clearButton.click(() => { this.onClearClicked(); });
            this.$editButton.click(() => { this.toggleEdit(); });


            $('input[type=radio]').click(function(){
                    mapInterface.source = sources[this.value];
            });
            $('input[type=radio]:first').attr("checked", true);

            // disable buttons
            this.$submitButton.disable();
            this.$clearButton.disable();
            this.$editButton.disable();

            // initialize
            this.setAllValues();
        },
        
        /**
         * click listeners
         */
        onSubmitClicked: function() {
            this.$coords.disable();

            this.$drawButton.disable();
            $loadButton.disable();

            mapInterface.mapTransform();
            
            this.$editButton.enable();
        },

        onClearClicked: function() {
            mapInterface.clearMap();

            this.$rotation.enable();
            this.setRotation();
            this.$coords.val("");
            this.$coords.enable();
            
            this.$drawButton.enable();
            this.$submitButton.disable();
            this.$editButton.disable();
            this.$clearButton.disable();
            
            $loadButton.enable();
        },

        onDrawClicked: function() {
            mapInterface.toggleDrawMode();
        },

        toggleEdit: function() {
            this.$editButton.attr("selected") ? this.$editButton.removeAttr("selected") : this.$editButton.attr("selected", "selected");
            mapInterface.toggleEditMode();
        },

        /**
         * input listeners 
         */
        onCoordsChange: function() {
            var coords = mapInterface.box ? mapInterface.box.getCoordinates()[0] : [["", ""], [], ["", ""]];
            var lon1 = tryParseFloat(this.$lon1.val(), coords[0][0]);
            var lat1 = tryParseFloat(this.$lat1.val(), coords[0][1]);
            var lon2 = tryParseFloat(this.$lon2.val(), coords[2][0]);
            var lat2 = tryParseFloat(this.$lat2.val(), coords[2][1]);

            // if one or more input is empty
            if ($(".coord").toArray().some((e) => { return !($(e).val());})) {
                return;
            }
            mapInterface.updateBox(lon1, lat1, lon2, lat2);
        },

        onNumRowsChange: function() {
            mapInterface.setNumRows(tryParseInt(this.$numRows.val()), mapInterface.numRows);
            if (mapInterface.box) {
                mapInterface.drawGrid();
            }
        },

        onNumColsChange: function() {
            mapInterface.setNumCols(tryParseInt(this.$numCols.val()), mapInterface.numCols);
            if (mapInterface.box) {
                mapInterface.drawGrid();
            }
        },

        onRotationChange: function() {
            mapInterface.map.getView().setRotation(
                deg_to_rad(tryParseFloat(this.$rotation.val()), mapInterface.rotation));
        },

        /**
         * setters
         */        
        setAllValues: function() {
            this.setRotation();
            this.$numRows.val(mapInterface.numRows);
            this.$numCols.val(mapInterface.numCols);
            this.setCoords();
        },        

        setRotation: function() {    
            this.$rotation.val(rad_to_deg(mapInterface.rotation));
        },        

        setCoords: function() {
            if (!mapInterface.box) { return; }
            var coords = mapInterface.box.getCoordinates()[0];
            $("input[name=lon1]").val(coords[0][0]);
            $("input[name=lat1]").val(coords[0][1]);
            $("input[name=lon2]").val(coords[2][0]);
            $("input[name=lat2]").val(coords[2][1]);
        },

        /**
         * map listeners
         */

        onBoxDrawn: function() {
            this.setRotation();
            this.setCoords();
            this.$rotation.disable();
            this.$clearButton.enable();
            this.$submitButton.enable();
        }
    }
}

/**
 * Wave tab view
 */
function WaveTab() {
    return {
        $elem: $(".wave-tab"),
        $tab: $("#wave-tab"),

        $asymmetry: $("input[name=a-input]"),       
        $stability: $("input[name=u-input]"),
        $wave_height: $("input[name=wave-height]"),       
        $wave_period: $("input[name=wave-period]"),

        a_val: null,
        u_val: null,
        wave_height: null,
        wave_period: null,

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            // init values
            this.a_val = this.$asymmetry.val();
            this.u_val = this.$stability.val();
            this.wave_height = this.$wave_height.val();
            this.wave_period = this.$wave_period.val();
            // attach listeners
            this.$asymmetry.change(() => { this.onAsymmetryChange(); });
            this.$stability.change(() => { this.onStabilityChange(); });
            this.$wave_height.change(() => { this.onWaveHeightChange(); });
            this.$wave_period.change(() => { this.onWavePeriodChange(); });
        },

        /** listeners
         */
        onAsymmetryChange: function() {
            this.a_val = this.$asymmetry.val();
        },

        onStabilityChange: function() {
            this.u_val = this.$stability.val();
        },

        onWaveHeightChange: function() {
            this.wave_height = this.$wave_height.val();
        },

        onWavePeriodChange: function() {
            this.wave_period = this.$wave_period.val();
        }

    }
}

/**
* Controls tab view
*/
function ControlsTab() {
    return {
        $elem: $(".control-tab"),
        $tab: $("#control-tab"),

        $shelf_slope: $("input[name=shelf-slope]"),
        $shoreface_slope: $("input[name=shoreface-slope]"),
        $end_year: $("input[name=end-date]"),
        $length_timestep: $("input[name=timestep]"),
        $save_interval: $("input[name=save-interval]"),

        shelf_slope: null,
        shoreface_slope: null,
        end_year: null,
        length_timestep: null,
        save_interval: null,

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            // init values
            this.shelf_slope = this.$shelf_slope.val();
            this.shoreface_slope = this.$shoreface_slope.val();
            this.end_year = this.$end_year.val();
            this.length_timestep = this.$length_timestep.val();
            this.save_interval = this.$save_interval.val();
            // attach listeners
            this.$shelf_slope.change(() => { this.onShelfSlopeChange(); });
            this.$shoreface_slope.change(() => { this.onShorefaceSlopeChange(); });
            this.$end_year.change(() => { this.onEndYearChange(); });
            this.$length_timestep.change(() => { this.onTimestepLengthChange(); });
            this.$save_interval.change(() => { this.onSaveIntervalChange(); });
        },

        /** listeners
         */
        onShelfSlopeChange: function() {
            this.shelf_slope = this.$shelf_slope.val();
        },

        onShorefaceSlopeChange: function() {
            this.shoreface_slope = this.$shoreface_slope.val();
        },

        onEndYearChange: function() {
            this.end_year = this.$end_year.val();
        },

        onTimestepLengthChange: function() {
            this.length_timestep = this.$length_timestep.val();
        },

        onSaveIntervalChange: function() {
            this.save_interval = this.$save_interval.val();
        }
    }
}

/**
 * Run tab view
 */
function RunTab() {
    return {
        $elem: $(".run-tab"),
        $tab: $("#run-tab"),
        
        $runButton: $(".run-button"),

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            this.$runButton.click(() => { onRun(); });
        }
    }
}

/**
 * Modal window
 */
 function ModalInterface() {
    return {
        $modal: $(".modal"),
        $fillInput: $("input[name=fill]"),
        $alignmentButtons: $(".alignment-buttons .button"),
        $cell: $(".cell"),

        feature: null,
        percentFull: 0,
        orientation: 0,

        init: function() {
            // set up listeners
            this.$fillInput.change(() => {
                this.setFill();
                this.display();
            });
            this.$alignmentButtons.each((i, elem) => {
                $(elem).click(() => {
                    this.setOrientation(parseInt($(elem).attr('id')));
                    this.display();
                });
            });
            this.$modal.find(".modal-ok-button").click(() => { this.onOkClicked(); } );
            this.$modal.find(".modal-cancel-button").click(() => { this.onCancelClicked() });
        },

        open: function(feature) {
            this.feature = feature;
            this.$fillInput.val(feature.get("fill")*100);
            this.setFill();
            this.orientation = feature.get('orientation');
            this.display();
            this.$modal.show();
        },

        setFill: function() {
            this.percentFull = this.$fillInput.val();
        },

        setOrientation(n) {
            this.orientation = n;
        },

        display: function() {
            this.$cell.empty();
            var $cellFull = $("<div class='cell-full'></div>");
            var $cellEmpty = $("<div class='cell-empty'></div>");

            var horzStyleFull = {
                "width": "" + this.percentFull + "%",
                "height": "100%"
            }
            var horzStyleEmpty = {
                "width": "" + (100-this.percentFull) + "%",
                "height": "100%"
            }
            
            var vertStyleFull = {
                "height": "" + this.percentFull + "%",
                "width": "100%"
            }
            var vertStyleEmpty = {
                "height": "" + (100-this.percentFull) + "%",
                "width": "100%"
            }

            switch(this.orientation) {
                case 0:
                    $cellFull.css(horzStyleFull).appendTo(this.$cell);
                    $cellEmpty.css(horzStyleEmpty).appendTo(this.$cell);
                    break;
                case 1:
                    $cellFull.css(vertStyleFull).appendTo(this.$cell);
                    $cellEmpty.css(vertStyleEmpty).appendTo(this.$cell);
                    break;
                case 2:
                    $cellEmpty.css(horzStyleEmpty).css({"float":"left"}).appendTo(this.$cell);
                    $cellFull.css(horzStyleFull).css({"float":"right"}).appendTo(this.$cell);
                    break;
                case 3:
                    $cellEmpty.css(vertStyleEmpty).css({"float":"left"}).appendTo(this.$cell);
                    $cellFull.css(vertStyleFull).css({"float":"right"}).appendTo(this.$cell);
                    break;
            }
        },

        save: function() {
            var i = this.feature.get('id');
            mapInterface.cemGrid[i] = this.percentFull/100;
            mapInterface.updateFeature(this.feature, this.percentFull/100, this.orientation);
        },

        close: function() {
            this.feature = null;
            this.$modal.hide();
        },

        /**
         * Listeners
         */        
        onOkClicked: function() {
            // save changes
            this.save();
            this.close();
        },

        onCancelClicked: function() {
            this.close();
        }
    };
}

