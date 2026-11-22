
/*

To fix the bug in the original sfmpq in the early version 1.31

The original sfmpq will fail to open files once the maximum number of files has been modified, even if the files exist.

*/


#include <stormlib.h>

#include <string>
#include <vector>


BOOL WINAPI MpqCompactArchive(HANDLE hMpq) {
	return SFileCompactArchive(hMpq, 0, 0);
}

BOOL WINAPI MpqDeleteFile(HANDLE hMpq, const char* szFileName) {
	if (strcmp(szFileName, "(attributes)") == 0) {
		return TRUE;
	}
	return SFileRemoveFile(hMpq, szFileName, 0);
}

BOOL WINAPI MpqCloseUpdatedArchive(HANDLE hMpq, DWORD dwUnknown) {
	return SFileCloseArchive(hMpq);
}

BOOL WINAPI MpqAddFileToArchiveEx(HANDLE hMpq, LPCSTR szFileName, LPCSTR szArchivedName, DWORD dwFlags, DWORD dwCompressionType, DWORD dwCompressLevel) {
	if (dwFlags & 0x00000001) {
		dwFlags &= ~0x00000001;
		dwFlags |= MPQ_FILE_REPLACEEXISTING;
	}
	return SFileAddFileEx(hMpq, szFileName, szArchivedName, dwFlags, dwCompressionType, dwCompressionType);
}

HANDLE WINAPI MpqOpenArchiveForUpdate(LPCSTR szFileName, DWORD dwFlags, DWORD dwMaximumFilesInArchive) {
	HANDLE hMpq = 0;
	if (SFileOpenArchive(szFileName, 0, 0, &hMpq)) {
		if (dwMaximumFilesInArchive > SFileGetMaxFileCount(hMpq)) {
			SFileSetMaxFileCount(hMpq, dwMaximumFilesInArchive);
		}
		return hMpq;
	}
	return 0;
}
