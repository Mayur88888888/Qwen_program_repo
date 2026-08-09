# CAD Monolith - HTML-Based User Interface

## Overview

This directory contains a modern, professional HTML-based user interface for the CAD Monolith application. The interface provides a comprehensive set of CAD tools and features accessible through any modern web browser.

## Files

- **cad_interface.html** - Main HTML file with complete UI structure and styling
- **cad_app.js** - JavaScript application logic for CAD functionality

## Features

### Menu System
- **File Menu**: New, Open, Save, Save As, Export (STEP, IGES, STL, DXF), Import, Print, Close
- **Edit Menu**: Undo, Redo, Cut, Copy, Paste, Duplicate, Delete, Select All, Deselect All
- **View Menu**: Standard views (Front, Top, Right, Isometric), Zoom controls, Grid/Snap/Ortho toggles, Render modes
- **Draw Menu**: Line, Circle, Arc, Rectangle, Polygon, Ellipse, Spline, Polyline, Point, Construction Line
- **Modify Menu**: Move, Copy, Rotate, Scale, Mirror, Offset, Array, Trim, Extend, Fillet, Chamfer, Explode
- **3D Model Menu**: Primitives (Box, Sphere, Cylinder, Cone, Torus, Wedge), Operations (Extrude, Revolve, Sweep, Loft), Boolean operations, Feature tools
- **Constraints Menu**: Geometric constraints (Coincident, Parallel, Perpendicular, etc.) and Dimensional constraints
- **Measure Menu**: Distance, Angle, Radius, Diameter, Area, Volume, Mass Properties
- **Tools Menu**: Layer Manager, Block Editor, Material Browser, Appearance Editor, Script Runner, Macro Recorder, Options
- **Help Menu**: Help Contents, Tutorials, Keyboard Shortcuts, Updates, About

### Toolbar
Quick access buttons for frequently used commands organized in logical groups:
- File operations (New, Open, Save)
- Edit operations (Undo, Redo)
- Drawing tools (Line, Circle, Rectangle, Arc)
- Modify tools (Move, Copy, Rotate, Scale, Mirror)
- Edit modifiers (Trim, Extend, Fillet, Chamfer)
- 3D modeling (Box, Sphere, Cylinder, Extrude)
- Constraints (Dimension, Fixed, Coincident)
- Measurement tools
- View controls (Grid, Snap, Ortho)
- Zoom controls
- Render modes (Wireframe, Shaded, Realistic)

### Left Panel - Feature Tree
- **Browser Tab**: Hierarchical view of model structure (Origin, Planes, Sketches, Bodies, Construction)
- **Layers Tab**: Layer visibility and color management
- **Materials Tab**: Material library (Aluminum, Bronze, Copper, Gold, Platinum, Steel, Titanium)

### Viewport
- Interactive canvas with pan, zoom, and rotate capabilities
- View cube for quick orientation changes
- Selection information display
- Grid status indicator
- Snap indicator
- Measurement overlay

### Right Panel - Properties
Collapsible sections for editing object properties:
- **General**: Name, Type, Layer, Visibility, Locked state
- **Position**: X, Y, Z coordinates
- **Dimensions**: Length, Width, Height, Radius
- **Material**: Material selection, Color picker, Density, Mass
- **Constraints**: Active constraint badges with add/remove buttons
- **Appearance**: Opacity, Roughness, Metallic sliders

### Bottom Panel - Command Line
- Command input field with keyboard support
- Command history with success/error indicators
- Support for direct command entry

### Status Bar
- Model name display
- Units selector (mm, cm, m, in, ft)
- Grid spacing indicator
- Real-time coordinate display (X, Y, Z)
- Memory usage monitor
- FPS counter

### Context Menu
Right-click menu with context-sensitive options:
- New, Open
- Copy, Paste, Delete
- Properties
- Hide, Isolate

### Modal Dialogs
- File operations confirmation
- Options configuration
- Help and documentation
- Keyboard shortcuts reference

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New File |
| Ctrl+O | Open File |
| Ctrl+S | Save |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+E | Zoom Extents |
| L | Line tool |
| C | Circle tool |
| R | Rectangle tool |
| A | Arc tool |
| M | Move tool |
| G | Toggle Grid |
| S | Toggle Snap |
| F8 | Toggle Ortho |
| Delete | Delete selection |
| Escape | Deselect all |
| F1 | Help |

## Mouse Controls

| Action | Control |
|--------|---------|
| Pan | Middle mouse button drag |
| Rotate | Ctrl + Left mouse button drag |
| Zoom | Mouse wheel |
| Select | Left click |
| Context Menu | Right click |

## Usage

### Opening the Interface

Simply open `cad_interface.html` in any modern web browser:

```bash
# Using a local server (recommended)
python -m http.server 8000
# Then navigate to http://localhost:8000/ui/cad_interface.html

# Or directly open the file
file:///path/to/CAD_Monolith/ui/cad_interface.html
```

### Basic Workflow

1. **Start a new drawing**: Click File > New or press Ctrl+N
2. **Select a drawing tool**: Use toolbar buttons or keyboard shortcuts (L for Line, C for Circle, etc.)
3. **Draw in viewport**: Click and drag in the main canvas area
4. **Modify objects**: Select objects and use modify tools
5. **Adjust properties**: Use the right panel to edit properties
6. **Save your work**: Click File > Save or press Ctrl+S

### Command Line Usage

The command line accepts standard CAD commands:
```
> LINE      # Start line command
> CIRCLE    # Start circle command
> MOVE      # Start move command
> ZOOM E    # Zoom to extents
> REGEN     # Regenerate view
> HELP      # Show help
```

## Styling

The interface uses a modern dark theme with the following color scheme:
- Primary background: #1a1a2e
- Secondary background: #16213e
- Tertiary background: #0f3460
- Primary accent: #e94560
- Secondary accent: #00adb5
- Text primary: #eeeeee
- Text secondary: #b0b0b0

## Browser Compatibility

Tested and compatible with:
- Chrome 90+
- Firefox 88+
- Safari 14+
- Edge 90+

## Integration with CAD Monolith Core

This HTML interface is designed to work with the CAD Monolith C++ backend through:
- WebAssembly compilation (future)
- IPC communication
- REST API (future)

## Future Enhancements

Planned improvements:
- WebGL-based 3D rendering
- Real-time collaboration
- Cloud storage integration
- Plugin system
- Advanced surface modeling tools
- Simulation and analysis tools
- CAM integration

## License

Part of the CAD Monolith project. See main LICENSE file for details.

## Support

For issues and feature requests, please refer to the main CAD Monolith documentation.
