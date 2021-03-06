#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' "\
"version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(linker,"/MERGE:.rdata=.text")
#pragma comment(linker,"/SECTION:.text,EWR")

#include <Windows.h>

extern void InitApplication();
extern void RunMessageLoop();
extern void UnloadApplication();
extern void EnsureSingleInstance();
extern void ShowUI();
extern void InitPlayerState();
extern void InitResources(HMODULE h);

extern void LoadSettings();

// Entry point
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR pstrCmdLine, int nCmdShow){

    EnsureSingleInstance();
    LoadSettings();

    InitApplication();
    InitPlayerState();
    ShowUI();
    RunMessageLoop();
    UnloadApplication();

}
