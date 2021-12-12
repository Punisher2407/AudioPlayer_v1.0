#include "DirManager.h"

void GetFileDirectory(wchar_t* path, wchar_t* out) {
	int i = 0;
	int k = 0;
	int LastIndex = 0;
	TCHAR buf[MAX_PATH];
	StringCchCopy(buf, MAX_PATH, path);
	while (1) {
		if (buf[i] == L'\\')
			LastIndex = i;
		if (buf[i] == 0)
			break;
		i++;
	}
	lstrcpy(out, L"");
	buf[LastIndex + 1] = 0;
	lstrcat(out, buf);

}

void GetFileDirectoryA(char* path, char* out) {
	int i = 0;
	int k = 0;
	int LastIndex = 0;
	char buf[MAX_PATH];
	lstrcpyA(buf, path);
	while (1) {
		if (buf[i] == '\\')
			LastIndex = i;
		if (buf[i] == 0)
			break;
		i++;
	}
	lstrcpyA(out, "");
	buf[LastIndex + 1] = 0;
	lstrcatA(out, buf);

}