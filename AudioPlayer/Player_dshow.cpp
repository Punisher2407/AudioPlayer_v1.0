#include "player_dshow.h"
#include "errors.h"
#include "resource.h"

//imports
void Close();
void GetMultimediaInfoString(WCHAR* text, size_t size);
void OnPlayerEvent(PLAYER_EVENT evt);
extern HWND hWnd;
extern PLAYER_STATE PlayerState;
extern long Volume;
extern PLAYERSETTINGS Settings; //settings

//module state
static IGraphBuilder* pGraph = NULL;
static IMediaControl* pControl = NULL;
static IMediaSeeking* pSeek = NULL;
IMediaEventEx* pEvent = NULL;

static IBasicAudio* pAudio = NULL;

IBaseFilter* pSource = NULL;
IBaseFilter* pSplitter = NULL;
IBaseFilter* pAudioDecoder = NULL;
IBaseFilter* pAudioRenderer = NULL;
static bool fUseSplitter = false;
GUID guidStreamSubType = { 0,0,0,{0,0,0,0,0,0,0,0} };

//implementation

BOOL InsertSplitter(TCHAR* file, const SPLITTER_DATA* sd) {
	IPin* pin1 = NULL;
	IPin* pin2 = NULL;
	HRESULT hr;
	IFileSourceFilter* fs = NULL;

	if (sd->IsSource == true) 
	{ 
		pSource = FindFilter(sd->FilterName); 
	}
	else {
		pSource = FindFilter(L"File Source (Async.)");
	}
	if (pSource == NULL) 
	{ 
		goto end_fail; 
	}

	hr = pGraph->AddFilter(pSource, L"source");
	if (FAILED(hr)) 
	{ 
		goto end_fail; 
	}
	hr = pSource->QueryInterface(IID_IFileSourceFilter, (void**)&fs);
	if (FAILED(hr)) 
	{
		goto end_fail; 
	}
	hr = fs->Load(file, NULL);
	fs->Release();
	if (FAILED(hr)) 
	{ 
		goto end_fail; 
	}

	if (sd->IsSource == false) {
		fUseSplitter = true;
		hr = GetUnconnectedPin(pSource, PINDIR_OUTPUT, &pin1);
		if (FAILED(hr)) 
		{ 
			goto end_fail; 
		}
		pSplitter = FindFilter(sd->FilterName);
		if (pSplitter == NULL) 
		{ 
			goto end_fail; 
		}
		hr = pGraph->AddFilter(pSplitter, L"splitter");
		if (FAILED(hr)) 
		{
			goto end_fail; 
		}
		hr = GetUnconnectedPin(pSplitter, PINDIR_INPUT, &pin2);
		if (FAILED(hr)) 
		{
			goto end_fail;
		}
		hr = pGraph->ConnectDirect(pin1, pin2, NULL);
		if (FAILED(hr)) 
		{ 
			goto end_fail; 
		}

	}
	else {
		fUseSplitter = false;
	}

	if (pin1 != NULL)
		pin1->Release();
	if (pin2 != NULL)
		pin2->Release();

	return TRUE;

end_fail:

	if (pin1 != NULL)
		pin1->Release();
	if (pin2 != NULL)
		pin2->Release();
	if (pSplitter != NULL) 
	{ 
		pGraph->RemoveFilter(pSplitter); 
		pSplitter->Release(); 
		pSplitter = NULL;
	}
	if (pSource != NULL) 
	{ 
		pGraph->RemoveFilter(pSource); 
		pSource->Release(); 
		pSource = NULL;
	}
	return FALSE;

}

