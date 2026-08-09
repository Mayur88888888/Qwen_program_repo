# CAD Studio - Professional Web-Based CAD Interface

## Overview

CAD Studio is a modern, professional HTML5-based CAD interface designed for maximum working area with collapsible panels and comprehensive drawing tools.

## Features

### Drawing Tools (2D)
- **Line** (L) - Two-point line creation with rubber-band preview
- **Circle** (C) - Center-radius circle with dynamic preview
- **Rectangle** (R) - Corner-to-corner rectangle with live preview
- **Arc** (A) - Center-start-end arc creation
- **Polyline** (P) - Multi-segment polyline

### Modify Tools
- **Move** (M) - Move selected objects
- **Copy** (CO) - Copy objects with base point
- **Rotate** (RO) - Rotate around specified point
- **Scale** (SC) - Scale objects proportionally
- **Trim** (TR) - Trim objects to boundaries
- **Extend** (EX) - Extend objects to boundaries
- **Offset** (O) - Create parallel copies
- **Mirror** (MI) - Mirror across axis

### 3D Modeling
- **Box** - Create 3D box/solid
- **Sphere** - Create sphere
- **Cylinder** - Create cylinder
- **Cone** - Create cone
- **Extrude** - Extrude 2D to 3D
- **Revolve** - Revolve profile around axis
- **Loft** - Loft between cross-sections
- **Boolean Operations**: Union, Subtract, Intersect

### View Controls
- **Grid Toggle** (G) - Show/hide grid with major/minor lines
- **Snap Toggle** (S) - Snap to grid points
- **Ortho Mode** (F8) - Constrain to horizontal/vertical
- **Zoom** - Mouse wheel or +/- buttons
- **Pan** - Middle mouse button drag
- **View Cube** - Top, Bottom, Left, Right, Isometric views
- **Zoom Extents** - Fit all geometry in view

### Interactive Features
- **Real-time Preview** - Rubber-band lines showing what you're drawing
- **Snap Indicator** - Visual feedback when snapping to grid
- **Coordinate Display** - Live X, Y, Z coordinates
- **Command Line** - AutoCAD-style command input
- **Feature Tree** - Hierarchical object browser
- **Properties Panel** - Edit object properties
- **Collapsible Panels** - Maximize drawing area

## Usage

### Opening the Application

1. **Direct Open**: Double-click `cad_studio.html` in your file browser
2. **Local Server** (recommended):
   ```bash
   cd /workspace/CAD_Monolith/ui
   python3 -m http.server 8000
   ```
   Then open: http://localhost:8000/cad_studio.html

### Basic Drawing Workflow

#### Draw a Line
1. Click the **Line** button (📏) or press `L`
2. Click to specify first point
3. Move mouse to see preview (rubber-banding)
4. Click to specify second point
5. Line is created automatically

#### Draw a Circle
1. Click the **Circle** button (⭕) or press `C`
2. Click to specify center point
3. Move mouse to set radius (dynamic preview)
4. Click to complete circle

#### Draw a Rectangle
1. Click the **Rectangle** button (⬜) or press `R`
2. Click to specify first corner
3. Move mouse to see rectangle preview
4. Click to specify opposite corner

### Command Line Usage

Type commands directly in the command bar at the bottom:

```
LINE     or L      - Start line command
CIRCLE   or C      - Start circle command
RECTANGLE or R     - Start rectangle command
MOVE     or M      - Start move command
DELETE   or E      - Delete selected objects
ZOOM     or Z      - Zoom to extents
GRID               - Toggle grid
SNAP               - Toggle snap
ORTHO              - Toggle ortho mode
UNDO     or U      - Undo last action
REDO               - Redo last action
HELP     or ?      - Show help
```

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| L | Line tool |
| C | Circle tool |
| R | Rectangle tool |
| A | Arc tool |
| P | Polyline tool |
| M | Move tool |
| G | Toggle grid |
| S | Toggle snap |
| F8 | Toggle ortho |
| ESC | Cancel current command |
| Delete | Delete selection |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |

### Panel Management

- **Left Panel**: Click ◀ button to collapse/expand feature tree
- **Right Panel**: Click ▶ button to collapse/expand properties
- **Bottom Panel**: Drag border to resize command history

### Grid & Snap

- **Grid**: Visual reference with minor (50 units) and major (250 units) lines
- **Snap**: Constrains cursor to 10-unit grid intervals
- **Ortho**: Constrains movement to horizontal/vertical axes

## Technical Details

### Architecture

```
┌─────────────────────────────────────────────┐
│                 Header Bar                   │
│  Logo | File | Edit | View | Draw | ...     │
├─────────────────────────────────────────────┤
│                Toolbar                       │
│  [File] [Undo/Redo] [Draw] [Modify] [...]   │
├──────────┬──────────────────────┬───────────┤
│ Feature  │                      │ Properties│
│ Tree     │    DRAWING AREA      │ Panel     │
│          │    (HTML5 Canvas)    │           │
│          │    - Grid            │ - Geometry│
│          │    - Axes            │ - Position│
│          │    - Objects         │ - Dimensions
│          │    - Preview         │           │
├──────────┴──────────────────────┴───────────┤
│              Command Line                    │
│  Command: [input field]                     │
│  History: LINE - Created                    │
├─────────────────────────────────────────────┤
│              Status Bar                      │
│  Grid:ON | Snap:ON | Ortho:OFF | Units:mm  │
└─────────────────────────────────────────────┘
```

### Rendering Pipeline

1. Clear canvas
2. Draw grid (if enabled)
3. Draw axes (X=red, Y=green)
4. Draw all features
5. Draw selection highlights
6. Draw tool preview (rubber-band)
7. Update overlays (coordinates, snap indicator)

### Coordinate System

- **World Coordinates**: Origin at center, X right, Y up
- **Screen Coordinates**: Origin at top-left, X right, Y down
- **Transformation**: Automatic conversion with zoom and pan support

### Data Structure

```javascript
{
  type: 'line' | 'circle' | 'rect' | 'arc',
  color: '#RRGGBB',
  visible: true,
  // Type-specific properties:
  points: [{x, y}, {x, y}],      // for lines
  center: {x, y}, radius: number, // for circles
  x, y, width, height             // for rectangles
}
```

## Future Enhancements

### Planned Features
- DXF/DWG import/export
- Layer management
- Dimension tools
- Constraint solver
- Block/library support
- Print/plot functionality
- Multi-viewport support

### Backend Integration Ready

The UI is designed for future integration with C++ geometric kernels:

```javascript
// Current: Pure JavaScript
app.state.features.push({...});

// Future: Backend via WebAssembly/WebSocket
backend.kernel.createLine(p1, p2);
backend.commands.execute('LINE', params);
```

## Browser Compatibility

- ✅ Chrome/Edge 80+
- ✅ Firefox 75+
- ✅ Safari 13+
- ✅ Opera 67+

Requires HTML5 Canvas and ES6 JavaScript support.

## Files

- `cad_studio.html` - Complete standalone application (all-in-one)
- `README_STUDIO.md` - This documentation file

## License

Open source for educational and commercial use.

---

**Quick Start**: Open `cad_studio.html` → Press `L` → Click two points → Done!
