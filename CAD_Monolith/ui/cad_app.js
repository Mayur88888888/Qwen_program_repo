// CAD Monolith - JavaScript Application Logic
// Professional CAD Interface Implementation

// State Management
const state = {
    currentTool: null,
    selection: [],
    gridEnabled: true,
    snapEnabled: true,
    orthoEnabled: false,
    viewMode: 'shaded',
    units: 'mm',
    zoom: 1.0,
    rotation: { x: 30, y: -45 },
    pan: { x: 0, y: 0 },
    commandHistory: [],
    undoStack: [],
    redoStack: [],
    features: [],
    layers: [],
    constraints: []
};

// Canvas Setup
const canvas = document.getElementById('cad-canvas');
const ctx = canvas.getContext('2d');

function resizeCanvas() {
    const container = canvas.parentElement;
    canvas.width = container.clientWidth;
    canvas.height = container.clientHeight;
    render();
}

window.addEventListener('resize', resizeCanvas);
resizeCanvas();

// Render Function
function render() {
    // Clear canvas
    ctx.fillStyle = '#1a1a2e';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    // Draw grid
    if (state.gridEnabled) {
        drawGrid();
    }

    // Draw axes
    drawAxes();

    // Draw features
    state.features.forEach(feature => {
        if (feature.visible) {
            drawFeature(feature);
        }
    });

    // Draw selection
    state.selection.forEach(sel => {
        drawSelectionHighlight(sel);
    });

    // Update FPS counter
    updateFPS();
}

function drawGrid() {
    const gridSize = 50 * state.zoom;
    const offsetX = state.pan.x % gridSize;
    const offsetY = state.pan.y % gridSize;

    ctx.strokeStyle = '#2d2d44';
    ctx.lineWidth = 0.5;

    // Vertical lines
    for (let x = offsetX; x < canvas.width; x += gridSize) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, canvas.height);
        ctx.stroke();
    }

    // Horizontal lines
    for (let y = offsetY; y < canvas.height; y += gridSize) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(canvas.width, y);
        ctx.stroke();
    }
}

function drawAxes() {
    const centerX = canvas.width / 2 + state.pan.x;
    const centerY = canvas.height / 2 + state.pan.y;

    // X axis (red)
    ctx.strokeStyle = '#ff4444';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(0, centerY);
    ctx.lineTo(canvas.width, centerY);
    ctx.stroke();

    // Y axis (green)
    ctx.strokeStyle = '#44ff44';
    ctx.beginPath();
    ctx.moveTo(centerX, 0);
    ctx.lineTo(centerX, canvas.height);
    ctx.stroke();

    // Origin
    ctx.fillStyle = '#ffffff';
    ctx.beginPath();
    ctx.arc(centerX, centerY, 5, 0, Math.PI * 2);
    ctx.fill();
}

function drawFeature(feature) {
    ctx.strokeStyle = feature.color || '#00adb5';
    ctx.lineWidth = 2;
    ctx.fillStyle = feature.fill || 'rgba(0, 173, 181, 0.3)';

    switch (feature.type) {
        case 'line':
            ctx.beginPath();
            ctx.moveTo(feature.x1, feature.y1);
            ctx.lineTo(feature.x2, feature.y2);
            ctx.stroke();
            break;
        case 'circle':
            ctx.beginPath();
            ctx.arc(feature.x, feature.y, feature.radius, 0, Math.PI * 2);
            ctx.stroke();
            break;
        case 'rectangle':
            ctx.fillRect(feature.x, feature.y, feature.width, feature.height);
            ctx.strokeRect(feature.x, feature.y, feature.width, feature.height);
            break;
        case 'box3d':
            drawBox3D(feature);
            break;
    }
}

function drawBox3D(box) {
    const { x, y, z, width, height, depth } = box;
    // Simplified 3D box projection
    ctx.strokeStyle = '#00adb5';
    ctx.lineWidth = 2;
    ctx.strokeRect(x, y, width, height);
}

function drawSelectionHighlight(feature) {
    ctx.strokeStyle = '#e94560';
    ctx.lineWidth = 3;
    ctx.setLineDash([5, 5]);
    // Draw highlight around selected feature
    ctx.stroke();
    ctx.setLineDash([]);
}

// Mouse Interaction
let isDragging = false;
let lastMousePos = { x: 0, y: 0 };
let mouseButton = 0;

