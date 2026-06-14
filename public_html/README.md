# SkyAware Features

## Customize display via URL query strings

Syntax: <ip_address>/skyaware/?parameter=value

Examples:

    http://<ip_address>/skyaware/?sidebar=hide

    http://<ip_address>/skyaware/?altitudeChart=hide

    http://<ip_address>/skyaware/?rangeRings=hide

    http://<ip_address>/skyaware/?ringCount=3&ringBaseDistance=100&ringInterval=50

| Parameter | Possible Values | Description |
| :-------- | :-------------- | :---------- |
| banner | show/hide | Show or hide the header banner |
| altitudeChart | show/hide | Show or hide the altitude chart |
| aircraftTrails | show/hide | Show or hide all aircraft trails |
| aircraftLabels | show/hide | Show or hide flight-number labels on map icons |
| aircraftLabelDetails | show/hide | When labels are shown, also display ICAO aircraft type code on a second line |
| map | show/hide | Show or hide the map panel |
| sidebar | show/hide | Show or hide the sidebar |
| zoomOut | 0 - 5 | Zoom out by a relative amount |
| zoomIn | 0 - 5 | Zoom in by a relative amount |
| zoom | 1 - 20 | Set an absolute OpenLayers zoom level |
| moveNorth | 0 - 5 | Pan the map north |
| moveSouth | 0 - 5 | Pan the map south |
| moveWest | 0 - 5 | Pan the map west |
| moveEast | 0 - 5 | Pan the map east |
| lat | -90 to 90 | Center the map at this latitude (must be used together with `lon`) |
| lon | -180 to 180 | Center the map at this longitude (must be used together with `lat`) |
| baseLayer | layer name | Select the active base map layer by name (see layer names below) |
| displayUnits | nautical/imperial/metric | Set the display units |
| rangeRings | show/hide | Show or hide range rings |
| ringCount | integer | Number of range rings |
| ringBaseDistance | integer | Radius of the innermost ring |
| ringInterval | integer | Distance between rings |

### Base layer names for `baseLayer`

| Name | Description |
| :--- | :---------- |
| osm | OpenStreetMap |
| esri_satellite | ESRI Satellite imagery |
| esri_topo | ESRI Topographic |
| esri_street | ESRI Street |
| carto_dark_all | CARTO Dark (with labels) |
| carto_dark_nolabels | CARTO Dark (no labels) |
| carto_light_all | CARTO Light (with labels) |
| carto_light_nolabels | CARTO Light (no labels) |
| bing_aerial | Bing Aerial (requires Bing API key in config.js) |
| bing_roads | Bing Roads (requires Bing API key in config.js) |
| VFR_Sectional | FAA VFR Sectional Chart (requires FAALayers enabled) |
| IFR_AreaLow | FAA IFR Area Low (requires FAALayers enabled) |
| IFR_High | FAA IFR High (requires FAALayers enabled) |
| VFR_Terminal | FAA VFR Terminal Chart (requires FAALayers enabled) |

## New World/US/Europe Basemaps and Overlays

Click the OpenLayers icon on the bottom right of the map to select baselayers and overlays

## Ability to show/hide columns in the aircraft table

The "Select Columns" button on the aircraft table allows you to choose which columns to show for your preferred display
