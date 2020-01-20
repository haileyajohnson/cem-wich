
/******************
 * Global variables
 ******************/

// config vars
var configJSON;
const CLIENT_ID = '762501139172-rjf0ia3vv9edu6gg0m46aoij519khuk7.apps.googleusercontent.com';
var mode;

// application interface objects
var mapInterface;

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
    // configuration parameters
    configJSON = {};

    // create applicaiton interfaces
    mapInterface = MapInterface();
    mapInterface.initMap();
    
    // create tabs
    gridTab = GridTab();
    waveTab = WaveTab();
    controlTab = ControlsTab();
    runTab = RunTab();
    gridTab.init();
    waveTab.init();
    controlTab.init();
    runTab.init();
    
    // create modal dialog window
    modal = ModalInterface();
    modal.init();

    // init buttons
    $saveButton = $(".save-button");
    $loadButton = $(".load-button");

    // open grid tab
    onTabChange(gridTab);

    // set up mode inputs
    mode = 1;
    $('input[type=radio][name=mode]:first').attr("checked", true);
    $('input[type=radio][name=mode]').change(function(){ mode = parseInt(this.value); });
}

/**
 * Google sign in
 */
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

/*******************
 *  Event listeners 
 *******************/

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
    disableAll();
    // create payload
    var input_data = {
        grid: mapInterface.cemGrid,
        nRows: mapInterface.numRows,
        nCols: mapInterface.numCols,
        polyGrid: mapInterface.polyGrid,
        source: controlTab.source,
        start: controlTab.start_year,
        geometry: mapInterface.box.getCoordinates(),
        cellWidth: mapInterface.getCellWidth(),
        cellLength: mapInterface.getCellLength(),
        asymmetry: parseFloat(this.waveTab.a_val),
        stability: parseFloat(this.waveTab.u_val),
        waveHeights: this.waveTab.wave_heights,
        wavePeriods: this.waveTab.wave_periods,
        waveAngles: this.waveTab.wave_angles,
        shelfSlope: parseFloat(this.controlTab.shelf_slope),
        shorefaceSlope: parseFloat(this.controlTab.shoreface_slope),
        crossShoreRef: parseFloat(this.controlTab.cross_shore_ref),
        refDepth: parseFloat(this.controlTab.ref_depth),
        minClosureDepth: parseFloat(this.controlTab.min_closure_depth),
        numTimesteps: controlTab.num_timesteps,
        lengthTimestep: parseFloat(this.controlTab.length_timestep),
        saveInterval: parseInt(this.controlTab.save_interval),
        mode: mode
    }
    // ensure necessary values are present and valid
    var status = validateData(input_data);
    if (status == 0) {
        // pass to python
        $.post('/initialize', {
            type: "json",
            input_data: JSON.stringify(input_data)
        }).done(() => { onUpdate(0); }).fail((err) => { /*error report*/});
    }
}

function onUpdate(timestep) {
    $.get('/update/' + timestep).done((resp) => {
        new_time = resp.data.timestep;
        mapInterface.updateDisplay(resp.data.grid);
        runTab.displayTimestep(new_time);
        runTab.updateOutput(resp.data.results);
        if (new_time < controlTab.num_timesteps) {
            onUpdate(new_time);
        } else {
            onModelComplete();
        }
    }).fail(() => { /* error reporting */});
}

function onModelComplete() {
    $.get('/finalize').done(() => {
        mapInterface.mapTransform(FilterDates(controlTab.end_year), false);
        enableAll();
    }).fail(() => {/* error reporting */});
}