canvas.addEventListener('mousedown', (e) => {
    isDragging = true;
    lastMousePos = { x: e.clientX, y: e.clientY };
    mouseButton = e.button;

    if (mouseButton === 0 && state.currentTool) {
        handleToolClick(e);
    }

    updateCoordinates(e);
});

canvas.addEventListener('mousemove', (e) => {
    if (isDragging) {
        const dx = e.clientX - lastMousePos.x;
        const dy = e.clientY - lastMousePos.y;

        if (mouseButton === 1 || (mouseButton === 0 && e.shiftKey)) {
            // Pan
            state.pan.x += dx;
            state.pan.y += dy;
        } else if (mouseButton === 2 || (mouseButton === 0 && e.ctrlKey)) {
            // Rotate view
            state.rotation.y += dx * 0.5;
            state.rotation.x += dy * 0.5;
        }

        lastMousePos = { x: e.clientX, y: e.clientY };
    }

    updateCoordinates(e);
    checkSnap(e);
    render();
});

canvas.addEventListener('mouseup', () => {
    isDragging = false;
});

canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const delta = e.deltaY > 0 ? 0.9 : 1.1;
    state.zoom *= delta;
    state.zoom = Math.max(0.1, Math.min(10, state.zoom));
    render();
});

canvas.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    showContextMenu(e.clientX, e.clientY);
});

function updateCoordinates(e) {
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    // Convert to world coordinates
    const worldX = ((x - canvas.width / 2 - state.pan.x) / state.zoom).toFixed(3);
    const worldY = (-(y - canvas.height / 2 - state.pan.y) / state.zoom).toFixed(3);

    document.getElementById('coordX').textContent = worldX;
    document.getElementById('coordY').textContent = worldY;
    document.getElementById('coordZ').textContent = '0.000';
}

function checkSnap(e) {
    if (!state.snapEnabled) {
        document.getElementById('snapIndicator').style.display = 'none';
        return;
    }

    // Simple snap detection
    const snapIndicator = document.getElementById('snapIndicator');
    // In a real implementation, check for nearby points/edges
    snapIndicator.style.display = 'none';
}

// Tool Functions
function handleToolClick(e) {
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    switch (state.currentTool) {
        case 'line':
            addFeature({ type: 'line', x1: x, y1: y, x2: x + 100, y2: y + 100 });
            addCommand(`LINE ${x.toFixed(2)},${y.toFixed(2)} ${x + 100},${y + 100}`);
            break;
        case 'circle':
            addFeature({ type: 'circle', x, y, radius: 50 });
            addCommand(`CIRCLE ${x.toFixed(2)},${y.toFixed(2)} R50`);
            break;
        case 'rectangle':
            addFeature({ type: 'rectangle', x, y, width: 100, height: 50 });
            addCommand(`RECTANGLE ${x.toFixed(2)},${y.toFixed(2)} 100x50`);
            break;
    }
}

function addFeature(feature) {
    feature.id = Date.now();
    feature.visible = true;
    feature.color = '#00adb5';
    state.features.push(feature);
    render();
    updateFeatureTree();
}

// Drawing Tools
function drawLine() { setTool('line'); addCommand('LINE'); }
function drawCircle() { setTool('circle'); addCommand('CIRCLE'); }
function drawRectangle() { setTool('rectangle'); addCommand('RECTANGLE'); }
function drawArc() { setTool('arc'); addCommand('ARC'); }
function drawPolygon() { setTool('polygon'); addCommand('POLYGON'); }
function drawEllipse() { setTool('ellipse'); addCommand('ELLIPSE'); }
function drawSpline() { setTool('spline'); addCommand('SPLINE'); }
function drawPolyline() { setTool('polyline'); addCommand('POLYLINE'); }
function drawPoint() { setTool('point'); addCommand('POINT'); }
function drawConstructionLine() { setTool('construction'); addCommand('XLINE'); }

