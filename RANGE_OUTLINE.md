# Range Outline Feature Documentation

## Overview

The Range Outline feature tracks and visualizes the maximum detection range of the ADS-B receiver at each bearing (0-359 degrees). It displays an altitude-colored outline on the map showing the actual coverage area based on received aircraft positions, providing a real-time view of detection capabilities that adapts to environmental conditions, antenna characteristics, and terrain. Each segment of the outline is colored according to the altitude of the aircraft at maximum range for that bearing, creating a gradient visualization that shows both coverage and altitude information.

## How It Works

### Backend (C)

#### Data Tracking (`track.c`)

The backend continuously tracks aircraft positions and calculates the maximum detection range at each degree bearing from the receiver location:

1. **Position Updates** (`updatePosition()` at line 626):
   - Each time an aircraft position is decoded, `update_range_outline()` is called
   - Distance and bearing from receiver to aircraft are calculated using great circle formulas

2. **Range Outline Updates** (`update_range_outline()` starting at line 273):
   - Accepts aircraft object to access altitude information
   - Calculates the great circle distance from receiver to aircraft
   - Calculates the bearing from receiver to aircraft (0-359 degrees)
   - Rounds bearing to nearest integer degree
   - Retrieves altitude (prefers barometric, falls back to geometric)
   - Updates maximum range for that bearing if:
     - This is the first position at this bearing, OR
     - This distance exceeds the previous maximum for this bearing
   - Records timestamp and altitude of the update

3. **Data Structure** (`dump1090.h` lines 409-414):
   ```c
   double range_outline_max[RANGE_OUTLINE_DEGREES];      // Maximum range at each bearing (meters)
   uint64_t range_outline_updated[RANGE_OUTLINE_DEGREES]; // Timestamp when each bearing was last updated
   int range_outline_altitude[RANGE_OUTLINE_DEGREES];     // Altitude (feet) of aircraft at maximum range
   char *range_outline_persistence_file;                   // File to persist range outline data
   uint64_t range_outline_retention_ms;                    // Current retention period in milliseconds
   ```

4. **Data Expiration** (`expireRangeOutline()` at line 1472 in `track.c`):
   - Called every second by `trackPeriodicUpdate()`
   - Iterates through all 360 bearings
   - Resets bearings to 0 if timestamp exceeds retention period
   - Allows the outline to adapt to changing conditions over time

#### Data Persistence (`dump1090.c`)

Range outline data persists across application restarts:

1. **Save Function** (`saveRangeOutline()` starting at line 105):
   - Writes binary file with three arrays
   - Called every 60 seconds by `backgroundTasks()`
   - Also called at shutdown
   - File format: `double[360] ranges | uint64_t[360] timestamps | int[360] altitudes`

2. **Load Function** (`loadRangeOutline()` starting at line 125):
   - Reads binary file at startup
   - Restores previous range, timestamp, and altitude data
   - Logs success/failure messages

3. **Storage Location**:
   - Default: `/tmp/range_outline.dat`
   - When `--write-json <dir>` is used: `<dir>/range_outline.dat`

#### JSON Generation (`net_io.c`)

The backend generates JSON output for the web interface:

1. **Function** (`generateRangeOutlineJson()` starting at line 1732):
   - Generates JSON with current timestamp, range array, timestamp array, and altitude array
   - Applies retention filter: outputs 0/null for bearings outside retention window
   - Called by `backgroundTasks()` at the JSON update interval

2. **JSON Format**:
   ```json
   {
     "now": 1760394506.0,
     "range_outline": [104761, 211010, ...360 values in meters...],
     "range_outline_timestamps": [1760390035.1, 1760389259.8, ...360 values in seconds...],
     "range_outline_altitudes": [35000, 28000, null, ...360 values in feet or null...]
   }
   ```

3. **Output File**: `data/range_outline.json` (written at same interval as `aircraft.json`)

### Frontend (JavaScript)

#### Initialization (`public_html/script.js`)

1. **Global Variables** (lines 10-13):
   ```javascript
   var RangeOutlineFeature = null;  // OpenLayers feature containing the polygon
   var RangeOutlineLayer = null;    // OpenLayers vector layer for display
   var ShowRangeOutline = false;    // Toggle state
   var RangeOutlineData = null;     // Cached JSON data from server
   ```

2. **Startup** (`initRangeOutline()`):
   - Called from `end_load_history()` after map initialization
   - Reads `ShowRangeOutline` preference from localStorage
   - If enabled, fetches data and updates checkbox visual state
   - Ensures UI state matches saved preference after browser refresh

