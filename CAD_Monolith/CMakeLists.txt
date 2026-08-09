cmake_minimum_required(VERSION 3.20)
project(CadMonolith VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows Specific Settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DNOMINMAX) # Prevent min/max macro conflicts
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    
    # Enable strict warning levels for safety
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus)
    
    # Enable AddressSanitizer for Debug builds (optional but recommended for dev)
    # add_compile_options(/fsanitize=address) 
endif()

# Dependencies (Vendored or System)
# For this standalone blueprint, we assume standard lib + OpenGL headers
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# Include Directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# ==============================================================================
# CORE STABILITY KERNEL (The "Voss Protocol")
# ==============================================================================

set(CORE_SOURCES
    src/Core/Result.cpp
    src/Core/MemoryGuard.cpp
    src/Core/SessionManager.cpp
    src/Core/TransactionManager.cpp
    src/Threading/HeartbeatMonitor.cpp
    src/Kernel/GeometryKernelProxy.cpp
    src/Kernel/WorkerThread.cpp
)

add_library(CadCore STATIC ${CORE_SOURCES})
target_link_libraries(CadCore PRIVATE Threads::Threads)

# ==============================================================================
# GEOMETRY ENGINE
# ==============================================================================

set(GEOMETRY_SOURCES
    src/Geometry/BRepBody.cpp
    src/Geometry/NURBS.cpp
    src/Geometry/ConstraintGraph.cpp
    src/Geometry/GeometrySanitizer.cpp
)

add_library(CadGeometry STATIC ${GEOMETRY_SOURCES})
target_link_libraries(CadGeometry PRIVATE CadCore)

# ==============================================================================
# PERSISTENCE LAYER
# ==============================================================================

set(PERSISTENCE_SOURCES
    src/Persistence/PersistenceManager.cpp
    src/Persistence/FileAtomizer.cpp
)

add_library(CadPersistence STATIC ${PERSISTENCE_SOURCES})
target_link_libraries(CadPersistence PRIVATE CadCore)

# ==============================================================================
# MAIN APPLICATION (Windows Entry Point)
# ==============================================================================

set(APP_SOURCES
    src/main.cpp
    src/UI/MainWindow.cpp
    src/UI/Viewport.cpp
    src/UI/AppController.cpp
)

add_executable(CadMonolith WIN32 ${APP_SOURCES})
target_link_libraries(CadMonolith PRIVATE 
    CadCore 
    CadGeometry 
    CadPersistence 
    OpenGL::GL 
    glu32 # For basic GLU utilities on Windows
)

# Post-build step to create recovery directory
add_custom_command(TARGET CadMonolith POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CadMonolith>/recovery"
    COMMENT "Creating recovery directory for crash dumps..."
)