BOOL InsertAudioDecoder(IPin* pin, TCHAR* lpFilterName) {
	HRESULT hr;
	IBaseFilter* pf = NULL;
	IPin* pin2 = NULL;
	IPin* pins[10];
	IEnumPins* pEnum = NULL;
	ULONG c, i;
	PIN_DIRECTION PinDir;

	pf = FindFilter(lpFilterName);
	if (pf == NULL)
		goto end_fail;
	hr = pGraph->AddFilter(pf, L"audiodecoder");
	if (FAILED(hr))
		goto end_fail;
	hr = GetUnconnectedPin(pf, PINDIR_INPUT, &pin2);
	if (FAILED(hr))
		goto end_fail;
	hr = pGraph->ConnectDirect(pin, pin2, NULL);
	if (FAILED(hr))
		goto end_fail;
	pin2->Release(); 
	pin2 = NULL;
	hr = pf->EnumPins(&pEnum);
	if (FAILED(hr))
		goto end_fail;
	hr = pEnum->Next(10, pins, &c);
	if (FAILED(hr))
		c = 0;
	for (i = 0; i < c; i++) {
		pins[i]->QueryDirection(&PinDir);
		if (PinDir == PINDIR_OUTPUT)
			pGraph->Render(pins[i]);
		pins[i]->Release();
	}

	if (pEnum != NULL) 
	{ 
		pEnum->Release(); 
	}

	pAudioDecoder = pf;

	return TRUE;
end_fail:
	if (pf != NULL) 
	{ 
		pGraph->RemoveFilter(pf), pf->Release(); 
	}
	if (pin2 != NULL) 
	{ 
		pin2->Release(); 
	}
	if (pEnum != NULL) 
	{ 
		pEnum->Release(); 
	}
	return FALSE;

}

bool pin_GetAudioInfo(IPin* pin, SMP_AUDIOINFO* pAudioInfo) {
	IEnumMediaTypes* pEnum = NULL;
	AM_MEDIA_TYPE* amt = NULL;
	WAVEFORMATEX* wf = NULL;
	HRESULT hr;
	ULONG res = 0;

	hr = pin->EnumMediaTypes(&pEnum);
	if (FAILED(hr))
		return false;
	hr = pEnum->Next(1, &amt, &res);
	if (FAILED(hr) || res == 0) 
	{ 
		pEnum->Release();
		return false; 
	}
	if (amt->formattype != FORMAT_WaveFormatEx) 
	{ 
		pEnum->Release(); 
		return false; 
	}
	wf = (WAVEFORMATEX*)amt->pbFormat;
	pAudioInfo->chans = wf->nChannels;
	pAudioInfo->BitsPerSample = wf->wBitsPerSample;
	pAudioInfo->BitsPerSecond = wf->nAvgBytesPerSec * 8;
	pAudioInfo->nFreq = wf->nSamplesPerSec;
	pAudioInfo->wFormatTag = wf->wFormatTag;

	if (amt->subtype == MEDIASUBTYPE_DOLBY_AC3) {
		pAudioInfo->wFormatTag = AUDIO_AC3;
	}

	if (amt->subtype == MEDIASUBTYPE_MPEG2_AUDIO) {
		pAudioInfo->wFormatTag = AUDIO_MPEG2AAC;
	}
	pEnum->Release();
	MyDeleteMediaType(amt);
	return true;
}