3. **UI Setup** (`initialize_map()`):
   - Adds checkbox to settings panel (HTML in `public_html/index.html`)
   - Registers click handler for `toggleRangeOutline()`
   - Synchronizes checkbox appearance with boolean state

#### Data Fetching

1. **Periodic Updates** (`fetchData()`):
   - If `ShowRangeOutline` is enabled, calls `fetchRangeOutline()` each refresh interval
   - Runs alongside aircraft data updates

2. **AJAX Request** (`fetchRangeOutline()`):
   - Fetches `data/range_outline.json` from server
   - 5-second timeout, no caching
   - On success: stores data and calls `updateRangeOutline()`
   - On failure: silently ignores (outline is optional feature)

#### Altitude-Colored Outline Rendering

1. **Coordinate Conversion** (`updateRangeOutline()`):
   - Validates data structure (must have 360 ranges, timestamps, and altitudes)
   - Iterates through all 360 bearings
   - For each bearing:
     - Checks if range > 0 and timestamp > 0 (backend sends 0 for expired data)
     - Uses effective range (actual if valid, 1 meter if invalid to create small circle at center)
     - Calculates lat/lon using `destinationPoint()` Haversine formula
     - Converts to map projection coordinates
     - Stores altitude information for coloring

2. **Haversine Calculation** (`destinationPoint()`):
   - Takes lat/lon origin, bearing, and distance in meters
   - Returns destination lat/lon point
   - Earth radius: 6,371,000 meters
   - Uses standard spherical trigonometry formulas

3. **Multi-Segment Rendering**:
   - Creates 360 individual LineString segments (one for each degree transition)
   - Each segment connects bearing N to bearing N+1 (with wraparound from 359 to 0)
   - Always draws complete circle: segments without data stay at 1 meter radius
   - Color calculation for each segment:
     - If both endpoints have valid altitude: uses midpoint altitude
     - If only one endpoint has valid altitude: uses that altitude
     - If no altitude data: uses default blue color
   - Uses existing `PlaneObject.prototype.getAltitudeColor()` function for altitude-to-color mapping
   - Uses existing `PlaneObject.prototype.hslRepr()` function for HSL color conversion
   - Layer properties:
     - Width: 2 pixels
     - No fill
     - zIndex: 99 (below site circles, above base layers)
   - Creates smooth gradient effect as colors transition between adjacent segments

#### User Controls

1. **Toggle Function** (`toggleRangeOutline()`):
   - Inverts `ShowRangeOutline` boolean
   - Saves state to localStorage for persistence
   - When enabling:
     - Fetches fresh data from server
     - Data fetch callback updates display
   - When disabling:
     - Hides the layer (`setVisible(false)`)
     - Clears all line segment features from the source

2. **State Persistence**:
   - Uses browser localStorage with key `ShowRangeOutline`
   - Values: `'true'` or `'false'` (string)
   - Restored on page load by `initRangeOutline()`

## User Configuration

### Backend Configuration

#### Command-Line Options

The feature adds one new command-line option and uses existing options:

- **Range Outline Retention Period**: Use `--range-outline-retention <hours>` to control data retention
  - Default: 24 hours
  - Specified in hours (accepts decimal values)
  - Example: `--range-outline-retention 48` for 48 hours
  - Example: `--range-outline-retention 0.5` for 30 minutes
  - Controls how long range data is kept before expiring

- **Data Directory**: Use `--write-json <directory>` to specify where JSON and persistence files are written
  - Default JSON location: Uses the directory specified by `--write-json`
  - Persistence file: Automatically placed in same directory as `range_outline.dat`
  - If `--write-json` not specified, persistence uses `/tmp/range_outline.dat`

- **JSON Update Interval**: Use `--write-json-every <seconds>` to control update frequency
  - Default: 1.0 seconds
  - Minimum: 0.1 seconds
  - Affects how often `range_outline.json` is regenerated

#### Retention Period Details

The data retention period controls how long range data is kept before expiring.

**Default Value**: 24 hours (defined in `dump1090.h` line 278 as `RANGE_OUTLINE_DEFAULT_RETENTION_HOURS`)

**Runtime Configuration**: Use the `--range-outline-retention <hours>` command-line option to override the default at startup.

**Retention Period Behavior**:
- Data older than the retention period is automatically expired and reset to 0
- Expiration happens every second via `expireRangeOutline()` in `track.c`
- Allows outline to adapt to changing conditions (weather, seasonal foliage, antenna adjustments)
- Stored internally as `Modes.range_outline_retention_ms` (converted to milliseconds)

### Frontend Configuration

#### User Controls

