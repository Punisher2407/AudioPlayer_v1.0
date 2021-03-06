#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

#include "resource.h"
#include "player.h"
#include "ScrollbarControl.h"

#include "PlayListEditor.h"
#include "DirManager.h"
#include "Settings.h"
#include "errors.h"

#define S_TRACK_SIZE 20
#define TIMER_UPDATE_POSITION 2
#define VOLUME_INCREMENT 10

DWORD WINAPI DirThreadFunc(TCHAR* dir);
void DisplayOpenedFileTags();
extern int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
extern TAGS_GENERIC OpenedFileTags;
extern bool fOpenedFileTags;
extern HWND hlstBuffer;

//GLOBAL VARIABLES
HWND hMainWnd;
HWND hMainDlg;
HWND hPlaybackDlg;
ScrollbarControl Progress;
HWND hScrollbar;
HWND hVolume;
HWND hVideoWindow;
HWND hCoverWnd;
HACCEL hAccel;
HICON hSMPIcon = NULL;
HMENU hcMainMenu = NULL;
HMENU hcPlaylistMenu = NULL;

RECT rcInfoBox = { 0 };
TCHAR txtMediaInfo[1000] = { 0 };

HANDLE hPipe = NULL;
HANDLE hMutex;
HANDLE hThread;

BOOL IsTimerWorking = FALSE;
bool fPlaylistBlocked = FALSE;

TCHAR strInputURL[500] = L"";
TCHAR strInputFiles[5000] = L"";
bool fInputURL = false;
TCHAR strInputFolder[MAX_PATH] = L"";
bool IsTrayIcon = false;
bool fCursorShown = true;
int pls_LastSortColumn = 99;
bool pls_SortReverse = false;
bool fBlockContextMenu = false;
long CurrVolume = 0;
DWORD pos_prev = 0;
ULONG_PTR gdip_code = 0;
HMODULE hResModule = NULL;

TCHAR strMediafileFilter[] = L"All files\0*.*\0\
Audio files\0*.wav;*.mp1;*.mp2;*.mp3;*.wma;*.cda;*.aac;*.ogg;*.flac;*.m4a\0\
Images\0*.jpg;*.jpeg;*.bmp;*.png;*.gif\0\
MIDI\0*.mid;*.midi\0\
Playlist\0*.m3u;*.lst\0\
MPEG Audio\0*.mp1;*.mp2;*.mp3\0\
Waveform Audio\0*.wav\0\
Advanced systems format (Windows Media)\0*.wma;*.wmv;*.asf\0\
CD Digital Audio Tracks\0*.cda\0\
Advanced audio coding\0*.aac\0\
Vorbis OGG\0*.ogg\0\
Motion picture experts group\0*.mpg;*.mpeg;*.ts;*.mp4\0";

TCHAR strPlaylistFilter[] = L"Playlist\0*.m3u;*.lst\0LST playlist\0*.lst\0M3U playlist\0*.m3u\0All files\0*.*\0";

//settings

DWORD UpdatePosTimerPeriod = 1000;


//========================================
void UpdatePlaylist() {
	int pw, ph;
	RECT rc;
	BOOL res;
	res = GetClientRect(hMainDlg, &rc);
	if (res == FALSE)return;
	pw = rc.right - rc.left;
	ph = rc.bottom - rc.top;

	SetWindowPos(PlayList, HWND_TOP, 0, 40, pw, ph - 40, SWP_SHOWWINDOW | SWP_NOZORDER);
}

// size changing for the window
void UpdateView() {
	int w, h;
	int w_playlist, h_playlist;
	int w_cover, h_cover;
	int w_total, h_total;
	int x_cover;

	int right = 0, bottom = 0;
	RECT rc;

	GetClientRect(hMainWnd, &rc);//get main window size
	w_total = rc.right - rc.left;
	h_total = rc.bottom - rc.top;

	GetWindowRect(hPlaybackDlg, &rc);//get playback window size
	w = rc.right - rc.left;
	h = rc.bottom - rc.top;
	right = w;
	bottom = h;
	w_playlist = w;//playlist width is the same

	/*calculate others*/
	w_cover = w_total - w_playlist;
	h_playlist = h_total - h;
	h_cover = h_playlist;
	rcInfoBox.top = 4; rcInfoBox.bottom = h;
	rcInfoBox.left = w + 4; rcInfoBox.right = w_total;

	if (Settings.fShowPlaylist == false) 
	{ 
		x_cover = 0; 
		w_cover = w_total; 
	}//if no playlist, cover takes all width
	else 
		x_cover = w;

	if (Settings.fShowPlaylist == true && Settings.fShowCover == false)
		w_playlist = w_total;//if no cover, playlist takes all width

	/*adjust subwindows positions...*/
	SetWindowPos(hPlaybackDlg, HWND_TOP, 0, 0, w, h, SWP_SHOWWINDOW);

	if (Settings.fShowPlaylist == true)
	{
		SetWindowPos(hMainDlg, HWND_TOP, 0, h, w_playlist, h_playlist, SWP_NOACTIVATE);
		ShowWindow(hMainDlg, SW_SHOW);
		UpdatePlaylist();
	}
	else
	{
		ShowWindow(hMainDlg, SW_HIDE);
	}

	if (Settings.fShowCover == true)
	{
		SetWindowPos(hCoverWnd, HWND_TOP, x_cover, h, w_cover, h_cover, SWP_NOACTIVATE);
		ShowWindow(hCoverWnd, SW_SHOW);
	}
	else
	{
		ShowWindow(hCoverWnd, SW_HIDE);
	}

}

void SaveWindowSize()
{
	RECT rc2;
	/*Set window size in settings*/
	GetWindowRect(hMainWnd, &rc2);
	Settings.WndX = rc2.left;
	Settings.WndY = rc2.top;
	Settings.WndWidth = rc2.right - rc2.left;
	Settings.WndHeight = rc2.bottom - rc2.top;
	Settings.WndMaximized = (IsZoomed(hMainWnd) != FALSE);
}

void ProcessMessages() {
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (IsIconic(hMainWnd) == FALSE) {
			if (TranslateAccelerator(hMainWnd, hAccel, &msg))continue;
		}
		else {
			if (TranslateAccelerator(hVideoWindow, hAccel, &msg))continue;
		}
		if (IsDialogMessage(hMainDlg, &msg))
			continue;
		if (IsDialogMessage(hPlaybackDlg, &msg))
			continue;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

}