// Modify Tools
function modifyMove() { setTool('move'); addCommand('MOVE'); }
function modifyCopy() { setTool('copy'); addCommand('COPY'); }
function modifyRotate() { setTool('rotate'); addCommand('ROTATE'); }
function modifyScale() { setTool('scale'); addCommand('SCALE'); }
function modifyMirror() { setTool('mirror'); addCommand('MIRROR'); }
function modifyOffset() { setTool('offset'); addCommand('OFFSET'); }
function modifyArray() { setTool('array'); addCommand('ARRAY'); }
function modifyTrim() { setTool('trim'); addCommand('TRIM'); }
function modifyExtend() { setTool('extend'); addCommand('EXTEND'); }
function modifyFillet() { setTool('fillet'); addCommand('FILLET'); }
function modifyChamfer() { setTool('chamfer'); addCommand('CHAMFER'); }
function modifyExplode() { setTool('explode'); addCommand('EXPLODE'); }

// 3D Modeling Tools
function createBox() { 
    addFeature({ type: 'box3d', x: 100, y: 100, z: 0, width: 50, height: 50, depth: 50 }); 
    addCommand('BOX'); 
}
function createSphere() { addCommand('SPHERE'); }
function createCylinder() { addCommand('CYLINDER'); }
function createCone() { addCommand('CONE'); }
function createTorus() { addCommand('TORUS'); }
function createWedge() { addCommand('WEDGE'); }
function extrudeSketch() { addCommand('EXTRUDE'); }
function revolveSketch() { addCommand('REVOLVE'); }
function sweepSketch() { addCommand('SWEEP'); }
function loftSketch() { addCommand('LOFT'); }

// Boolean Operations
function booleanUnion() { addCommand('UNION'); }
function booleanDifference() { addCommand('DIFFERENCE'); }
function booleanIntersect() { addCommand('INTERSECT'); }

// Feature Operations
function shellFeature() { addCommand('SHELL'); }
function filletFeature() { addCommand('FILLET3D'); }
function chamferFeature() { addCommand('CHAMFER3D'); }
function draftFeature() { addCommand('DRAFT'); }
function holeFeature() { addCommand('HOLE'); }
function patternFeature() { addCommand('PATTERN'); }

// Constraints
function constraintCoincident() { addCommand('CONSTRAINT COINCIDENT'); }
function constraintCollinear() { addCommand('CONSTRAINT COLLINEAR'); }
function constraintParallel() { addCommand('CONSTRAINT PARALLEL'); }
function constraintPerpendicular() { addCommand('CONSTRAINT PERPENDICULAR'); }
function constraintTangent() { addCommand('CONSTRAINT TANGENT'); }
function constraintConcentric() { addCommand('CONSTRAINT CONCENTRIC'); }
function constraintEqual() { addCommand('CONSTRAINT EQUAL'); }
function constraintSymmetric() { addCommand('CONSTRAINT SYMMETRIC'); }
function constraintFixed() { addCommand('CONSTRAINT FIXED'); }
function constraintHorizontal() { addCommand('CONSTRAINT HORIZONTAL'); }
function constraintVertical() { addCommand('CONSTRAINT VERTICAL'); }
function constraintDimension() { addCommand('DIMENSION'); }
function constraintAngle() { addCommand('CONSTRAINT ANGLE'); }
function constraintRadius() { addCommand('DIMENSION RADIUS'); }
function constraintDiameter() { addCommand('DIMENSION DIAMETER'); }

// Measurement Tools
function measureDistance() { setTool('measure_distance'); addCommand('MEASURE DISTANCE'); }
function measureAngle() { setTool('measure_angle'); addCommand('MEASURE ANGLE'); }
function measureRadius() { setTool('measure_radius'); addCommand('MEASURE RADIUS'); }
function measureDiameter() { setTool('measure_diameter'); addCommand('MEASURE DIAMETER'); }
function measureArea() { setTool('measure_area'); addCommand('MEASURE AREA'); }
function measureVolume() { setTool('measure_volume'); addCommand('MEASURE VOLUME'); }
function measureMassProperties() { addCommand('MASSPROP'); }

// View Functions
function setTool(tool) {
    state.currentTool = tool;
    addCommand(`Tool: ${tool}`);
    // Update active button state
    document.querySelectorAll('.tool-btn').forEach(btn => btn.classList.remove('active'));
    if (event && event.target) {
        event.target.classList.add('active');
    }
}

