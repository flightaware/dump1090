// -*- mode: javascript; indent-tabs-mode: nil; c-basic-offset: 8 -*-
"use strict";

// Define our global variables
var OLMap         = null;
var StaticFeatures = new ol.Collection();
var SiteCircleFeatures = new ol.Collection();
var PlaneIconFeatures = new ol.Collection();
var PlaneTrailFeatures = new ol.Collection();
var Planes        = {};
var PlanesOrdered = [];
var PlaneFilter   = {};
var SelectedPlane = null;
var SelectedAllPlanes = false;
var HighlightedPlane = null;
var FollowSelected = false;
var infoBoxOriginalPosition = {};
var customAltitudeColors = true;

var SpecialSquawks = {
        '7500' : { cssClass: 'squawk7500', markerColor: 'rgb(255, 85, 85)', text: 'Aircraft Hijacking' },
        '7600' : { cssClass: 'squawk7600', markerColor: 'rgb(0, 255, 255)', text: 'Radio Failure' },
        '7700' : { cssClass: 'squawk7700', markerColor: 'rgb(255, 255, 0)', text: 'General Emergency' }
};

// Get current map settings
var CenterLat, CenterLon, ZoomLvl, MapType, SiteCirclesCount, SiteCirclesBaseDistance, SiteCirclesInterval;

var Dump1090Version = "unknown version";
var RefreshInterval = 1000;

var PlaneRowTemplate = null;

var TrackedAircraft = 0;
var TrackedAircraftPositions = 0;
var TrackedHistorySize = 0;

var SitePosition = null;

var ReceiverClock = null;

var LastReceiverTimestamp = 0;
var StaleReceiverCount = 0;
var FetchPending = null;

var MessageCountHistory = [];
var MessageRate = 0;

var NBSP='\u00a0';

var layers;
var layerGroup;

// piaware vs flightfeeder
var isFlightFeeder = false;

var checkbox_div_map = new Map ([
        ['#icao_col_checkbox', '#icao'],
        ['#flag_col_checkbox', '#flag'],
        ['#ident_col_checkbox', '#flight'],
        ['#reg_col_checkbox', '#registration'],
        ['#ac_col_checkbox', '#aircraft_type'],
        ['#squawk_col_checkbox', '#squawk'],
        ['#alt_col_checkbox', '#altitude'],
        ['#speed_col_checkbox', '#speed'],
        ['#vrate_col_checkbox', '#vert_rate'],
        ['#distance_col_checkbox', '#distance'],
        ['#heading_col_checkbox', '#track'],
        ['#messages_col_checkbox', '#msgs'],
        ['#msg_age_col_checkbox', '#seen'],
        ['#rssi_col_checkbox', '#rssi'],
        ['#lat_col_checkbox', '#lat'],
        ['#lon_col_checkbox', '#lon'],
        ['#datasource_col_checkbox', '#data_source'],
        ['#airframes_col_checkbox', '#airframes_mode_s_link'],
        ['#fa_modes_link_checkbox', '#flightaware_mode_s_link'],
        ['#fa_photo_link_checkbox', '#flightaware_photo_link'],

]);

function processReceiverUpdate(data) {
	// Loop through all the planes in the data packet
        var now = data.now;
        var acs = data.aircraft;

        // Detect stats reset
        if (MessageCountHistory.length > 0 && MessageCountHistory[MessageCountHistory.length-1].messages > data.messages) {
                MessageCountHistory = [{'time' : MessageCountHistory[MessageCountHistory.length-1].time,
                                        'messages' : 0}];
        }

        // Note the message count in the history
        MessageCountHistory.push({ 'time' : now, 'messages' : data.messages});
        // .. and clean up any old values
        if ((now - MessageCountHistory[0].time) > 30)
                MessageCountHistory.shift();

	for (var j=0; j < acs.length; j++) {
                var ac = acs[j];
                var hex = ac.hex;
                var squawk = ac.squawk;
                var plane = null;

		// Do we already have this plane object in Planes?
		// If not make it.

		if (Planes[hex]) {
			plane = Planes[hex];
		} else {
			plane = new PlaneObject(hex);
                        plane.filter = PlaneFilter;
                        plane.tr = PlaneRowTemplate.cloneNode(true);

                        if (hex[0] === '~') {
                                // Non-ICAO address
                                plane.tr.cells[0].textContent = hex.substring(1);
                                $(plane.tr).css('font-style', 'italic');
                        } else {
                                plane.tr.cells[0].textContent = hex;
                        }

                        // set flag image if available
                        if (ShowFlags && plane.icaorange.flag_image !== null) {
                                $('img', plane.tr.cells[1]).attr('src', FlagPath + plane.icaorange.flag_image);
                                $('img', plane.tr.cells[1]).attr('title', plane.icaorange.country);
                        } else {
                                $('img', plane.tr.cells[1]).css('display', 'none');
                        }

                        plane.tr.addEventListener('click', function(h, evt) {
                                if (evt.srcElement instanceof HTMLAnchorElement) {
                                        evt.stopPropagation();
                                        return;
                                }

                                if (!$("#map_container").is(":visible")) {
                                        showMap();
                                }
                                selectPlaneByHex(h, false);
                                adjustSelectedInfoBlockPosition();
                                evt.preventDefault();
                        }.bind(undefined, hex));

                        plane.tr.addEventListener('dblclick', function(h, evt) {
                                if (!$("#map_container").is(":visible")) {
                                        showMap();
                                }
                                selectPlaneByHex(h, true);
                                adjustSelectedInfoBlockPosition();
                                evt.preventDefault();
                        }.bind(undefined, hex));

                        Planes[hex] = plane;
                        PlanesOrdered.push(plane);
		}

		// Call the function update
		plane.updateData(now, ac);
	}
}

function fetchData() {
        if (FetchPending !== null && FetchPending.state() == 'pending') {
                // don't double up on fetches, let the last one resolve
                return;
        }

	FetchPending = $.ajax({ url: 'data/aircraft.json',
                                timeout: 5000,
                                cache: false,
                                dataType: 'json' });
        FetchPending.done(function(data) {
                var now = data.now;

                processReceiverUpdate(data);

                // update timestamps, visibility, history track for all planes - not only those updated
                for (var i = 0; i < PlanesOrdered.length; ++i) {
                        var plane = PlanesOrdered[i];
                        plane.updateTick(now, LastReceiverTimestamp);
                }
                
		selectNewPlanes();
		refreshTableInfo();
		refreshSelected();
		refreshHighlighted();
                
                if (ReceiverClock) {
                        var rcv = new Date(now * 1000);
                        ReceiverClock.render(rcv.getUTCHours(),rcv.getUTCMinutes(),rcv.getUTCSeconds());
                }

                // Check for stale receiver data
                if (LastReceiverTimestamp === now) {
                        StaleReceiverCount++;
                        if (StaleReceiverCount > 5) {
                                $("#update_error_detail").text("The data from dump1090 hasn't been updated in a while. Maybe dump1090 is no longer running?");
                                $("#update_error").css('display','block');
                        }
                } else { 
                        StaleReceiverCount = 0;
                        LastReceiverTimestamp = now;
                        $("#update_error").css('display','none');
                }
	});

        FetchPending.fail(function(jqxhr, status, error) {
                $("#update_error_detail").text("AJAX call failed (" + status + (error ? (": " + error) : "") + "). Maybe dump1090 is no longer running?");
                $("#update_error").css('display','block');
        });
}