void UpdateGUI() {
	int id;
	MENUITEMINFO mi = { 0 };
	switch (CurrMode) {
	case NORMAL:
		id = ID_MODE_NORMAL; 
		break;
	case REPEAT_FILE: 
		id = ID_MODE_REPEATFILE; 
		break;
	case REPEAT_LIST: 
		id = ID_MODE_REPEATLIST; 
		break;
	case RANDOM: 
		id = ID_MODE_RANDOM; 
		break;
	}
	CheckMenuRadioItem(GetMenu(hMainWnd), ID_MODE_NORMAL, ID_MODE_RANDOM, id, MF_BYCOMMAND);

	mi.cbSize = sizeof(mi);
	mi.fMask = MIIM_STATE;
	if (Settings.fShowPlaylist == true)
		mi.fState = MFS_CHECKED;
	else 
		mi.fState = MFS_UNCHECKED;
	SetMenuItemInfo(GetMenu(hMainWnd), ID_SHOWPLAYLIST, FALSE, &mi);
	mi.cbSize = sizeof(mi);
	mi.fMask = MIIM_STATE;
	if (Settings.fShowCover == true)
		mi.fState = MFS_CHECKED;
	else mi.fState = MFS_UNCHECKED;
	SetMenuItemInfo(GetMenu(hMainWnd), ID_SHOWCOVER, FALSE, &mi);
}

// for tray
void ShowContextMenu(BYTE nMenuNum, int x, int y) {
	MENUITEMINFO mi = { 0 };
	if (nMenuNum == MNUM_MAINMENU) {
		mi.cbSize = sizeof(mi);
		mi.fMask = MIIM_STRING;
		if (IsTrayIcon == false) {
			mi.dwTypeData = L"Свернуть в значок";
		}
		else 
		{ 
			mi.dwTypeData = L"Развернуть в окно"; 
		}
		SetMenuItemInfo(hcMainMenu, ID_MINIMIZE, FALSE, &mi);
		TrackPopupMenu(hcMainMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, x, y, 0, hMainWnd, NULL);
	}
	else {
		TrackPopupMenu(hcPlaylistMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, x, y, 0, hMainWnd, NULL);
	}
}

// for tray icon
void SwitchTrayIcon() {
	TCHAR str[256] = L"";
	int i, c;
	NOTIFYICONDATA nd = { 0 };
	nd.cbSize = sizeof(nd);
	nd.hWnd = hMainWnd;
	nd.uID = 1;
	nd.uCallbackMessage = UM_TRAYICON;
	nd.hIcon = hSMPIcon;
	if (PlayerState == FILE_NOT_LOADED) {
		StringCchCopy(nd.szTip, 128, L"Audio Player");
	}
	else {
		GetCurrTrackShortName(str); StringCchCat(str, 256, L" / Audio Player");
		c = lstrlen(str);
		for (i = 0; i < 126; i++) {
			if (i >= c)break;
			nd.szTip[i] = str[i];
		}
		nd.szTip[i] = 0;
	}
	nd.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	if (IsTrayIcon == false) {
		IsTrayIcon = true;
		ShowWindow(hMainWnd, SW_HIDE);
		Shell_NotifyIcon(NIM_ADD, &nd);
	}
	else {
		IsTrayIcon = false;
		ShowWindow(hMainWnd, SW_SHOW);
		Shell_NotifyIcon(NIM_DELETE, &nd);
	}
}

// info at the bottom of the tray icon
void UpdateTrayIconText() {
	TCHAR str[256] = L"";
	int i, c;
	NOTIFYICONDATA nd = { 0 };

	if (IsTrayIcon == false) { return; }
	nd.cbSize = sizeof(nd);
	nd.hWnd = hMainWnd;
	nd.uID = 1;
	if (PlayerState == FILE_NOT_LOADED) {
		StringCchCopy(nd.szTip, 128, L"Audio Player");
	}
	else {
		GetCurrTrackShortName(str); StringCchCat(str, 256, L" / Audio Player");
		c = lstrlen(str);
		for (i = 0; i < 126; i++) {
			if (i >= c)break;
			nd.szTip[i] = str[i];
		}
		nd.szTip[i] = 0;
	}
	nd.uFlags = NIF_TIP;
	Shell_NotifyIcon(NIM_MODIFY, &nd);
}

void DisplayFileInfo(int n) {

	//display info from tags

	if (n == CurrentTrack && PlayerState != FILE_NOT_LOADED) {
		//if file is opened, display info from it
		DisplayOpenedFileTags();
		return;
	}

	WCHAR text[5000] = L"";
	TCHAR ext[10] = L"";
	WCHAR buf[MAX_PATH] = L"";
	TAGS_GENERIC info = { 0 };
	BOOL res = FALSE;
	int h, sec, min;
	GetPlaylistElement(n, buf);
	GetFileExtension(buf, ext);

	//read tags
	res = ReadTagsv2(buf, &info);
	if (res == FALSE && lstrcmpi(ext, L"flac") == 0) { res = ReadFlacTags(buf, &info); }
	if (res == FALSE)ReadApeTags(buf, &info);
	if (res == FALSE)ReadTagsV1(buf, &info);

	//display tags in message box
	StringCchCopy(text, 5000, buf);
	StringCchCat(text, 5000, L"\n\nНазвание: ");
	StringCchCat(text, 5000, info.title);
	StringCchCat(text, 5000, L"\nИсполнитель: ");
	StringCchCat(text, 5000, info.artist);
	StringCchCat(text, 5000, L"\nАльбом: ");
	StringCchCat(text, 5000, info.album);
	StringCchCat(text, 5000, L"\nГод: ");
	StringCchCat(text, 5000, info.year);
	StringCchCat(text, 5000, L"\n\nКомпозитор: ");
	StringCchCat(text, 5000, info.composer);
	StringCchCat(text, 5000, L"\nОписание: ");
	StringCchCat(text, 5000, info.comments);
	StringCchCat(text, 5000, L"\nURL: ");
	StringCchCat(text, 5000, info.URL);
	if (info.length != 0) {
		sec = info.length / 1000;
		h = sec / 3600;
		sec = sec % 3600;
		min = sec / 60; sec = sec % 60;
		StringCchCat(text, 5000, L"\nДлина: ");
		StringCchPrintf(buf, 256, L"%02d:%02d:%02d\n", h, min, sec);
		StringCchCat(text, 5000, buf);
	}
	StringCchCat(text, 5000, L"\n\nТип метаданных:");
	switch (info.type) {
	case TAG_ID3V1:StringCchCat(text, 5000, L"ID3V1"); break;
	case TAG_ID3V2:StringCchCat(text, 5000, L"ID3V2"); break;
	case TAG_APE:StringCchCat(text, 5000, L"APE"); break;
	case TAG_FLAC:StringCchCat(text, 5000, L"FLAC (Vorbis comment)"); break;
	case TAG_NO:
	default:StringCchCat(text, 5000, L"Нет"); break;
	}
	MessageBox(hMainWnd, text, L"Информация о файле", MB_OK);
}