/*********
 * Helpers
 *********/

 function disableAll() {
    // save config
    $saveButton.disable();

    // grid tab
    gridTab.$clearButton.disable();
    gridTab.$editButton.disable();

    // wave tab
    waveTab.$asymmetry.disable();
    waveTab.$stability.disable();
    waveTab.$wave_height.disable();
    waveTab.$wave_period.disable();

    // conds tab
    controlTab.$shelf_slope.disable();
    controlTab.$shoreface_slope.disable();
    controlTab.$start_year.disable();
    controlTab.$end_year.disable();
    controlTab.$length_timestep.disable();
    controlTab.$save_interval.disable();

    // run tab
    runTab.$runButton.enable();
    runTab.$outputButton.enable();

 }

 function enableAll() {
    // save config
    $saveButton.enable();

    // grid tab
    gridTab.$clearButton.enable();
    gridTab.$editButton.enable();

    // wave tab
    waveTab.$asymmetry.enable();
    waveTab.$stability.enable();
    waveTab.$wave_height.enable();
    waveTab.$wave_period.enable();

    // conds tab
    controlTab.$shelf_slope.enable();
    controlTab.$shoreface_slope.enable();
    controlTab.$start_year.enable();
    controlTab.$end_year.enable();
    controlTab.$length_timestep.enable();
    controlTab.$save_interval.enable();

    // run tab
    runTab.$runButton.enable();
    runTab.$outputButton.enable();
 }

 function getFilterDates(year) {
    var start_date = year + "-01-01";
    var end_date = year + "-12-31";
    return [start_date, end_date];
 }

/*********************
 * Save/load functions
 *********************/

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
        source: gridTab.source
    };
    if (coords) {
        gridConfig.points = [coords[0], coords[2]];
    }
    configJSON.gridConfig = gridConfig;

    // save wave inputs
    configJSON.waveConfig = {
        asymmetry: waveTab.a_val,
        stability: waveTab.u_val,
        waveHeights: waveTab.wave_heights,
        wavePeriods: waveTab.wave_periods,
        waveAngles: waveTab.wave_angles
    };

    // save config inputs
    configJSON.controlConfig = {
        shelfSlope: controlTab.shelf_slope,
        shorefaceSlope: controlTab.shoreface_slope,
        crossShoreRef: controlTab.cross_shore_ref,
        refDepth: controlTab.ref_depth,
        minClosureDepth: controlTab.min_closure_depth,
        startYear: controlTab.start_year,
        endYear: controlTab.end_year,
        lengthTimestep: controlTab.length_timestep,
        saveInterval: controlTab.save_interval
    };
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
    gridTab.source = gridConfig.source;
    if (gridConfig.points) {
        var points = gridConfig.points;
        mapInterface.updateBox(points[0][0], points[0][1], points[1][0], points[1][1]);
    }
    gridTab.setAllValues();

    // load wave inpts
    if (configJSON.waveConfig) {
        var waveConfig = configJSON.waveConfig;
        waveTab.a_val = waveConfig.asymmetry;
        waveTab.u_val = waveConfig.stability;
        waveTab.wave_heights = waveConfig.waveHeights;
        waveTab.wave_periods = waveConfig.wavePeriods;
        waveTab.wave_angles = waveConfig.waveAngles;
        waveTab.setAllValues();
    }

    // load config inputs
    if (configJSON.controlConfig) {
        var controlConfig = configJSON.controlConfig;
        controlTab.shelf_slope = controlConfig.shelfSlope;
        controlTab.shoreface_slope = controlConfig.shorefaceSlope;
        controlTab.cross_shore_ref = controlConfig.crossShoreRef;
        controlTab.ref_depth = controlConfig.refDepth;
        controlTab.min_closure_depth = controlConfig.minClosureDepth;
        controlTab.start_year = controlConfig.startYear;
        controlTab.end_year = controlConfig.endYear;
        controlTab.length_timestep = controlConfig.lengthTimestep;
        controlTab.save_interval = controlConfig.saveInterval;
        controlTab.setAllValues();
    }
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