var PositionHistorySize = 0;
function initialize() {
        // Set page basics
        document.title = PageName;

        flightFeederCheck();

        PlaneRowTemplate = document.getElementById("plane_row_template");

        refreshClock();

        $("#loader").removeClass("hidden");

        if (ExtendedData || window.location.hash == '#extended') {
                $("#extendedData").removeClass("hidden");
        }

        // Set up map/sidebar splitter
		$("#sidebar_container").resizable({
			handles: {
				w: '#splitter'
			},
			minWidth: 350
		});

		// Set up datablock splitter
		$('#selected_infoblock').resizable({
			handles: {
				s: '#splitter-infoblock'
			},
			containment: "#sidebar_container",
			minHeight: 50
		});

		$('#close-button').on('click', function() {
			if (SelectedPlane !== null) {
				var selectedPlane = Planes[SelectedPlane];
				SelectedPlane = null;
				selectedPlane.selected = null;
				selectedPlane.clearLines();
				selectedPlane.updateMarker();         
				refreshSelected();
				refreshHighlighted();
				$('#selected_infoblock').hide();
			}
		});

		// this is a little hacky, but the best, most consitent way of doing this. change the margin bottom of the table container to the height of the overlay
		$('#selected_infoblock').on('resize', function() {
			$('#sidebar_canvas').css('margin-bottom', $('#selected_infoblock').height() + 'px');
		});
		// look at the window resize to resize the pop-up infoblock so it doesn't float off the bottom or go off the top
		$(window).on('resize', function() {
			var topCalc = ($(window).height() - $('#selected_infoblock').height() - 60);
			// check if the top will be less than zero, which will be overlapping/off the screen, and set the top correctly. 
			if (topCalc < 0) {
				topCalc = 0;
				$('#selected_infoblock').css('height', ($(window).height() - 60) +'px');
			}
			$('#selected_infoblock').css('top', topCalc + 'px');
		});

		// to make the infoblock responsive 
		$('#sidebar_container').on('resize', function() {
			if ($('#sidebar_container').width() < 500) {
				$('#selected_infoblock').addClass('infoblock-container-small');
			} else {
				$('#selected_infoblock').removeClass('infoblock-container-small');
			}
		});
	
        // Set up event handlers for buttons
        $("#toggle_sidebar_button").click(toggleSidebarVisibility);
        $("#expand_sidebar_button").click(expandSidebar);
        $("#show_map_button").click(showMap);

        // Set initial element visibility
        $("#show_map_button").hide();
        $("#range_ring_column").hide();
        setColumnVisibility();

        // Initialize other controls
        initializeUnitsSelector();

        // Set up altitude filter button event handlers and validation options
        $("#altitude_filter_form").submit(onFilterByAltitude);
        $("#altitude_filter_form").validate({
            errorPlacement: function(error, element) {
                return true;
            },
            
            rules: {
                minAltitude: {
                    number: true,
                    min: -99999,
                    max: 99999
                },
                maxAltitude: {
                    number: true,
                    min: -99999,
                    max: 99999
                }
            }
        });

        // check if the altitude color values are default to enable the altitude filter
        if (ColorByAlt.air.h.length === 3 && ColorByAlt.air.h[0].alt === 2000 && ColorByAlt.air.h[0].val === 20 && ColorByAlt.air.h[1].alt === 10000 && ColorByAlt.air.h[1].val === 140 && ColorByAlt.air.h[2].alt === 40000 && ColorByAlt.air.h[2].val === 300) {
            customAltitudeColors = false;
        }


        $("#altitude_filter_reset_button").click(onResetAltitudeFilter);

        $('#settingsCog').on('click', function() {
        	$('#settings_infoblock').toggle();
        });

        $('#column_select').on('click', function() {
                $('#column_select_window').toggle();
        });

        $('#column_select_close_box').on('click', function() {
                $('#column_select_window').hide();
        });

        $('#settings_close').on('click', function() {
            $('#settings_infoblock').hide();
        });

        $('#groundvehicle_filter').on('click', function() {
        	filterGroundVehicles(true);
        	refreshSelected();
        	refreshHighlighted();
        	refreshTableInfo();
        });

        $('#blockedmlat_filter').on('click', function() {
        	filterBlockedMLAT(true);
        	refreshSelected();
        	refreshHighlighted();
        	refreshTableInfo();
        });

        $('#grouptype_checkbox').on('click', function() {
		toggleGroupByDataType(true);
	});

        $('#altitude_checkbox').on('click', function() {
        	toggleAltitudeChart(true);
        });

        $('#selectall_checkbox').on('click', function() {
		toggleAllPlanes(true);
        })

        $('#select_all_column_checkbox').on('click', function() {
                toggleAllColumns(true);
        })

        // Event handlers for to column checkboxes
        checkbox_div_map.forEach(function (checkbox, div) {
                $(div).on('click', function() {
                        toggleColumn(checkbox, div, true);
                });
        });

        // Force map to redraw if sidebar container is resized - use a timer to debounce
        var mapResizeTimeout;
        $("#sidebar_container").on("resize", function() {
            clearTimeout(mapResizeTimeout);
            mapResizeTimeout = setTimeout(updateMapSize, 10);
        });

        filterGroundVehicles(false);
        filterBlockedMLAT(false);
        toggleAltitudeChart(false);
        toggleAllPlanes(false);
        toggleGroupByDataType(false);
        toggleAllColumns(false);

        // Get receiver metadata, reconfigure using it, then continue
        // with initialization
        $.ajax({ url: 'data/receiver.json',
                 timeout: 5000,
                 cache: false,
                 dataType: 'json' })

                .done(function(data) {
                        if (typeof data.lat !== "undefined") {
                                SiteShow = true;
                                SiteLat = data.lat;
                                SiteLon = data.lon;
                                DefaultCenterLat = data.lat;
                                DefaultCenterLon = data.lon;
                        }
                        
                        Dump1090Version = data.version;
                        RefreshInterval = data.refresh;
                        PositionHistorySize = data.history;
                })

                .always(function() {
                        initialize_map();
                        start_load_history();
                });
}

var CurrentHistoryFetch = null;
var PositionHistoryBuffer = [];
var HistoryItemsReturned = 0;
function start_load_history() {
	let url = new URL(window.location.href);
	let params = new URLSearchParams(url.search);
	if (PositionHistorySize > 0 && params.get('nohistory') !== 'true') {
		$("#loader_progress").attr('max',PositionHistorySize);
		console.log("Starting to load history (" + PositionHistorySize + " items)");
		//Load history items in parallel
		for (var i = 0; i < PositionHistorySize; i++) {
			load_history_item(i);
		}
	} else {
		// Nothing to load
		end_load_history();
	}
}

function load_history_item(i) {
        console.log("Loading history #" + i);
        $("#loader_progress").attr('value',i);

        $.ajax({ url: 'data/history_' + i + '.json',
                 timeout: 5000,
                 cache: false,
                 dataType: 'json' })

                .done(function(data) {
					PositionHistoryBuffer.push(data);
					HistoryItemsReturned++;
					$("#loader_progress").attr('value',HistoryItemsReturned);
					if (HistoryItemsReturned == PositionHistorySize) {
						end_load_history();
					}
                })

                .fail(function(jqxhr, status, error) {
					//Doesn't matter if it failed, we'll just be missing a data point
					HistoryItemsReturned++;
					if (HistoryItemsReturned == PositionHistorySize) {
						end_load_history();
					}
                });
}

function end_load_history() {
        $("#loader").addClass("hidden");

        console.log("Done loading history");

        if (PositionHistoryBuffer.length > 0) {
                var now, last=0;

                // Sort history by timestamp
                console.log("Sorting history");
                PositionHistoryBuffer.sort(function(x,y) { return (x.now - y.now); });

                // Process history
                for (var h = 0; h < PositionHistoryBuffer.length; ++h) {
                        now = PositionHistoryBuffer[h].now;
                        console.log("Applying history " + (h + 1) + "/" + PositionHistoryBuffer.length + " at: " + now);
                        processReceiverUpdate(PositionHistoryBuffer[h]);

                        // update track
                        console.log("Updating tracks at: " + now);
                        for (var i = 0; i < PlanesOrdered.length; ++i) {
                                var plane = PlanesOrdered[i];
                                plane.updateTrack(now, last);
                        }

                        last = now;
                }

                // Final pass to update all planes to their latest state
                console.log("Final history cleanup pass");
                for (var i = 0; i < PlanesOrdered.length; ++i) {
                        var plane = PlanesOrdered[i];
                        plane.updateTick(now);
                }

                LastReceiverTimestamp = last;
        }

        PositionHistoryBuffer = null;

        console.log("Completing init");

        refreshTableInfo();
        refreshSelected();
        refreshHighlighted();
        reaper();

        // Setup our timer to poll from the server.
        window.setInterval(fetchData, RefreshInterval);
        window.setInterval(reaper, 60000);

        // And kick off one refresh immediately.
        fetchData();

        // update the display layout from any URL query strings
        applyUrlQueryStrings();
}

// Function to apply any URL query value to the map before we start
function applyUrlQueryStrings() {
    // if asked, toggle featrues at start
    let url = new URL(window.location.href);
    let params = new URLSearchParams(url.search);

    // be sure we start with a 'clean' layout, but only if we need it
    var allOptions = [
        'banner',
        'altitudeChart',
        'aircraftTrails',
        'map',
        'sidebar',
        'zoomOut',
        'zoomIn',
        'moveNorth',
        'moveSouth',
        'moveWest',
        'moveEast',
        'displayUnits',
        'rangeRings',
        'ringCount',
        'ringBaseDistance',
        'ringInterval'
    ]

    var needReset = false;
    for (var option of allOptions) {
        if (params.has(option)) {
            needReset = true;
            break;
        }
    }
    
    if (needReset) {
        resetMap();
    }

    if (params.get('banner') === 'hide') {
        hideBanner();
    }
    if (params.get('altitudeChart') === 'hide') {
	$('#altitude_checkbox').removeClass('settingsCheckboxChecked');
        $('#altitude_chart').hide();
    }
    if (params.get('altitudeChart') === 'show') {
        $('#altitude_checkbox').addClass('settingsCheckboxChecked');
        $('#altitude_chart').show();
    }
    if (params.get('aircraftTrails') === 'show') {
        selectAllPlanes();
    }
    if (params.get('aircraftTrails') === 'hide') {
        deselectAllPlanes();
    }
    if (params.get('map') === 'show') {
        showMap();
    }
    if (params.get('map') === 'hide') {
        expandSidebar();
    }
    if (params.get('sidebar') === 'show') {
        $("#sidebar_container").show();
        updateMapSize();
    }
    if (params.get('sidebar') === 'hide') {
        $("#sidebar_container").hide();
        updateMapSize();
    }
    if (params.get('zoomOut')) {
        zoomMap(params.get('zoomOut'), true);
    }
    if (params.get('zoomIn')) {
        zoomMap(params.get('zoomIn'), false);
    }
    if (params.get('moveNorth')) {
        moveMap(params.get('moveNorth'), true, false);
    }
    if (params.get('moveSouth')) {
        moveMap(params.get('moveSouth'), true, true);
    }
    if (params.get('moveEast')) {
        moveMap(params.get('moveEast'), false, false);
    }
    if (params.get('moveWest')) {
        moveMap(params.get('moveWest'), false, true);
    }
    if (params.get('displayUnits')) {
        setDisplayUnits(params.get('displayUnits'));
    }
    if (params.get('rangeRings')) {
        setRangeRingVisibility(params.get('rangeRings'));
    }
    if (params.get('ringCount')) {
        setRingCount(params.get('ringCount'));
    }
    if (params.get('ringBaseDistance')) {
        setRingBaseDistance(params.get('ringBaseDistance'));
    }
    if (params.get('ringInterval')) {
        setRingInterval(params.get('ringInterval'));
    }
}

// Make a LineString with 'points'-number points
// that is a closed circle on the sphere such that the
// great circle distance from 'center' to each point is
// 'radius' meters
function make_geodesic_circle(center, radius, points) {
        var angularDistance = radius / 6378137.0;
        var lon1 = center[0] * Math.PI / 180.0;
        var lat1 = center[1] * Math.PI / 180.0;
        var geom;
        for (var i = 0; i <= points; ++i) {
            var bearing = i * 2 * Math.PI / points;

            var lat2 = Math.asin( Math.sin(lat1)*Math.cos(angularDistance) +
                Math.cos(lat1)*Math.sin(angularDistance)*Math.cos(bearing) );
            var lon2 = lon1 + Math.atan2(Math.sin(bearing)*Math.sin(angularDistance)*Math.cos(lat1),
                Math.cos(angularDistance)-Math.sin(lat1)*Math.sin(lat2));

            lat2 = lat2 * 180.0 / Math.PI;
            lon2 = lon2 * 180.0 / Math.PI;
            if (!geom) {
                geom = new ol.geom.LineString([[lon2, lat2]]);
            } else {
                geom.appendCoordinate([lon2, lat2]);
            }
        }
        return geom;
}