void DisplayOpenedFileTags() {
	WCHAR text[5000] = L"";
	WCHAR buf[MAX_PATH] = L"";
	int min, h, sec;
	GetPlaylistElement(CurrentTrack, buf);
	StringCchCopy(text, 5000, buf);
	if (fOpenedFileTags == true) {
		StringCchCat(text, 5000, L"\n\nНазвание: ");
		StringCchCat(text, 5000, OpenedFileTags.title);
		StringCchCat(text, 5000, L"\nИсполнитель: ");
		StringCchCat(text, 5000, OpenedFileTags.artist);
		StringCchCat(text, 5000, L"\nАльбом: ");
		StringCchCat(text, 5000, OpenedFileTags.album);
		StringCchCat(text, 5000, L"\nГод: ");
		StringCchCat(text, 5000, OpenedFileTags.year);
		StringCchCat(text, 5000, L"\n\nКомпозитор: ");
		StringCchCat(text, 5000, OpenedFileTags.composer);
		StringCchCat(text, 5000, L"\nОписание: ");
		StringCchCat(text, 5000, OpenedFileTags.comments);
		StringCchCat(text, 5000, L"\nURL: ");
		StringCchCat(text, 5000, OpenedFileTags.URL);
		if (OpenedFileTags.length != 0) {
			sec = OpenedFileTags.length / 1000;
			h = sec / 3600;
			sec = sec % 3600;
			min = sec / 60; sec = sec % 60;
			StringCchCat(text, 5000, L"\nДлина: ");
			StringCchPrintf(buf, 256, L"%02d:%02d:%02d\n", h, min, sec);
			StringCchCat(text, 5000, buf);
		}
	}

	StringCchCat(text, 5000, L"\n\nТип метаданных:");
	switch (OpenedFileTags.type) {
	case TAG_ID3V1:StringCchCat(text, 5000, L"ID3V1"); break;
	case TAG_ID3V2:StringCchCat(text, 5000, L"ID3V2"); break;
	case TAG_APE:StringCchCat(text, 5000, L"APE"); break;
	case TAG_FLAC:StringCchCat(text, 5000, L"FLAC (Vorbis comment)"); break;
	case TAG_NO:
	default:StringCchCat(text, 5000, L"Нет"); break;
	}
	MessageBox(hMainWnd, text, L"Информация о файле", MB_OK);
}

// metadata window with info
void DisplayMultimediaInfo() {
	WCHAR text[5000] = L"";
	GetMultimediaInfoString(text, 5000);
	MessageBox(hMainWnd, text, L"Информация о файле", MB_OK);
}

// takes multimedia info and create meta_string from this file
void GetMultimediaInfo(TCHAR* text, int len) {
	WORD wRes = 0;
	SMP_AUDIOINFO ai = { 0 };
	FOURCC_EXTRACTOR* fcc = NULL;
	TCHAR buf[MAX_PATH];
	PLAYER_STREAM stream = STREAM_UNKNOWN;

	GetPlaylistElement(CurrentTrack, buf);
	StringCchCopy(text, len, buf);

	wRes = GetMultimediaInfo(&ai, &stream);

	StringCchCat(text, len, L"\n\nФормат: ");
	switch (stream) {
	case STREAM_AVI:
		StringCchCat(text, len, L"Audio-Video Interleaved\n"); break;
	case STREAM_ASF:
		StringCchCat(text, len, L"Advanced Systems Format\n"); break;
	case STREAM_MPEG1:
		StringCchCat(text, len, L"MPEG1\n"); break;
	case STREAM_MPEG1VCD:
		StringCchCat(text, len, L"MPEG1 VideoCD\n"); break;
	case STREAM_MPEG2:
		StringCchCat(text, len, L"MPEG2\n"); break;
	case STREAM_WAVE:
		StringCchCat(text, len, L"Waveform Audio\n"); break;
	case STREAM_QUICKTIME:
		StringCchCat(text, len, L"Apple(tm) Quick Time Movie\n"); break;
	case STREAM_UNKNOWN:
	default:
		StringCchCat(text, len, L"[неизвестно]\n"); break;
	}
	StringCchCat(text, len, L"Аудио: ");
	if (wRes == INFORES_AUDIO || wRes == INFORES_BOTH) {

		switch (ai.wFormatTag) {
		case WAVE_FORMAT_PCM:
			StringCchCat(text, len, L"PCM Waveform Audio"); break;
		case AUDIO_DVI_ADPCM:
			StringCchCat(text, len, L"DVI ADPCM"); break;
		case AUDIO_MPEG1:
			StringCchCat(text, len, L"MPEG1 Layer 1/2"); break;
		case WAVE_FORMAT_MPEGLAYER3:
			StringCchCat(text, len, L"MPEG1 Layer3"); break;
		case WAVE_FORMAT_DOLBY_AC3_SPDIF:
		case AUDIO_AC3:
			StringCchCat(text, len, L"DOLBY AC3"); 
			break;
		case WAVE_FORMAT_WMAVOICE9:		
		case WAVE_FORMAT_WMAVOICE10:		
		case WAVE_FORMAT_MSAUDIO1:
		case WAVE_FORMAT_WMAUDIO2:		
		case WAVE_FORMAT_WMAUDIO3:	
		case WAVE_FORMAT_WMAUDIO_LOSSLESS:
		case WAVE_FORMAT_WMASPDIF: StringCchCat(text, len, L"Windows Media Audio"); break;
		case AUDIO_AAC4:
		case AUDIO_AAC3:
		case AUDIO_AAC2:
		case AUDIO_AAC:
		case AUDIO_MPEG4AAC:
		case WAVE_FORMAT_MPEG_ADTS_AAC:
		case WAVE_FORMAT_MPEG_RAW_AAC:
		case WAVE_FORMAT_NOKIA_MPEG_ADTS_AAC:
		case WAVE_FORMAT_NOKIA_MPEG_RAW_AAC:
		case WAVE_FORMAT_VODAFONE_MPEG_ADTS_AAC:
		case WAVE_FORMAT_VODAFONE_MPEG_RAW_AAC:
			StringCchCat(text, len, L"Advanced Audio Coding (AAC)"); 
			break;
		case AUDIO_FLAC:
			StringCchCat(text, len, L"Free Lossless Audio Codec (FLAC)"); 
			break;
		case AUDIO_WAVEPACK:
			StringCchCat(text, len, L"WavePack"); 
			break;
		case AUDIO_AMR:
			StringCchCat(text, len, L"VOICEAGE AMR"); 
			break;
		case AUDIO_MPEG2AAC:
			StringCchCat(text, len, L"MPEG2"); 
			break;
		default:
			StringCchCat(text, len, L"неизвестен"); 
			break;
		}
		StringCchPrintf(buf, 256, L" | %d кан. | %d Гц |", (int)ai.chans, (int)ai.nFreq);
		StringCchCat(text, len, buf);
		StringCchPrintf(buf, 256, L" %d кбит/с\n", (int)(ai.BitsPerSecond / 1000.0)); StringCchCat(text, 5000, buf);
	}
	else 
	{ 
		StringCchCat(text, len, L"[Нет]\n"); 
	}
}