WORD DS_GetMultimediaInfo(SMP_AUDIOINFO* pAudioInfo, PLAYER_STREAM* pStreamType) {
	HRESULT hr;
	IEnumPins* pEnum = NULL;
	IBaseFilter* pf = NULL;
	IPin* pins[10];
	UINT i;
	ULONG c = 0;
	PIN_DIRECTION PinDir;
	bool fAudio = false;

	if (pSplitter != NULL)pf = pSplitter;
	else pf = pSource;
	if (pf == NULL)return INFORES_NO;

	hr = pf->EnumPins(&pEnum);
	if (FAILED(hr))return INFORES_NO;
	if (SUCCEEDED(hr))hr = pEnum->Next(10, pins, &c);
	if (FAILED(hr))c = 0;
	for (i = 0; i < c; i++) {
		pins[i]->QueryDirection(&PinDir);
		if (PinDir == PINDIR_INPUT)continue;
		if (CheckMediaType(pins[i], MEDIATYPE_Audio) != FALSE) {
			if (fAudio == true)continue;
			fAudio = pin_GetAudioInfo(pins[i], pAudioInfo);
			continue;
		}
	}

	for (i = 0; i < c; i++) {
		pins[i]->Release();
	}
	pEnum->Release();
	*pStreamType = STREAM_UNKNOWN;
	if (guidStreamSubType == MEDIASUBTYPE_Avi)
		*pStreamType = STREAM_AVI;
	if (guidStreamSubType == MEDIASUBTYPE_Asf)
		*pStreamType = STREAM_ASF;
	if (guidStreamSubType == MEDIASUBTYPE_MPEG1System || guidStreamSubType == MEDIASUBTYPE_MPEG1Audio
		|| guidStreamSubType == MEDIASUBTYPE_MPEG1Video)
		*pStreamType = STREAM_MPEG1;
	if (guidStreamSubType == MEDIASUBTYPE_MPEG1VideoCD)
		*pStreamType = STREAM_MPEG1VCD;
	if (guidStreamSubType == MEDIASUBTYPE_WAVE)
		*pStreamType = STREAM_WAVE;
	if (guidStreamSubType == MEDIASUBTYPE_MPEG2_PROGRAM ||
		guidStreamSubType == MEDIASUBTYPE_MPEG2_TRANSPORT ||
		guidStreamSubType == MEDIASUBTYPE_MPEG2_VIDEO
		)
		*pStreamType = STREAM_MPEG2;
	if (guidStreamSubType == MEDIASUBTYPE_QTMovie)
		*pStreamType = STREAM_QUICKTIME;
	if (guidStreamSubType == GUID_NULL)
		*pStreamType = STREAM_UNKNOWN;

	if (fAudio) {
		return INFORES_NO;
	}
	return INFORES_BOTH;

}

void SearchFilters() {
	HRESULT hr;
	ULONG c, i;
	IPin* pins[10];
	IPin* pin = NULL;
	PIN_DIRECTION PinDir;
	IEnumPins* pEnum = NULL;


	guidStreamSubType = GUID_NULL;
	if (pSource == NULL) {
		pSource = FindFileSource(pGraph);
		if (pSource == NULL) 
		{ 
			return; 
		}
	}

	//resaecrh sourcefilter pins
	hr = pSource->EnumPins(&pEnum);
	if (SUCCEEDED(hr))
		hr = pEnum->Next(10, pins, &c);
	if (FAILED(hr))
		c = 0;
	for (i = 0; i < c; i++) {
		pins[i]->QueryDirection(&PinDir);
		if (PinDir == PINDIR_INPUT)
			continue;
		if (CheckMediaType(pins[i], MEDIATYPE_Audio) != FALSE) {
			if (pAudioDecoder == NULL)
				pAudioDecoder = GetDownstreamFilter(pins[i]);
			continue;
		}
		if (CheckMediaType(pins[i], MEDIATYPE_Stream) != FALSE) {
			if (pSplitter == NULL) 
			{ 
				pSplitter = GetDownstreamFilter(pins[i]); 
			}
			continue;
		}

	}

	for (i = 0; i < c; i++) {
		pins[i]->Release();
	}
	pEnum->Release();
	if (pSplitter == NULL)
		goto check_render;
	//ressearch splitter pins

	hr = pSplitter->EnumPins(&pEnum);
	if (SUCCEEDED(hr))
		hr = pEnum->Next(10, pins, &c);
	if (FAILED(hr))
		c = 0;
	for (i = 0; i < c; i++) {
		pins[i]->QueryDirection(&PinDir);
		if (PinDir == PINDIR_INPUT)
			continue;
		if (CheckMediaType(pins[i], MEDIATYPE_Audio) != FALSE) {
			if (pAudioDecoder == NULL)
				pAudioDecoder = GetDownstreamFilter(pins[i]);
			continue;
		}
	}
	for (i = 0; i < c; i++) {
		pins[i]->Release();
	}
	pEnum->Release();
	//get stream subtype
	if (pSplitter != NULL && pSource != NULL) {
		if (pin != NULL)
			pin->Release();
		pin = NULL;
		pin = GetOutputPin(pSource);
		if (pin != NULL) 
		{ 
			GetSubType(pin, &guidStreamSubType); 
		}
		pin->Release();
	}

check_render:
	IAMAudioRendererStats* pRnd = NULL;
	IBasicVideo* pVid = NULL;

	if (pAudioDecoder != NULL) {
		hr = pAudioDecoder->QueryInterface(IID_IAMAudioRendererStats, (void**)&pRnd);
		if (hr == S_OK) {
			pAudioRenderer = pAudioDecoder; pAudioDecoder = NULL;
			pRnd->Release();
		}
	}


	if (pAudioDecoder != NULL) {
		pin = GetOutputPin(pAudioDecoder);
		if (pin != NULL) {
			pAudioRenderer = GetDownstreamFilter(pin);
			pin->Release(); pin = NULL;
		}
	}
}