// Initalizes the map and starts up our timers to call various functions
function initialize_map() {
        // Load stored map settings if present
        CenterLat = Number(localStorage['CenterLat']) || DefaultCenterLat;
        CenterLon = Number(localStorage['CenterLon']) || DefaultCenterLon;
        ZoomLvl = Number(localStorage['ZoomLvl']) || DefaultZoomLvl;
        MapType = localStorage['MapType'];
        var groupByDataTypeBox = localStorage.getItem('groupByDataType');

        // Set SitePosition, initialize sorting
        if (SiteShow && (typeof SiteLat !==  'undefined') && (typeof SiteLon !==  'undefined')) {
	        SitePosition = [SiteLon, SiteLat];
		if (groupByDataTypeBox === 'deselected') {
			sortByDistance();
		}
        } else {
	        SitePosition = null;
                PlaneRowTemplate.cells[9].style.display = 'none'; // hide distance column
                document.getElementById("distance").style.display = 'none'; // hide distance header
                if (groupByDataTypeBox === 'deselected') {
			sortByAltitude();
		}
        }

        // Maybe hide flag info
        if (!ShowFlags) {
                PlaneRowTemplate.cells[1].style.display = 'none'; // hide flag column
                document.getElementById("flag").style.display = 'none'; // hide flag header
                document.getElementById("infoblock_country").style.display = 'none'; // hide country row
        }

        // Initialize OL3

        layers = createBaseLayers();

        var iconsLayer = new ol.layer.Vector({
                name: 'ac_positions',
                type: 'overlay',
                title: 'Aircraft positions',
                source: new ol.source.Vector({
                        features: PlaneIconFeatures,
                })
        });

        layers.push(new ol.layer.Group({
                title: 'Overlays',
                layers: [
                        new ol.layer.Vector({
                                name: 'site_pos',
                                type: 'overlay',
                                title: 'Site position and range rings',
                                source: new ol.source.Vector({
                                        features: StaticFeatures,
                                })
                        }),

                        new ol.layer.Vector({
                                name: 'ac_trail',
                                type: 'overlay',
                                title: 'Selected aircraft trail',
                                source: new ol.source.Vector({
                                        features: PlaneTrailFeatures,
                                })
                        }),

                        iconsLayer
                ]
        }));

        var foundType = false;
        var baseCount = 0;

        layerGroup = new ol.layer.Group({
                layers: layers
        })

        ol.control.LayerSwitcher.forEachRecursive(layerGroup, function(lyr) {
                if (!lyr.get('name'))
                        return;

                if (lyr.get('type') === 'base') {
                    baseCount++;
                        if (MapType === lyr.get('name')) {
                                foundType = true;
                                lyr.setVisible(true);
                        } else {
                                lyr.setVisible(false);
                        }

                        lyr.on('change:visible', function(evt) {
                                if (evt.target.getVisible()) {
                                        MapType = localStorage['MapType'] = evt.target.get('name');
                                }
                        });
                } else if (lyr.get('type') === 'overlay') {
                        var visible = localStorage['layer_' + lyr.get('name')];
                        if (visible != undefined) {
                                // javascript, why must you taunt me with gratuitous type problems
                                lyr.setVisible(visible === "true");
                        }

                        lyr.on('change:visible', function(evt) {
                                localStorage['layer_' + evt.target.get('name')] = evt.target.getVisible();
                        });
                }
        })

        if (!foundType) {
                ol.control.LayerSwitcher.forEachRecursive(layerGroup, function(lyr) {
                        if (foundType)
                                return;
                        if (lyr.get('type') === 'base') {
                                lyr.setVisible(true);
                                foundType = true;
                        }
                });
        }

        OLMap = new ol.Map({
                target: 'map_canvas',
                layers: layers,
                view: new ol.View({
                        center: ol.proj.fromLonLat([CenterLon, CenterLat]),
                        zoom: ZoomLvl
                }),
                controls: [new ol.control.Zoom(),
                           new ol.control.Rotate(),
                           new ol.control.Attribution({collapsed: true}),
                           new ol.control.ScaleLine({units: DisplayUnits})
                          ],
                loadTilesWhileAnimating: true,
                loadTilesWhileInteracting: true
        });

        if (baseCount > 1) {
            OLMap.addControl(new ol.control.LayerSwitcher());
        }

	// Listeners for newly created Map
        OLMap.getView().on('change:center', function(event) {
                var center = ol.proj.toLonLat(OLMap.getView().getCenter(), OLMap.getView().getProjection());
                localStorage['CenterLon'] = center[0]
                localStorage['CenterLat'] = center[1]
                if (FollowSelected) {
                        // On manual navigation, disable follow
                        var selected = Planes[SelectedPlane];
						if (typeof selected === 'undefined' ||
							(Math.abs(center[0] - selected.position[0]) > 0.0001 &&
							Math.abs(center[1] - selected.position[1]) > 0.0001)){
                                FollowSelected = false;
                                refreshSelected();
                                refreshHighlighted();
                        }
                }
        });
    
        OLMap.getView().on('change:resolution', function(event) {
                ZoomLvl = localStorage['ZoomLvl']  = OLMap.getView().getZoom();
                for (var plane in Planes) {
                        Planes[plane].updateMarker(false);
                };
        });

        OLMap.on(['click', 'dblclick'], function(evt) {
                var hex = evt.map.forEachFeatureAtPixel(evt.pixel,
                                                        function(feature, layer) {
                                                                return feature.hex;
                                                        },
                                                        {
                                                                layerFilter: function(layer) {
                                                                        return (layer === iconsLayer);
                                                                },
                                                                hitTolerance: 5,
                                                        });
                if (hex) {
                        selectPlaneByHex(hex, (evt.type === 'dblclick'));
                        adjustSelectedInfoBlockPosition();
                        evt.stopPropagation();
                } else {
                        deselectAllPlanes();
                        evt.stopPropagation();
                }
        });


    // show the hover box
    OLMap.on('pointermove', function(evt) {
        var hex = evt.map.forEachFeatureAtPixel(evt.pixel,
            function(feature, layer) {
                    return feature.hex;
            },
            {
                layerFilter: function(layer) {
                        return (layer === iconsLayer);
                },
                hitTolerance: 5,
            }
        );

        if (hex) {
            highlightPlaneByHex(hex);
        } else {
            removeHighlight();
        }

    })

    // handle the layer settings pane checkboxes
	OLMap.once('postrender', function(e) {
		toggleLayer('#nexrad_checkbox', 'nexrad');
		toggleLayer('#sitepos_checkbox', 'site_pos');
		toggleLayer('#actrail_checkbox', 'ac_trail');
		toggleLayer('#acpositions_checkbox', 'ac_positions');
	});

	// Add home marker if requested
	if (SitePosition) {
                var markerStyle = new ol.style.Style({
                        image: new ol.style.Circle({
                                radius: 7,
                                snapToPixel: false,
                                fill: new ol.style.Fill({color: 'black'}),
                                stroke: new ol.style.Stroke({
                                        color: 'white', width: 2
                                })
                        })
                });

                var feature = new ol.Feature(new ol.geom.Point(ol.proj.fromLonLat(SitePosition)));
                feature.setStyle(markerStyle);
                StaticFeatures.push(feature);

		$('#range_ring_column').show();

                setRangeRings();

                $('#range_rings_button').click(onSetRangeRings);
                $("#range_ring_form").validate({
                    errorPlacement: function(error, element) {
                        return true;
                    },
                    rules: {
                        ringCount: {
                            number: true,
		            min: 0
                        },
                        baseRing: {
                            number: true,
                            min: 0
                        },
                        ringInterval: {
                            number: true,
                            min: 0
                        }
                    }
                });

                if (SiteCircles) {
                    createSiteCircleFeatures();
                }
	}

        // Add terrain-limit rings. To enable this:
        //
        //  create a panorama for your receiver location on heywhatsthat.com
        //
        //  note the "view" value from the URL at the top of the panorama
        //    i.e. the XXXX in http://www.heywhatsthat.com/?view=XXXX
        //
        // fetch a json file from the API for the altitudes you want to see:
        //
        //  wget -O /usr/share/dump1090-mutability/html/upintheair.json \
        //    'http://www.heywhatsthat.com/api/upintheair.json?id=XXXX&refraction=0.25&alts=3048,9144'
        //
        // NB: altitudes are in _meters_, you can specify a list of altitudes

        // kick off an ajax request that will add the rings when it's done
        var request = $.ajax({ url: 'upintheair.json',
                               timeout: 5000,
                               cache: true,
                               dataType: 'json' });
        request.done(function(data) {
                var ringStyle = new ol.style.Style({
                        fill: null,
                        stroke: new ol.style.Stroke({
                                color: '#000000',
                                width: 1
                        })
                });

                for (var i = 0; i < data.rings.length; ++i) {
                        var geom = new ol.geom.LineString();
                        var points = data.rings[i].points;
                        if (points.length > 0) {
                                for (var j = 0; j < points.length; ++j) {
                                        geom.appendCoordinate([ points[j][1], points[j][0] ]);
                                }
                                geom.appendCoordinate([ points[0][1], points[0][0] ]);
                                geom.transform('EPSG:4326', 'EPSG:3857');

                                var feature = new ol.Feature(geom);
                                feature.setStyle(ringStyle);
                                StaticFeatures.push(feature);
                        }
                }
        });

        request.fail(function(jqxhr, status, error) {
                // no rings available, do nothing
        });
}