void RunTimer() {
	if (IsTimerWorking != FALSE)return;
	SetTimer(hMainWnd, TIMER_UPDATE_POSITION, UpdatePosTimerPeriod, NULL);
	IsTimerWorking = TRUE;
}

void StopTimer() {
	if (IsTimerWorking == FALSE)return;

	KillTimer(hMainWnd, TIMER_UPDATE_POSITION);

	IsTimerWorking = FALSE;
}

// get all the audiofiles pathes
void ProcessOFNString(TCHAR* str, WORD nFileOffset) {
	int i, n;
	TCHAR* p = NULL;
	TCHAR FileName[MAX_PATH + 10] = L"";

	i = lstrlen(str);
	if (str[i + 1] == 0) {
		AddPlaylistElement(str);
		return;
	}
	p = &(str[nFileOffset]);
	n = nFileOffset;
	while (1) {
		StringCchCopy(FileName, MAX_PATH + 10, str);
		StringCchCat(FileName, MAX_PATH + 10, L"\\");
		StringCchCat(FileName, MAX_PATH + 10, p);
		AddPlaylistElement(FileName);
		i = lstrlen(p) + 1;
		n += i;
		if (str[n] == 0)break;
		if (n >= 5000)break;
		p = &(str[n]);
	}
}

// check for the initialize
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {

	switch (uMsg) {
	case BFFM_VALIDATEFAILED:
		return TRUE;
	case BFFM_INITIALIZED:
		if (lstrcmp(strInputFolder, L"") == 0)
			break;
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LONG)strInputFolder);
		break;
	}

	return 0;
}

// NOT_WORKING YET, for command line using.
void ProcessCommandLine(wchar_t* cmd) {
	int i, j = 0;

	BOOL fOpenFolder = FALSE;
	BOOL fAddFile = FALSE;
	BOOL FirstFile = TRUE;

	wchar_t filename[MAX_PATH] = L"";
	LPWSTR* args = NULL;
	int NumArgs = 0;

	args = CommandLineToArgvW(cmd, &NumArgs);
	if (args == NULL)
		return;
	if (NumArgs == 1) {
		if (IsTrayIcon == true)
			SwitchTrayIcon();
		LocalFree(args); return;
	}
	for (i = 1; i < NumArgs; i++) {
		if (args[i][0] == '/') {
			switch (args[i][1]) {
			case 'F':
			case 'f':
				fOpenFolder = TRUE; 
				break;
			case 'a':
			case 'A':
				fAddFile = TRUE;
			}
			continue;
		}

		if (fOpenFolder != FALSE) {
			StringCchCopy(filename, MAX_PATH, args[i]);
			StringCchCat(filename, MAX_PATH, L"\\");
			Playlist_AddDirectory(filename);
			continue;
		}
		else {
			if (FirstFile != FALSE) {
				FirstFile = FALSE;
				if (fAddFile == FALSE) {
					//open new file - discard existing playlist
					Close();
					ClearPlaylist();
				}
			}
			AddPlaylistElement(args[i]);
			continue;
		}
	}//end for

	if (fAddFile == FALSE && fOpenFolder == FALSE) {
		PlayTrackByNumber(0);
	}
	LocalFree(args);
}

// playing timer
void UpdateLength() {
	DWORD l;
	int h, m, s;
	TCHAR buf[256] = L"";
	TCHAR str[256] = L"";
	TCHAR cover[MAX_PATH] = L"";
	LVITEM item = { 0 };


	if (PlayerState == FILE_NOT_LOADED)
		return;
	l = GetLength() / 1000;
	h = (int)l / 3600;
	m = (int)l % 3600;
	s = m % 60;
	m = m / 60;

	StringCchPrintf(buf, 256, L"%02d:%02d:%02d", h, m, s);
	SetDlgItemText(hPlaybackDlg, IDC_LENGTH, buf);

	GetCurrTrackShortName(str);
	SetDlgItemText(hPlaybackDlg, IDC_CURRENTFILE, str);
	StringCchCat(str, 256, L" / Audio Player");
	SetWindowText(hMainWnd, str);
	UpdateTrayIconText();

	Progress = ScrollbarControl(SB_CTL, hScrollbar, 0, (int)(l + S_TRACK_SIZE), S_TRACK_SIZE);
	GetPlaylistElement(CurrentTrack, buf);
	InvalidateRect(hCoverWnd, NULL, TRUE);

	GetMultimediaInfo(txtMediaInfo, 1000);
	InvalidateRect(hMainWnd, &rcInfoBox, TRUE);
}

// full timer 
void UpdatePosition() {
	if (PlayerState == FILE_NOT_LOADED) 
	{ 
		Progress.SetTrackPosition(0); 
		return; 
	}
	DWORD p;
	TCHAR buf[256];
	int h, m, s;

	p = GetPosition() / 1000;

	if (p == pos_prev)
		return;

	h = (int)p / 3600;
	m = (int)p % 3600;
	s = m % 60;
	m = m / 60;

	StringCchPrintf(buf, 256, L"%02d:%02d:%02d", h, m, s);

	SetDlgItemText(hPlaybackDlg, IDC_POSITION, buf);
	Progress.SetTrackPosition((int)p);
	pos_prev = p;
}

void UpdateVolume()//call when CurrVolume changes programmatically
{
	SetVolume(100 - CurrVolume);
	SendMessage(hVolume, TBM_SETPOS, TRUE, CurrVolume);
}

// start open dialog in other threads
DWORD WINAPI DirThreadFunc(TCHAR* dir) {
	HWND hWnd;
	HMENU hMenu;
	MENUITEMINFO mi = { 0 };

	fPlaylistBlocked = true;
	hMenu = GetMenu(hMainWnd);
	hWnd = GetDlgItem(hMainDlg, ID_ADDFILE);
	EnableWindow(hWnd, FALSE);
	hWnd = GetDlgItem(hMainDlg, ID_ADDFOLDER);
	EnableWindow(hWnd, FALSE);
	hWnd = GetDlgItem(hMainDlg, ID_CLEAR);
	EnableWindow(hWnd, FALSE);
	hWnd = GetDlgItem(hMainDlg, ID_DELETE);
	EnableWindow(hWnd, FALSE);
	mi.cbSize = sizeof(mi);
	mi.fMask = MIIM_STATE;
	mi.fState = MFS_DISABLED;
	SetMenuItemInfo(hMenu, ID_ADDFILE, FALSE, &mi);
	SetMenuItemInfo(hMenu, ID_ADDFOLDER, FALSE, &mi);
	SetMenuItemInfo(hMenu, ID_OPEN, FALSE, &mi);


	Playlist_AddDirectory(dir);

	hWnd = GetDlgItem(hMainDlg, ID_ADDFILE);
	EnableWindow(hWnd, TRUE);
	hWnd = GetDlgItem(hMainDlg, ID_ADDFOLDER);
	EnableWindow(hWnd, TRUE);
	hWnd = GetDlgItem(hMainDlg, ID_CLEAR);
	EnableWindow(hWnd, TRUE);
	hWnd = GetDlgItem(hMainDlg, ID_DELETE);
	EnableWindow(hWnd, TRUE);
	mi.fState = MFS_ENABLED;
	SetMenuItemInfo(hMenu, ID_ADDFILE, FALSE, &mi);
	SetMenuItemInfo(hMenu, ID_ADDFOLDER, FALSE, &mi);
	SetMenuItemInfo(hMenu, ID_OPEN, FALSE, &mi);
	fPlaylistBlocked = false;

	return 0;
}