The web interface provides a simple on/off toggle:

1. **Location**: Settings panel → "Range Outline" checkbox (below "Site Position and Range Rings")

2. **Behavior**:
   - Click to enable: Fetches data and displays polygon on map
   - Click to disable: Hides polygon, stops fetching updates
   - State persists across browser sessions via localStorage

#### Visual Customization

The outline is automatically colored based on altitude using the same color scale as aircraft markers. Each line segment is colored according to the altitude of the aircraft at maximum range for that bearing.

**Altitude Color Scale**:
- The colors are determined by `PlaneObject.prototype.getAltitudeColor()` in `planeObject.js`
- Higher altitudes appear in different hues than lower altitudes
- Colors smoothly gradient between adjacent bearings

**Line Width**: To change the line thickness, edit `public_html/script.js` in the `updateRangeOutline()` function around line 2995:
```javascript
lineFeature.setStyle(new ol.style.Style({
    stroke: new ol.style.Stroke({
        color: color,
        width: 2  // Change this value (in pixels)
    })
}));
```

**Default Color for No Altitude Data**: Segments without altitude information use `'rgba(0, 128, 255, 0.8)'` (semi-transparent blue). This can be changed in the same location around line 2992.

## Technical Flow

### Startup Sequence

1. **Backend Initialization** (`dump1090.c` `main()`):
   ```
   modesInitConfig()
       ↓ Sets default persistence file: /tmp/range_outline.dat
       ↓ Sets default retention: 24 hours * 3600 * 1000 ms
   Parse --write-json argument
       ↓ Updates persistence file path: <json_dir>/range_outline.dat
   loadRangeOutline()
       ↓ Reads persisted data from file
       ↓ Restores range_outline_max[] and range_outline_updated[]
   writeJsonToFile("range_outline.json", generateRangeOutlineJson)
       ↓ Writes initial JSON (may be empty or contain loaded data)
   ```

2. **Frontend Initialization** (`script.js`):
   ```
   initialize()
       ↓ Sets up UI, hides range_outline_column initially
   initialize_map()
       ↓ Shows range_outline_column if SitePosition is configured
       ↓ Registers checkbox click handler
   end_load_history()
       ↓ initRangeOutline()
           ↓ Reads localStorage['ShowRangeOutline']
           ↓ If 'true': sets ShowRangeOutline = true
           ↓ Calls fetchRangeOutline()
           ↓ Updates checkbox appearance
   fetchData() starts periodic refresh loop
   ```

### Runtime Data Flow

1. **Aircraft Position Received**:
   ```
   demodulate2400() / demodulate2400AC() [demod_2400.c]
       ↓ Decodes Mode S message
   detectModeS() [mode_s.c]
       ↓ Validates message
   useModesMessage() [mode_s.c]
       ↓ Processes valid message
   decodeModesMessage() [mode_s.c]
       ↓ Extracts position data
   trackUpdateFromMessage() [track.c]
       ↓ Updates aircraft state
   updatePosition() [track.c]
       ↓ Validates new position
       ↓ update_range_outline(aircraft)
           ↓ greatcircle(receiver, aircraft) → distance in meters
           ↓ get_bearing(receiver, aircraft) → bearing 0-359°
           ↓ bearing_idx = round(bearing) % 360
           ↓ Get altitude (prefer barometric, fallback to geometric)
           ↓ if (distance > max[bearing_idx] || no data yet)
               ↓ range_outline_max[bearing_idx] = distance
               ↓ range_outline_updated[bearing_idx] = now (ms)
               ↓ range_outline_altitude[bearing_idx] = altitude (feet)
   ```

2. **Periodic Updates (Every 1 Second)**:
   ```
   trackPeriodicUpdate() [track.c]
       ↓ expireRangeOutline()
           ↓ for each bearing (0-359):
               ↓ if (now - updated[bearing]) > retention_ms
                   ↓ range_outline_max[bearing] = 0
                   ↓ range_outline_updated[bearing] = 0
   ```

3. **JSON Generation (Every 1 Second by Default)**:
   ```
   backgroundTasks() [dump1090.c]
       ↓ if (now >= next_json)
           ↓ writeJsonToFile("range_outline.json", generateRangeOutlineJson)
               ↓ generateRangeOutlineJson() [net_io.c]
                   ↓ for each bearing (0-359):
                       ↓ if updated[bearing] != 0 && (now - updated[bearing]) <= retention_ms
                           ↓ output: range_outline[bearing] = max[bearing]
                           ↓ output: range_outline_timestamps[bearing] = updated[bearing]
                           ↓ output: range_outline_altitudes[bearing] = altitude[bearing] or null
                       ↓ else
                           ↓ output: range_outline[bearing] = 0
                           ↓ output: range_outline_timestamps[bearing] = 0
                           ↓ output: range_outline_altitudes[bearing] = null
   ```