function createSiteCircleFeatures() {
    // Clear existing circles first
    SiteCircleFeatures.forEach(function(circleFeature) {
       StaticFeatures.remove(circleFeature); 
    });
    SiteCircleFeatures.clear();

    var circleStyle = function(distance) {
    	return new ol.style.Style({
            fill: null,
            stroke: new ol.style.Stroke({
                    color: '#000000',
                    width: 1
            }),
            text: new ol.style.Text({
            	font: '10px Helvetica Neue, sans-serif',
            	fill: new ol.style.Fill({ color: '#000' }),
				offsetY: -8,
				text: format_distance_long(distance, DisplayUnits, 0)

			})
		});
    };

    var conversionFactor = 1000.0;
    if (DisplayUnits === "nautical") {
        conversionFactor = 1852.0;
    } else if (DisplayUnits === "imperial") {
        conversionFactor = 1609.0;
    }

    for (var i=0; i < SiteCirclesCount; ++i) {
	    var distance = (SiteCirclesBaseDistance + (SiteCirclesInterval * i)) * conversionFactor;
            var circle = make_geodesic_circle(SitePosition, distance, 360);
            circle.transform('EPSG:4326', 'EPSG:3857');
            var feature = new ol.Feature(circle);
            feature.setStyle(circleStyle(distance));
            StaticFeatures.push(feature);
            SiteCircleFeatures.push(feature);
    }
}

// This looks for planes to reap out of the master Planes variable
function reaper() {
        //console.log("Reaping started..");

        // Look for planes where we have seen no messages for >300 seconds
        var newPlanes = [];
        for (var i = 0; i < PlanesOrdered.length; ++i) {
                var plane = PlanesOrdered[i];
                if (plane.seen > 300) {
                        // Reap it.                                
                        plane.tr.parentNode.removeChild(plane.tr);
                        plane.tr = null;
                        delete Planes[plane.icao];
                        plane.destroy();
                } else {
                        // Keep it.
                        newPlanes.push(plane);
                }
        };

        PlanesOrdered = newPlanes;
        refreshTableInfo();
        refreshSelected();
        refreshHighlighted();
}

// Page Title update function
function refreshPageTitle() {
        if (!PlaneCountInTitle && !MessageRateInTitle) {
                document.title = PageName;
                return;
        }

        var aircraftCount = "";
        var rate = "";

        if (PlaneCountInTitle) {
                aircraftCount += TrackedAircraft;
        }

        if (MessageRateInTitle && MessageRate) {
                rate += ' - ' + MessageRate.toFixed(1) + ' msg/sec';
        }

        document.title = '(' + aircraftCount + ') ' + PageName + rate;
}

// Refresh the detail window about the plane
function refreshSelected() {
        if (MessageCountHistory.length > 1) {
                var message_time_delta = MessageCountHistory[MessageCountHistory.length-1].time - MessageCountHistory[0].time;
                var message_count_delta = MessageCountHistory[MessageCountHistory.length-1].messages - MessageCountHistory[0].messages;
                if (message_time_delta > 0)
                        MessageRate = message_count_delta / message_time_delta;
        } else {
                MessageRate = null;
        }

	refreshPageTitle();
       
        var selected = false;
	if (typeof SelectedPlane !== 'undefined' && SelectedPlane != "ICAO" && SelectedPlane != null) {
    	        selected = Planes[SelectedPlane];
        }
        
        $('#dump1090_infoblock').css('display','block');
        $('#dump1090_version').text(Dump1090Version);
        $('#dump1090_total_ac').text(TrackedAircraft);
        $('#dump1090_total_ac_positions').text(TrackedAircraftPositions);
        $('#dump1090_total_history').text(TrackedHistorySize);

        if (MessageRate !== null) {
                $('#dump1090_message_rate').text(MessageRate.toFixed(1));
        } else {
                $('#dump1090_message_rate').text("n/a");
        }

        setSelectedInfoBlockVisibility();

        if (!selected) {
                return;
        }
      
        if (selected.flight !== null && selected.flight !== "") {
                $('#selected_callsign').text(selected.flight);
        } else {
                $('#selected_callsign').text('n/a');
        }
        $('#selected_flightaware_link').html(getFlightAwareModeSLink(selected.icao, selected.flight, "Visit Flight Page"));

        if (selected.registration !== null) {
                $('#selected_registration').text(selected.registration);
        } else {
                $('#selected_registration').text("n/a");
        }

        if (selected.icaotype !== null) {
                $('#selected_icaotype').text(selected.icaotype);
        } else {
                $('#selected_icaotype').text("n/a");
        }

        // Not using this logic for the redesigned info panel at the time, but leaving it in  if/when adding it back
        // var emerg = document.getElementById('selected_emergency');
        // if (selected.squawk in SpecialSquawks) {
        //         emerg.className = SpecialSquawks[selected.squawk].cssClass;
        //         emerg.textContent = NBSP + 'Squawking: ' + SpecialSquawks[selected.squawk].text + NBSP ;
        // } else {
        //         emerg.className = 'hidden';
        // }

		$("#selected_altitude").text(format_altitude_long(selected.altitude, selected.vert_rate, DisplayUnits));

		$('#selected_onground').text(format_onground(selected.altitude));

        if (selected.squawk === null || selected.squawk === '0000') {
                $('#selected_squawk').text('n/a');
        } else {
                $('#selected_squawk').text(selected.squawk);
        }
	
		$('#selected_speed').text(format_speed_long(selected.gs, DisplayUnits));
		$('#selected_ias').text(format_speed_long(selected.ias, DisplayUnits));
		$('#selected_tas').text(format_speed_long(selected.tas, DisplayUnits));
		$('#selected_vertical_rate').text(format_vert_rate_long(selected.baro_rate, DisplayUnits));
		$('#selected_vertical_rate_geo').text(format_vert_rate_long(selected.geom_rate, DisplayUnits));
        $('#selected_icao').text(selected.icao.toUpperCase());
        $('#airframes_post_icao').attr('value',selected.icao);
		$('#selected_track').text(format_track_long(selected.track));

        if (selected.seen <= 1) {
                $('#selected_seen').text('now');
        } else {
                $('#selected_seen').text(selected.seen.toFixed(1) + 's');
        }

        if (selected.seen_pos <= 1) {
               $('#selected_seen_pos').text('now');
        } else {
               $('#selected_seen_pos').text(selected.seen_pos.toFixed(1) + 's');
        }

        $('#selected_country').text(selected.icaorange.country);
        if (ShowFlags && selected.icaorange.flag_image !== null) {
                $('#selected_flag').removeClass('hidden');
                $('#selected_flag img').attr('src', FlagPath + selected.icaorange.flag_image);
                $('#selected_flag img').attr('title', selected.icaorange.country);
        } else {
                $('#selected_flag').addClass('hidden');
        }

	if (selected.position === null) {
                $('#selected_position').text('n/a');
                $('#selected_follow').addClass('hidden');
        } else {
                $('#selected_position').text(format_latlng(selected.position));
                $('#position_age').text(selected.seen_pos.toFixed(1) + 's');
                $('#selected_follow').removeClass('hidden');
                if (FollowSelected) {
                        $('#selected_follow').css('font-weight', 'bold');
                        OLMap.getView().setCenter(ol.proj.fromLonLat(selected.position));
                } else {
                        $('#selected_follow').css('font-weight', 'normal');
                }
	}
		if (selected.getDataSource() === "adsb_icao") {
			$('#selected_source').text("ADS-B");
		} else if (selected.getDataSource() === "tisb_trackfile" || selected.getDataSource() === "tisb_icao" || selected.getDataSource() === "tisb_other") {
			$('#selected_source').text("TIS-B");
		} else if (selected.getDataSource() === "mlat") {
			$('#selected_source').text("MLAT");
		} else {
			$('#selected_source').text("Other");
		}
		$('#selected_category').text(selected.category ? selected.category : "n/a");
        $('#selected_sitedist').text(format_distance_long(selected.sitedist, DisplayUnits));
        $('#selected_rssi').text(selected.rssi.toFixed(1) + ' dBFS');
        $('#selected_message_count').text(selected.messages);
		$('#selected_photo_link').html(getFlightAwarePhotoLink(selected.registration));
		
		$('#selected_altitude_geom').text(format_altitude_long(selected.alt_geom, selected.geom_rate, DisplayUnits));
        $('#selected_mag_heading').text(format_track_long(selected.mag_heading));
        $('#selected_true_heading').text(format_track_long(selected.true_heading));
        $('#selected_ias').text(format_speed_long(selected.ias, DisplayUnits));
        $('#selected_tas').text(format_speed_long(selected.tas, DisplayUnits));
        if (selected.mach == null) {
                $('#selected_mach').text('n/a');
        } else {
                $('#selected_mach').text(selected.mach.toFixed(3));
        }
        if (selected.roll == null) {
                $('#selected_roll').text('n/a');
        } else {
                $('#selected_roll').text(selected.roll.toFixed(1));
        }
        if (selected.track_rate == null) {
                $('#selected_trackrate').text('n/a');
        } else {
                $('#selected_trackrate').text(selected.track_rate.toFixed(2));
        }
        $('#selected_geom_rate').text(format_vert_rate_long(selected.geom_rate, DisplayUnits));
        if (selected.nav_qnh == null) {
                $('#selected_nav_qnh').text("n/a");
        } else {
                $('#selected_nav_qnh').text(selected.nav_qnh.toFixed(1) + " hPa");
        }
        $('#selected_nav_altitude').text(format_altitude_long(selected.nav_altitude, 0, DisplayUnits));
        $('#selected_nav_heading').text(format_track_long(selected.nav_heading));
        if (selected.nav_modes == null) {
                $('#selected_nav_modes').text("n/a");
        } else {
                $('#selected_nav_modes').text(selected.nav_modes.join());
		}
		if (selected.nic_baro == null) {
			$('#selected_nic_baro').text("n/a");
		} else {
			if (selected.nic_baro == 1) {
				$('#selected_nic_baro').text("cross-checked");
			} else {
				$('#selected_nic_baro').text("not cross-checked");
			}
		}

		$('#selected_nac_p').text(format_nac_p(selected.nac_p));
		$('#selected_nac_v').text(format_nac_v(selected.nac_v));
		if (selected.rc == null) {
			$('#selected_rc').text("n/a");
		} else if (selected.rc == 0) {
			$('#selected_rc').text("unknown");
		} else {
			$('#selected_rc').text(format_distance_short(selected.rc, DisplayUnits));
		}

		if (selected.sil == null || selected.sil_type == null) {
			$('#selected_sil').text("n/a");
		} else {
			var sampleRate = "";
			var silDesc = "";
			if (selected.sil_type == "perhour") {
				sampleRate = " per flight hour";
			} else if (selected.sil_type == "persample") {
				sampleRate = " per sample";
			}
			
			switch (selected.sil) {
				case 0:
					silDesc = "&gt; 1×10<sup>-3</sup>";
					break;
				case 1:
					silDesc = "≤ 1×10<sup>-3</sup>";
					break;
				case 2:
					silDesc = "≤ 1×10<sup>-5</sup>";
					break;
				case 3:
					silDesc = "≤ 1×10<sup>-7</sup>";
					break;
				default:
					silDesc = "n/a";
					sampleRate = "";
					break;
			}
			$('#selected_sil').html(silDesc + sampleRate);
		}

        if (selected.version == null) {
                $('#selected_version').text('none');
        } else if (selected.version == 0) {
                $('#selected_version').text('v0 (DO-260)');
        } else if (selected.version == 1) {
                $('#selected_version').text('v1 (DO-260A)');
        } else if (selected.version == 2) {
                $('#selected_version').text('v2 (DO-260B)');
        } else {
                $('#selected_version').text('v' + selected.version);
        }

        }

