#ifndef _ENTRY_CPP
#define _ENTRY_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "../Resources/Resource.H"

#include "Entry.H"
#include "Init.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BUFFERSIZE 1024*1024

HINSTANCE ghAppInstance = NULL;

char gsAppCopyRight[64];
char gsFileVersion[32];
char gsTitleCaption[64];

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct _tag_Configuration_Info{
	unsigned char cDelimiter;
	char sSourceFile[MAX_PATH];
	char sTargetFolder[MAX_PATH];
	bool bSilent;
} CONFIGINFO, *LPCONFIGINFO;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct _tag_TargetHandle {
	HANDLE hHandle;
	int iColumns;
	__int64 i64RowCount;
} TARGETHANDLE, *LPTARGETHANDLE;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct _tag_TargetHandles {
	TARGETHANDLE *Handles;
	int iHandles;
} TARGETHANDLES, *LPTARGETHANDLES;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Returns the number of directories created.
__inline int CreateFolderStructure(const char *sPath)
{
    char sDir[MAX_PATH];

    int iNameSz = strlen(sPath);
	int iCount = 0;
	int iPos = 0;

    for(iPos = 0; iPos < iNameSz; iPos++)
    {
        if(sPath[iPos] == '\\' || sPath[iPos] == '/')
        {
            sDir[iPos] = '\0';
            CreateDirectory(sDir, NULL);
			iCount++;
        }

        sDir[iPos] = sPath[iPos];
    }

	 return iCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
	FILE_BEGIN 0
		The starting point is zero or the beginning of the file.
		If this flag is specified, then the liDistanceToMove parameter
			is interpreted as an unsigned value.

	FILE_CURRENT 1
		The start point is the current value of the file pointer.

	FILE_END 2
		The starting point is the current end-of-file position.
*/
__inline __int64 FileSeek(HANDLE hFile, __int64 l64Distance, DWORD dwMoveMethod)
{
	LARGE_INTEGER liDistance;
	LARGE_INTEGER liNewPosition;

	memset(&liDistance, 0, sizeof(liDistance));
	memset(&liNewPosition, 0, sizeof(liNewPosition));

	liDistance.QuadPart = l64Distance;

	if(!SetFilePointerEx(hFile, liDistance, &liNewPosition, dwMoveMethod))
	{
		//Fix me: Need to handle this error here.
	}

	return liNewPosition.QuadPart;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
	Returns the current file marker position.
*/
__inline __int64 FileSeek(HANDLE hFile)
{
	return FileSeek(hFile, 0, FILE_CURRENT);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

__inline int CountDelimiters(CONFIGINFO *pCI, const char *sBuf, DWORD dwBufSz, DWORD *dwPos)
{
	int iDelimiter = 0;
	for(; ((DWORD)*dwPos) < dwBufSz; ((DWORD)*dwPos)++)
	{
		if(sBuf[((DWORD)*dwPos)] == '\n')
		{
			((DWORD)*dwPos)++;
			return iDelimiter + 1;
		}
		else if((unsigned char)sBuf[((DWORD)*dwPos)] == pCI->cDelimiter)
		{
			iDelimiter++;
		}
	}
	return iDelimiter;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

__inline int FindLastLineFeed(const char *sBuf, DWORD dwBufSz)
{
	for(DWORD dwPos = dwBufSz - 1; dwPos > 0; dwPos--)
	{
		if(sBuf[dwPos] == '\n')
		{
			return dwPos+1;
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

__inline HANDLE GetFileHandle(CONFIGINFO *pCI, TARGETHANDLES *pTH, int iColumns)
{
	for(int iHandle = 0; iHandle < pTH->iHandles; iHandle++)
	{
		if(pTH->Handles[iHandle].iColumns == iColumns)
		{
			pTH->Handles[iHandle].i64RowCount++;
			return pTH->Handles[iHandle].hHandle;
		}
	}

	if(!pCI->bSilent)
	{
		printf("Found row with %d columns.\n", iColumns);
	}

	pTH->Handles = (TARGETHANDLE *) realloc(pTH->Handles, sizeof(TARGETHANDLE) * (pTH->iHandles + 1));

	char sNewFullFile[MAX_PATH];
	char sNewFile[MAX_PATH];
	char sNewExt[MAX_PATH];

	_splitpath_s(pCI->sSourceFile,
		NULL, NULL, NULL, NULL,
		sNewFile, sizeof(sNewFile),
		sNewExt, sizeof(sNewExt));

	sprintf_s(sNewFullFile, sizeof(sNewFullFile),
		"%s\\%s (%d Columns)%s", pCI->sTargetFolder, sNewFile, iColumns, sNewExt);

	CreateFolderStructure(sNewFullFile);

	if((pTH->Handles[pTH->iHandles].hHandle = CreateFile(sNewFullFile, GENERIC_WRITE, 0, NULL,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		printf("Failed to create file \"%s\" for %d column rows.\n", sNewFullFile, iColumns);
		return NULL;
	}

	SetEndOfFile(pTH->Handles[pTH->iHandles].hHandle);

	pTH->Handles[pTH->iHandles].iColumns = iColumns;
	pTH->Handles[pTH->iHandles].i64RowCount = 1;

	return pTH->Handles[pTH->iHandles++].hHandle;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProcessFile(CONFIGINFO *pCI)
{
	bool bResult = true;
	HANDLE hSource = NULL;
	char *sBuf = NULL;
	DWORD dwBytesRead = 0;
	DWORD dwBytesWritten = 0;
	DWORD dwWorkingBytes = 0;

	TARGETHANDLES TH;
	memset(&TH, 0, sizeof(TH));

	if((hSource = CreateFile(pCI->sSourceFile, GENERIC_READ, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		printf("Failed to read \"%s\".\n", pCI->sSourceFile);
		return false;
	}

	if(!(sBuf = (char *) calloc(BUFFERSIZE, sizeof(char))))
	{
		printf("Failed to allocate %d bytes of RAM.\n", BUFFERSIZE * sizeof(char));
		CloseHandle(hSource);
		return false;
	}

	__int64 i64FilePos = 0;
	int iDelimiters = 0;
	DWORD dwBufPos = 0;
	DWORD dwLastPos = 0;
	HANDLE hTarget = NULL;
	
	do {
		ReadFile(hSource, sBuf, BUFFERSIZE, &dwBytesRead, NULL);

		dwWorkingBytes = FindLastLineFeed(sBuf, dwBytesRead);
		i64FilePos += dwWorkingBytes;
		FileSeek(hSource, i64FilePos, FILE_BEGIN);

		for(dwBufPos = 0, dwLastPos = 0; (iDelimiters = CountDelimiters(pCI, sBuf, dwWorkingBytes, &dwBufPos)) > 0;)
		{
			if((dwBufPos - dwLastPos) > 0)
			{
				if((hTarget = GetFileHandle(pCI, &TH, iDelimiters)))
				{
					if(!WriteFile(hTarget, sBuf + dwLastPos, dwBufPos - dwLastPos, &dwBytesWritten, NULL))
					{
						printf("Failed to write %d bytes. Out of disk space?", dwBufPos - dwLastPos);
						bResult = false;
						break;
					}
				}
				else{
					printf("Failed to create file for %d column rows.", iDelimiters);
					bResult = false;
					break;
				}
			}
			dwLastPos = dwBufPos;
		}

	} while(dwBytesRead == BUFFERSIZE && bResult);

	CloseHandle(hSource);
	free(sBuf);

	if(TH.iHandles > 0)
	{
		if(bResult && !pCI->bSilent)
		{
			printf("\nParse summary:\n");
			for(int iHandle = 0; iHandle < TH.iHandles; iHandle++)
			{
				printf("\t%d columns, %I64d rows.\n",
					TH.Handles[iHandle].iColumns,
					TH.Handles[iHandle].i64RowCount);
			}
		}
		for(int iHandle = 0; iHandle < TH.iHandles; iHandle++)
		{
			CloseHandle(TH.Handles[iHandle].hHandle);
		}
		free(TH.Handles);
	}

	return bResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int IsCmd(const char *sArgument, const char *sCommand)
{
	int iCommandLen = strlen(sCommand);
	if(_strnicmp(sArgument, sCommand, iCommandLen) == 0)
	{
		return iCommandLen;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int GetFlagedValue(const char *sBuf, int iSkpLen, int iBufSz, char *sOutBuf)
{
	int iWPos = 0;
	int iRPos = iSkpLen;

	while(iRPos < iBufSz)
	{
		sOutBuf[iWPos++] = sBuf[iRPos++];
	}

	sOutBuf[iWPos] = '\0';

	return iWPos; //The length of the string returned in sOutBuf.
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WriteHelp(void)
{
	printf("\n%s - %s\n", gsTitleCaption, gsFileVersion);
	printf("%s\n\n", gsAppCopyRight);

	printf("Syntax:\n");
	printf(" SplitOnCol [/Source:...] [/Target:...] [/Delimiter...] [/Silent]\n");
	printf("  /?          - Displays this help message.\n");
	printf("  /Source:    - Defines the source file that will be split.\n");
	printf("  /Target:    - Defines the target folder will be filled with split files.\n");
	printf("  /Delimiter: - Defines the column delimiter to split on (one character only).\n");
	printf("  /Silent     - Tells the application not to print verbose information.\n");
	printf("\n");

	printf(" Delimiter can be one of the following:\n");
	printf("  \\t - Tab character\n");
	printf("  \\p - Pipe character\n");
	printf("  \\s - Space character\n");
	printf("\n");

	printf(" Example:\n");
	printf("  SplitOnCol /Source:\"C:\\FY 2008.txt\" /Target:C:\\Temp /Delimiter:\\t\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ParseCmdLine(CONFIGINFO *pCI, int iArgs, char *sArgs[])
{
	int iArgSz = 0;

	if(iArgs <= 1)
	{
		WriteHelp();
		return false;
	}

	for(int iArg = 1; iArg < iArgs; iArg++)
	{
		if(IsCmd(sArgs[iArg], "/?") || IsCmd(sArgs[iArg], "-?"))
		{
			WriteHelp();
			return false;
		}
		else if(IsCmd(sArgs[iArg], "/Silent"))
		{
			pCI->bSilent = true;
		}
		else if((iArgSz = IsCmd(sArgs[iArg], "/Source:")))
		{
			GetFlagedValue(sArgs[iArg], iArgSz, strlen(sArgs[iArg]), pCI->sSourceFile);
		}
		else if((iArgSz = IsCmd(sArgs[iArg], "/Target:")))
		{
			GetFlagedValue(sArgs[iArg], iArgSz, strlen(sArgs[iArg]), pCI->sTargetFolder);
		}
		else if((iArgSz = IsCmd(sArgs[iArg], "/Delimiter:")))
		{
			char sDelimiter[100];
			GetFlagedValue(sArgs[iArg], iArgSz, strlen(sArgs[iArg]), sDelimiter);
			if(_strcmpi(sDelimiter, "\\t") == 0)
			{
				pCI->cDelimiter = '\t';
			}
			else if(_strcmpi(sDelimiter, "\\s") == 0)
			{
				pCI->cDelimiter = ' ';
			}
			else if(_strcmpi(sDelimiter, "\\p") == 0)
			{
				pCI->cDelimiter = '|';
			}
			else{
				if(strlen(sDelimiter) != 1)
				{
					printf("Unles you specity the delimiter as \\t, \\p or \\s,"
						" the delimiter must be only one character");
					return false;
				}
				else{
					pCI->cDelimiter = sDelimiter[0];
				}
			}
		}
		else{
			printf("Unknown argument: [%s]\n", sArgs[iArg]);
			return false;
		}
	}

	if(strlen(pCI->sTargetFolder) == 0)
	{
		printf("No target folder was specified.\n");
		return false;
	}

	if(pCI->sSourceFile == 0)
	{
		printf("No source file was specified.\n");
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int iArg, char *sArg[])
{
	if(!InitializeApp())
    {
		printf("Failed to initialize the appication.");
		return 1;
    }

	CONFIGINFO CI;
	memset(&CI, 0, sizeof(CONFIGINFO));

	strcpy_s(CI.sSourceFile, sizeof(CI.sSourceFile), "C:\\HMC (1).TXT");
	strcpy_s(CI.sTargetFolder, sizeof(CI.sTargetFolder), "C:\\Temp");
	CI.cDelimiter = '\t';

	int iResult = 1;

	if(ParseCmdLine(&CI, iArg, sArg))
	{
		iResult = !ProcessFile(&CI);
	}

	if(!UninitializeApp())
    {
        return 1;
    }

	return iResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