4. **Persistence (Every 60 Seconds + Shutdown)**:
   ```
   backgroundTasks() [dump1090.c]
       ↓ if (now >= next_range_outline_save)
           ↓ saveRangeOutline()
               ↓ fopen(persistence_file, "wb")
               ↓ fwrite(range_outline_max[360])
               ↓ fwrite(range_outline_updated[360])
               ↓ fwrite(range_outline_altitude[360])
               ↓ fclose()
   ```

5. **Frontend Display (Every Refresh Interval)**:
   ```
   fetchData() [script.js]
       ↓ if (ShowRangeOutline)
           ↓ fetchRangeOutline()
               ↓ $.ajax("data/range_outline.json")
               ↓ on success:
                   ↓ RangeOutlineData = data
                   ↓ updateRangeOutline()
                       ↓ Build points array for all 360 bearings:
                           ↓ range = data.range_outline[bearing]
                           ↓ timestamp = data.range_outline_timestamps[bearing]
                           ↓ altitude = data.range_outline_altitudes[bearing]
                           ↓ isValid = (range > 0 && timestamp > 0)
                           ↓ effectiveRange = isValid ? range : 1
                           ↓ point = destinationPoint(receiver, bearing, effectiveRange)
                           ↓ points[bearing] = {coord, altitude, isValid}
                       ↓ Clear existing features from layer
                       ↓ for bearing = 0 to 359:
                           ↓ nextBearing = (bearing + 1) % 360
                           ↓ Create LineString from points[bearing] to points[nextBearing]
                           ↓ Calculate color based on altitude(s)
                           ↓ Apply style with calculated color
                           ↓ Add feature to layer
                       ↓ RangeOutlineLayer.setVisible(true)
   ```

## File Modifications

### New Files
- `RANGE_OUTLINE.md` - This documentation file

### Modified Files

1. **`dump1090.h`** - Data structure definitions
   - Lines 276-277: Constants for degrees and default retention
   - Lines 409-414: State variables in `struct _Modes` (includes altitude array)

2. **`dump1090.c`** - Persistence and initialization
   - Lines 105-149: `saveRangeOutline()` and `loadRangeOutline()` functions
   - Default configuration in `modesInitConfig()`
   - Periodic save logic in `backgroundTasks()` (every 60 seconds)
   - JSON generation call in `backgroundTasks()`
   - Path update when `--write-json` parsed
   - Load persisted data at startup
   - Write initial JSON at startup
   - Save data at shutdown

3. **`track.c`** - Position tracking and expiration
   - Lines 273-300: `update_range_outline()` function (updated to accept aircraft object and store altitude)
   - Line 635: Call to `update_range_outline()` in `updatePosition()`
   - Lines 1472-1480: `expireRangeOutline()` function
   - Line 1497: Call to `expireRangeOutline()` in `trackPeriodicUpdate()`

4. **`net_io.c`** - JSON generation
   - Lines 1732-1789: `generateRangeOutlineJson()` function (includes altitude array output)

5. **`net_io.h`** - Function declaration
   - Declaration of `generateRangeOutlineJson()`

6. **`public_html/index.html`** - UI element
   - Range outline checkbox in settings panel

7. **`public_html/script.js`** - Frontend logic
   - Lines 10-13: Global variables
   - Line 221-224: Fetch call in `fetchData()`
   - Line 759: Initialize on startup in `end_load_history()`
   - Lines 1115-1126: Checkbox setup in `initialize_map()`
   - Line 1148: Show column if site position configured
   - Lines 2878-3056: Range outline functions (fetch, update with multi-segment rendering, destinationPoint, toggle, init)

### Generated Files

1. **`data/range_outline.json`** - Runtime JSON output
   - Generated by backend every JSON update interval
   - Read by frontend for display

2. **`data/range_outline.dat` (or `/tmp/range_outline.dat`)** - Persistence file
   - Binary format: ranges (double[360]) + timestamps (uint64[360]) + altitudes (int[360])
   - Updated every 60 seconds and at shutdown
   - Loaded at startup
   - **Note**: If upgrading from a version without altitude support, delete this file to start fresh

## Design Decisions

### Why 360 Degrees?
- Provides sufficient angular resolution for most use cases
- Balance between detail and memory/performance
- 1-degree precision is fine for typical ADS-B reception ranges (50-400+ km)