function refreshHighlighted() {
	// this is following nearly identical logic, etc, as the refreshSelected function, but doing less junk for the highlighted pane
	var highlighted = false;

	if (typeof HighlightedPlane !== 'undefined' && HighlightedPlane !== null) {
		highlighted = Planes[HighlightedPlane];
	}

	var infoBox = $('#highlighted_infoblock');

	// no highlighted plane or in process of removing plane
	if (!highlighted || !highlighted.marker) {
		infoBox.fadeOut();
		return;
	}

	var mapCanvas = $('#map_canvas');
	var markerCoordinates = highlighted.marker.getGeometry().getCoordinates();
	var markerPosition = OLMap.getPixelFromCoordinate(markerCoordinates);
	var x = markerPosition[0];
	var y = markerPosition[1];
	if (x < 0 || y < 0 || x > mapCanvas.width() || y > mapCanvas.height()) {
		infoBox.fadeOut();
		return;
	}
	x = x + 20;
	y = y + 60;
	var w = infoBox.outerWidth() + 20;
	var h = infoBox.outerHeight();
	if (x > mapCanvas.width() - w) {
		x -= w + 20;
	}
	if (y > mapCanvas.height() - h) {
		y -= h;
	}
	if (infoBox.css('visibility', 'visible')) {
		infoBox.animate({ left: x, top: y }, 500);
	} else {
		infoBox.css({ left: x, top: y });
	}
	infoBox.fadeIn(100);

	if (highlighted.flight !== null && highlighted.flight !== "") {
		$('#highlighted_callsign').text(highlighted.flight);
	} else {
		$('#highlighted_callsign').text('n/a');
	}

	if (highlighted.icaotype !== null) {
		$('#higlighted_icaotype').text(highlighted.icaotype);
	} else {
		$('#higlighted_icaotype').text("n/a");
	}

	if (highlighted.getDataSource() === "adsb_icao") {
		$('#highlighted_source').text("ADS-B");
	} else if (highlighted.getDataSource() === "tisb_trackfile" || highlighted.getDataSource() === "tisb_icao" || highlighted.getDataSource() === "tisb_other") {
		$('#highlighted_source').text("TIS-B");
	} else if (highlighted.getDataSource() === "mlat") {
		$('#highlighted_source').text("MLAT");
	} else {
		$('#highlighted_source').text("Other");
	}

	if (highlighted.registration !== null) {
		$('#highlighted_registration').text(highlighted.registration);
	} else {
		$('#highlighted_registration').text("n/a");
	}

	$('#highlighted_speed').text(format_speed_long(highlighted.speed, DisplayUnits));

	$("#highlighted_altitude").text(format_altitude_long(highlighted.altitude, highlighted.vert_rate, DisplayUnits));

	$('#highlighted_icao').text(highlighted.icao.toUpperCase());

}

function refreshClock() {
	$('#clock_div').text(new Date().toLocaleString());
	var c = setTimeout(refreshClock, 500);
}

function removeHighlight() {
	HighlightedPlane = null;
	refreshHighlighted();
}

// Refreshes the larger table of all the planes
function refreshTableInfo() {
    var show_squawk_warning = false;

    TrackedAircraft = 0
    TrackedAircraftPositions = 0
    TrackedHistorySize = 0

    $(".altitudeUnit").text(get_unit_label("altitude", DisplayUnits));
    $(".speedUnit").text(get_unit_label("speed", DisplayUnits));
    $(".distanceUnit").text(get_unit_label("distance", DisplayUnits));
    $(".verticalRateUnit").text(get_unit_label("verticalRate", DisplayUnits));

    for (var i = 0; i < PlanesOrdered.length; ++i) {
	var tableplane = PlanesOrdered[i];
    TrackedHistorySize += tableplane.history_size;
	if (tableplane.seen >= 58 || tableplane.isFiltered()) {
        tableplane.tr.className = "plane_table_row hidden";
    } else {
        TrackedAircraft++;
        var classes = "plane_table_row";

        if (tableplane.position !== null && tableplane.seen_pos < 60) {
            ++TrackedAircraftPositions;
		}

		if (tableplane.getDataSource() === "adsb_icao") {
        	classes += " vPosition";
        } else if (tableplane.getDataSource() === "tisb_trackfile" || tableplane.getDataSource() === "tisb_icao" || tableplane.getDataSource() === "tisb_other") {
        	classes += " tisb";
        } else if (tableplane.getDataSource() === "mlat") {
        	classes += " mlat";
        } else {
        	classes += " other";
        }

		if (tableplane.icao == SelectedPlane)
            classes += " selected";
                    
        if (tableplane.squawk in SpecialSquawks) {
            classes = classes + " " + SpecialSquawks[tableplane.squawk].cssClass;
            show_squawk_warning = true;
		}			                

        // ICAO doesn't change
        if (tableplane.flight) {
                tableplane.tr.cells[2].innerHTML = getFlightAwareModeSLink(tableplane.icao, tableplane.flight, tableplane.flight);
        } else {
		// Show _registration if ident is not present
		tableplane.tr.cells[2].innerHTML = (tableplane.registration !== null ? getFlightAwareIdentLink(tableplane.registration, '_' + tableplane.registration) : "");
        }
        tableplane.tr.cells[3].textContent = (tableplane.registration !== null ? tableplane.registration : "");
        tableplane.tr.cells[4].textContent = (tableplane.icaotype !== null ? tableplane.icaotype : "");
        tableplane.tr.cells[5].textContent = (tableplane.squawk !== null ? tableplane.squawk : "");
        tableplane.tr.cells[6].innerHTML = format_altitude_brief(tableplane.altitude, tableplane.vert_rate, DisplayUnits);
        tableplane.tr.cells[7].textContent = format_speed_brief(tableplane.gs, DisplayUnits);
        tableplane.tr.cells[8].textContent = format_vert_rate_brief(tableplane.vert_rate, DisplayUnits);
        tableplane.tr.cells[9].textContent = format_distance_brief(tableplane.sitedist, DisplayUnits);
        tableplane.tr.cells[10].textContent = format_track_brief(tableplane.track);
        tableplane.tr.cells[11].textContent = tableplane.messages;
        tableplane.tr.cells[12].textContent = tableplane.seen.toFixed(0);
        tableplane.tr.cells[13].textContent = (tableplane.rssi !== null ? tableplane.rssi : "");
        tableplane.tr.cells[14].textContent = (tableplane.position !== null ? tableplane.position[1].toFixed(4) : "");
        tableplane.tr.cells[15].textContent = (tableplane.position !== null ? tableplane.position[0].toFixed(4) : "");
        tableplane.tr.cells[16].textContent = format_data_source(tableplane.getDataSource());
        tableplane.tr.cells[17].innerHTML = getAirframesModeSLink(tableplane.icao);
        tableplane.tr.cells[18].innerHTML = getFlightAwareModeSLink(tableplane.icao, tableplane.flight);
        tableplane.tr.cells[19].innerHTML = getFlightAwarePhotoLink(tableplane.registration);
        tableplane.tr.className = classes;
	}
}

if (show_squawk_warning) {
            $("#SpecialSquawkWarning").css('display','block');
    } else {
            $("#SpecialSquawkWarning").css('display','none');
    }

    resortTable();
}

//
// ---- table sorting ----
//

function compareAlpha(xa,ya) {
        if (xa === ya)
                return 0;
        if (xa < ya)
                return -1;
        return 1;
}

function compareNumeric(xf,yf) {
        if (Math.abs(xf - yf) < 1e-9)
                return 0;

        return xf - yf;
}

