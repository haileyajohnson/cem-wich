
/**
 * Global variables
 */
var mapInterface;
var modelInterface;
const CLIENT_ID = '762501139172-rjf0ia3vv9edu6gg0m46aoij519khuk7.apps.googleusercontent.com';
var $selectedTab;

function Tab(name, id, loadFunc) {
    return {
        name: name,
        id: id,
        loadFunc: loadFunc
    };
}


function loadApp() {
    var onImmediateFailed = function () {
        $('.g-sign-in').removeClass('hidden');
        $('.output').text('(Log in to see the result.)');
        $('.g-sign-in .button').click(function () {
            ee.data.authenticateViaPopup(function () {
                // If the login succeeds, hide the login button and run the analysis.
                $('.g-sign-in').addClass('hidden');
                initialize();
            });
        });
    };



    // Attempt to authenticate using existing credentials.
    ee.data.authenticate(CLIENT_ID, initialize, null, null, onImmediateFailed);
}

function initialize() {
    mapInterface = MapInterface();
    mapInterface.initMap();
}

/**
 * Button listeners
 */

function onGoClicked() {
     mapInterface.mapTransform();
}

function onDrawClicked() {
    mapInterface.toggleDrawMode();
}