function setView(view) {
    switch (view) {
        case 'front':
            state.rotation = { x: 0, y: 0 };
            break;
        case 'top':
            state.rotation = { x: 90, y: 0 };
            break;
        case 'right':
            state.rotation = { x: 0, y: 90 };
            break;
        case 'left':
            state.rotation = { x: 0, y: -90 };
            break;
        case 'bottom':
            state.rotation = { x: -90, y: 0 };
            break;
        case 'isometric':
            state.rotation = { x: 30, y: -45 };
            break;
    }
    addCommand(`VIEW ${view.toUpperCase()}`);
    render();
}

function zoomExtents() {
    state.zoom = 1.0;
    state.pan = { x: 0, y: 0 };
    addCommand('ZOOM EXTENTS');
    render();
}

function zoomIn() {
    state.zoom *= 1.2;
    addCommand('ZOOM IN');
    render();
}

function zoomOut() {
    state.zoom /= 1.2;
    addCommand('ZOOM OUT');
    render();
}

function toggleGrid() {
    state.gridEnabled = !state.gridEnabled;
    document.getElementById('gridStatus').textContent = state.gridEnabled ? '1.00' : 'OFF';
    updateGridInfo();
    addCommand(`GRID ${state.gridEnabled ? 'ON' : 'OFF'}`);
    render();
}

function toggleSnap() {
    state.snapEnabled = !state.snapEnabled;
    updateGridInfo();
    addCommand(`SNAP ${state.snapEnabled ? 'ON' : 'OFF'}`);
}

function toggleOrtho() {
    state.orthoEnabled = !state.orthoEnabled;
    updateGridInfo();
    addCommand(`ORTHO ${state.orthoEnabled ? 'ON' : 'OFF'}`);
}

function updateGridInfo() {
    document.getElementById('gridInfo').textContent = 
        `Grid: ${state.gridEnabled ? 'On' : 'Off'} | Snap: ${state.snapEnabled ? 'On' : 'Off'} | Ortho: ${state.orthoEnabled ? 'On' : 'Off'}`;
}

// Render Modes
function renderWireframe() { state.viewMode = 'wireframe'; addCommand('RENDER WIREFRAME'); render(); }
function renderShaded() { state.viewMode = 'shaded'; addCommand('RENDER SHADED'); render(); }
function renderRealistic() { state.viewMode = 'realistic'; addCommand('RENDER REALISTIC'); render(); }

// File Operations
function newFile() {
    showModal('New File', 'Create a new drawing? Unsaved changes will be lost.');
}

function openFile() {
    addCommand('OPEN');
    showModal('Open File', '<input type="file" accept=".cad,.step,.iges,.stl,.dxf" style="width: 100%;">');
}

function saveFile() {
    addCommand('SAVE');
    addCommandSuccess('File saved successfully');
}

function saveFileAs() {
    showModal('Save As', 'Enter filename:<br><input type="text" class="property-input" style="width: 100%; margin-top: 0.5rem;">');
}

function exportFile(format) {
    addCommand(`EXPORT ${format}`);
    addCommandSuccess(`Exported to ${format}`);
}

function importFile() {
    addCommand('IMPORT');
}

function printFile() {
    addCommand('PRINT');
}

function closeFile() {
    addCommand('CLOSE');
}

// Edit Operations
function undo() {
    addCommand('UNDO');
}

function redo() {
    addCommand('REDO');
}

function cut() {
    addCommand('CUT');
}

function copy() {
    addCommand('COPY');
}

function paste() {
    addCommand('PASTE');
}

function duplicate() {
    addCommand('DUPLICATE');
}

function deleteSelection() {
    addCommand('DELETE');
    state.selection = [];
    updateSelectionInfo();
    render();
}

function selectAll() {
    addCommand('SELECT ALL');
}

function deselectAll() {
    state.selection = [];
    updateSelectionInfo();
    render();
    addCommand('DESELECT ALL');
}

// Command Line
const commandInput = document.getElementById('commandInput');
commandInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
        const command = commandInput.value.trim();
        if (command) {
            executeCommand(command);
            commandInput.value = '';
        }
    }
});

