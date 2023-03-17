"use strict";

function PlaneObject(icao) {
	// Info about the plane
        this.icao      = icao;
        this.icaorange = findICAORange(icao);
        this.flight    = null;
        this.squawk    = null;
        this.selected  = false;
        this.category  = null;

	// Basic location information
        this.altitude       = null;
        this.alt_baro       = null;
        this.alt_geom       = null;

        this.speed          = null;
        this.gs             = null;
        this.ias            = null;
        this.tas            = null;

        this.track          = null;
        this.track_rate     = null;
        this.mag_heading    = null;
        this.true_heading   = null;
        this.mach           = null;
        this.roll           = null;
        this.nav_altitude   = null;
        this.nav_heading    = null;
        this.nav_modes      = null;
        this.nav_qnh        = null;
        this.rc				= null;
		
        this.nac_p			= null;
        this.nac_v			= null;
        this.nic_baro		= null;
        this.sil_type		= null;
        this.sil			= null;

        this.baro_rate      = null;
        this.geom_rate      = null;
        this.vert_rate      = null;

        this.version        = null;
        this.uat_version    = null;

        this.prev_position = null;
        this.prev_position_time = null;
        this.position  = null;
        this.position_from_mlat = false
        this.sitedist  = null;

	// Data packet numbers
	this.messages  = null;
        this.rssi      = null;

        // Track history as a series of line segments
        this.elastic_feature = null;
        this.track_linesegs = [];
        this.history_size = 0;

	// When was this last updated (receiver timestamp)
        this.last_message_time = null;
        this.last_position_time = null;

        // When was this last updated (seconds before last update)
        this.seen = null;
        this.seen_pos = null;

        // Display info
        this.visible = true;
        this.marker = null;
        this.markerStyle = null;
        this.markerIcon = null;
        this.markerStaticStyle = null;
        this.markerStaticIcon = null;
        this.markerStyleKey = null;
        this.markerSvgKey = null;
        this.filter = {};

        // start from a computed registration, let the DB override it
        // if it has something else.
        this.registration = registration_from_hexid(this.icao);
        this.icaotype = null;
        this.typeDescription = null;
        this.wtc = null;

        this.heard_on_1090 = false;
        this.heard_on_978 = false;
        this.heard_on_tisb = false;
        this.heard_on_adsr = false;

        // request metadata
        getAircraftData(this.icao).done(function(data) {
                if ("r" in data) {
                        this.registration = data.r;
                }

                if ("t" in data) {
                        this.icaotype = data.t;
                }

                if ("desc" in data) {
                        this.typeDescription = data.desc;
                }

                if ("wtc" in data) {
                        this.wtc = data.wtc;
                }

                if (this.selected) {
                        refreshSelected();
                }
        }.bind(this));
}

PlaneObject.prototype.isFiltered = function() {
    // aircraft type filter
    if (this.filter.aircraftTypeCode) {
        if (this.icaotype === null || (typeof this.icaotype === 'string' && !this.icaotype.toUpperCase().trim().match(this.filter.aircraftTypeCode))) {
                return true;
        }
    }

    // aircraft ident filter
    if (this.filter.aircraftIdent) {
        if (this.flight === null || (typeof this.flight === 'string' && !this.flight.toUpperCase().trim().match(this.filter.aircraftIdent))) {
                return true;
        }
    }

    var dataSource = this.getDataSource();
    if (dataSource === 'uat') {
        if (!this.filter.UAT) return true;
    } else if (dataSource === 'adsb_icao') {
        if (!this.filter.ADSB) return true;
    } else if (dataSource === 'mlat') {
        if (!this.filter.MLAT) return true;
    } else if (dataSource === 'tisb_trackfile' || dataSource === 'tisb_icao' || dataSource === 'tisb_other') {
        if (!this.filter.TISB) return true;
    } else {
        if (!this.filter.Other) return true;
    }

    if (this.filter.minAltitude !== undefined && this.filter.maxAltitude !== undefined) {
        if (this.altitude === null || this.altitude === undefined) {
                return true;
        }

        var planeAltitude = this.altitude === "ground" ? 0 : convert_altitude(this.altitude, this.filter.altitudeUnits);
        var isFilteredByAltitude = planeAltitude < this.filter.minAltitude || planeAltitude > this.filter.maxAltitude;
        if (isFilteredByAltitude) {
                return true;
        }
    }
    if (this.filter.minSpeedFilter !== undefined && this.filter.maxSpeedFilter !== undefined) {
        if (this.speed === null || this.speed === undefined) {
                return true;
        }

        var convertedSpeed = convert_speed(this.speed, this.filter.speedUnits)
        var isFilteredBySpeed = convertedSpeed < this.filter.minSpeedFilter || convertedSpeed > this.filter.maxSpeedFilter;
        if (isFilteredBySpeed) {
                return true;
        }
    }

    // filter out ground vehicles
    if (typeof this.filter.groundVehicles !== 'undefined' && this.filter.groundVehicles === 'filtered') {
        if (typeof this.category === 'string' && this.category.startsWith('C')) {
                return true;
        }
    }

    // filter out blocked MLAT flights
    if (typeof this.filter.blockedMLAT !== 'undefined' && this.filter.blockedMLAT === 'filtered') {
        if (typeof this.icao === 'string' && this.icao.startsWith('~')) {
                return true;
        }
    }

    return false;
}