DWORD WINAPI ThreadFunc(PVOID param) {

	WCHAR buf[1024];
	DWORD dwCount;

	CoInitialize(NULL);
	hPipe = CreateNamedPipeW(L"\\\\.\\pipe\\AudioPlayer", PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		1, 0, 0, NMPWAIT_USE_DEFAULT_WAIT, NULL);
	if (hPipe == INVALID_HANDLE_VALUE) 
	{ 
		MessageBoxW(0, L"Pipe subsystem failed", 0, 0); 
		return 1; 
	}
	while (1) {
		ConnectNamedPipe(hPipe, NULL);
		ReadFile(hPipe, buf, sizeof(buf), &dwCount, NULL);
		if (dwCount / 2 < 1024) {
			buf[dwCount / 2] = L'\0';
		}
		ProcessCommandLine(buf);
		SetForegroundWindow(hMainWnd);
		DisconnectNamedPipe(hPipe);
	}
}

//callback invoked by player on event
void OnPlaybackEvent(PLAYER_EVENT evt) {
	switch (evt) {
	case EVT_USERABORT:Close(); break;
		//errors
	case EVT_FILE_CLOSED:
	case EVT_ERRORABORT:
		MessageBeep(MB_ICONERROR);
		//complete
	case EVT_COMPLETE:Close();
		SetThreadExecutionState(ES_CONTINUOUS);
		PlayNextTrack();
		break;
	}
}

void ProcessNotify(WPARAM NotifyValue) {
	Player_ProcessNotify(NotifyValue);
}

void StartPlayingPlaylist() {

	int n = 0;
	if (CurrMode == RANDOM)n = rand() % CountTracks;
	if ((UINT)n >= CountTracks)n = 0;
	PlayTrackByNumber(n);

}

void PlayClick() {
	RunTimer();
	if (PlayerState == FILE_NOT_LOADED) {

		StartPlayingPlaylist();
	}
	else {
		Play();
	}
}

void PauseClick()
{

	if (PlayerState == PLAYING)
		Pause();
	else if (PlayerState == PAUSED)
		Resume();

}

BOOL CALLBACK IntegrDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	wchar_t buf[256];


	switch (uMsg) {
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:switch (LOWORD(wParam)) {
	case IDOK:
	case IDCANCEL:
		EndDialog(hDlg, 0); 
		return TRUE;
	}
	break;
	case WM_CLOSE:
		EndDialog(hDlg, 0); return TRUE;
	}
	return FALSE;
}

// working with scroll bar
BOOL CALLBACK PlaybackDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	DWORD pos;

	switch (uMsg) {
	case WM_INITDIALOG:

		return TRUE;

	case WM_HSCROLL:

		if (lParam == (LPARAM)hScrollbar) {
			Progress.ProcessScrollEvent((short)LOWORD(wParam), (short)HIWORD(wParam));
			pos = Progress.GetTrackPos() * 1000;
			SetPosition(pos);
			UpdatePosition();
		}
		break;

	case WM_VSCROLL:
		if (lParam == (LPARAM)hVolume) {
			/*if(PlayerState!=FILE_NOT_LOADED)*/
			CurrVolume = SendMessage(hVolume, TBM_GETPOS, 0, 0);
			SetVolume(100 - CurrVolume);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_PLAY:PlayClick(); break;
		case ID_PAUSE:PauseClick(); break;
		case ID_STOP:
			Rewind();
			UpdatePosition();
			Stop();
			StopTimer();
			break;
		case ID_REWIND:Rewind(); break;
		case ID_NEXTTRACK:PlayTrackByNumber(CurrentTrack + 1); break;
		case ID_PREVTRACK:PlayTrackByNumber(CurrentTrack - 1); break;
		}break;

	}
	return FALSE;
}