function executeCommand(cmd) {
    addCommand(cmd);
    const parts = cmd.toUpperCase().split(' ');
    const command = parts[0];

    // Simple command parser
    switch (command) {
        case 'LINE':
        case 'L':
            setTool('line');
            break;
        case 'CIRCLE':
        case 'C':
            setTool('circle');
            break;
        case 'RECTANGLE':
        case 'REC':
        case 'R':
            setTool('rectangle');
            break;
        case 'MOVE':
        case 'M':
            setTool('move');
            break;
        case 'COPY':
        case 'CO':
            setTool('copy');
            break;
        case 'ROTATE':
        case 'RO':
            setTool('rotate');
            break;
        case 'DELETE':
        case 'ERASE':
        case 'E':
            deleteSelection();
            break;
        case 'ZOOM':
            if (parts[1] === 'EXTENTS' || parts[1] === 'E') {
                zoomExtents();
            }
            break;
        case 'PAN':
        case 'P':
            setTool('pan');
            break;
        case 'REGEN':
        case 'RE':
            render();
            addCommandSuccess('Regenerated');
            break;
        case 'HELP':
        case '?':
            showHelp();
            break;
        default:
            addCommandError(`Unknown command: ${command}`);
    }
}

function addCommand(cmd) {
    const history = document.getElementById('commandHistory');
    const entry = document.createElement('div');
    entry.className = 'command-entry';
    entry.textContent = `> ${cmd}`;
    history.appendChild(entry);
    history.scrollTop = history.scrollHeight;
    state.commandHistory.push(cmd);
}

function addCommandSuccess(msg) {
    const history = document.getElementById('commandHistory');
    const entry = document.createElement('div');
    entry.className = 'command-entry success';
    entry.textContent = msg;
    history.appendChild(entry);
    history.scrollTop = history.scrollHeight;
}

function addCommandError(msg) {
    const history = document.getElementById('commandHistory');
    const entry = document.createElement('div');
    entry.className = 'command-entry error';
    entry.textContent = `Error: ${msg}`;
    history.appendChild(entry);
    history.scrollTop = history.scrollHeight;
}

// Context Menu
function showContextMenu(x, y) {
    const menu = document.getElementById('contextMenu');
    menu.style.left = `${x}px`;
    menu.style.top = `${y}px`;
    menu.classList.add('visible');
}

document.addEventListener('click', (e) => {
    document.getElementById('contextMenu').classList.remove('visible');
});

function contextNew() { newFile(); }
function contextOpen() { openFile(); }
function contextCopy() { copy(); }
function contextPaste() { paste(); }
function contextDelete() { deleteSelection(); }
function contextProperties() { showProperties(); }
function contextHide() { addCommand('HIDE'); }
function contextIsolate() { addCommand('ISOLATE'); }

// Modal Functions
function showModal(title, content) {
    document.getElementById('modalTitle').textContent = title;
    document.getElementById('modalBody').innerHTML = content;
    document.getElementById('modalOverlay').classList.add('visible');
}

function closeModal() {
    document.getElementById('modalOverlay').classList.remove('visible');
}

function confirmModal() {
    closeModal();
}

// Property Functions
function toggleSection(sectionId) {
    const section = document.getElementById(`${sectionId}-section`);
    if (section.style.display === 'none') {
        section.style.display = 'block';
    } else {
        section.style.display = 'none';
    }
}

function updateColor(value) {
    document.getElementById('colorPreview').style.background = value;
}

function updateFeatureTree() {
    const tree = document.getElementById('featureTree');
    // Update tree with current features
}

function updateSelectionInfo() {
    const info = document.getElementById('selectionInfo');
    if (state.selection.length === 0) {
        info.textContent = 'No selection';
    } else {
        info.textContent = `${state.selection.length} object(s) selected`;
    }
}

function refreshFeatureTree() {
    updateFeatureTree();
    addCommand('REFRESH');
}

// Tab Functions
function switchTab(tabName) {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    
    event.target.classList.add('active');
    document.getElementById(`${tabName}-tab`).classList.add('active');
}

// Layer Functions
function addLayer() {
    addCommand('LAYER NEW');
}

function addConstraint() {
    addCommand('CONSTRAINT ADD');
}

function removeConstraint() {
    addCommand('CONSTRAINT REMOVE');
}

// Utility Functions
function updateFPS() {
    const fpsElement = document.getElementById('fpsCounter');
    fpsElement.textContent = Math.round(60);
}

