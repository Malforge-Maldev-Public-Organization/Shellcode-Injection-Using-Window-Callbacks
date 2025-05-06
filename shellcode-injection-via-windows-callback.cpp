#include <windows.h>

// Minimal 64-bit shellcode to show MessageBox
unsigned char shellcode[] = {
    0x48, 0x83, 0xEC, 0x28, // sub rsp, 0x28
    0x48, 0x31, 0xC9,       // xor rcx, rcx
    0x48, 0x8D, 0x15, 0x1E, 0x00, 0x00, 0x00, // lea rdx, [rel msg]
    0x4C, 0x8D, 0x05, 0x1F, 0x00, 0x00, 0x00, // lea r8, [rel title]
    0x48, 0x31, 0xC9,       // xor rcx, rcx
    0x48, 0xB8, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // mov rax, MessageBoxA address placeholder
    0xFF, 0xD0,             // call rax
    0x48, 0x83, 0xC4, 0x28, // add rsp, 0x28
    0xC3,                   // ret
    0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x00, // "Hello World\0"
    0x54, 0x65, 0x73, 0x74, 0x00 // "Test\0"
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static LPVOID shellcodeAddr = NULL;
    if (msg == WM_USER + 100) {
        shellcodeAddr = VirtualAlloc(NULL, sizeof(shellcode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!shellcodeAddr) {
            MessageBoxW(hwnd, L"Memory allocation failed", L"Error", MB_OK | MB_ICONERROR);
            return 0;
        }
        memcpy(shellcodeAddr, shellcode, sizeof(shellcode));
        ((void(*)())shellcodeAddr)();
        VirtualFree(shellcodeAddr, 0, MEM_RELEASE);
        shellcodeAddr = NULL;
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"InjectWindow";

    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"Window class registration failed", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, L"InjectWindow", L"Shellcode Demo",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Window creation failed", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SendMessageW(hwnd, WM_USER + 100, 0, 0);

    MSG msg = {0};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