// MAIN one
BOOL CALLBACK MainDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	OPENFILENAMEW ofn = { 0 };
	POINT pt = { 0,0 };
	wchar_t FileName[MAX_PATH] = L"";
	BOOL success = FALSE;
	LPNMITEMACTIVATE lpnmitem;
	LPNMLVKEYDOWN pnkd;
	BROWSEINFO bi = { 0 };
	PIDLIST_ABSOLUTE pidres = NULL;
	DWORD id;

	int i, n;

	switch (uMsg) {
	case WM_INITDIALOG:return TRUE;
	case WM_CHILDACTIVATE:
		RedrawWindow(hDlg, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE);
		ShowWindow(hDlg, SW_SHOW); break;
	case WM_SIZE:
		UpdatePlaylist();
		RedrawWindow(hDlg, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE);
		ShowWindow(hDlg, SW_SHOW);
		break;
	case WM_NOTIFY:
		lpnmitem = (LPNMITEMACTIVATE)lParam;
		switch (lpnmitem->hdr.code) {
		case NM_DBLCLK:
			i = GetPlaylistSelectedElement();
			Stop();

			PlayTrackByNumber(i);
			RunTimer(); break;
		case NM_RCLICK:
			success = GetCursorPos(&pt);

			if (success == FALSE) {
				pt.x = (LONG)lpnmitem->ptAction.x;
				pt.y = (LONG)lpnmitem->ptAction.y;
				ClientToScreen(PlayList, &pt);
			}

			ShowContextMenu(MNUM_PLAYLISTMENU, pt.x, pt.y);
			fBlockContextMenu = true;
			return TRUE;
		case LVN_KEYDOWN:
			pnkd = (LPNMLVKEYDOWN)lParam;
			//KEYBOARD EVENT
			switch (pnkd->wVKey) {
			case VK_SPACE:
				i = GetPlaylistSelectedElement();
				Stop();
				PlayTrackByNumber(i);
				RunTimer(); break;
			case VK_DELETE:
				n = ListView_GetSelectionMark(PlayList);

				for (i = CountTracks; i >= 0; i--) {
					if (IsPlaylistItemSelected(i))DeletePlaylistElement(i);
				}


				if (((UINT)n) >= CountTracks)n = CountTracks - 1;
				if (n == -1)break;
				SetPlaylistSelectedElement(n);
				break;

			}break;
			//END OF KEYBOARD EVENT
		case LVN_COLUMNCLICK://click column to sort
			LPNMLISTVIEW pclick = (LPNMLISTVIEW)lParam;
			TCHAR buf[MAX_PATH] = L"";
			GetPlaylistElement(CurrentTrack, buf);
			if (pls_LastSortColumn == pclick->iSubItem) { pls_SortReverse = !pls_SortReverse; }
			pls_LastSortColumn = pclick->iSubItem;
			ListView_SortItemsEx(PlayList, CompareFunc, pclick->iSubItem);

			CurrentTrack = FindTrack(buf);
			if (CurrentTrack == -1)CurrentTrack = 0;
			break;
		}

		break;

	case WM_COMMAND:switch (LOWORD(wParam)) {

	case ID_ADDFILE:
		if (fPlaylistBlocked)break;
		for (i = 0; i < sizeof(strInputFiles) / sizeof(strInputFiles[0]); i++) {
			strInputFiles[i] = 0;
		}
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.lpstrFile = strInputFiles;
		ofn.nMaxFile = sizeof(strInputFiles);
		ofn.lpstrFilter = strMediafileFilter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
		success = GetOpenFileNameW(&ofn);
		if (!success) { break; }
		ProcessOFNString(strInputFiles, ofn.nFileOffset);
		break;
	case ID_DELETE:if (fPlaylistBlocked)break;
		n = ListView_GetSelectionMark(PlayList);

		for (i = CountTracks; i >= 0; i--) {
			if (IsPlaylistItemSelected(i))DeletePlaylistElement(i);
		}

		if (((UINT)n) >= CountTracks)n = CountTracks - 1;
		if (n == -1)break;
		SetPlaylistSelectedElement(n);
		break;

	case ID_CLEAR:if (fPlaylistBlocked)break;
		Close();
		ClearPlaylist(); break;

	case ID_ADDFOLDER:
		if (fPlaylistBlocked)
			break;

		bi.hwndOwner = hMainWnd;
		bi.pidlRoot = NULL;

		bi.pszDisplayName = FileName;
		bi.lpfn = BrowseCallbackProc;
		pidres = SHBrowseForFolder(&bi);
		if (pidres == NULL)break;
		SHGetPathFromIDList(pidres, strInputFolder);
		CoTaskMemFree(pidres);
		StringCchCat(strInputFolder, MAX_PATH, L"\\");
		//ADD FOLDER IN ASYNC MODE
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)DirThreadFunc, (LPVOID)strInputFolder, 0, &id);
		break;
	}
				   break;


	}
	return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	OPENFILENAME ofn = { 0 };

	HDC hDC; PAINTSTRUCT ps; RECT rc;
	wchar_t FileName[MAX_PATH] = L"";
	int i, n;
	BOOL success = FALSE;
	BROWSEINFO bi = { 0 };
	PIDLIST_ABSOLUTE pidres = NULL;
	DWORD id;
	POINT pt = { 0 };
	int d;

	switch (uMsg) {

	case WM_MOUSEWHEEL:d = GET_WHEEL_DELTA_WPARAM(wParam);
		SetVolume(GetVolume() + (d / WHEEL_DELTA) * VOLUME_INCREMENT);
		SendMessage(hVolume, TBM_SETPOS, TRUE, 100 - GetVolume()); break;
	case WM_SIZE: UpdateView(); SaveWindowSize(); break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case ID_ADDFILE:
		case ID_ADDFOLDER:
		case ID_CLEAR:
			SendMessage(hMainDlg, uMsg, wParam, lParam); 
			break;
		case ID_VOLUMEUP:SetVolume(GetVolume() + VOLUME_INCREMENT);
			SendMessage(hVolume, TBM_SETPOS, TRUE, 100 - GetVolume()); break;
		case ID_VOLUMEDOWN:SetVolume(GetVolume() - VOLUME_INCREMENT);
			SendMessage(hVolume, TBM_SETPOS, TRUE, 100 - GetVolume()); break;
		case ID_PLAYPAUSE:if (PlayerState == PLAYING)Pause();
						 else PlayClick();
			break;

		case ID_PLAY:
		case ID_PAUSE:
		case ID_STOP:
		case ID_REWIND:
		case ID_NEXTTRACK:
		case ID_PREVTRACK:
			SendMessage(hPlaybackDlg, uMsg, wParam, lParam); break;

		case ID_LOADLIST:if (fPlaylistBlocked)break;
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.lpstrFile = FileName;
			ofn.nMaxFile = sizeof(FileName);
			ofn.lpstrFilter = strPlaylistFilter;
			ofn.nFilterIndex = 1;
			ofn.lpstrTitle = L"Загрузить список воспроизведения";
			ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_LONGNAMES;
			success = GetOpenFileNameW(&ofn);
			if (!success) { break; }
			ClearPlaylist();
			LoadTextPlaylist(FileName);
			break;
		case ID_SAVELIST:
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.lpstrFile = FileName;
			ofn.nMaxFile = sizeof(FileName);
			ofn.lpstrFilter = strPlaylistFilter;
			ofn.nFilterIndex = 1;
			ofn.lpstrTitle = L"Сохранить список воспроизведения";
			ofn.Flags = OFN_HIDEREADONLY | OFN_LONGNAMES;
			ofn.lpstrDefExt = L"lst";
			StringCchCopy(FileName, MAX_PATH, L"playlist.lst");
			success = GetSaveFileNameW(&ofn);
			if (!success) { break; }
			SaveTextPlaylist(FileName);
			break;
		case ID_SETTINGS:
			DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SETTINGS), hWnd, SettingsDlgProc);
			InvalidateRect(hCoverWnd, NULL, TRUE);
			break;
		case IDC_INTEGR:
			DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INTEGR), hWnd, IntegrDlgProc);
			break;
		case ID_OPEN:
			if (fPlaylistBlocked)
				break;
			for (i = 0; i < sizeof(strInputFiles) / sizeof(strInputFiles[0]); i++) {
				strInputFiles[i] = 0;
			}
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.lpstrFile = strInputFiles;
			ofn.nMaxFile = sizeof(strInputFiles);
			ofn.lpstrFilter = strMediafileFilter;
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
			success = GetOpenFileNameW(&ofn);
			if (!success) { break; }
			ClearPlaylist();
			ProcessOFNString(strInputFiles, ofn.nFileOffset);

			PlayTrackByNumber(0);
			break;
		case ID_SHOWPLAYLIST:Settings.fShowPlaylist = !Settings.fShowPlaylist;
			UpdateGUI();
			UpdateView(); break;
		case ID_SHOWCOVER:Settings.fShowCover = !Settings.fShowCover;
			UpdateGUI();
			UpdateView(); break;
		case ID_FILE_INFORMATION:
			DisplayFileInfo(GetPlaylistSelectedElement()); 
			break;
		case ID_FILEINFO:
			DisplayMultimediaInfo(); break;
		case ID_PROP_AUDIO:
			ShowPropertyPage(TN_AUDIO_DECODER);
			break;
		case ID_PROP_AUDIOOUT:
			ShowPropertyPage(TN_AUDIO_OUT);
			break;
		case ID_PROP_SPLITTER:
			ShowPropertyPage(TN_SPLITTER);
			break;
		case ID_MODE_NORMAL:
			CheckMenuRadioItem(GetMenu(hMainWnd), ID_MODE_NORMAL, ID_MODE_RANDOM, ID_MODE_NORMAL, MF_BYCOMMAND);
			CurrMode = NORMAL; 
			break;
		case ID_MODE_REPEATLIST:
			CheckMenuRadioItem(GetMenu(hMainWnd), ID_MODE_NORMAL, ID_MODE_RANDOM, ID_MODE_REPEATLIST, MF_BYCOMMAND);
			CurrMode = REPEAT_LIST; 
			break;
		case ID_MODE_REPEATFILE:
			CheckMenuRadioItem(GetMenu(hMainWnd), ID_MODE_NORMAL, ID_MODE_RANDOM, ID_MODE_REPEATFILE, MF_BYCOMMAND);
			CurrMode = REPEAT_FILE; 
			break;
		case ID_MODE_RANDOM:
			CheckMenuRadioItem(GetMenu(hMainWnd), ID_MODE_NORMAL, ID_MODE_RANDOM, ID_MODE_RANDOM, MF_BYCOMMAND);
			CurrMode = RANDOM; 
			break;
		case ID_PLAY_SELECTED:
			PlayTrackByNumber(GetPlaylistSelectedElement()); 
			break;
		case ID_SELECTALL:
			Playlist_SelectAll(); 
			break;
		case ID_DELETE:
			if (fPlaylistBlocked)
				break;
			n = ListView_GetSelectionMark(PlayList);
			for (i = CountTracks; i >= 0; i--) {
				if (IsPlaylistItemSelected(i))
					DeletePlaylistElement(i);
			}
			if (((UINT)n) >= CountTracks)
				n = CountTracks - 1;
			if (n == -1)
				break;
			SetPlaylistSelectedElement(n);
			break;
		case ID_MINIMIZE:
			SwitchTrayIcon(); 
			break;
		case ID_EXIT:
			SendMessage(hMainWnd, WM_CLOSE, 0, 0); 
			break;
		}
		//after WM_COMMAND
		if (HIWORD(wParam) == 0) {
			InvalidateRect(hPlaybackDlg, NULL, TRUE);
			UpdateWindow(hPlaybackDlg);
		}
		break;
	case WM_SYSCOMMAND:
		/*if (wParam == SC_SCREENSAVE) {
			if (IsPlayingVideo == true)return 0;
		}*/
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	case MM_MCINOTIFY:
		ProcessNotify(wParam);
		break;
	case WM_CONTEXTMENU:
		if (fBlockContextMenu == true) { fBlockContextMenu = false; break; }
		ShowContextMenu(MNUM_MAINMENU, (int)GET_X_LPARAM(lParam), (int)GET_Y_LPARAM(lParam));
		break;
	case UM_TRAYICON:
		if (lParam == WM_RBUTTONDOWN || lParam == WM_CONTEXTMENU) {
			if (GetCursorPos(&pt) == FALSE) {
				GetClientRect(GetDesktopWindow(), &rc);
				pt.x = rc.right; pt.y = rc.bottom;
			}
			SetForegroundWindow(hMainWnd);
			ShowContextMenu(MNUM_MAINMENU, pt.x, pt.y);
			PostMessage(hMainWnd, WM_NULL, 0, 0);
		}
		if (lParam == WM_LBUTTONDBLCLK) { SwitchTrayIcon(); }
		break;
	case WM_TIMER:
		UpdatePosition();
		break;
	case WM_PAINT:hDC = BeginPaint(hWnd, &ps);

		GetClientRect(hWnd, &rc);
		DrawText(hDC, txtMediaInfo, wcslen(txtMediaInfo), &rcInfoBox, 0);

		EndPaint(hWnd, &ps); break;

	case WM_CLOSE:
		SaveWindowSize();
		if (IsTrayIcon == true)SwitchTrayIcon();

		if (Settings.fLoadDefaultPls == true) {
			TCHAR buf[MAX_PATH];

			GetDataDir(buf, MAX_PATH);
			StringCchCat(buf, MAX_PATH, L"default.lst");
			SaveTextPlaylist(buf);
		}

		if (Settings.fRememberPosition == true)SaveLastPosition();
		WriteSettings();

		Close();
		TerminateThread(hThread, 0);
		CloseHandle(hThread);
		CloseHandle(hPipe);
		ReleaseMutex(hMutex);
		DestroyWindow(hWnd); break;
	case WM_DESTROY:PostQuitMessage(0); break;
	default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;

}