// Appends data to the running track so we can get a visual tail on the plane
// Only useful for a long running browser session.
PlaneObject.prototype.updateTrack = function(receiver_timestamp, last_timestamp) {
        if (!this.position)
                return false;
        if (this.prev_position && this.position[0] == this.prev_position[0] && this.position[1] == this.prev_position[1])
                return false;

        var projHere = ol.proj.fromLonLat(this.position);
        var projPrev;
        var prev_time;
        if (this.prev_position === null) {
                projPrev = projHere;
                prev_time = this.last_position_time;
        } else {
                projPrev = ol.proj.fromLonLat(this.prev_position);
                prev_time = this.prev_position_time;
        }

        this.prev_position = this.position;
        this.prev_position_time = this.last_position_time;

        if (this.track_linesegs.length == 0) {
                // Brand new track
                //console.log(this.icao + " new track");
                var newseg = { fixed: new ol.geom.LineString([projHere]),
                               feature: null,
                               update_time: this.last_position_time,
                               estimated: false,
                               ground: (this.altitude === "ground"),
                               altitude: this.altitude
                             };
                this.track_linesegs.push(newseg);
                this.history_size ++;
                return;
        }

        var lastseg = this.track_linesegs[this.track_linesegs.length - 1];

        // Determine if track data are intermittent/stale
        // Time difference between two position updates should not be much
        // greater than the difference between data inputs
        // MLAT data are given some more leeway

        var time_difference = (this.last_position_time - prev_time) - (receiver_timestamp - last_timestamp);
        var stale_timeout = (this.position_from_mlat ? 30 : 5);
        var est_track = (time_difference > stale_timeout);

        // Also check if the position was already stale when it was exported by dump1090
        // Makes stale check more accurate for history points spaced 30 seconds apart
        est_track = est_track || ((receiver_timestamp - this.last_position_time) > stale_timeout);

        var ground_track = (this.altitude === "ground");
        
        if (est_track) {

                if (!lastseg.estimated) {
                        // >5s gap in data, create a new estimated segment
                        //console.log(this.icao + " switching to estimated");
                        lastseg.fixed.appendCoordinate(projPrev);
                        this.track_linesegs.push({ fixed: new ol.geom.LineString([projPrev]),
                                                   feature: null,
                                                   update_time: prev_time,
                                                   altitude: 0,
                                                   estimated: true });
                        this.history_size += 2;
                } else {
                        // Keep appending to the existing dashed line; keep every point
                        lastseg.fixed.appendCoordinate(projPrev);
                        lastseg.update_time = prev_time;
                        this.history_size++;
                }

                return true;
        }
        
        if (lastseg.estimated) {
                // We are back to good data (we got two points close in time), switch back to
                // solid lines.
                lastseg.fixed.appendCoordinate(projPrev);
                lastseg = { fixed: new ol.geom.LineString([projPrev]),
                            feature: null,
                            update_time: prev_time,
                            estimated: false,
                            ground: (this.altitude === "ground"),
                            altitude: this.altitude };
                this.track_linesegs.push(lastseg);
                this.history_size += 2;
                return true;
        }
        
        if ( (lastseg.ground && this.altitude !== "ground") ||
             (!lastseg.ground && this.altitude === "ground") || this.altitude !== lastseg.altitude ) {
                //console.log(this.icao + " ground state changed");
                // Create a new segment as the ground state changed.
                // assume the state changed halfway between the two points
                // FIXME needs reimplementing post-google

                lastseg.fixed.appendCoordinate(projPrev);
                this.track_linesegs.push({ fixed: new ol.geom.LineString([projPrev]),
                                           feature: null,
                                           update_time: prev_time,
                                           estimated: false,
                                           altitude: this.altitude,
                                           ground: (this.altitude === "ground") });
                this.history_size += 2;
                return true;
        }
        
        // Add more data to the existing track.
        // We only retain some historical points, at 5+ second intervals,
        // plus the most recent point
        if (prev_time - lastseg.update_time >= 5) {
                // enough time has elapsed; retain the last point and add a new one
                //console.log(this.icao + " retain last point");
                lastseg.fixed.appendCoordinate(projPrev);
                lastseg.update_time = prev_time;
                this.history_size ++;
        }

        return true;
};

