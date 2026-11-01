#include "server/server.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
const auto launch_start = std::chrono::steady_clock::now();
int smoke(const std::filesystem::path& output) {
    double ready_ms=0;
    const bool passed=wb::scaffold_smoke(launch_start,ready_ms);
    if(!output.empty()) {
        std::ofstream file(output);
        file << "{\"launch_to_ready_ms\":" << ready_ms << ",\"scope\":\"HTTP health and FEA only; full smoke pending\",\"passed\":"
             << (passed?"true":"false") << "}\n";
        file.close();
        if(!file) return 1;
    }
    return passed?0:1;
}
}
#ifdef _WIN32
#include <shellapi.h>
namespace {
wb::LocalServer* active=nullptr;
std::wstring url;
void open_browser() {
    ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
}
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if(message==WM_COMMAND) {
        if(LOWORD(wparam)==1) {open_browser();return 0;}
        if(LOWORD(wparam)==2) {DestroyWindow(window);return 0;}
    }
    if(message==WM_APP+1) {
        ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);open_browser();return 0;
    }
    if(message==WM_DESTROY) {if(active) active->stop();PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,wparam,lparam);
}
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int) {
    int count=0;
    auto raw=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!raw) return 1;
    std::vector<std::wstring> args(raw,raw+count);LocalFree(raw);
    bool test=false,no_browser=false;
    std::filesystem::path output;
    for(std::size_t i=1;i<args.size();++i) {
        if(args[i]==L"--smoke-test") test=true;
        else if(args[i]==L"--no-browser") no_browser=true;
        else if(args[i]==L"--out" && i+1<args.size()) output=args[++i];
        else return 2;
    }
    if(test) return smoke(output);
    HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\PassScienceWorkbench");
    if(!mutex) return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        HWND previous=FindWindowW(L"PassScienceWorkbenchWindow",nullptr);
        if(previous) PostMessageW(previous,WM_APP+1,0,0);
        CloseHandle(mutex);return previous?0:1;
    }
    int result=1;
    try {
        wchar_t executable[32768]{};
        const auto length=GetModuleFileNameW(nullptr,executable,32768);
        if(length==0 || length>=32768) throw std::runtime_error("Executable path unavailable");
        wb::LocalServer server(std::filesystem::path(executable).parent_path()/L"web");
        if(!server.start()) throw std::runtime_error("Server unavailable");
        httplib::Client client("127.0.0.1",server.port());
        auto health=client.Get("/api/health",{{"X-Session-Token",server.token()}});
        if(!health || health->status!=200) throw std::runtime_error("Health check failed");
        const auto address="http://127.0.0.1:"+std::to_string(server.port())+"/session?token="+server.token();
        url.assign(address.begin(),address.end());active=&server;
        WNDCLASSW wc{};wc.lpfnWndProc=window_proc;wc.hInstance=instance;
        wc.lpszClassName=L"PassScienceWorkbenchWindow";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        if(!RegisterClassW(&wc)) throw std::runtime_error("Window registration failed");
        HWND window=CreateWindowW(wc.lpszClassName,L"Unified Engineering Workbench",
            WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,480,190,
            nullptr,nullptr,instance,nullptr);
        if(!window) throw std::runtime_error("Window creation failed");
        CreateWindowW(L"STATIC",L"Unified Engineering Workbench is running locally.",WS_CHILD|WS_VISIBLE,
            20,20,430,30,window,nullptr,instance,nullptr);
        CreateWindowW(L"BUTTON",L"Open",WS_CHILD|WS_VISIBLE|WS_TABSTOP,20,70,120,32,
            window,reinterpret_cast<HMENU>(1),instance,nullptr);
        CreateWindowW(L"BUTTON",L"Quit",WS_CHILD|WS_VISIBLE|WS_TABSTOP,160,70,120,32,
            window,reinterpret_cast<HMENU>(2),instance,nullptr);
        ShowWindow(window,SW_SHOW);
        if(!no_browser) open_browser();
        MSG msg{};
        while(GetMessageW(&msg,nullptr,0,0)>0) {
            if(!IsDialogMessageW(window,&msg)) {TranslateMessage(&msg);DispatchMessageW(&msg);}
        }
        active=nullptr;result=0;
    } catch(...) {
        active=nullptr;
        MessageBoxW(nullptr,L"Unable to start the local workbench. Close another instance and try again.",
            L"Launch error",MB_OK|MB_ICONERROR);
    }
    CloseHandle(mutex);return result;
}
#else
#include <iostream>
int main(int argc,char** argv) {
    bool test=false;
    std::filesystem::path output;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--smoke-test") test=true;
        else if(arg=="--out" && i+1<argc) output=argv[++i];
        else return 2;
    }
    if(test) return smoke(output);
    std::cerr<<"Native launcher targets Windows; browser UI remains under development.\n";
    return 2;
}
#endif