function sortByICAO()     { sortBy('icao',    compareAlpha,   function(x) { return x.icao; }); }
function sortByFlight()   { sortBy('flight',  compareAlpha,   function(x) { return x.flight ? x.flight : x.registration; }); }
function sortByRegistration()   { sortBy('registration',    compareAlpha,   function(x) { return x.registration; }); }
function sortByAircraftType()   { sortBy('icaotype',        compareAlpha,   function(x) { return x.icaotype; }); }
function sortBySquawk()   { sortBy('squawk',  compareAlpha,   function(x) { return x.squawk; }); }
function sortByAltitude() { sortBy('altitude',compareNumeric, function(x) { return (x.altitude == "ground" ? -1e9 : x.altitude); }); }
function sortBySpeed()    { sortBy('speed',   compareNumeric, function(x) { return x.gs; }); }
function sortByVerticalRate()   { sortBy('vert_rate',      compareNumeric, function(x) { return x.vert_rate; }); }
function sortByDistance() { sortBy('sitedist',compareNumeric, function(x) { return x.sitedist; }); }
function sortByTrack()    { sortBy('track',   compareNumeric, function(x) { return x.track; }); }
function sortByMsgs()     { sortBy('msgs',    compareNumeric, function(x) { return x.messages; }); }
function sortBySeen()     { sortBy('seen',    compareNumeric, function(x) { return x.seen; }); }
function sortByCountry()  { sortBy('country', compareAlpha,   function(x) { return x.icaorange.country; }); }
function sortByRssi()     { sortBy('rssi',    compareNumeric, function(x) { return x.rssi }); }
function sortByLatitude()   { sortBy('lat',   compareNumeric, function(x) { return (x.position !== null ? x.position[1] : null) }); }
function sortByLongitude()  { sortBy('lon',   compareNumeric, function(x) { return (x.position !== null ? x.position[0] : null) }); }
function sortByDataSource() { sortBy('data_source',     compareAlpha, function(x) { return x.getDataSource() } ); }

var sortId = '';
var sortCompare = null;
var sortExtract = null;
var sortAscending = true;

function sortFunction(x,y) {
        var xv = x._sort_value;
        var yv = y._sort_value;

        // always sort missing values at the end, regardless of
        // ascending/descending sort
        if (xv == null && yv == null) return x._sort_pos - y._sort_pos;
        if (xv == null) return 1;
        if (yv == null) return -1;

        var c = sortAscending ? sortCompare(xv,yv) : sortCompare(yv,xv);
        if (c !== 0) return c;

        return x._sort_pos - y._sort_pos;
}

function resortTable() {
        // number the existing rows so we can do a stable sort
        // regardless of whether sort() is stable or not.
        // Also extract the sort comparison value.
        for (var i = 0; i < PlanesOrdered.length; ++i) {
                PlanesOrdered[i]._sort_pos = i;
                PlanesOrdered[i]._sort_value = sortExtract(PlanesOrdered[i]);
        }

        PlanesOrdered.sort(sortFunction);
        
        var tbody = document.getElementById('tableinfo').tBodies[0];
        for (var i = 0; i < PlanesOrdered.length; ++i) {
                tbody.appendChild(PlanesOrdered[i].tr);
        }
}

function sortBy(id,sc,se) {
        if (id !== 'data_source') {
                $('#grouptype_checkbox').removeClass('settingsCheckboxChecked');
		localStorage.setItem('groupByDataType', 'deselected');
        }

        if (id === sortId) {
                sortAscending = !sortAscending;
                PlanesOrdered.reverse(); // this correctly flips the order of rows that compare equal
        } else {
                sortAscending = true;
        }

        sortId = id;
        sortCompare = sc;
        sortExtract = se;

        resortTable();
}

function selectPlaneByHex(hex,autofollow) {
        //console.log("select: " + hex);
	// If SelectedPlane has something in it, clear out the selected
	if (SelectedAllPlanes) {
		deselectAllPlanes();
	}

	if (SelectedPlane != null) {
		Planes[SelectedPlane].selected = false;
		Planes[SelectedPlane].clearLines();
		Planes[SelectedPlane].updateMarker();
                $(Planes[SelectedPlane].tr).removeClass("selected");
		// scroll the infoblock back to the top for the next plane to be selected
		$('.infoblock-container').scrollTop(0);
	}

	// If we are clicking the same plane, we are deselecting it.
	// (unless it was a doubleclick..)
	if (SelectedPlane === hex && !autofollow) {
		hex = null;
	}

	if (hex !== null) {
		// Assign the new selected
		SelectedPlane = hex;
		Planes[SelectedPlane].selected = true;
		Planes[SelectedPlane].updateLines();
		Planes[SelectedPlane].updateMarker();
	    $(Planes[SelectedPlane].tr).addClass("selected");
	} else { 
		SelectedPlane = null;
	}

	if (SelectedPlane !== null && autofollow) {
		FollowSelected = true;
		if (OLMap.getView().getZoom() < 8)
			OLMap.getView().setZoom(8);
	} else {
		FollowSelected = false;
	} 

	refreshSelected();
	refreshHighlighted();
}

function highlightPlaneByHex(hex) {

	if (hex != null) {
		HighlightedPlane = hex;
	}
}

// loop through the planes and mark them as selected to show the paths for all planes
function selectAllPlanes() {
    HighlightedPlane = null;
	// if all planes are already selected, deselect them all
	if (SelectedAllPlanes) {
		deselectAllPlanes();
	} else {
		// If SelectedPlane has something in it, clear out the selected
		if (SelectedPlane != null) {
			Planes[SelectedPlane].selected = false;
			Planes[SelectedPlane].clearLines();
			Planes[SelectedPlane].updateMarker();
			$(Planes[SelectedPlane].tr).removeClass("selected");
		}

		SelectedPlane = null;
		SelectedAllPlanes = true;

		for(var key in Planes) {
			if (Planes[key].visible && !Planes[key].isFiltered()) {
				Planes[key].selected = true;
				Planes[key].updateLines();
				Planes[key].updateMarker();
			}
		}
	}

	$('#selectall_checkbox').addClass('settingsCheckboxChecked');

	refreshSelected();
	refreshHighlighted();
}

// on refreshes, try to find new planes and mark them as selected
function selectNewPlanes() {
	if (SelectedAllPlanes) {
		for (var key in Planes) {
			if (!Planes[key].visible || Planes[key].isFiltered()) {
				Planes[key].selected = false;
				Planes[key].clearLines();
				Planes[key].updateMarker();
			} else {
				if (Planes[key].selected !== true) {
					Planes[key].selected = true;
					Planes[key].updateLines();
					Planes[key].updateMarker();
				}
			}
		}
	}
}

function toggleGroupByDataType(switchToggle) {
	if (typeof localStorage['groupByDataType'] === 'undefined') {
		localStorage.setItem('groupByDataType', 'deselected');
	}

	var groupByDataType = localStorage.getItem('groupByDataType');
	if (switchToggle === true) {
		groupByDataType = (groupByDataType === 'deselected') ? 'selected' : 'deselected';
	}

	if (groupByDataType === 'deselected') {
		$('#grouptype_checkbox').removeClass('settingsCheckboxChecked');
	} else {
		sortByDataSource();
		$('#grouptype_checkbox').addClass('settingsCheckboxChecked');
	}

	localStorage.setItem('groupByDataType', groupByDataType);
}

function toggleAllPlanes(switchToggle) {
	if (typeof localStorage['allPlanesSelection'] === 'undefined') {
		localStorage.setItem('allPlanesSelection','deselected');
	}

	var allPlanesSelection = localStorage.getItem('allPlanesSelection');
	if (switchToggle === true) {
		allPlanesSelection = (allPlanesSelection === 'deselected') ? 'selected' : 'deselected';
	}

	if (allPlanesSelection === 'deselected') {
		deselectAllPlanes();
	} else {
		selectAllPlanes();
	}

	localStorage.setItem('allPlanesSelection', allPlanesSelection);
}

// deselect all the planes
function deselectAllPlanes() {
	for(var key in Planes) {
		Planes[key].selected = false;
		Planes[key].clearLines();
		Planes[key].updateMarker();
		$(Planes[key].tr).removeClass("selected");
	}
	$('#selectall_checkbox').removeClass('settingsCheckboxChecked');
	SelectedPlane = null;
	SelectedAllPlanes = false;
	refreshSelected();
	refreshHighlighted();
}

function toggleFollowSelected() {
        FollowSelected = !FollowSelected;
        if (FollowSelected && OLMap.getView().getZoom() < 8)
                OLMap.getView().setZoom(8);
        refreshSelected();
}

function resetMap() {
        // Reset localStorage values and map settings
        localStorage['CenterLat'] = CenterLat = DefaultCenterLat;
        localStorage['CenterLon'] = CenterLon = DefaultCenterLon;
        localStorage['ZoomLvl']   = ZoomLvl = DefaultZoomLvl;

        // Reset to default range rings
        localStorage['SiteCirclesCount'] = SiteCirclesCount = DefaultSiteCirclesCount;
        localStorage['SiteCirclesBaseDistance'] = SiteCirclesBaseDistance = DefaultSiteCirclesBaseDistance;
        localStorage['SiteCirclesInterval'] = SiteCirclesInterval = DefaultSiteCirclesInterval;
        setRangeRings();
        createSiteCircleFeatures();

        // Set and refresh
        OLMap.getView().setZoom(ZoomLvl);
	OLMap.getView().setCenter(ol.proj.fromLonLat([CenterLon, CenterLat]));
	
	selectPlaneByHex(null,false);
}

function updateMapSize() {
    OLMap.updateSize();
}

function toggleSidebarVisibility(e) {
    if (e) {
        e.preventDefault();
    }
    $("#sidebar_container").toggle();
    $("#expand_sidebar_control").toggle();
    $("#toggle_sidebar_button").toggleClass("show_sidebar");
    $("#toggle_sidebar_button").toggleClass("hide_sidebar");
    updateMapSize();
}

function expandSidebar(e) {
    if (e) {
        e.preventDefault();
    }
    $("#map_container").hide()
    $("#toggle_sidebar_control").hide();
    $("#splitter").hide();
    $("#sudo_buttons").hide();
    $("#show_map_button").show();
    $("#sidebar_container").width("100%");
    setColumnVisibility();
    setSelectedInfoBlockVisibility();
    updateMapSize();
}