// This is to remove the line from the screen if we deselect the plane
PlaneObject.prototype.clearLines = function() {
        for (var i = this.track_linesegs.length - 1; i >= 0 ; --i) {
                var seg = this.track_linesegs[i];
                if (seg.feature !== null) {
                        PlaneTrailFeatures.remove(seg.feature);
                        seg.feature = null;
                }
        }

        if (this.elastic_feature !== null) {
                PlaneTrailFeatures.remove(this.elastic_feature);
                this.elastic_feature = null;
        }
};

PlaneObject.prototype.getDataSource = function() {
    // MLAT
    if (this.position_from_mlat) {
        return 'mlat';
    }

    // Classify as UAT if we heard it on 978 Mhz until we hear it from another source
    if (this.heard_on_978 && !this.heard_on_tisb) {
        return 'uat';
    }

    // Not MLAT, but position reported - ADSB or variants
    if (this.position !== null) {
        return this.addrtype;
    }

    // Otherwise Mode S
    return 'mode_s';

    // TODO: add support for Mode A/C
};

PlaneObject.prototype.getMarkerColor = function() {
        // Emergency squawks override everything else
        if (this.squawk in SpecialSquawks)
                return SpecialSquawks[this.squawk].markerColor;

        var h, s, l;

        var colorArr = this.getAltitudeColor();

        h = colorArr[0];
        s = colorArr[1];
        l = colorArr[2];

        // If we have not seen a recent position update, change color
        if (this.seen_pos > 15) {
                h += ColorByAlt.stale.h;
                s += ColorByAlt.stale.s;
                l += ColorByAlt.stale.l;
        }

        // If this marker is selected, change color
        if (this.selected && !SelectedAllPlanes){
                h += ColorByAlt.selected.h;
                s += ColorByAlt.selected.s;
                l += ColorByAlt.selected.l;
        }

        // If this marker is a mlat position, change color
        if (this.position_from_mlat) {
                h += ColorByAlt.mlat.h;
                s += ColorByAlt.mlat.s;
                l += ColorByAlt.mlat.l;
        }

        return this.hslRepr([h, s, l])
}

PlaneObject.prototype.hslRepr = function(hsl) {
        var h, s, l;
        h = hsl[0];
        s = hsl[1];
        l = hsl[2];

        if (h < 0) {
                h = (h % 360) + 360;
        } else if (h >= 360) {
                h = h % 360;
        }

        if (s < 5) s = 5;
        else if (s > 95) s = 95;

        if (l < 5) l = 5;
        else if (l > 95) l = 95;

        return 'hsl(' + (h/5).toFixed(0)*5 + ',' + (s/5).toFixed(0)*5 + '%,' + (l/5).toFixed(0)*5 + '%)'
}

PlaneObject.prototype.getAltitudeColor = function(altitude) {
        var h, s, l;

        if (typeof altitude === 'undefined') {
            altitude = this.altitude;
        }

        if (altitude === null) {
                h = ColorByAlt.unknown.h;
                s = ColorByAlt.unknown.s;
                l = ColorByAlt.unknown.l;
        } else if (altitude === "ground") {
                h = ColorByAlt.ground.h;
                s = ColorByAlt.ground.s;
                l = ColorByAlt.ground.l;
        } else {
                s = ColorByAlt.air.s;
                l = ColorByAlt.air.l;

                // find the pair of points the current altitude lies between,
                // and interpolate the hue between those points
                var hpoints = ColorByAlt.air.h;
                h = hpoints[0].val;
                for (var i = hpoints.length-1; i >= 0; --i) {
                        if (altitude > hpoints[i].alt) {
                                if (i == hpoints.length-1) {
                                        h = hpoints[i].val;
                                } else {
                                        h = hpoints[i].val + (hpoints[i+1].val - hpoints[i].val) * (altitude - hpoints[i].alt) / (hpoints[i+1].alt - hpoints[i].alt)
                                }
                                break;
                        }
                }
        }

         if (h < 0) {
                h = (h % 360) + 360;
        } else if (h >= 360) {
                h = h % 360;
        }

        if (s < 5) s = 5;
        else if (s > 95) s = 95;

        if (l < 5) l = 5;
        else if (l > 95) l = 95;

        return [h, s, l];
}

