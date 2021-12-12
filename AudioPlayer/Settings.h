#ifndef SMPSETTINGS_H
#define SMPSETTINGS_H

#ifndef UNICODE
#define UNICODE
#endif

#include "player.h"
#include "PlaylistEditor.h"
#include "DirManager.h"

//extern
extern WCHAR ProgramFileName[256];
extern long CurrVolume;
extern void UpdateVolume();

//this module
extern PLAYERSETTINGS Settings;
void RestoreLastPosition();
void SaveLastPosition();
void WriteSettings();
void LoadSettings();
BOOL CALLBACK SettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void GetDataDir(WCHAR* dir, int maxcount);

#endif
