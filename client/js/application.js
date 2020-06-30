/******************
 * Global variables
 ******************/

// config vars
var configJSON;
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
var $errorMessage;
var $errorContainer;

function loadApp() {
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
    $errorContainer = $("#error-container");
    $errorContainer.hide();
    $errorMessage = $("#error-message");
    $("#close").click(() => { hideErrorMessage(); })

    // open grid tab
    onTabChange(gridTab);

    // set up mode inputs
    mode = 1;
    $('input[type=radio][name=mode]:first').attr("checked", true);
    $('input[type=radio][name=mode]').change(function(){ mode = parseInt(this.value); });
}

function showErrorMessage(msg) {
    $errorMessage.text(msg);
    $errorContainer.show();
}

function hideErrorMessage() {
    $errorContainer.hide();
}

/*******************
 *  Event listeners 
 *******************/

function onTabChange(newTab) {
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
    runTab.clearOutput();
    disableAll();
    // create payload
    var input_data = {
        grid: mapInterface.cemGrid,
        asymmetry: parseFloat(this.waveTab.a_val),
        stability: parseFloat(this.waveTab.u_val),
        waveHeights: this.waveTab.wave_heights,
        wavePeriods: this.waveTab.wave_periods,
        waveAngles: this.waveTab.wave_angles,
        shelfSlope: parseFloat(this.controlTab.shelf_slope),
        shorefaceSlope: parseFloat(this.controlTab.shoreface_slope),
        depthOfClosure: parseFloat(this.controlTab.closure_depth),
        numTimesteps: controlTab.num_timesteps,
        lengthTimestep: parseFloat(this.controlTab.length_timestep),
        saveInterval: parseInt(this.controlTab.save_interval),
        mode: mode
    };
    // validate payload
    if (validateInputData(input_data) == 0) {
        // send request
        $.post('/initialize', {
            type: "json",
            input_data: JSON.stringify(input_data)
        }).done((resp) => {
            onUpdate(0); 
        }).fail((err) => {
            showErrorMessage(JSON.parse(err.responseText).message);
        });
    } else {
        showErrorMessage("One or more inputs are invalid.");
    }
}

function onUpdate(timestep) {
    $.get('/update/' + timestep).done((resp) => {
        try {
            resp = JSON.parse(resp);
            if (resp.status != 200) {
                throw(new Error(resp.message))
            }
        } catch (err){
            showErrorMessage(err.message);
            return;
        }
        new_time = resp.timestep;
        runTab.displayTimestep(new_time);
        if (resp.grid.length > 0) {
            mapInterface.updateDisplay(resp.grid);
        }
        if (resp.shoreline.length > 0) {
            mapInterface.displayShoreline(resp.shoreline, 'red', true);
        }
        runTab.updateOutput(resp.results, new_time);
        if (new_time < controlTab.num_timesteps) {
            onUpdate(new_time);
        } else {
            onModelComplete();
        }
    }).fail((err) => { showErrorMessage(JSON.parse(err.responseText).message); });
}

function onModelComplete() {
    $.get('/finalize').done((resp) => {
        try {
            resp = JSON.parse(resp);
            if (resp.status != 200) {
                throw(new Error(resp.message))
            }
        } catch (err){
            showErrorMessage(err.message);
            return;
        }

        if (resp.ref_shoreline.length > 0) {
            mapInterface.displayShoreline(resp.ref_shoreline, 'black', false);
        }
        if (resp.final_shoreline.length > 0) {
            mapInterface.displayShoreline(resp.final_shoreline, 'blue', false);
        }
        if (resp.im_url.length > 0) {
            mapInterface.displayPhoto(resp.im_url);
        }

        enableAll();
    }).fail((err) => { showErrorMessage(JSON.parse(err.responseText).message); });
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
    gridTab.$start_year.disable();

    // wave tab
    waveTab.$asymmetry.disable();
    waveTab.$stability.disable();
    waveTab.$wave_height.disable();
    waveTab.$wave_period.disable();

    // conds tab
    controlTab.$shelf_slope.disable();
    controlTab.$shoreface_slope.disable();
    controlTab.$end_year.disable();
    controlTab.$length_timestep.disable();
    controlTab.$save_interval.disable();
    controlTab.$closure_depth.disable();

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
    gridTab.$start_year.enable();

    // wave tab
    waveTab.$asymmetry.enable();
    waveTab.$stability.enable();
    waveTab.$wave_height.enable();
    waveTab.$wave_period.enable();

    // conds tab
    controlTab.$shelf_slope.enable();
    controlTab.$shoreface_slope.enable();
    controlTab.$end_year.enable();
    controlTab.$length_timestep.enable();
    controlTab.$save_interval.enable();
    controlTab.$closure_depth.enable();

    // run tab
    runTab.$runButton.enable();
    runTab.$outputButton.enable();
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
        source: gridTab.source,
        startYear: gridTab.start_year,
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
    if (configJSON.viewSettings) {
        if (configJSON.viewSettings.center) { mapInterface.map.getView().setCenter(configJSON.viewSettings.center); }
        if (configJSON.viewSettings.zoom) { mapInterface.map.getView().setZoom(configJSON.viewSettings.zoom); }
    }

    // load grid inputs
    var gridConfig = configJSON.gridConfig;
    if (gridConfig) {
        if (gridConfig.numRows) { mapInterface.setNumRows(gridConfig.numRows); }
        if (gridConfig.numCols) { mapInterface.setNumCols(gridConfig.numCols); }
        if (gridConfig.rotation) { mapInterface.map.getView().setRotation(gridConfig.rotation); }
        gridTab.source = gridConfig.source ? gridConfig.source : gridTab.source;
        gridTab.start_year = gridConfig.startYear ? gridConfig.startYear : gridTab.start_year;
        if (gridConfig.points) {
            var points = gridConfig.points;
            mapInterface.updateBox(points[0][0], points[0][1], points[1][0], points[1][1]);
        }
        gridTab.setAllValues();
    }

    // load wave inpts
    var waveConfig = configJSON.waveConfig;
    if (waveConfig) {
        waveTab.a_val = waveConfig.asymmetry;
        waveTab.u_val = waveConfig.stability;
        waveTab.wave_heights = waveConfig.waveHeights;
        waveTab.wave_periods = waveConfig.wavePeriods;
        waveTab.wave_angles = waveConfig.waveAngles;
        waveTab.setAllValues();
    }

    // load config inputs
    var controlConfig = configJSON.controlConfig;
    if (controlConfig) {
        controlTab.shelf_slope = controlConfig.shelfSlope ? controlConfig.shelfSlope : controlTab.shelf_slope;
        controlTab.shoreface_slope = controlConfig.shorefaceSlope ? controlConfig.shorefaceSlope : controlTab.shoreface_slope;
        controlTab.cross_shore_ref = controlConfig.crossShoreRef ? controlConfig.crossShoreRef : controlTab.cross_shore_ref;
        controlTab.ref_depth = controlConfig.refDepth ? controlConfig.refDepth : controlTab.ref_depth;
        controlTab.min_closure_depth = controlConfig.minClosureDepth ? controlConfig.minClosureDepth : controlTab.min_closure_depth;
        controlTab.end_year = controlConfig.endYear ? controlConfig.endYear : controlTab.end_year;
        controlTab.length_timestep = controlConfig.lengthTimestep ? controlConfig.lengthTimestep : controlTab.length_timestep;
        controlTab.save_interval = controlConfig.saveInterval ? controlConfig.saveInterval : controlTab.save_interval;
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