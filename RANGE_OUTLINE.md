# Range Outline Feature Documentation

## Overview

The Range Outline feature tracks and visualizes the maximum detection range of the ADS-B receiver at each bearing (0-359 degrees). It displays a polygon on the map showing the actual coverage area based on received aircraft positions, providing a real-time view of detection capabilities that adapts to environmental conditions, antenna characteristics, and terrain.

## How It Works

### Backend (C)

#### Data Tracking (`track.c`)

The backend continuously tracks aircraft positions and calculates the maximum detection range at each degree bearing from the receiver location:

1. **Position Updates** (`updatePosition()` at line 626):
   - Each time an aircraft position is decoded, `update_range_outline()` is called
   - Distance and bearing from receiver to aircraft are calculated using great circle formulas

2. **Range Outline Updates** (`update_range_outline()` starting at line 273):
   - Calculates the great circle distance from receiver to aircraft
   - Calculates the bearing from receiver to aircraft (0-359 degrees)
   - Rounds bearing to nearest integer degree
   - Updates maximum range for that bearing if:
     - This is the first position at this bearing, OR
     - This distance exceeds the previous maximum for this bearing
   - Records timestamp of the update

3. **Data Structure** (`dump1090.h` lines 410-412):
   ```c
   double range_outline_max[360];      // Maximum range at each bearing (meters)
   uint64_t range_outline_updated[360]; // Timestamp of last update (milliseconds)
   uint64_t range_outline_retention_ms; // Data retention period (milliseconds)
   ```

4. **Data Expiration** (`track.c` `expireRangeOutline()` starting at line 1468):
   - Called every second by `trackPeriodicUpdate()`
   - Checks each bearing's timestamp
   - Resets range and timestamp to 0 if data exceeds retention period
   - Allows the outline to adapt to changing conditions over time

#### Data Persistence (`dump1090.c`)

Range outline data persists across application restarts:

1. **Save Function** (`saveRangeOutline()` starting at line 104):
   - Writes binary file with version header and both arrays
   - Called every 60 seconds by `backgroundTasks()` (line 634)
   - Also called at shutdown (line 1027)
   - File format: `uint32_t version | double[360] ranges | uint64_t[360] timestamps`

2. **Load Function** (`loadRangeOutline()` starting at line 127):
   - Reads binary file at startup (called at line 940)
   - Validates version number
   - Restores previous range and timestamp data
   - Logs success/failure messages

3. **Storage Location**:
   - Default: `/tmp/range_outline.dat`
   - When `--write-json <dir>` is used: `<dir>/range_outline.dat`
   - Configured at lines 199 and 833-836

#### JSON Generation (`net_io.c`)

The backend generates JSON output for the web interface:

1. **Function** (`generateRangeOutlineJson()` starting at line 1732):
   - Generates JSON with current timestamp, range array, and timestamp array
   - Applies retention filter: outputs 0 for bearings outside retention window
   - Called by `backgroundTasks()` at the JSON update interval (line 608)