PlaneObject.prototype.updateIcon = function() {
        var scaleFactor = Math.max(0.2, Math.min(1.2, 0.15 * Math.pow(1.25, ZoomLvl))).toFixed(1);

        var col = this.getMarkerColor();
        var opacity = 1.0;
        var outline = (this.position_from_mlat ? OutlineMlatColor : OutlineADSBColor);
        var add_stroke = (this.selected && !SelectedAllPlanes) ? ' stroke="black" stroke-width="1px"' : '';
        var baseMarker = getBaseMarker(this.category, this.icaotype, this.typeDescription, this.wtc);
        var rotation = this.track;
        if (rotation === null) {
                rotation = this.true_heading;
        }
        if (rotation === null) {
                rotation = this.mag_heading;
        }
        if (rotation === null) {
                rotation = 0;
        }
        //var transparentBorderWidth = (32 / baseMarker.scale / scaleFactor).toFixed(1);

        var svgKey = col + '!' + outline + '!' + baseMarker.svg + '!' + add_stroke + "!" + scaleFactor;
        var styleKey = opacity + '!' + rotation + '!' + AircraftLabels;

        // New icon or marker change
        if (this.markerStyle === null || this.markerIcon === null || this.markerSvgKey != svgKey) {
                //console.log(this.icao + " new icon and style " + this.markerSvgKey + " -> " + svgKey);

                var icon = new ol.style.Icon({
                        anchor: [0.5, 0.5],
                        anchorXUnits: 'fraction',
                        anchorYUnits: 'fraction',
                        scale: 1.2 * scaleFactor,
                        imgSize: baseMarker.size,
                        src: svgPathToURI(baseMarker.svg, outline, col, add_stroke),
                        rotation: (baseMarker.noRotate ? 0 : rotation * Math.PI / 180.0),
                        opacity: opacity,
                        rotateWithView: (baseMarker.noRotate ? false : true)
                });

                this.markerIcon = icon;

                if (AircraftLabels && this.flight != null) {
                        this.markerStyle = new ol.style.Style({
                                image: this.markerIcon,
                                text: new ol.style.Text({
                                        text: this.flight.trim(),
                                        fill: new ol.style.Fill({color: 'white'}),
                                        backgroundFill: new ol.style.Stroke({color: 'rgba(0, 47, 93, 0.8'}),
                                        textAlign: 'center',
                                        offsetY: -20,
                                        font: '10px Helvetica',
                                        padding: [1,0,0,2]
                                })
                        });
                } else {
                        this.markerStyle = new ol.style.Style({
                                image: this.markerIcon
                        });
                };

                this.markerStaticIcon = null;
                this.markerStaticStyle = new ol.style.Style({});

                this.markerStyleKey = styleKey;
                this.markerSvgKey = svgKey;

                if (this.marker !== null) {
                        this.marker.setStyle(this.markerStyle);
                        this.markerStatic.setStyle(this.markerStaticStyle);
                }
        }

        // Rotation or aircraft label display change
        if (this.markerStyleKey != styleKey) {
                //console.log(this.icao + " new rotation");
                this.markerIcon.setRotation(rotation * Math.PI / 180.0);
                this.markerIcon.setOpacity(opacity);
                if (this.staticIcon) {
                        this.staticIcon.setOpacity(opacity);
                }

                if (AircraftLabels && this.flight != null) {
                        this.markerStyle = new ol.style.Style({
                                image: this.markerIcon,
                                text: new ol.style.Text({
                                        text: this.flight.trim(),
                                        fill: new ol.style.Fill({color: 'white'}),
                                        backgroundFill: new ol.style.Stroke({color: 'rgba(0, 47, 93, 0.8)'}),
                                        textAlign: 'center',
                                        offsetY: -20,
                                        font: '10px Helvetica',
                                        padding: [1,0,0,2]
                                })
                        });
                } else {
                        this.markerStyle = new ol.style.Style({
                                image: this.markerIcon
                        });
                };
                if (this.marker !== null) {
                        this.marker.setStyle(this.markerStyle);
                }
                this.markerStyleKey = styleKey;
        }

        return true;
};