function showMap() {
    $("#map_container").show()
    $("#toggle_sidebar_control").show();
    $("#splitter").show();
    $("#sudo_buttons").show();
    $("#show_map_button").hide();
    $("#sidebar_container").width("470px");
    setColumnVisibility();
    setSelectedInfoBlockVisibility();
    updateMapSize();    
}

function showColumn(table, columnId, visible) {
    var index = $(columnId).index();
    if (index >= 0) {
        var cells = $(table).find("td:nth-child(" + (index + 1).toString() + ")");
        if (visible) {
            cells.show();
        } else {
            cells.hide();
        }
    }
}

function setColumnVisibility() {
    var mapIsVisible = $("#map_container").is(":visible");
    var infoTable = $("#tableinfo");

    var defaultCheckBoxes = [
        '#icao_col_checkbox',
        '#flag_col_checkbox',
        '#ident_col_checkbox',
        '#squawk_col_checkbox',
        '#alt_col_checkbox',
        '#speed_col_checkbox',
        '#distance_col_checkbox',
        '#heading_col_checkbox',
        '#messages_col_checkbox',
        '#msg_age_col_checkbox'
    ]

    // Show default columns if checkboxes have not been set
    for (var i=0; i < defaultCheckBoxes.length; i++) {
        var checkBoxdiv = defaultCheckBoxes[i];
        var columnDiv = checkbox_div_map.get(checkBoxdiv)

        if (typeof localStorage[checkBoxdiv] === 'undefined') {
                $(checkBoxdiv).addClass('settingsCheckboxChecked');
                localStorage.setItem(checkBoxdiv, 'selected');
                showColumn(infoTable, columnDiv, true);
        }
    }

    // Now check local storage checkbox status
    checkbox_div_map.forEach(function (div, checkbox) {
        var status = localStorage.getItem(checkbox);
        if (status === 'selected') {
                $(checkbox).addClass('settingsCheckboxChecked');
                showColumn(infoTable, div, true);
        } else {
                $(checkbox).removeClass('settingsCheckboxChecked');
                showColumn(infoTable, div, false);
        }
    });
}

function setSelectedInfoBlockVisibility() {
    var mapIsVisible = $("#map_container").is(":visible");
    var planeSelected = (typeof SelectedPlane !== 'undefined' && SelectedPlane != null && SelectedPlane != "ICAO");

    if (planeSelected && mapIsVisible) {
        $('#selected_infoblock').show();
		$('#sidebar_canvas').css('margin-bottom', $('#selected_infoblock').height() + 'px');
    }
    else {
        $('#selected_infoblock').hide();
		$('#sidebar_canvas').css('margin-bottom', 0);
	}
}

// Reposition selected plane info box if it overlaps plane marker
function adjustSelectedInfoBlockPosition() {
    if (typeof Planes === 'undefined' || typeof SelectedPlane === 'undefined' || Planes === null) {
        return;
    }

    var selectedPlane = Planes[SelectedPlane];

    if (selectedPlane === undefined || selectedPlane === null || selectedPlane.marker === undefined || selectedPlane.marker === null) {
        return;
    }

    try {
        // Get marker position
        var marker = selectedPlane.marker;
        var markerCoordinates = selectedPlane.marker.getGeometry().getCoordinates();
		var markerPosition = OLMap.getPixelFromCoordinate(markerCoordinates);
		
        // Get map size
        var mapCanvas = $('#map_canvas');
        var mapExtent = getExtent(0, 0, mapCanvas.width(), mapCanvas.height());

        // Check for overlap
        if (isPointInsideExtent(markerPosition[0], markerPosition[1], infoBoxExtent)) {
            // Array of possible new positions for info box
            var candidatePositions = [];
            candidatePositions.push( { x: 40, y: 60 } );
            candidatePositions.push( { x: 40, y: markerPosition[1] + 80 } );

            // Find new position
            for (var i = 0; i < candidatePositions.length; i++) {
                var candidatePosition = candidatePositions[i];
                var candidateExtent = getExtent(candidatePosition.x, candidatePosition.y, infoBox.outerWidth(), infoBox.outerHeight());

                if (!isPointInsideExtent(markerPosition[0],  markerPosition[1], candidateExtent) && isPointInsideExtent(candidatePosition.x, candidatePosition.y, mapExtent)) {
                    // Found a new position that doesn't overlap marker - move box to that position
                    infoBox.css("left", candidatePosition.x);
                    infoBox.css("top", candidatePosition.y);
                    return;
                }
            }
        }
    } 
    catch(e) { }
}

function getExtent(x, y, width, height) {
    return {
        xMin: x,
        yMin: y,
        xMax: x + width - 1,
        yMax: y + height - 1,
    };
}

function isPointInsideExtent(x, y, extent) {
    return x >= extent.xMin && x <= extent.xMax && y >= extent.yMin && y <= extent.yMax;
}

function initializeUnitsSelector() {
    // Get display unit preferences from local storage
    if (!localStorage.getItem('displayUnits')) {
        localStorage['displayUnits'] = "nautical";
    }
    var displayUnits = localStorage['displayUnits'];
    DisplayUnits = displayUnits;

    setAltitudeLegend(displayUnits);

    // Initialize drop-down
    var unitsSelector = $("#units_selector");
    unitsSelector.val(displayUnits);
    unitsSelector.on("change", onDisplayUnitsChanged);
}

function onDisplayUnitsChanged(e) {

    if (e) {
        var displayUnits = e.target.value;
        // Save display units to local storage
        localStorage['displayUnits'] = displayUnits;
    }

    DisplayUnits = localStorage['displayUnits'];

    setAltitudeLegend(DisplayUnits);

    // Update filters
    updatePlaneFilter();

    // Refresh data
    refreshTableInfo();
    refreshSelected();
    refreshHighlighted();

    // Redraw range rings
    if (SitePosition !== null && SitePosition !== undefined && SiteCircles) {
        createSiteCircleFeatures();
    }

    // Reset map scale line units
    OLMap.getControls().forEach(function(control) {
        if (control instanceof ol.control.ScaleLine) {
            control.setUnits(DisplayUnits);
        }
    });
}

function setAltitudeLegend(units) {
    if (units === 'metric') {
        $('#altitude_chart_button').addClass('altitudeMeters');
    } else {
        $('#altitude_chart_button').removeClass('altitudeMeters');
    }
}

function onFilterByAltitude(e) {
    e.preventDefault();
    updatePlaneFilter();
    refreshTableInfo();

    var selectedPlane = Planes[SelectedPlane];
    if (selectedPlane !== undefined && selectedPlane !== null && selectedPlane.isFiltered()) {
        SelectedPlane = null;
        selectedPlane.selected = false;
        selectedPlane.clearLines();
        selectedPlane.updateMarker();         
        refreshSelected();
        refreshHighlighted();
    }
}

function filterGroundVehicles(switchFilter) {
	if (typeof localStorage['groundVehicleFilter'] === 'undefined') {
		localStorage.setItem('groundVehicleFilter' , 'not_filtered');
	}

	var groundFilter = localStorage.getItem('groundVehicleFilter');
	if (switchFilter === true) {
		groundFilter = (groundFilter === 'not_filtered') ? 'filtered' : 'not_filtered';
	}
	if (groundFilter === 'not_filtered') {
		$('#groundvehicle_filter').addClass('settingsCheckboxChecked');
	} else {
		$('#groundvehicle_filter').removeClass('settingsCheckboxChecked');
	}

	localStorage.setItem('groundVehicleFilter',groundFilter);
	PlaneFilter.groundVehicles = groundFilter;
}

function filterBlockedMLAT(switchFilter) {
	if (typeof localStorage['blockedMLATFilter'] === 'undefined') {
		localStorage.setItem('blockedMLATFilter','not_filtered');
	}

	var blockedMLATFilter = localStorage.getItem('blockedMLATFilter');
	if (switchFilter === true) {
		blockedMLATFilter = (blockedMLATFilter === 'not_filtered') ? 'filtered' : 'not_filtered';
	}
	if (blockedMLATFilter === 'not_filtered') {
		$('#blockedmlat_filter').addClass('settingsCheckboxChecked');
	} else {
		$('#blockedmlat_filter').removeClass('settingsCheckboxChecked');
	}
	localStorage.setItem('blockedMLATFilter', blockedMLATFilter);
	PlaneFilter.blockedMLAT = blockedMLATFilter;
}

function toggleAltitudeChart(switchToggle) {
	if (typeof localStorage['altitudeChart'] === 'undefined') {
		localStorage.setItem('altitudeChart','show');
	}

	var altitudeChartDisplay = localStorage.getItem('altitudeChart');
	if (switchToggle === true) {
		altitudeChartDisplay = (altitudeChartDisplay === 'show') ? 'hidden' : 'show';
	}

	// if you're using custom colors always hide the chart
	if (customAltitudeColors === true) {
        	altitudeChartDisplay = 'hidden';
		// also hide the control option
        	$('#altitude_chart_container').hide();
    	}

	if (altitudeChartDisplay === 'show') {
		$('#altitude_checkbox').addClass('settingsCheckboxChecked');
		$('#altitude_chart').show();
	} else {
		$('#altitude_checkbox').removeClass('settingsCheckboxChecked');
		$('#altitude_chart').hide();
	}

	localStorage.setItem('altitudeChart', altitudeChartDisplay);
}

function onResetAltitudeFilter(e) {
    $("#altitude_filter_min").val("");
    $("#altitude_filter_max").val("");

    updatePlaneFilter();
    refreshTableInfo();
}

function updatePlaneFilter() {
    var minAltitude = parseFloat($("#altitude_filter_min").val().trim());
    var maxAltitude = parseFloat($("#altitude_filter_max").val().trim());

    if (minAltitude === NaN) {
        minAltitude = -Infinity;
    }

    if (maxAltitude === NaN) {
        maxAltitude = Infinity;
    }

    PlaneFilter.minAltitude = minAltitude;
    PlaneFilter.maxAltitude = maxAltitude;
    PlaneFilter.altitudeUnits = DisplayUnits;
}

