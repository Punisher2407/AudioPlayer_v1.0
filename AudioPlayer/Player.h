#ifndef PLAYER_H
#define PLAYER_H

#include "common.h"
#include <MMReg.h>

//extern vars
extern HWND hVideoWindow;

//declarations of vars
extern HWND hWnd;
extern PLAYER_STATE PlayerState;
extern long Volume;
extern bool FullScreen;
extern RECT VRect;

//exports

BOOL Player_OpenFile(WCHAR* filename);
void PlayFile(TCHAR* lpstrFileName);
void Play();
void Pause();
void Resume();
void Stop();
void Close();
DWORD GetLength();
DWORD GetPosition();
void Rewind();
void SetPosition(LONGLONG dwPos);
void SetVolume(long vol);
WORD GetMultimediaInfo(SMP_AUDIOINFO* pAudioInfo, PLAYER_STREAM* pStreamType);
void GetMultimediaInfoString(WCHAR* text,size_t size);
int GetVolume();

// Sets callback function to notify about player events (called from UI)
void Player_SetEventCallback(PLAYER_EVENT_CALLBACK callback);

// Processes messages related to playback events. 
// Called by window procedure (in ui.cpp) that receives messages.
void Player_ProcessNotify(WPARAM NotifyValue);

// Show property page for the specified node
void ShowPropertyPage(TOPOLOGY_NODE node);

#endif