// Update our data
PlaneObject.prototype.updateData = function(receiver_timestamp, data, receiver_source) {
        if (receiver_source == "dump1090-fa") {
                this.heard_on_1090 = true;
                // Ignore messages on 1090 for now if we heard it on 978. We will show multiple data sources in a later release
                if (this.heard_on_978)
                        return
        } else if (receiver_source == "skyaware978") {
                this.heard_on_978 = true;
        }
        // Update all of our data
        this.messages = data.messages;
        this.rssi = data.rssi;
        this.last_message_time = receiver_timestamp - data.seen;


        // simple fields
        var fields = ["alt_baro", "alt_geom", "gs", "ias", "tas", "track",
                      "track_rate", "mag_heading", "true_heading", "mach",
                      "roll", "nav_heading", "nav_modes",
                      "nac_p", "nac_v", "nic_baro", "sil_type", "sil",
                      "nav_qnh", "baro_rate", "geom_rate", "rc",
                      "squawk", "category", "version", "uat_version"];

        for (var i = 0; i < fields.length; ++i) {
                if (fields[i] in data) {
                        this[fields[i]] = data[fields[i]];
                } else {
                        this[fields[i]] = null;
                }
        }

        // fields with more complex behaviour

        if ('type' in data)
                this.addrtype	= data.type;
        else
                this.addrtype   = 'adsb_icao';

        if (this.addrtype == "tisb_trackfile" || this.addrtype == "tisb_icao" || this.addrtype == "tisb_other") {
                this.heard_on_tisb = true;
        }

        if (this.addrtype == "adsr_icao") {
                this.heard_on_adsr = true;
        }

        // don't expire callsigns
        if ('flight' in data)
                this.flight	= data.flight;

        if ('lat' in data && 'lon' in data) {
                this.position   = [data.lon, data.lat];
                this.last_position_time = receiver_timestamp - data.seen_pos;

                if (SitePosition !== null) {
                        this.sitedist = ol.sphere.getDistance(SitePosition, this.position);
                }

                this.position_from_mlat = false;
                if (typeof data.mlat !== "undefined") {
                        for (var i = 0; i < data.mlat.length; ++i) {
                                if (data.mlat[i] === "lat" || data.mlat[i] == "lon") {
                                        this.position_from_mlat = true;
                                        break;
                                }
                        }
                }
        }

        // Pick an altitude
        if ('alt_baro' in data) {
                this.altitude = data.alt_baro;
        } else if ('alt_geom' in data) {
                this.altitude = data.alt_geom;
        } else {
                this.altitude = null;
        }

        // Pick a selected altitude
        if ('nav_altitude_fms' in data) {
                this.nav_altitude = data.nav_altitude_fms;
        } else if ('nav_altitude_mcp' in data) {
                this.nav_altitude = data.nav_altitude_mcp;
        } else {
                this.nav_altitude = null;
        }

        // Pick vertical rate from either baro or geom rate
        // geometric rate is generally more reliable (smoothed etc)
        if ('geom_rate' in data) {
                this.vert_rate = data.geom_rate;
        } else if ('baro_rate' in data) {
                this.vert_rate = data.baro_rate;
        } else {
                this.vert_rate = null;
        }

        // Pick a speed
        if ('gs' in data) {
                this.speed = data.gs;
        } else if ('tas' in data) {
                this.speed = data.tas;
        } else if ('ias' in data) {
                this.speed = data.ias;
        } else {
                this.speed = null;
        }
};

PlaneObject.prototype.updateTick = function(receiver_timestamp, last_timestamp) {
        // recompute seen and seen_pos
        this.seen = receiver_timestamp - this.last_message_time;
        this.seen_pos = (this.last_position_time === null ? null : receiver_timestamp - this.last_position_time);
        
        // If no packet in over 58 seconds, clear the plane.
        if (this.seen > 58) {
                if (this.visible) {
                        //console.log("hiding " + this.icao);
                        this.clearMarker();
                        this.visible = false;
                        if (SelectedPlane == this.icao)
                                selectPlaneByHex(null,false);
                }
        } else {
                if (this.position !== null && (this.selected || this.seen_pos < 60)) {
                        this.visible = true;
                        if (this.updateTrack(receiver_timestamp, last_timestamp)) {
                                this.updateLines();
                                this.updateMarker(true);
                        } else { 
                                this.updateMarker(false); // didn't move
                        }
                } else {
                        this.clearMarker();
                        this.visible = false;
                }
        }
};

