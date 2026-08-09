#include <windows.h>
#include <commctrl.h>
#include <iostream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "Core/SessionManager.h"
#include "Core/TransactionManager.h"
#include "Kernel/GeometryKernelProxy.h"
#include "Persistence/PersistenceManager.h"
#include "UI/MainWindow.h"

using namespace Cad;

// Global variables
HINSTANCE g_hInstance = nullptr;
HWND g_hMainWnd = nullptr;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);

// Entry point for Windows GUI application
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    
    g_hInstance = hInstance;
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Initialize CAD Session Manager (The Voss Protocol)
    std::cout << "===========================================\n";
    std::cout << "  CAD MONOLITH - Initializing Core System\n";
    std::cout << "  Following Voss Protocol for Stability\n";
    std::cout << "===========================================\n\n";
    
    auto& sessionMgr = SessionManager::getInstance();
    auto initResult = sessionMgr.initialize();
    
    if (!initResult.isSuccess()) {
        MessageBoxA(nullptr, 
            "Failed to initialize CAD kernel.\n\n" 
            "Error: " + initResult.getError().message + "\n\n"
            "The application will now exit.",
            "Initialization Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    // Register window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_CADMONOLITH);
    wcex.lpszClassName = L"CadMonolithWindowClass";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wcex)) {
        MessageBoxA(nullptr, "Failed to register window class", "Error", MB_ICONERROR);
        return 1;
    }
    
    // Create main window
    g_hMainWnd = CreateWindowExW(
        0,
        L"CadMonolithWindowClass",
        L"CAD Monolith - Professional Desktop CAD",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 960,
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (!g_hMainWnd) {
        MessageBoxA(nullptr, "Failed to create main window", "Error", MB_ICONERROR);
        return 1;
    }
    
    // Initialize MainWindow UI components
    if (!InitializeMainWindow(g_hMainWnd, hInstance)) {
        MessageBoxA(nullptr, "Failed to initialize UI", "Error", MB_ICONERROR);
        return 1;
    }
    
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    
    std::cout << "[MAIN] Application window created successfully\n";
    std::cout << "[MAIN] Entering message loop...\n\n";
    
    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup
    std::cout << "[MAIN] Shutting down...\n";
    SessionManager::destroyInstance();
    TransactionManager::destroyInstance();
    GeometryKernelProxy::destroyInstance();
    PersistenceManager::destroyInstance();
    
    return static_cast<int>(msg.wParam);
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId) {
            case IDM_FILE_NEW:
                OnFileNew(hWnd);
                break;
            case IDM_FILE_OPEN:
                OnFileOpen(hWnd);
                break;
            case IDM_FILE_SAVE:
                OnFileSave(hWnd);
                break;
            case IDM_FILE_EXIT:
                DestroyWindow(hWnd);
                break;
            case IDM_EDIT_UNDO:
                OnEditUndo(hWnd);
                break;
            case IDM_EDIT_REDO:
                OnEditRedo(hWnd);
                break;
            case IDM_HELP_ABOUT:
                DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, AboutDlgProc);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
        
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // Drawing handled by Viewport control
            // This is just a placeholder
            
            EndPaint(hWnd, &ps);
        }
        break;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
        
    case WM_SIZE:
        OnResize(hWnd, LOWORD(lParam), HIWORD(lParam));
        break;
        
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// About dialog procedure
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (message) {
    case WM_INITDIALOG:
        {
            char buffer[256];
            sprintf_s(buffer, "CAD Monolith v1.0.0\nBuild: %s %s", __DATE__, __TIME__);
            SetDlgItemTextA(hDlg, IDC_STATIC_VERSION, buffer);
        }
        return TRUE;
        
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// ============================================================================
// Command Handlers
// ============================================================================

void OnFileNew(HWND hWnd) {
    std::cout << "[UI] File > New\n";
    auto& session = SessionManager::getInstance();
    session.startNewSession("");
}

void OnFileOpen(HWND hWnd) {
    std::cout << "[UI] File > Open\n";
    
    OPENFILENAMEA ofn = {};
    char szFile[MAX_PATH] = "";
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = "CAD Files (*.cad)\0*.cad\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "cad";
    
    if (GetOpenFileNameA(&ofn)) {
        auto& persistence = PersistenceManager::getInstance();
        auto result = persistence.loadFile(szFile);
        
        if (result.isSuccess()) {
            std::cout << "[UI] File loaded successfully: " << szFile << "\n";
        } else {
            MessageBoxA(hWnd, 
                ("Failed to load file: " + result.getError().message).c_str(),
                "Load Error", MB_ICONWARNING);
        }
    }
}

void OnFileSave(HWND hWnd) {
    std::cout << "[UI] File > Save\n";
    
    // Placeholder - would get current document data and save
    auto& persistence = PersistenceManager::getInstance();
    DocumentData data;
    data.sessionId = SessionManager::getInstance().getSessionId();
    data.entityCount = 0;
    
    auto result = persistence.saveFile("C:\\temp\\test.cad", data);
    if (!result.isSuccess()) {
        MessageBoxA(hWnd, 
            ("Failed to save file: " + result.getError().message).c_str(),
            "Save Error", MB_ICONWARNING);
    }
}

void OnEditUndo(HWND hWnd) {
    std::cout << "[UI] Edit > Undo\n";
    auto& tm = TransactionManager::getInstance();
    auto result = tm.undo();
    
    if (!result.isSuccess()) {
        // Could show status bar message "Nothing to undo"
    }
}

void OnEditRedo(HWND hWnd) {
    std::cout << "[UI] Edit > Redo\n";
    auto& tm = TransactionManager::getInstance();
    auto result = tm.redo();
    
    if (!result.isSuccess()) {
        // Could show status bar message "Nothing to redo"
    }
}

bool InitializeMainWindow(HWND hWnd, HINSTANCE hInstance) {
    (void)hInstance;
    // In real implementation, would create toolbar, ribbon, viewport, etc.
    std::cout << "[UI] Main window initialized\n";
    return true;
}

void OnResize(HWND hWnd, int width, int height) {
    // Resize viewport and other controls
    (void)hWnd;
    (void)width;
    (void)height;
}