2. **JSON Format**:
   ```json
   {
     "now": 1760394506.0,
     "range_outline": [104761, 211010, ...360 values in meters...],
     "range_outline_timestamps": [1760390035.1, 1760389259.8, ...360 values in seconds...]
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

2. **Startup** (`initRangeOutline()` at line 3017):
   - Called from `end_load_history()` (line 760) after map initialization
   - Reads `ShowRangeOutline` preference from localStorage
   - If enabled, fetches data and updates checkbox visual state
   - Ensures UI state matches saved preference after browser refresh

3. **UI Setup** (`initialize_map()` starting at line 1117):
   - Adds checkbox to settings panel (HTML at `public_html/index.html` line 134)
   - Registers click handler for `toggleRangeOutline()`
   - Synchronizes checkbox appearance with boolean state

#### Data Fetching

1. **Periodic Updates** (`fetchData()` at line 221):
   - If `ShowRangeOutline` is enabled, calls `fetchRangeOutline()` each refresh interval
   - Runs alongside aircraft data updates

2. **AJAX Request** (`fetchRangeOutline()` starting at line 2879):
   - Fetches `data/range_outline.json` from server
   - 5-second timeout, no caching
   - On success: stores data and calls `updateRangeOutline()`
   - On failure: silently ignores (outline is optional feature)

#### Polygon Rendering

1. **Coordinate Conversion** (`updateRangeOutline()` starting at line 2897):
   - Validates data structure (must have 360 ranges and timestamps)
   - Iterates through all 360 bearings
   - For each bearing:
     - Checks if range > 0 and timestamp > 0 (backend sends 0 for expired data)
     - Uses effective range (actual if valid, 1 meter if invalid to keep point at center)
     - Calculates lat/lon using `destinationPoint()` Haversine formula
     - Converts to map projection coordinates
   - Closes polygon by appending first coordinate to end

2. **Haversine Calculation** (`destinationPoint()` starting at line 2977):
   - Takes lat/lon origin, bearing, and distance in meters
   - Returns destination lat/lon point
   - Earth radius: 6,371,000 meters
   - Uses standard spherical trigonometry formulas

3. **OpenLayers Integration**:
   - Creates `ol.geom.Polygon` from coordinate array
   - First render: creates Feature with blue stroke style, adds to new Vector layer
   - Subsequent updates: updates existing Feature geometry
   - Layer properties:
     - Stroke: `rgba(0, 128, 255, 0.8)` (semi-transparent blue)
     - Width: 2 pixels
     - No fill (removed per user request)
     - zIndex: 99 (below site circles, above base layers)

#### User Controls

1. **Toggle Function** (`toggleRangeOutline()` starting at line 2995):
   - Inverts `ShowRangeOutline` boolean
   - Saves state to localStorage for persistence
   - When enabling:
     - Fetches fresh data from server
     - Data fetch callback updates display
   - When disabling:
     - Hides the layer (`setVisible(false)`)
     - Clears feature geometry

2. **State Persistence**:
   - Uses browser localStorage with key `ShowRangeOutline`
   - Values: `'true'` or `'false'` (string)
   - Restored on page load by `initRangeOutline()`

## User Configuration

### Backend Configuration

#### Command-Line Options

No new command-line options were added. The feature uses existing options:

- **Data Directory**: Use `--write-json <directory>` to specify where JSON and persistence files are written
  - Default JSON location: Uses the directory specified by `--write-json`
  - Persistence file: Automatically placed in same directory as `range_outline.dat`
  - If `--write-json` not specified, persistence uses `/tmp/range_outline.dat`

- **JSON Update Interval**: Use `--write-json-every <seconds>` to control update frequency
  - Default: 1.0 seconds
  - Minimum: 0.1 seconds
  - Affects how often `range_outline.json` is regenerated

#### Runtime Settings

**Data Retention Period** (defined in `dump1090.h` line 277):
```c
#define RANGE_OUTLINE_DEFAULT_RETENTION_HOURS 24
```

- **Default**: 24 hours
- **Behavior**: Data older than retention period is automatically expired and reset to 0
- **Purpose**: Allows outline to adapt to changing conditions (weather, seasonal foliage, antenna adjustments)
- **Stored in**: `Modes.range_outline_retention_ms` (converted to milliseconds)
- **To Change**: Modify the `#define` constant and recompile

**Note**: There is currently no command-line option to change retention period at runtime. This could be added if needed by:
1. Adding a new command-line option like `--range-outline-retention <hours>`
2. Parsing it in `main()` and updating `Modes.range_outline_retention_ms`

### Frontend Configuration

#### User Controls

The web interface provides a simple on/off toggle:

1. **Location**: Settings panel → "Range Outline" checkbox (below "Site Position and Range Rings")