BOOL BuildGraph(MEDIATYPE mt, TCHAR* file) {
	IPin* pin = NULL;
	IPin* pins[10];
	IEnumPins* pEnum = NULL;
	ULONG c, i;
	HRESULT hr;
	BOOL res;
	IBaseFilter* pf;
	AM_MEDIA_TYPE* amt = NULL;
	PIN_DIRECTION PinDir;

	switch (mt) {
	case MT_AUDIO:
		res = InsertSplitter(file, &sdBassSource);
		if (res == FALSE)
			res = InsertSplitter(file, &sdAsfReader);
		if (res == FALSE)
			res = InsertSplitter(file, &sdGretechMp3);
		if (res == FALSE)
			res = InsertSplitter(file, &sdNeroSplitter);
		if (res == FALSE)
			res = InsertSplitter(file, &sdMpeg1Splitter);
		if (res == FALSE)
			goto end_fail;
		break;
	default:
		goto end_fail;

	}

	if (fUseSplitter == true)
		pf = pSplitter;
	else pf = pSource;
	//connect splitter to decoders
	bool fVideoRendered = false;
	bool fAudioRendered = false;

	hr = pf->EnumPins(&pEnum);
	if (SUCCEEDED(hr))
		hr = pEnum->Next(10, pins, &c);
	if (FAILED(hr))
		c = 0;
	for (i = 0; i < c; i++) {
		pins[i]->QueryDirection(&PinDir);
		if (PinDir == PINDIR_INPUT)
			continue;
		if (CheckMediaType(pins[i], MEDIATYPE_Audio) != FALSE) {
			if (!fAudioRendered)
			{
				res = InsertAudioDecoder(pins[i], L"ffdshow Audio Decoder");

				if (res == FALSE)
				{
					pGraph->Render(pins[i]);
				}
				else
				{
					fAudioRendered = true;
				}
				continue;
			}
		}
		pGraph->Render(pins[i]);
	}

	for (i = 0; i < c; i++) {
		pins[i]->Release();
	}


	if (pEnum != NULL) 
	{
		pEnum->Release(); 
		pEnum = NULL; 
	}
	if (pin != NULL) 
	{ 
		pin->Release(); 
		pin = NULL; 
	}

	return TRUE;

end_fail:

	if (pSource != NULL) 
	{ 
		pGraph->RemoveFilter(pSource); 
		pSource->Release(); 
		pSource = NULL; 
	}
	if (pSplitter != NULL) 
	{ 
		pGraph->RemoveFilter(pSplitter); 
		pSplitter->Release(); 
		pSplitter = NULL; 
	}
	if (pEnum != NULL) 
	{ 
		pEnum->Release(); 
	}

	return FALSE;
}