function uploadWaveFile() {    
    // getting a hold of the file reference
    var file = event.target.files[0];
    if (!file) { return; }

    // setting up the reader
    var reader = new FileReader();
    reader.readAsText(file,'UTF-8');

    reader.onload = (readerEvent) => {
        var content = readerEvent.target.result;
        waveTab.readWaveFile(content);
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
        $cellSize: $(".cell-dimensions"),

        $lon1: $("input[name=lon1]"),
        $lat1: $("input[name=lat1]"),
        $lon2: $("input[name=lon2]"),
        $lat2: $("input[name=lat2]"),
        $coords: $(".coord"),

        $drawButton: $(".draw-button"),
        $submitButton: $(".submit-button"),
        $clearButton: $(".clear-button"),
        $editButton: $(".edit-button"),
        $exportButton: $(".export-button"),

        source: -1,

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
            this.$exportButton.click(() => { this.exportGrid(); });

            $('input[type=radio][name=source]').click(($elem) => {
                    this.source = $elem.value;
            });
            $('input[type=radio][name=source]:first').attr("checked", true);

            // disable buttons
            this.$submitButton.disable();
            this.$clearButton.disable();
            this.$editButton.disable();
            this.$exportButton.disable();

            // initialize
            this.setAllValues();
        },
        
        /*****************
         * click listeners
         *****************/
        onSubmitClicked: function() {
            this.$coords.disable();

            this.$drawButton.disable();
            $loadButton.disable();
            $('input[type=radio][name=source]').disable();
            this.$numRows.disable();
            this.$numCols.disable();
            this.$submitButton.disable();

            mapInterface.mapTransform(getFilterDates(controlTab.start_year), true);            
            this.$editButton.enable();
            this.$exportButton.enable();
            runTab.$runButton.enable();

            this.$cellSize.text("Cell size: " + mapInterface.getCellLength().toFixed(3) + "m x " + mapInterface.getCellWidth().toFixed(3) + "m");
        },

        onClearClicked: function() {
            mapInterface.clearMap();

            this.$rotation.enable();
            this.setRotation();
            this.$coords.val("");
            this.$coords.enable();
            $('input[type=radio][name=source]').enable();            
            this.$numRows.enable();
            this.$numCols.enable();
            
            this.$drawButton.enable();
            this.$submitButton.disable();
            this.$editButton.disable();
            this.$clearButton.disable();
            this.$exportButton.disable();
            
            $loadButton.enable();
            runTab.$runButton.disable();
            runTab.$outputButton.disable();
            runTab.clearOutput();
        },

        onDrawClicked: function() {
            mapInterface.toggleDrawMode();
        },

        toggleEdit: function() {
            this.$editButton.attr("selected") ? this.$editButton.removeAttr("selected") : this.$editButton.attr("selected", "selected");
            mapInterface.toggleEditMode();
        },        

        exportGrid: function() {
            var a = document.createElement("a");
            var content = "data:text/csv;charset=utf-8,";
            // write nRows and nCols
            content += mapInterface.numRows + ", " + mapInterface.numCols + "\n";
            // write row size and col size
            content += mapInterface.getCellLength() + ", " + mapInterface.getCellWidth() + "\n";
            // write grid
            content += mapInterface.cemGrid.map(e => e.join(",")).join("\n");
            a.href = encodeURI(content);
            a.download = "grid.txt";
            a.click();
        },

        /*****************
         * input listeners 
         *****************/
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

        /*********
         * setters
         *********/        
        setAllValues: function() {
            this.setRotation();
            this.$numRows.val(mapInterface.numRows);
            this.$numCols.val(mapInterface.numCols);
            this.setCoords();            
            $("input[type=radio][name=source]:eq(" + this.source + ")").attr("checked", true);
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

        /***************
         * map listeners
         ***************/
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
        $upload_button: $(".upload-button"),

        a_val: 50,
        u_val: 50,
        wave_heights: [1.5],
        wave_periods: [10],
        wave_angles: [-1],

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            // init values
            this.setAllValues();
            // attach listeners
            this.$asymmetry.change(() => { this.onAsymmetryChange(); });
            this.$stability.change(() => { this.onStabilityChange(); });
            this.$wave_height.change(() => { this.onWaveHeightChange(); });
            this.$wave_period.change(() => { this.onWavePeriodChange(); });
            this.$upload_button.click(() => { $('#wave-file-input').trigger('click'); });
        },

        readWaveFile: function(csv) {
            try {
                var H = [];
                var T = [];
                var theta = [];
                var rows = csv.split('\n');
                for (var i = 0; i < rows.length; i++) {
                    var cols = rows[i].split(',');
                    if (cols[0]) {
                        H.push(parseFloat(cols[0]));
                        T.push(parseFloat(cols[1]));
                        theta.push(parseFloat(cols[2]));
                    }
                }
                this.wave_heights = H;
                this.wave_periods = T;
                this.wave_angles = theta;

                this.disableWaveInput();
                this.a_val = -1;
                this.u_val = -1;
            } catch (e)
            {
                return 1;
            }
        },

        /***********
         * listeners
         ***********/
        onAsymmetryChange: function() {
            this.a_val = this.$asymmetry.val();
        },

        onStabilityChange: function() {
            this.u_val = this.$stability.val();
        },

        onWaveHeightChange: function() {
            this.wave_heights[0] = parseFloat(this.$wave_height.val());
        },

        onWavePeriodChange: function() {
            this.wave_periods[0] = parseFloat(this.$wave_period.val());
        },

        /*********
         * setters
         *********/
        setAllValues: function() {
            if (this.a_val && this.u_val) {
                this.$asymmetry.val(this.a_val);
                this.$stability.val(this.u_val);
                this.$wave_height.val(this.wave_heights[0]);
                this.$wave_period.val(this.wave_periods[0]);
            }
            else {
                this.disableWaveInput();
            }
        },

        disableWaveInput: function() {
            this.$asymmetry.disable();
            this.$stability.disable();
            this.$wave_height.disable();
            this.$wave_period.disable();
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
        $cross_shore_ref: $("input[name=cross-shore-reference]"),
        $ref_depth: $("input[name=shelf-depth-at-ref]"),
        $min_closure_depth: $("input[name=min-shelf-depth]"),
        $start_year: $("input[name=start-date]"),
        $end_year: $("input[name=end-date]"),
        $length_timestep: $("input[name=timestep]"),
        $save_interval: $("input[name=save-interval]"),

        shelf_slope: .001,
        shoreface_slope: .01,
        cross_shore_ref: 10.0,
        ref_depth: 10.0,
        min_closure_depth: 10.0,
        start_year: 1985,
        end_year: new Date().getFullYear() - 1,
        length_timestep: 1,
        save_interval: 365,
        num_timesteps: null,

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            // init values
            this.setAllValues();
            // attach listeners
            this.$shelf_slope.change(() => { this.onShelfSlopeChange(); });
            this.$shoreface_slope.change(() => { this.onShorefaceSlopeChange(); });
            this.$cross_shore_ref.change(() => { this.onCrossShoreRefChange(); });
            this.$ref_depth.change(() => { this.onRefDepthChange(); });
            this.$min_closure_depth.change(() => { this.onMinClosureDepthChange(); });
            this.$end_year.change(() => { this.onEndYearChange(); });
            this.$length_timestep.change(() => { this.onTimestepLengthChange(); });
            this.$save_interval.change(() => { this.onSaveIntervalChange(); });
        },

        getNumTimesteps: function() {
            var startDate = new Date(this.start_year + "-01-01");
            var endDate = new Date(this.end_year + "-12-31");
            var millis = endDate.getTime() - startDate.getTime();
            var days = millis / (1000 * 60 * 60 * 24);
            this.num_timesteps = Math.floor(days / this.length_timestep);
            if (runTab) { runTab.displayNumTimesteps(); }
        },

        /***********
         * listeners
         ***********/
        onShelfSlopeChange: function() {
            this.shelf_slope = this.$shelf_slope.val();
        },

        onShorefaceSlopeChange: function() {
            this.shoreface_slope = this.$shoreface_slope.val();
        },
        
        onCrossShoreRefChange: function() {
            this.cross_shore_ref = this.$cross_shore_ref.val();
        },
        
        onRefDepthChange: function() {
            this.ref_depth = this.$ref_depth.val();
        },
        
        onMinClosureDepthChange: function() {
            this.min_closure_depth = this.$min_closure_depth.val();
        },

        onStartYearChange: function() {
            this.start_year = this.$start_year.val();
            this.getNumTimesteps();
            runTab.displayTimestep(0);
        },

        onEndYearChange: function() {
            this.end_year = this.$end_year.val();
            this.getNumTimesteps();
            runTab.displayTimestep(0);
        },

        onTimestepLengthChange: function() {
            this.length_timestep = this.$length_timestep.val();
            this.getNumTimesteps();
            runTab.displayTimestep(0);
        },

        onSaveIntervalChange: function() {
            this.save_interval = this.$save_interval.val();
        },

        /*********
         * setters
         *********/
        setAllValues: function() {
            this.getNumTimesteps();     
            this.$shelf_slope.val(this.shelf_slope);
            this.$shoreface_slope.val(this.shoreface_slope);
            this.$cross_shore_ref.val(this.cross_shore_ref);
            this.$ref_depth.val(this.ref_depth);
            this.$min_closure_depth.val(this.min_closure_depth);
            this.$start_year.val(this.start_year);
            this.$end_year.val(this.end_year);
            this.$length_timestep.val(this.length_timestep);
            this.$save_interval.val(this.save_interval);  
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
        
        $output: $(".output-table > tbody"),
        $timestep: $(".timestep"),
        $runButton: $(".run-button"),
        $outputButton: $(".output-button"),

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            this.$runButton.click(() => { onRun(); });

            this.$runButton.disable();
            this.$outputButton.disable();

            this.displayTimestep(0);
            this.displayNumTimesteps();
        },

        /***********
         * callbacks
         ***********/
        updateOutput: function(msg) {
            // var text = this.$output.html();
            // var rotation = msg.sp_pca.rotation;
            // var scale = msg.sp_pca.scale;
            // var S = msg.t_pca;

            // var $trow = $("<tr></tr>");
            // $trow.append($("<td></td>").text(msg.time));
            // $trow.append($("<td></td>").text(rotation.toFixed(3)));
            // $trow.append($("<td></td>").text(scale.toFixed(3)));

            // if (S.length >= 3) {
            //     $trow.append($("<td></td>").text(S[0].toFixed(3)))
            //     .append($("<td></td>").text(S[1].toFixed(3)))
            //     .append($("<td></td>").text(S[2].toFixed(3)));
            // } else {
            //     $trow.append($("<td></td>").text("---"))
            //     .append($("<td></td>").text("---"))
            //     .append($("<td></td>").text("---"));
            // }

            // $trow.appendTo(this.$output);
        },

        clearOutput: function() {
            this.$output.empty();
        },

        displayTimestep: function(t) {
            $(this.$timestep.find(".current-step")[0]).text("t = " + t);
        },

        displayNumTimesteps: function() {
            $(this.$timestep.find(".num-timesteps")[0]).text(controlTab.num_timesteps);
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
        $cell: $(".cell"),

        feature: null,
        percentFull: 0,

        init: function() {
            // set up listeners
            this.$fillInput.change(() => {
                this.setFill();
                this.display();
            });
            this.$modal.find(".modal-ok-button").click(() => { this.onOkClicked(); } );
            this.$modal.find(".modal-cancel-button").click(() => { this.onCancelClicked() });
        },

        open: function(feature) {
            this.feature = feature;
            this.$fillInput.val(feature.get("fill")*100);
            this.setFill();
            this.display();
            this.$modal.show();
        },

        setFill: function() {
            this.percentFull = this.$fillInput.val();
        },

        display: function() {
            this.$cell.empty();
            var $cellFull = $("<div class='cell-full'></div>");
            var $cellEmpty = $("<div class='cell-empty'></div>");

            var styleFull = {
                "height": "" + this.percentFull + "%",
                "width": "100%"
            }
            var styleEmpty = {
                "height": "" + (100-this.percentFull) + "%",
                "width": "100%"
            }


            $cellEmpty.css(styleEmpty).css({"float":"left"}).appendTo(this.$cell);
            $cellFull.css(styleFull).css({"float":"right"}).appendTo(this.$cell);
        },

        save: function() {
            var i = this.feature.get('id');
            mapInterface.cemGrid[i] = this.percentFull/100;
            mapInterface.updateFeature(this.feature, this.percentFull/100);
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