2. **Behavior**:
   - Click to enable: Fetches data and displays polygon on map
   - Click to disable: Hides polygon, stops fetching updates
   - State persists across browser sessions via localStorage

#### Visual Customization

To modify the outline appearance, edit `public_html/script.js` line 2948:

```javascript
RangeOutlineFeature.setStyle(new ol.style.Style({
    stroke: new ol.style.Stroke({
        color: 'rgba(0, 128, 255, 0.8)',  // Color: RGBA (red, green, blue, alpha)
        width: 2                           // Line width in pixels
    })
}));
```

**Color Options**:
- Current: `rgba(0, 128, 255, 0.8)` - semi-transparent blue
- Examples:
  - Solid red: `'rgba(255, 0, 0, 1.0)'`
  - Green: `'rgba(0, 255, 0, 0.8)'`
  - Yellow: `'rgba(255, 255, 0, 0.8)'`
  - Purple: `'rgba(128, 0, 255, 0.8)'`

**Width**: Change the `width` value (in pixels) to make the line thicker or thinner

**Fill** (currently disabled): To add interior fill, add after the stroke definition:
```javascript
fill: new ol.style.Fill({
    color: 'rgba(0, 128, 255, 0.2)'  // Very transparent for subtle fill
})
```

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
       ↓ update_range_outline(lat, lon)
           ↓ greatcircle(receiver, aircraft) → distance in meters
           ↓ get_bearing(receiver, aircraft) → bearing 0-359°
           ↓ bearing_idx = round(bearing) % 360
           ↓ if (distance > max[bearing_idx] || no data yet)
               ↓ range_outline_max[bearing_idx] = distance
               ↓ range_outline_updated[bearing_idx] = now (ms)
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
                       ↓ else
                           ↓ output: range_outline[bearing] = 0
                           ↓ output: range_outline_timestamps[bearing] = 0
   ```

4. **Persistence (Every 60 Seconds + Shutdown)**:
   ```
   backgroundTasks() [dump1090.c]
       ↓ if (now >= next_range_outline_save)
           ↓ saveRangeOutline()
               ↓ fopen(persistence_file, "wb")
               ↓ fwrite(version = 1)
               ↓ fwrite(range_outline_max[360])
               ↓ fwrite(range_outline_updated[360])
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
                       ↓ for bearing = 0 to 359:
                           ↓ range = data.range_outline[bearing]
                           ↓ timestamp = data.range_outline_timestamps[bearing]
                           ↓ isValid = (range > 0 && timestamp > 0)
                           ↓ effectiveRange = isValid ? range : 1
                           ↓ point = destinationPoint(receiver, bearing, effectiveRange)
                           ↓ coordinates.push(toMapProjection(point))
                       ↓ coordinates.push(coordinates[0])  // close polygon
                       ↓ polygon = new ol.geom.Polygon([coordinates])
                       ↓ RangeOutlineFeature.setGeometry(polygon)
                       ↓ RangeOutlineLayer.setVisible(true)
   ```

## File Modifications

### New Files
- `RANGE_OUTLINE.md` - This documentation file

### Modified Files

1. **`dump1090.h`** - Data structure definitions
   - Lines 276-277: Constants for degrees and default retention
   - Lines 410-412: State variables in `struct _Modes`

2. **`dump1090.c`** - Persistence and initialization
   - Lines 104-154: `saveRangeOutline()` and `loadRangeOutline()` functions
   - Lines 199-202: Default configuration in `modesInitConfig()`
   - Lines 523, 629-639: Periodic save logic in `backgroundTasks()`
   - Lines 608: JSON generation call
   - Lines 833-836: Path update when `--write-json` parsed
   - Line 940: Load persisted data at startup
   - Line 942: Write initial JSON at startup
   - Line 1027: Save data at shutdown

3. **`track.c`** - Position tracking and expiration
   - Lines 273-290: `update_range_outline()` function
   - Line 626: Call to `update_range_outline()` in `updatePosition()`
   - Lines 1468-1480: `expireRangeOutline()` function
   - Line 1497: Call to `expireRangeOutline()` in `trackPeriodicUpdate()`

4. **`net_io.c`** - JSON generation
   - Lines 1732-1779: `generateRangeOutlineJson()` function

5. **`net_io.h`** - Function declaration
   - Line 98: Declaration of `generateRangeOutlineJson()`

6. **`public_html/index.html`** - UI element
   - Lines 134-137: Range outline checkbox in settings panel

7. **`public_html/script.js`** - Frontend logic
   - Lines 10-13: Global variables
   - Lines 221-223: Fetch call in `fetchData()`
   - Line 364: Hide column initially in `initialize()`
   - Line 760: Initialize on startup in `end_load_history()`
   - Lines 1117-1127: Checkbox setup in `initialize_map()`
   - Line 1149: Show column if site position configured
   - Lines 2879-2924: Data fetching and rendering functions
   - Lines 2977-2989: Haversine calculation function
   - Lines 2995-3008: Toggle function
   - Lines 3017-3024: Initialization from localStorage

### Generated Files

1. **`data/range_outline.json`** - Runtime JSON output
   - Generated by backend every JSON update interval
   - Read by frontend for display

2. **`data/range_outline.dat` (or `/tmp/range_outline.dat`)** - Persistence file
   - Binary format: version (uint32) + ranges (double[360]) + timestamps (uint64[360])
   - Updated every 60 seconds and at shutdown
   - Loaded at startup

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
- Compact: 2 arrays × 360 elements + version header = ~7 KB
- Fast to read/write
- Simple implementation without dependencies
- Version field allows future format changes

### Why No Fill Color?
- User preference: outline-only provides clear boundary without obscuring map
- Reduces visual clutter
- Better visibility of aircraft icons inside coverage area

### Why Layer Visibility Instead of Add/Remove?
- More efficient: layer and feature persist in memory
- Faster toggle response
- Maintains geometry when disabled, instant re-show when enabled
- Standard OpenLayers pattern

### Why Client-Side Filtering Was Removed?
- Backend already applies retention filter before JSON generation
- Avoids duplicate logic
- Reduces frontend code complexity
- Backend retention check is more accurate (uses millisecond timestamps)

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
- **Solution**: Wait for aircraft to be received again, or increase retention period (requires recompile)

### Checkbox State Wrong After Refresh

- **Fixed**: Issue was resolved in `initRangeOutline()` by adding checkbox update (line 3023)
- If still occurring: Check browser localStorage is enabled

### Outline Doesn't Update

1. **Check ShowRangeOutline is enabled** (checkbox checked)
2. **Check fetchData() is running** (aircraft list updating?)
3. **Check backend is generating JSON** (`ls -l data/range_outline.json` shows recent timestamp)
4. **Check backend is receiving aircraft** (dump1090 console shows messages?)

## Future Enhancements

Potential improvements not currently implemented:

1. **Runtime Retention Configuration**:
   - Add command-line option: `--range-outline-retention <hours>`
   - Allow users to adjust without recompiling

2. **Web UI Retention Control**:
   - Add slider or dropdown in settings panel
   - Send retention value to backend (requires new API endpoint)

3. **Color Customization UI**:
   - Color picker in settings panel
   - Save preference to localStorage

4. **Multiple Outline Layers**:
   - Show 24-hour, 7-day, and 30-day outlines simultaneously
   - Different colors for each time period

5. **Export/Import**:
   - Download range outline data as GeoJSON
   - Import previously saved outlines

6. **Statistics Display**:
   - Total coverage area calculation
   - Coverage percentage by direction
   - Identify weak coverage areas

7. **Altitude-Based Outlines**:
   - Separate outlines for different altitude bands
   - Better understanding of ground-level vs. high-altitude coverage

8. **Historical Comparison**:
   - Overlay previous period's outline to see changes
   - Detect antenna degradation or improvements