// Check for running instance and, if one exists, pass command line and exit
void EnsureSingleInstance() {
	WCHAR* cmd;
	HANDLE hDestPipe;
	DWORD dwCount;
	//CHECK IF PROGRAM ALREADY RUN
	hMutex = CreateMutexW(NULL, TRUE, L"AudioPlayer");

	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		//program already run
		hDestPipe = CreateFileW(L"\\\\.\\pipe\\AudioPlayer",
			GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
		cmd = GetCommandLineW();
		WriteFile(hDestPipe, cmd, 2 * wcslen(cmd) + 1,
			&dwCount, NULL);
		CloseHandle(hDestPipe);
		CloseHandle(hMutex);
		ExitProcess(0);
	}
}


// Initializes libraries and creates UI
void InitApplication() {
	wchar_t wclass_name[] = L"MyClass";
	WNDCLASSEX wc;
	RECT rc = { 0 };
	int w = 0;
	int h = 0;

	int i = 0, j = 0;
	BOOL skip = FALSE;

	TCHAR buf[256] = L"";

	INITCOMMONCONTROLSEX ic;
	LVCOLUMN col = { 0 };

	HMENU hm = NULL;
	DWORD exStyle = WS_EX_COMPOSITED;

	CoInitialize(NULL);
	srand(GetTickCount());
	ic.dwSize = sizeof(ic);
	ic.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&ic);
	GdiplusStartup(&gdip_code, new GdiplusStartupInput(), NULL);
	InitErrorHandler();

	//player event callback
	Player_SetEventCallback(OnPlaybackEvent);

	hAccel = LoadAccelerators(hResModule, MAKEINTRESOURCE(ID_ACCEL));
	hm = LoadMenu(NULL, MAKEINTRESOURCE(IDR_CONTEXT_MENU));
	hcMainMenu = GetSubMenu(hm, 0);
	hcPlaylistMenu = GetSubMenu(hm, 1);

	HMODULE hLib = LoadLibrary(L"shell32.dll");
	if (hLib == NULL)
		MessageBox(0, L"Unable to load SHELL32.DLL", 0, MB_OK | MB_ICONERROR);
	hSMPIcon = LoadIcon(hLib, MAKEINTRESOURCE(41));
	if (hSMPIcon == NULL)
		hSMPIcon = LoadIcon(NULL, IDI_APPLICATION);

	wc.hIcon = hSMPIcon;
	//create wclass structure
	wc.cbSize = sizeof(wc);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);

	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
	wc.lpszClassName = wclass_name;
	wc.hIconSm = wc.hIcon;
	//register class
	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, L"Fail to create class", L"Error", MB_ICONEXCLAMATION | MB_OK);
		ExitProcess(-1);
	}

	//determine window size
	int ww = 615, wh = 650;
	if (Settings.WndMaximized != false) { ww = CW_USEDEFAULT; wh = 0; }

	//create window
	hMainWnd = CreateWindowEx(
		0, wclass_name, PROGRAM_TITLE, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX | WS_CLIPCHILDREN, Settings.WndX, Settings.WndY,
		ww, wh, NULL, NULL, GetModuleHandle(NULL), NULL
	);

	hWnd = hMainWnd;
	if (hMainWnd == NULL)
	{
		MessageBox(NULL, L"Fail to create window", L"Error", MB_ICONEXCLAMATION | MB_OK);
		ExitProcess(-1);
	}

	//playlist window
	hMainDlg = CreateDialog(hResModule, MAKEINTRESOURCE(IDD_MAIN), hMainWnd, MainDlgProc);

	//playback status window
	hPlaybackDlg = CreateDialog(hResModule, MAKEINTRESOURCE(IDD_PLAYBACK), hMainWnd, PlaybackDlgProc);

	//enable double-buffering to prevent flickering on text updates
	LONG_PTR es = GetWindowLongPtr(hPlaybackDlg, GWL_EXSTYLE);
	es = es | exStyle;
	SetWindowLongPtr(hPlaybackDlg, GWL_EXSTYLE, es);

	hScrollbar = GetDlgItem(hPlaybackDlg, IDC_SCROLLBAR);
	Progress = ScrollbarControl(SB_CTL, hScrollbar, 0, (int)(20 + S_TRACK_SIZE), S_TRACK_SIZE);

	PlayList = CreateWindowEx(0, WC_LISTVIEW, L"СПИСОК", WS_CHILD | WS_VISIBLE | LVS_SHOWSELALWAYS | LVS_REPORT, 0, 40,
		580, 160, hMainDlg, NULL, GetModuleHandle(NULL), NULL);
	ListView_SetExtendedListViewStyle(PlayList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	if (PlayList == NULL)Beep(300, 100);
	ShowWindow(PlayList, SW_SHOW);

	col.iSubItem = 0;
	col.pszText = L"название";
	col.cx = 150;
	col.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;
	ListView_InsertColumn(PlayList, 0, &col);
	col.iSubItem = 1;
	col.pszText = L"исполнитель";
	col.cx = 100;
	col.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;
	ListView_InsertColumn(PlayList, 1, &col);
	col.iSubItem = 2;
	col.pszText = L"альбом";
	col.cx = 100;
	col.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;
	ListView_InsertColumn(PlayList, 2, &col);
	col.iSubItem = 3;
	col.pszText = L"год";
	col.cx = 45;
	col.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;
	ListView_InsertColumn(PlayList, 3, &col);

	col.iSubItem = 4;
	col.pszText = L"файл";
	col.cx = 150;
	col.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;
	ListView_InsertColumn(PlayList, 4, &col);
	InitImageList();
	hVolume = GetDlgItem(hPlaybackDlg, IDC_VOLUME);
	SendMessage(hVolume, TBM_SETRANGE, TRUE, (LPARAM)MAKELONG(0, 100));
	CheckMenuRadioItem(GetMenu(hMainWnd), ID_MODE_NORMAL, ID_MODE_RANDOM, ID_MODE_NORMAL, MF_BYCOMMAND);

	CountTracks = 0;
	PlayerState = FILE_NOT_LOADED;
	Extensions[0] = TEXT("mp3"); Extensions[1] = TEXT("mid"); Extensions[2] = TEXT("wav");
	Extensions[3] = TEXT("midi"); Extensions[4] = TEXT("avi"); Extensions[5] = TEXT("mpeg");
	Extensions[6] = TEXT("wma"); Extensions[7] = TEXT("wmv"); Extensions[8] = TEXT("mpg");
	Extensions[9] = TEXT("ogg"); Extensions[10] = TEXT("cda"); Extensions[11] = TEXT("aac");
	Extensions[12] = TEXT("mkv"); Extensions[13] = TEXT("flac"); Extensions[14] = TEXT("mov");
	Extensions[15] = TEXT("wv"); Extensions[16] = TEXT("ape"); Extensions[17] = TEXT("m4a");


}

