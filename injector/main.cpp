// Simple DLL injector for borderless.dll into BeamNG.drive
//
// Usage:  injector.exe [pid]
//   If no PID is provided the injector searches for BeamNG.drive.x64.exe.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <TlHelp32.h>
#include <Shlwapi.h>
#include <cstdio>
#include <string>

#pragma comment(lib, "shlwapi.lib")

// ── Find process by name ────────────────────────────────────────────────────
static DWORD FindProcess(const wchar_t* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    DWORD pid = 0;
    if (Process32FirstW(snap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, name) == 0)
            {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// ── Resolve absolute path to borderless.dll next to this exe ────────────────
static std::wstring GetDllPath()
{
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    PathRemoveFileSpecW(buf);
    std::wstring path(buf);
    path += L"\\borderless.dll";
    return path;
}

// ── Inject ──────────────────────────────────────────────────────────────────
static bool Inject(DWORD pid, const std::wstring& dllPath)
{
    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!hProc)
    {
        wprintf(L"[!] Failed to open process %lu (error %lu). Run as administrator.\n",
                pid, GetLastError());
        return false;
    }

    size_t pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(hProc, nullptr, pathBytes,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem)
    {
        wprintf(L"[!] VirtualAllocEx failed (error %lu)\n", GetLastError());
        CloseHandle(hProc);
        return false;
    }

    if (!WriteProcessMemory(hProc, remoteMem, dllPath.c_str(), pathBytes, nullptr))
    {
        wprintf(L"[!] WriteProcessMemory failed (error %lu)\n", GetLastError());
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC pLoadLib = GetProcAddress(hKernel, "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(
        hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)pLoadLib,
        remoteMem, 0, nullptr);
    if (!hThread)
    {
        wprintf(L"[!] CreateRemoteThread failed (error %lu). Run as administrator.\n",
                GetLastError());
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return true;
}

// ── Main ────────────────────────────────────────────────────────────────────
int wmain(int argc, wchar_t* argv[])
{
    wprintf(L"BeamNG Borderless Ultrawide – Injector\n\n");

    DWORD pid = 0;

    if (argc >= 2)
    {
        pid = wcstoul(argv[1], nullptr, 10);
    }
    else
    {
        wprintf(L"[*] Searching for BeamNG.drive.x64.exe ...\n");
        pid = FindProcess(L"BeamNG.drive.x64.exe");
        if (!pid)
        {
            wprintf(L"[!] BeamNG.drive.x64.exe not found. "
                    L"Start the game first or pass its PID.\n");
            return 1;
        }
    }

    std::wstring dllPath = GetDllPath();
    wprintf(L"[*] Target PID : %lu\n", pid);
    wprintf(L"[*] DLL path   : %s\n", dllPath.c_str());

    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        wprintf(L"[!] borderless.dll not found next to the injector.\n");
        return 1;
    }

    if (Inject(pid, dllPath))
        wprintf(L"[+] Injection successful!\n");
    else
        wprintf(L"[-] Injection failed.\n");

    return 0;
}