function getFlightAwareIdentLink(ident, linkText) {
    if (ident !== null && ident !== "") {
        if (!linkText) {
            linkText = ident;
        }
        return "<a target=\"_blank\" href=\"https://flightaware.com/live/flight/" + ident.trim() + "\">" + linkText + "</a>";
    }

    return "";
}

function getFlightAwareModeSLink(code, ident, linkText) {
    if (code !== null && code.length > 0 && code[0] !== '~' && code !== "000000") {
        if (!linkText) {
            linkText = "FlightAware: " + code.toUpperCase();
        }

        var linkHtml = "<a target=\"_blank\" href=\"https://flightaware.com/live/modes/" + code ;
        if (ident !== null && ident !== "") {
            linkHtml += "/ident/" + ident.trim();
        }
        linkHtml += "/redirect\">" + linkText + "</a>";
        return linkHtml;
    }

    return "";
}

function getFlightAwarePhotoLink(registration) {
    if (registration !== null && registration !== "") {
        return "<a target=\"_blank\" href=\"https://flightaware.com/photos/aircraft/" + registration.replace(/[^0-9a-z]/ig,'') + "\">See Photos</a>";
    }

    return "";   
}

function getAirframesModeSLink(code) {
    if (code !== null && code.length > 0 && code[0] !== '~' && code !== "000000") {
        return "<a href=\"http://www.airframes.org/\" onclick=\"$('#airframes_post_icao').attr('value','" + code + "'); document.getElementById('horrible_hack').submit.call(document.getElementById('airframes_post')); return false;\">Airframes.org: " + code.toUpperCase() + "</a>";
    }

    return "";   
}


// takes in an elemnt jQuery path and the OL3 layer name and toggles the visibility based on clicking it
function toggleLayer(element, layer) {
        // set initial checked status
        ol.control.LayerSwitcher.forEachRecursive(layerGroup, function(lyr) {
		if (lyr.get('name') === layer && lyr.getVisible()) {
			$(element).addClass('settingsCheckboxChecked');
		}
	});
	$(element).on('click', function() {
		var visible = false;
		if ($(element).hasClass('settingsCheckboxChecked')) {
			visible = true;
		}
		ol.control.LayerSwitcher.forEachRecursive(layerGroup, function(lyr) {
			if (lyr.get('name') === layer) {
				if (visible) {
					lyr.setVisible(false);
					$(element).removeClass('settingsCheckboxChecked');
				} else {
					lyr.setVisible(true);
					$(element).addClass('settingsCheckboxChecked');
				}
			}
		});
	});
}

// check status.json if it has a serial number for a flightfeeder
function flightFeederCheck() {
    $.ajax('/status.json', {
        success: function(data) {
            if (data.type === "flightfeeder") {
                isFlightFeeder = true;
                updatePiAwareOrFlightFeeder();
            }
        }
    })
}

// updates the page to replace piaware with flightfeeder references
function updatePiAwareOrFlightFeeder() {
    if (isFlightFeeder) {
        $('.piAwareLogo').hide();
        $('.flightfeederLogo').show();
        PageName = 'FlightFeeder SkyAware';
    } else {
        $('.flightfeederLogo').hide();
        $('.piAwareLogo').show();
        PageName = 'PiAware SkyAware';
    }
    refreshPageTitle();
}

// Function to hide banner (ex. for a kiosk to show maximum data possible)
function hideBanner() {
    document.getElementById("header").style.display = 'none'; 
    document.getElementById("layout_container").style.height = '100%';
    updateMapSize();
}

// Helper function to restrict the range of the inputs
function restrictUrlRequest(c) {
    let v = parseFloat(c);
    if (v < 0) {
        v = 0;
    } else if (v > 5) {
        v = 5;
    }
    return v;
}

// Function to zoom, but not by too much per 'amount'
function zoomMap(c, zoomOut) {
    c = restrictUrlRequest(c);
    ZoomLvl = OLMap.getView().getZoom();
    if (zoomOut) {
        ZoomLvl *= Math.pow(0.95, c);
    } else {
        ZoomLvl /= Math.pow(0.95, c);
    }
    localStorage['ZoomLvl'] = ZoomLvl;
    OLMap.getView().setZoom(ZoomLvl);
}

// Function to move map at 0.005% of the extent per 'move'
function moveMap(c, moveVertical, moveDownLeft) {
    c = restrictUrlRequest(c);
    let cn = OLMap.getView().getCenter();
    let dist = 0;
    if (moveVertical) {
        dist = ol.extent.getHeight(OLMap.getView().getProjection().getExtent());
    } else {
        dist = ol.extent.getWidth(OLMap.getView().getProjection().getExtent());
    }
    let d = c * (dist * .005);
    // 'up' or 'right' needs a negative number
    if (moveDownLeft) {
        d *= -1.0;
    }
    if (moveVertical) {
        ol.coordinate.add(cn, [0, d]);
    } else {
        ol.coordinate.add(cn, [d, 0]);
    }
    OLMap.getView().setCenter(cn);
}

// Function to set displayUnits
function setDisplayUnits(units) {
    if (units === 'nautical') {
        localStorage['displayUnits'] = "nautical";
    } else if (units === 'metric') {
        localStorage['displayUnits'] = "metric";
    } else if (units === 'imperial') {
        localStorage['displayUnits'] = "imperial";
    }
    onDisplayUnitsChanged();
}

// Function to set range ring visibility
function setRangeRingVisibility (showhide) {
   var show = null;

   if (showhide === 'hide') {
        $('#sitepos_checkbox').removeClass('settingsCheckboxChecked')
        show = false;
   } else if (showhide === 'show') {
        $('#sitepos_checkbox').addClass('settingsCheckboxChecked')
        show = true;
   } else {
        return
   }

   ol.control.LayerSwitcher.forEachRecursive(layerGroup, function(lyr) {
        if (lyr.get('name') === 'site_pos') {
        lyr.setVisible(show);
        }
    });
}

// simple function to set range ring count
function setRingCount(val) {
    localStorage['SiteCirclesCount'] = val;
    setRangeRings();
    createSiteCircleFeatures();
}

// simple function to set range ring distance
function setRingBaseDistance(val) {
    localStorage['SiteCirclesBaseDistance'] = val;
    setRangeRings();
    createSiteCircleFeatures();
}

// simple function to set range ring interval
function setRingInterval(val) {
    localStorage['SiteCirclesInterval'] = val;
    setRangeRings();
    createSiteCircleFeatures();
}

// Set range ring globals and populate form values
function setRangeRings() {
    SiteCirclesCount = Number(localStorage['SiteCirclesCount']) || DefaultSiteCirclesCount;
    SiteCirclesBaseDistance = Number(localStorage['SiteCirclesBaseDistance']) || DefaultSiteCirclesBaseDistance;
    SiteCirclesInterval = Number(localStorage['SiteCirclesInterval']) || DefaultSiteCirclesInterval;

    // Populate text fields with current values
    $('#range_ring_count').val(SiteCirclesCount);
    $('#range_ring_base').val(SiteCirclesBaseDistance);
    $('#range_ring_interval').val(SiteCirclesInterval);
}

// redraw range rings with form values
function onSetRangeRings() {
    // Save state to localStorage
    localStorage.setItem('SiteCirclesCount', parseFloat($("#range_ring_count").val().trim()));
    localStorage.setItem('SiteCirclesBaseDistance', parseFloat($("#range_ring_base").val().trim()));
    localStorage.setItem('SiteCirclesInterval', parseFloat($("#range_ring_interval").val().trim()));

    setRangeRings();

    createSiteCircleFeatures();
}

function toggleColumn(div, checkbox, toggled) {
	if (typeof localStorage[checkbox] === 'undefined') {
		localStorage.setItem(checkbox, 'deselected');
	}

	var status = localStorage.getItem(checkbox);
	var infoTable = $("#tableinfo");

	if (toggled === true) {
		status = (status === 'deselected') ? 'selected' : 'deselected';
	}

	// Toggle checkbox and column visibility
	if (status === 'selected') {
		$(checkbox).addClass('settingsCheckboxChecked');
		showColumn(infoTable, div, true);
	} else {
		$(checkbox).removeClass('settingsCheckboxChecked');
		showColumn(infoTable, div, false);
		$('#select_all_column_checkbox').removeClass('settingsCheckboxChecked');
		localStorage.setItem('selectAllColumnsCheckbox', 'deselected');
	}

	localStorage.setItem(checkbox, status);
}

function toggleAllColumns(switchToggle) {
        if (typeof localStorage['selectAllColumnsCheckbox'] === 'undefined') {
                localStorage.setItem('selectAllColumnsCheckbox','deselected');
        }

        var infoTable = $("#tableinfo");

        var selectAllColumnsCheckbox = localStorage.getItem('selectAllColumnsCheckbox');

        if (switchToggle === true) {
                selectAllColumnsCheckbox = (selectAllColumnsCheckbox === 'deselected') ? 'selected' : 'deselected';

                checkbox_div_map.forEach(function (div, checkbox) {
                        if (selectAllColumnsCheckbox === 'deselected') {
                                $('#select_all_column_checkbox').removeClass('settingsCheckboxChecked');
                                $(checkbox).removeClass('settingsCheckboxChecked');
                                showColumn(infoTable, div, false);
                        } else {
                                $('#select_all_column_checkbox').addClass('settingsCheckboxChecked');
                                $(checkbox).addClass('settingsCheckboxChecked');
                                showColumn(infoTable, div, true);
                        }
                        localStorage.setItem(checkbox, selectAllColumnsCheckbox);
                });
        };

        if (selectAllColumnsCheckbox === 'deselected') {
                $('#select_all_column_checkbox').removeClass('settingsCheckboxChecked');
        } else {
                $('#select_all_column_checkbox').addClass('settingsCheckboxChecked');
        }

        localStorage.setItem('selectAllColumnsCheckbox', selectAllColumnsCheckbox);
}
