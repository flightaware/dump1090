// -*- mode: javascript; indent-tabs-mode: nil; c-basic-offset: 8 -*-
"use strict";

// Base layers configuration

function createBaseLayers() {
        var layers = [];

        var world = [];
        var us = [];
        var europe = [];

        world.push(new ol.layer.Tile({
                source: new ol.source.OSM(),
                name: 'osm',
                title: 'OpenStreetMap',
                type: 'base',
        }));

        world.push(new ol.layer.Tile({
                source: new ol.source.XYZ({
                        "url" : "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
	                "attributions" : "Tiles © Esri - Source: Esri, i-cubed, USDA, USGS, AEX, GeoEye, Getmapping, Aerogrid, IGN, IGP, UPR-EGP, and the GIS User Community",
                }),
	        name: 'esri_satellite',
                title: 'ESRI Satellite',
                type: 'base',
	}));

        world.push(new ol.layer.Tile({
                source: new ol.source.XYZ({
                        "url" : "https://server.arcgisonline.com/ArcGIS/rest/services/World_Topo_Map/MapServer/tile/{z}/{y}/{x}",
                        "attributions" : "Tiles © Esri - Source: Esri, i-cubed, USDA, USGS, AEX, GeoEye, Getmapping, Aerogrid, IGN, IGP, UPR-EGP, and the GIS User Community",
                }),
                name: 'esri_topo',
                title: 'ESRI Topographic',
                type: 'base',
        }));

        world.push(new ol.layer.Tile({
                source: new ol.source.XYZ({
			"url" : "https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/{z}/{y}/{x}",
                        "attributions" : "Tiles © Esri - Source: Esri, i-cubed, USDA, USGS, AEX, GeoEye, Getmapping, Aerogrid, IGN, IGP, UPR-EGP, and the GIS User Community",
                }),
                name: 'esri_street',
                title: 'ESRI Street',
                type: 'base',
        }));

        world.push(new ol.layer.Tile({
                source: new ol.source.OSM({
                        "url" : "https://{a-z}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png",
                        "attributions" : 'Courtesy of <a href="https://carto.com">CARTO.com</a>'
                               + ' using data by <a href="http://openstreetmap.org">OpenStreetMap</a>, under <a href="http://www.openstreetmap.org/copyright">ODbL</a>.',
                }),
                name: 'carto_dark_all',
                title: 'CARTO.com Dark',
                type: 'base',
        }));

        world.push(new ol.layer.Tile({
                source: new ol.source.OSM({
                        "url" : "https://{a-z}.basemaps.cartocdn.com/dark_nolabels/{z}/{x}/{y}.png",
                        "attributions" : 'Courtesy of <a href="https://carto.com">CARTO.com</a>'
                               + ' using data by <a href="http://openstreetmap.org">OpenStreetMap</a>, under <a href="http://www.openstreetmap.org/copyright">ODbL</a>.',
                }),
                name: 'carto_dark_nolabels',
                title: 'CARTO.com Dark (No Labels)',
                type: 'base',
        }));

        world.push(new ol.layer.Tile({
                source: new ol.source.OSM({
                        "url" : "https://{a-z}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png",
                        "attributions" : 'Courtesy of <a href="https://carto.com">CARTO.com</a>'
                               + ' using data by <a href="http://openstreetmap.org">OpenStreetMap</a>, under <a href="http://www.openstreetmap.org/copyright">ODbL</a>.',
                }),
                name: 'carto_light_all',
                title: 'CARTO.com Light',
                type: 'base',
        }));

        world.push(new ol.layer.Tile({
                source: new ol.source.OSM({
                        "url" : "https://{a-z}.basemaps.cartocdn.com/light_nolabels/{z}/{x}/{y}.png",
                        "attributions" : 'Courtesy of <a href="https://carto.com">CARTO.com</a>'
                               + ' using data by <a href="http://openstreetmap.org">OpenStreetMap</a>, under <a href="http://www.openstreetmap.org/copyright">ODbL</a>.',
                }),
                name: 'carto_light_nolabels',
                title: 'CARTO.com Light (No Labels)',
                type: 'base',
        }));


        if (BingMapsAPIKey) {
                world.push(new ol.layer.Tile({
                        source: new ol.source.BingMaps({
                                key: BingMapsAPIKey,
                                imagerySet: 'Aerial'
                        }),
                        name: 'bing_aerial',
                        title: 'Bing Aerial',
                        type: 'base',
                }));
                world.push(new ol.layer.Tile({
                        source: new ol.source.BingMaps({
                                key: BingMapsAPIKey,
                                imagerySet: 'RoadOnDemand'
                        }),
                        name: 'bing_roads',
                        title: 'Bing Roads',
                        type: 'base',
                }));
        }

        if (ChartBundleLayers) {
                var chartbundleTypes = {
                        sec: "Sectional Charts",
                        tac: "Terminal Area Charts",
                        hel: "Helicopter Charts",
                        enrl: "IFR Enroute Low Charts",
                        enra: "IFR Area Charts",
                        enrh: "IFR Enroute High Charts",
                        secgrids: "Sect. w/ SAR grid"
                };

                for (var type in chartbundleTypes) {
                        us.push(new ol.layer.Tile({
                                source: new ol.source.TileWMS({
                                        url: 'http://wms.chartbundle.com/wms',
                                        params: {LAYERS: type},
                                        projection: 'EPSG:3857',
                                        attributions: 'Tiles courtesy of <a href="http://www.chartbundle.com/">ChartBundle</a>'
                                }),
                                name: 'chartbundle_' + type,
                                title: chartbundleTypes[type],
                                type: 'base',
                                group: 'chartbundle'}));
                }
        }

        var nexrad_bottomLeft = ol.proj.fromLonLat([-171.0,9.0]);
        var nexrad_topRight = ol.proj.fromLonLat([-51.0,69.0]);
        var nexrad_extent = [nexrad_bottomLeft[0], nexrad_bottomLeft[1], nexrad_topRight[0], nexrad_topRight[1]];
        var nexrad = new ol.layer.Tile({
                name: 'nexrad',
                title: 'NEXRAD',
                type: 'overlay',
                opacity: 0.5,
                visible: false,
                extent: nexrad_extent,
        });
        us.push(nexrad);

        var refreshNexrad = function() {
                // re-build the source to force a refresh of the nexrad tiles
                var now = new Date().getTime();
                nexrad.setSource(new ol.source.XYZ({
                        url : 'http://mesonet{1-3}.agron.iastate.edu/cache/tile.py/1.0.0/nexrad-n0q-900913/{z}/{x}/{y}.png?_=' + now,
                        attributions: 'NEXRAD courtesy of <a href="http://mesonet.agron.iastate.edu/">IEM</a>'
                }));
        };

        refreshNexrad();
        window.setInterval(refreshNexrad, 5 * 60000);

        var createGeoJsonLayer = function (title, name, url, fill, stroke, showLabel = true) {
                return new ol.layer.Vector({
                    type: 'overlay',
                    title: title,
                    name: name,
                    zIndex: 99,
                    visible: false,
                    source: new ol.source.Vector({
                      url: url,
                      format: new ol.format.GeoJSON({
                        defaultDataProjection :'EPSG:4326',
                            projection: 'EPSG:3857'
                      })
                    }),
                    style: function style(feature) {
                        return new ol.style.Style({
                            fill: new ol.style.Fill({
                                color : fill
                            }),
                            stroke: new ol.style.Stroke({
                                color: stroke,
                                width: 1
                            }),
                            text: new ol.style.Text({
                                text: showLabel ? feature.get("name") : "",
                                overflow: OLMap.getView().getZoom() > 5,
                                scale: 1.25,
                                fill: new ol.style.Fill({
                                    color: '#000000'
                                }),
                                stroke: new ol.style.Stroke({
                                    color: '#FFFFFF',
                                    width: 2
                                })
                            })
                        });
                    }
                });
            };

        var dwd_bottomLeft = ol.proj.fromLonLat([1.9,46.2]);
        var dwd_topRight = ol.proj.fromLonLat([16.0,55.0]);
        var dwd_extent = [dwd_bottomLeft[0], dwd_bottomLeft[1], dwd_topRight[0], dwd_topRight[1]];
        var dwd = new ol.layer.Tile({
                source: new ol.source.TileWMS({
                        url: 'https://maps.dwd.de/geoserver/wms',
                        params: {LAYERS: 'dwd:RX-Produkt', validtime: (new Date()).getTime()},
                        projection: 'EPSG:3857',
                        attributions: 'Deutscher Wetterdienst (DWD)'
                }),
                name: 'radolan',
                title: 'DWD RADOLAN',
                type: 'overlay',
                opacity: 0.3,
                visible: false,
                zIndex: 99,
                maxZoom: 14,
                exent: dwd_extent,
        });

        var refreshDwd = function () {
                dwd.getSource().updateParams({"validtime": (new Date()).getTime()});
        };

        refreshDwd();
        window.setInterval(refreshDwd, 4 * 60000);

        europe.push(dwd);

        // Taken from https://github.com/alkissack/Dump1090-OpenLayers3-html
        us.push(createGeoJsonLayer('US A2A Refueling', 'usa2arefueling', 'geojson/US_A2A_refueling.geojson', 'rgba(52, 50, 168, 0.3)', 'rgba(52, 50, 168, 1)'));
        us.push(createGeoJsonLayer('US ARTCC Boundaries', 'usartccboundaries', 'geojson/US_ARTCC_boundaries.geojson', 'rgba(255, 0, 255, 0.3)', 'rgba(255, 0, 255, 1)', false));

        europe.push(createGeoJsonLayer('UK Radar Corridors', 'ukradarcorridors', 'geojson/UK_Mil_RC.geojson', 'rgba(22, 171, 22, 0.3)', 'rgba(22, 171, 22, 1)'));
        europe.push(createGeoJsonLayer('UK A2A Refueling', 'uka2arefueling', 'geojson/UK_Mil_AAR_Zones.geojson', 'rgba(52, 50, 168, 0.3)', 'rgba(52, 50, 168, 1)'));
        europe.push(createGeoJsonLayer('UK AWACS Orbits', 'uka2awacsorbits', 'geojson/UK_Mil_AWACS_Orbits.geojson', 'rgba(252, 186, 3, 0.3)', 'rgba(252, 186, 3, 1)', false));

        if (world.length > 0) {
                layers.push(new ol.layer.Group({
                        name: 'world',
                        title: 'Worldwide',
                        layers: world
                }));
        }

        if (us.length > 0) {
                layers.push(new ol.layer.Group({
                        name: 'us',
                        title: 'US',
                        layers: us
                }));
        }

        if (europe.length > 0) {
                layers.push(new ol.layer.Group({
                        name: 'europe',
                        title: 'Europe',
                        layers: europe,
                }));
        }

        return layers;
}