### Why 24-Hour Default Retention?
- Long enough to capture daily patterns and build complete outline
- Short enough to adapt to changing conditions (weather, foliage, antenna modifications)
- Prevents indefinite growth from one-off long-distance reception events

### Why Binary Persistence Format?
- Compact: 3 arrays × 360 elements = ~11 KB
- Fast to read/write
- Simple implementation without dependencies

### Why No Fill Color?
- User preference: outline-only provides clear boundary without obscuring map
- Reduces visual clutter
- Better visibility of aircraft icons inside coverage area

### Why Altitude-Based Coloring?
- Provides additional insight into coverage characteristics
- High-altitude aircraft typically have longer ranges
- Color gradients show both coverage and altitude profile at a glance
- Uses existing altitude color scale for consistency with aircraft display

### Why Multi-Segment Rendering Instead of Single Polygon?
- Allows individual coloring of each degree transition
- Creates smooth gradient effect between different altitudes
- More flexible for future enhancements
- Always shows complete circle (small radius for bearings without data yet)

### Why Layer Visibility Instead of Add/Remove?
- More efficient: layer and feature persist in memory
- Faster toggle response
- Maintains geometry when disabled, instant re-show when enabled
- Standard OpenLayers pattern

### Why Dual Retention Filtering?
- Backend applies actual expiration: zeros out in-memory data older than retention period
- JSON generation applies read-time filter: outputs 0 for expired bearings
- Frontend skips rendering points with 0 range values
- This layered approach ensures data consistency and clean visualization

## Troubleshooting

### Outline Not Appearing

1. **Check receiver position is configured**:
   - Settings panel → "Site Position and Range Rings" must be enabled
   - Position must be set via `--lat` and `--lon` command-line options

2. **Check data exists**:
   - Look for `data/range_outline.json` in your JSON directory
   - File should contain non-zero values in `range_outline` array
   - If all zeros, no aircraft have been received yet or data has expired

3. **Check browser console for errors**:
   - Press F12 to open developer tools
   - Look for failed AJAX requests or JavaScript errors

### Outline Shows Then Disappears

- **Cause**: Data has expired (exceeds retention period with no new aircraft at those bearings)
- **Solution**: Wait for aircraft to be received again, or increase retention period using `--range-outline-retention <hours>` and restart dump1090

### Checkbox State Wrong After Refresh

- Check browser localStorage is enabled
- Verify `initRangeOutline()` is being called on page load

### Outline Doesn't Update

1. **Check ShowRangeOutline is enabled** (checkbox checked)
2. **Check fetchData() is running** (aircraft list updating?)
3. **Check backend is generating JSON** (`ls -l data/range_outline.json` shows recent timestamp)
4. **Check backend is receiving aircraft** (dump1090 console shows messages?)

## Feature Highlights

### Altitude-Based Gradient Visualization
The range outline displays a color gradient based on the altitude of aircraft at maximum range for each bearing:

- **Purple/Magenta**: High altitude aircraft (typically 35,000+ feet)
- **Blue/Cyan**: Medium altitude aircraft (typically 15,000-30,000 feet)
- **Green/Yellow**: Lower altitude aircraft (below 15,000 feet)
- **Smooth Gradients**: Colors smoothly transition between adjacent bearings

This provides immediate visual feedback about your coverage profile:
- Areas with high-altitude colors suggest good line-of-sight coverage
- Lower altitude colors may indicate terrain limitations or closer aircraft
- Uniform coloring suggests consistent altitude coverage
- Varied coloring shows diverse altitude profiles across different directions

### Dynamic Coverage Display
- Starts as a small circle at receiver location
- Gradually expands outward as aircraft are tracked at each bearing
- Segments with no data yet remain at center (1 meter radius)
- Segments extend to maximum observed range when aircraft are detected

## Future Enhancements

Potential improvements not currently implemented:

1. **Multiple Outline Layers**:
   - Show 24-hour, 7-day, and 30-day outlines simultaneously
   - Different rendering styles for each time period

2. **Export/Import**:
   - Download range outline data as GeoJSON
   - Import previously saved outlines

3. **Statistics Display**:
   - Total coverage area calculation
   - Coverage percentage by direction
   - Identify weak coverage areas
   - Altitude distribution analysis

4. **Altitude Band Filtering**:
   - Toggle between different altitude bands
   - Compare low-altitude vs high-altitude coverage
   - Better understanding of ground-level vs. high-altitude capabilities

5. **Historical Comparison**:
   - Overlay previous period's outline to see changes
   - Detect antenna degradation or improvements
   - Track seasonal variations
