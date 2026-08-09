# CAD MONOLITH - Windows Build Instructions

## Prerequisites

Before building CAD Monolith on Windows, ensure you have the following:

### Required Software
1. **Visual Studio 2019 or 2022** with "Desktop development with C++" workload
   - Download: https://visualstudio.microsoft.com/downloads/
   - During installation, select:
     - MSVC v142/v143 - VS 2019/2022 C++ x64/x86 build tools
     - Windows 10/11 SDK
     - C++ CMake tools for Windows

### Optional (Recommended)
- **Git for Windows** - For version control
- **vcpkg** - For package management (if adding external dependencies later)

---

## Build Methods

### Method 1: Quick Build (Batch Script)

1. Open **"Developer Command Prompt for VS 2022"** (or your installed version)
   - Search in Start Menu for "Developer Command Prompt"

2. Navigate to the project directory:
   ```cmd
   cd C:\path\to\CAD_Monolith
   ```

3. Run the build script:
   ```cmd
   build_windows.bat
   ```

4. If successful, the executable will be at:
   ```
   build\bin\CadMonolith.exe
   ```

---

### Method 2: CMake Build (Recommended for Development)

1. Open **"Developer Command Prompt for VS 2022"**

2. Navigate to project directory:
   ```cmd
   cd C:\path\to\CAD_Monolith
   ```

3. Create and enter build directory:
   ```cmd
   mkdir build
   cd build
   ```

4. Configure with CMake:
   ```cmd
   cmake .. -G "Visual Studio 17 2022" -A x64
   ```
   
   For VS 2019, use:
   ```cmd
   cmake .. -G "Visual Studio 16 2019" -A x64
   ```

5. Build the project:
   ```cmd
   cmake --build . --config Release
   ```

6. The executable will be at:
   ```
   build\Release\CadMonolith.exe
   ```

---

### Method 3: Visual Studio IDE

1. Generate Visual Studio solution:
   ```cmd
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022" -A x64
   ```

2. Open the generated solution:
   ```cmd
   CadMonolith.sln
   ```

3. In Visual Studio:
   - Set configuration to **Release**
   - Set platform to **x64**
   - Build → Build Solution (Ctrl+Shift+B)

4. Run with F5 or find executable in `build\Release\`

---

## Running the Application

After successful build:

```cmd
cd build\bin
CadMonolith.exe
```

Or double-click `CadMonolith.exe` in Windows Explorer.

---

## Troubleshooting

### Error: "cl.exe not found"
- Run from **Developer Command Prompt**, not regular cmd
- Ensure Visual Studio C++ workload is installed

### Error: "Windows SDK not found"
- Reinstall Visual Studio with Windows 10/11 SDK component
- Or install Windows SDK separately from Microsoft

### Error: "OpenGL headers not found"
- OpenGL headers come with Windows SDK
- Ensure `gl.h` exists in `C:\Program Files (x86)\Windows Kits\10\Include\...\um\GL\`

### Linker Error: "unresolved external symbol"
- Ensure all `.lib` files are linked (opengl32.lib, glu32.lib, etc.)
- Check that all source files are compiled

### Runtime Error: "MSVCP140.dll missing"
- Install Visual C++ Redistributable:
  https://aka.ms/vs/17/release/vc_redist.x64.exe

---

## Build Configuration Options

### Debug Build
```cmd
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
```

### Enable Address Sanitizer (Debug only)
Add to CMakeLists.txt:
```cmake
add_compile_options(/fsanitize=address)
```

### Static Linking (No DLL dependencies)
Change in CMakeLists.txt:
```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

---

## Output Structure

After build:
```
CAD_Monolith/
├── build/
│   ├── bin/
│   │   └── CadMonolith.exe      # Main executable
│   ├── obj/                     # Object files
│   └── Release/                 # Release build output
├── recovery/                    # Auto-save directory
├── include/                     # Header files
└── src/                         # Source files
```

---

## System Requirements

### Minimum
- Windows 10 (64-bit)
- 4 GB RAM
- DirectX 11 compatible GPU
- 500 MB disk space

### Recommended
- Windows 11 (64-bit)
- 16 GB RAM
- Dedicated GPU with 4GB VRAM
- SSD for faster file operations

---

## Next Steps After Building

1. **Test Basic Functionality**
   - Launch application
   - Create new document
   - Test undo/redo
   - Save and reload file

2. **Run Recovery Test**
   - Create a document
   - Force-close application
   - Restart and verify auto-recovery

3. **Performance Testing**
   - Load large assemblies
   - Monitor FPS in viewport
   - Check memory usage

For technical support or bug reports, consult the implementation documentation.