//Initializes player state based on command line and settings
void InitPlayerState() {

	DWORD id = 0;
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadFunc, NULL,
		THREAD_PRIORITY_NORMAL, &id);

	WCHAR* cmd = GetCommandLineW();
	WCHAR buf[MAX_PATH] = L"";

	if (Settings.fLoadDefaultPls == true) {
		StringCchCopy(buf, MAX_PATH, L"");
		GetDataDir(buf, MAX_PATH);

		StringCchCat(buf, MAX_PATH, L"default.lst");
		LoadTextPlaylist(buf);
	}

	if (Settings.fRememberPosition == true)RestoreLastPosition();

	ProcessCommandLine(cmd);
}

void ShowUI() {
	if (Settings.WndMaximized != false)ShowWindow(hMainWnd, SW_MAXIMIZE);
	else ShowWindow(hMainWnd, SW_SHOWNORMAL);

	UpdateView();
	InvalidateRect(hMainWnd, NULL, TRUE);
	UpdateGUI();
}

void RunMessageLoop() {
	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (IsIconic(hMainWnd) == FALSE) {
			if (TranslateAccelerator(hMainWnd, hAccel, &msg))
				continue;
		}
		else {
			if (TranslateAccelerator(hVideoWindow, hAccel, &msg))
				continue;
		}
		if (IsDialogMessage(hMainDlg, &msg))
			continue;
		if (IsDialogMessage(hPlaybackDlg, &msg))
			continue;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void UnloadApplication() {
	CoUninitialize();
	GdiplusShutdown(gdip_code);
	ExitProcess(0);
}