PlaneObject.prototype.clearMarker = function() {
        if (this.marker) {
                PlaneIconFeatures.remove(this.marker);
                PlaneIconFeatures.remove(this.markerStatic);
                /* FIXME google.maps.event.clearListeners(this.marker, 'click'); */
                this.marker = this.markerStatic = null;
        }
};

// Update our marker on the map
PlaneObject.prototype.updateMarker = function(moved) {
        if (!this.visible || this.position == null || this.isFiltered()) {
                this.clearMarker();
                return;
        }
        
        this.updateIcon();
        if (this.marker) {
                if (moved) {
                        this.marker.setGeometry(new ol.geom.Point(ol.proj.fromLonLat(this.position)));
                        this.markerStatic.setGeometry(new ol.geom.Point(ol.proj.fromLonLat(this.position)));
                }
        } else {
                this.marker = new ol.Feature(new ol.geom.Point(ol.proj.fromLonLat(this.position)));
                this.marker.hex = this.icao;
                this.marker.setStyle(this.markerStyle);
                PlaneIconFeatures.push(this.marker);

                this.markerStatic = new ol.Feature(new ol.geom.Point(ol.proj.fromLonLat(this.position)));
                this.markerStatic.hex = this.icao;
                this.markerStatic.setStyle(this.markerStaticStyle);
                PlaneIconFeatures.push(this.markerStatic);
	}
};


// return the styling of the lines based on altitude
PlaneObject.prototype.altitudeLines = function(altitude) {
    var colorArr = this.getAltitudeColor(altitude);
    return new ol.style.Style({
        stroke: new ol.style.Stroke({
            color: 'hsl(' + (colorArr[0]/5).toFixed(0)*5 + ',' + (colorArr[1]/5).toFixed(0)*5 + '%,' + (colorArr[2]/5).toFixed(0)*5 + '%)',
            width: 2
        })
    })
}

// Update our planes tail line,
PlaneObject.prototype.updateLines = function() {
        if (!this.selected)
                return;

        if (this.track_linesegs.length == 0)
                return;

        var estimateStyle = new ol.style.Style({
                stroke: new ol.style.Stroke({
                        color: '#a08080',
                        width: 1.5,
                        lineDash: [3, 3]
                })
        });

        var airStyle = new ol.style.Style({
                stroke: new ol.style.Stroke({
                        color: '#000000',
                        width: 2
                })
        });

        var groundStyle = new ol.style.Style({
                stroke: new ol.style.Stroke({
                        color: '#408040',
                        width: 2
                })
        });

        // find the old elastic band so we can replace it in place
        // (which should be faster than remove-and-add when PlaneTrailFeatures is large)
        var oldElastic = -1;
        if (this.elastic_feature !== null) {
                oldElastic = PlaneTrailFeatures.getArray().indexOf(this.elastic_feature);
        }

        // create the new elastic band feature
        var lastseg = this.track_linesegs[this.track_linesegs.length - 1];
        var lastfixed = lastseg.fixed.getCoordinateAt(1.0);
        var geom = new ol.geom.LineString([lastfixed, ol.proj.fromLonLat(this.position)]);
        this.elastic_feature = new ol.Feature(geom);
        if (lastseg.estimated) {
                this.elastic_feature.setStyle(estimateStyle);
        } else {
                this.elastic_feature.setStyle(this.altitudeLines(lastseg.altitude));
        }

        if (oldElastic < 0) {
                PlaneTrailFeatures.push(this.elastic_feature);
        } else {
                PlaneTrailFeatures.setAt(oldElastic, this.elastic_feature);
        }

        // create any missing fixed line features
        for (var i = 0; i < this.track_linesegs.length; ++i) {
                var seg = this.track_linesegs[i];
                if (seg.feature === null) {
                        seg.feature = new ol.Feature(seg.fixed);
                        if (seg.estimated) {
                                seg.feature.setStyle(estimateStyle);
                        } else {
                                seg.feature.setStyle(this.altitudeLines(seg.altitude));
                        }

                        PlaneTrailFeatures.push(seg.feature);
                }
        }
};

PlaneObject.prototype.destroy = function() {
        this.clearLines();
        this.clearMarker();
};