void DS_Player_Close() {
	if (pSource != NULL) 
	{ 
		pSource->Release(); 
		pSource = NULL; 
	}
	if (pSplitter != NULL) 
	{ 
		pSplitter->Release(); 
		pSplitter = NULL; 
	}
	if (pAudioDecoder != NULL) 
	{ 
		pAudioDecoder->Release(); 
		pAudioDecoder = NULL; 
	}
	if (pAudioRenderer != NULL) 
	{ 
		pAudioRenderer->Release(); 
		pAudioRenderer = NULL; 
	}
	if (pControl != NULL) 
	{ 
		pControl->Release(); 
		pControl = NULL; 
	}
	if (pSeek != NULL) 
	{ 
		pSeek->Release(); 
		pSeek = NULL; 
	}

	if (pEvent != NULL) {
		pEvent->SetNotifyWindow(NULL, 0, 0);
		pEvent->Release();
		pEvent = NULL;
	}

	if (pAudio != NULL) 
	{ 
		pAudio->Release(); 
		pAudio = NULL; 
	}
	if (pGraph != NULL) 
	{ 
		pGraph->Release(); 
		pGraph = NULL; 
	}
}

HRESULT DS_Player_OpenFile(WCHAR* filename) {
	HRESULT hr;
	GUID tf = TIME_FORMAT_MEDIA_TIME;
	TCHAR ext[8] = L"";
	BOOL success = FALSE;
	bool autobuild = false;

	GetFileExtension(filename, ext);

	// Create the filter graph manager and query for interfaces.
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
		IID_IGraphBuilder, (void**)&pGraph);

	if (FAILED(hr)) 
	{ 
		ShowError(hr, SYSTEM_ERROR); Close(); 
		return hr; 
	}
	hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);
	if (FAILED(hr)) 
	{ 
		ShowError(hr, SYSTEM_ERROR); 
		Close(); 
		return hr; 
	}
	hr = pGraph->QueryInterface(IID_IMediaEventEx, (void**)&pEvent);
	if (FAILED(hr)) 
	{ 
		ShowError(hr, SYSTEM_ERROR); 
		Close(); 
		return hr; 
	}
	hr = pGraph->QueryInterface(IID_IMediaSeeking, (void**)&pSeek);
	if (FAILED(hr)) 
	{
		ShowError(hr, SYSTEM_ERROR); 
		Close(); 
		return hr; 
	}
	hr = pGraph->QueryInterface(IID_IBasicAudio, (void**)&pAudio);
	if (FAILED(hr)) 
	{ 
		ShowError(hr, SYSTEM_ERROR); 
		Close(); 
		return hr; 
	}
	
	bool play = false;

	//Try dynamic build filter graph
	if (lstrcmp(ext, L"mp3") == 0 || lstrcmp(ext, L"MP3") == 0 || lstrcmp(ext, L"Mp3") == 0) {
		success = BuildGraph(MT_AUDIO, filename);
		if (success != FALSE)
			play = true;
	}

	if (!play)
	{

		//try auto build
		hr = pGraph->RenderFile(filename, NULL);

		if (FAILED(hr)) {
			Close();
			return hr;
		}

		autobuild = true;
	}
	SearchFilters();

#ifdef DEBUG
	LogMessage(L"DS_Player_OpenFile", TRUE);
	LogMessage(filename, FALSE);

	if (autobuild) LogMessage(L"Codecs selected automatically", FALSE);

	LogAllFilters(pGraph);

	WCHAR strMediaInfo[5000] = L"";
	GetMultimediaInfoString(strMediaInfo, 5000);
	LogMessage(strMediaInfo, FALSE);
	LogMessage(L"***************", FALSE);
#endif

	pSeek->SetTimeFormat(&tf);
	pAudio->put_Volume(Volume);
	return S_OK;
}

BOOL DS_Player_Play() {
	HRESULT hr;

	hr = pControl->Run();

	if (FAILED(hr)) {
		HandlePlayError(hr, L"");
		Close();
		WPARAM wParam = MAKEWPARAM(ID_NEXTTRACK, 0);
		PostMessage(hWnd, WM_COMMAND, wParam, 0);
		return FALSE;
	}

	pEvent->SetNotifyWindow((LONG_PTR)hWnd, MM_MCINOTIFY, 0L);
	return TRUE;
}