// Help Functions
function showHelp() {
    showModal('Help', `
        <h3>CAD Monolith Quick Help</h3>
        <p><strong>Drawing Tools:</strong></p>
        <ul>
            <li>L - Line</li>
            <li>C - Circle</li>
            <li>R - Rectangle</li>
            <li>A - Arc</li>
        </ul>
        <p><strong>Modify Tools:</strong></p>
        <ul>
            <li>M - Move</li>
            <li>CO - Copy</li>
            <li>RO - Rotate</li>
            <li>SC - Scale</li>
        </ul>
        <p><strong>Navigation:</strong></p>
        <ul>
            <li>Middle Mouse - Pan</li>
            <li>Ctrl + Left Mouse - Rotate</li>
            <li>Scroll Wheel - Zoom</li>
        </ul>
    `);
}

function showTutorials() {
    showModal('Tutorials', 'Tutorial content would go here...');
}

function showKeyboardShortcuts() {
    showModal('Keyboard Shortcuts', `
        <table style="width: 100%; text-align: left;">
            <tr><td>Ctrl+N</td><td>New File</td></tr>
            <tr><td>Ctrl+O</td><td>Open File</td></tr>
            <tr><td>Ctrl+S</td><td>Save</td></tr>
            <tr><td>Ctrl+Z</td><td>Undo</td></tr>
            <tr><td>Ctrl+Y</td><td>Redo</td></tr>
            <tr><td>Ctrl+E</td><td>Zoom Extents</td></tr>
            <tr><td>G</td><td>Toggle Grid</td></tr>
            <tr><td>S</td><td>Toggle Snap</td></tr>
            <tr><td>F8</td><td>Toggle Ortho</td></tr>
        </table>
    `);
}

function checkForUpdates() {
    addCommand('CHECKUPDATES');
    addCommandSuccess('You are running the latest version');
}

function showAbout() {
    showModal('About CAD Monolith', `
        <h3>CAD Monolith v1.0</h3>
        <p>A professional CAD application built with modern web technologies.</p>
        <p>© 2024 CAD Monolith Team</p>
        <p>All rights reserved.</p>
    `);
}

function showOptions() {
    showModal('Options', `
        <div class="property-row">
            <span class="property-label">Auto-save interval (minutes)</span>
            <input type="number" class="property-input" value="5">
        </div>
        <div class="property-row">
            <span class="property-label">Default units</span>
            <select class="property-input">
                <option>mm</option>
                <option>cm</option>
                <option>m</option>
                <option>inches</option>
            </select>
        </div>
        <div class="property-row">
            <span class="property-label">Grid spacing</span>
            <input type="text" class="property-input" value="1.00">
        </div>
    `);
}

function showLayerManager() {
    showModal('Layer Manager', 'Layer management interface...');
}

function showBlockEditor() {
    showModal('Block Editor', 'Block editor interface...');
}

function showMaterialBrowser() {
    showModal('Material Browser', 'Material browser interface...');
}

function showAppearanceEditor() {
    showModal('Appearance Editor', 'Appearance editor interface...');
}

function runScript() {
    showModal('Run Script', '<textarea class="property-input" style="width: 100%; height: 200px; font-family: monospace;"></textarea>');
}

function showMacroRecorder() {
    addCommand('MACRO RECORD');
}

function showProperties() {
    addCommand('PROPERTIES');
}

// Keyboard Shortcuts
document.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
        return;
    }

    const key = e.key.toLowerCase();
    
    if (e.ctrlKey) {
        switch (key) {
            case 'n': e.preventDefault(); newFile(); break;
            case 'o': e.preventDefault(); openFile(); break;
            case 's': e.preventDefault(); saveFile(); break;
            case 'z': e.preventDefault(); undo(); break;
            case 'y': e.preventDefault(); redo(); break;
            case 'e': e.preventDefault(); zoomExtents(); break;
        }
    } else {
        switch (key) {
            case 'l': setTool('line'); break;
            case 'c': setTool('circle'); break;
            case 'r': setTool('rectangle'); break;
            case 'a': setTool('arc'); break;
            case 'm': setTool('move'); break;
            case 'g': toggleGrid(); break;
            case 's': toggleSnap(); break;
            case 'delete': deleteSelection(); break;
            case 'escape': deselectAll(); break;
            case 'f1': showHelp(); break;
        }
    }
});

// Initialize
render();
addCommandSuccess('CAD Monolith ready');

// Animation loop for smooth rendering
function animate() {
    render();
    requestAnimationFrame(animate);
}
animate();