BOOL DS_Player_Pause() {
	HRESULT hRes;

	hRes = pControl->Pause();
	if (FAILED(hRes)) {
		return FALSE;
	}

	return TRUE;
}

BOOL DS_Player_Resume() {
	HRESULT hRes;

	hRes = pControl->Run();

	if (FAILED(hRes)) { return FALSE; }

	return TRUE;
}

BOOL DS_Player_Stop() {
	HRESULT hRes;

	hRes = pControl->Stop();
	if (FAILED(hRes)) {
		Close();
		PlayerState = FILE_NOT_LOADED;
		return FALSE;
	}

	return TRUE;
}

DWORD DS_Player_GetLength() {

	HRESULT hRes;
	LONGLONG dur = { 0 };

	hRes = pSeek->GetDuration(&dur);
	if (FAILED(hRes)) {
#ifdef DEBUG
		LogMessage(L"IMediaSeeking::GetDuration failed", FALSE);
		HandleMediaError(hRes);
#endif
		return 0;
	}
	return dur / TIME_KOEFF;
}

DWORD DS_Player_GetPosition() {
	HRESULT hRes;
	LONGLONG pos;
	LONGLONG stoppos;

	hRes = pSeek->GetPositions(&pos, &stoppos);

	if (FAILED(hRes)) {
		return 0;
	}

	return pos / TIME_KOEFF;
}

BOOL DS_Player_Rewind() {
	HRESULT hRes;
	LONGLONG pos = 0;

	hRes = pSeek->SetPositions(&pos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);

	if (FAILED(hRes)) {
		return FALSE;
	}

	return TRUE;
}

BOOL DS_Player_SetPosition(LONGLONG pos) {
	HRESULT hRes;

	pos *= TIME_KOEFF;
	hRes = pSeek->SetPositions(&pos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);

	if (FAILED(hRes)) {
		ShowError(hRes, PLAY_ERROR);
		return FALSE;
	}

	return TRUE;
}

void DS_Player_SetVolume(long value)
{
	if (pAudio == NULL)return;
	pAudio->put_Volume(value);
}

void DS_ProcessNotify(WPARAM NotifyValue) {

	LONG EventCode;
	LONG_PTR param1;
	LONG_PTR param2;
	HRESULT hr;

	while (1) {
		if (pEvent == NULL)
			break;
		hr = pEvent->GetEvent(&EventCode, &param1, &param2, 100);
		if (hr == E_ABORT)
			break;

		switch (EventCode) {
			case EC_USERABORT:
				OnPlayerEvent(EVT_USERABORT); 
				break;
			//errors
			case EC_FILE_CLOSED:
				OnPlayerEvent(EVT_FILE_CLOSED); 
				break;
			case EC_ERRORABORT:OnPlayerEvent(EVT_ERRORABORT); 
			break;
			//complete
			case EC_COMPLETE:
				OnPlayerEvent(EVT_COMPLETE); 
				break;
		}

		if (pEvent != NULL) {
			pEvent->FreeEventParams(EventCode, param1, param2);
		}
	}//end while
}

void DS_ShowPropertyPage(TOPOLOGY_NODE node) {

	IBaseFilter* pFilter = NULL;

	switch (node) {
	case TN_SPLITTER:
		if (pSplitter == NULL)
			pFilter = pSource;
		else 
			pFilter = pSplitter;
		break;
	case TN_AUDIO_DECODER:
		pFilter = pAudioDecoder;
		break;
	case TN_AUDIO_OUT:
		pFilter = pAudioRenderer;
		break;
	}

	if (pFilter == NULL) {
		//can't show property page
		ShowError(0, ERROR_NOTSUPPORTED);
		return;
	}

	//show property page
	if (ShowFilterProperties(pFilter) == FALSE) {
		//no property page
		ShowError(0, ERROR_NOPROPERTIES);
	}
}
