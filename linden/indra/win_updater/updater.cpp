/** 
 * @file updater.cpp
 * @brief Windows auto-updater
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

//
// Usage: updater -url <url>
//

#include "linden_common.h"

#include <windows.h>
#include <wininet.h>

#define BUFSIZE 8192

int  gTotalBytesRead = 0;
DWORD gTotalBytes = -1;
HWND gWindow = NULL;
WCHAR gProgress[256];
char* gUpdateURL;

#if _DEBUG
FILE* logfile = 0;
#endif

char* wchars_to_utf8chars(WCHAR* in_chars)
{
	int tlen = 0;
	WCHAR* twc = in_chars;
	while (*twc++ != 0)
	{
		tlen++;
	}
	char* outchars = new char[tlen];
	char* res = outchars;
	for (int i=0; i<tlen; i++)
	{
		int cur_char = (int)(*in_chars++);
		if (cur_char < 0x80)
		{
			*outchars++ = (char)cur_char;
		}
		else
		{
			*outchars++ = '?';
		}
	}
	*outchars = 0;
	return res;
}

int WINAPI get_url_into_file(WCHAR *uri, char *path, int *cancelled)
{
	int success = FALSE;
	*cancelled = FALSE;

	HINTERNET hinet, hdownload;
	char data[BUFSIZE];		/* Flawfinder: ignore */
	unsigned long bytes_read;

#if _DEBUG
	fprintf(logfile,"Opening '%s'\n",path);
	fflush(logfile);
#endif	

	FILE* fp = fopen(path, "wb");		/* Flawfinder: ignore */

	if (!fp)
	{
#if _DEBUG
		fprintf(logfile,"Failed to open '%s'\n",path);
		fflush(logfile);
#endif	
		return success;
	}
	
#if _DEBUG
	fprintf(logfile,"Calling InternetOpen\n");
	fflush(logfile);
#endif	
	// Init wininet subsystem
	hinet = InternetOpen(L"LindenUpdater", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hinet == NULL)
	{
		return success;
	}

#if _DEBUG
	fprintf(logfile,"Calling InternetOpenUrl: %s\n",wchars_to_utf8chars(uri));
	fflush(logfile);
#endif	
	hdownload = InternetOpenUrl(hinet, uri, NULL, 0, INTERNET_FLAG_NEED_FILE, NULL);
	if (hdownload == NULL)
	{
#if _DEBUG
		DWORD err = GetLastError();
		fprintf(logfile,"InternetOpenUrl Failed: %d\n",err);
		fflush(logfile);
#endif	
		return success;
	}

	DWORD sizeof_total_bytes = sizeof(gTotalBytes);
	HttpQueryInfo(hdownload, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &gTotalBytes, &sizeof_total_bytes, NULL);
	
	DWORD total_bytes = 0;
	success = InternetQueryDataAvailable(hdownload, &total_bytes, 0, 0);
	if (success == FALSE)
	{
#if _DEBUG
		DWORD err = GetLastError();
		fprintf(logfile,"InternetQueryDataAvailable Failed: %d bytes Err:%d\n",total_bytes,err);
		fflush(logfile);
#endif	
 		return success;
	}

	success = FALSE;
	while(!success && !(*cancelled))
	{
		MSG msg;

#if _DEBUG
		fprintf(logfile,"Calling InternetReadFile\n");
		fflush(logfile);
#endif	
		if (!InternetReadFile(hdownload, data, BUFSIZE, &bytes_read))
		{
#if _DEBUG
			fprintf(logfile,"InternetReadFile Failed.\n");
			fflush(logfile);
#endif
			// ...an error occurred
			return FALSE;
		}

#if _DEBUG
		if (!bytes_read)
		{
			fprintf(logfile,"InternetReadFile Read 0 bytes.\n");
			fflush(logfile);
		}
#endif

#if _DEBUG
		fprintf(logfile,"Reading Data, bytes_read = %d\n",bytes_read);
		fflush(logfile);
#endif	
		
		if (bytes_read == 0)
		{
			// If InternetFileRead returns TRUE AND bytes_read == 0
			// we've successfully downloaded the entire file
			wsprintf(gProgress, L"Download complete.");
			success = TRUE;
		}
		else
		{
			// write what we've got, then continue
			fwrite(data, sizeof(char), bytes_read, fp);

			gTotalBytesRead += int(bytes_read);

			if (gTotalBytes != -1)
				wsprintf(gProgress, L"Downloaded: %d%%", 100 * gTotalBytesRead / gTotalBytes);
			else
				wsprintf(gProgress, L"Downloaded: %dK", gTotalBytesRead / 1024);

		}

#if _DEBUG
		fprintf(logfile,"Calling InvalidateRect\n");
		fflush(logfile);
#endif	
		
		// Mark the window as needing redraw (of the whole thing)
		InvalidateRect(gWindow, NULL, TRUE);

		// Do the redraw
#if _DEBUG
		fprintf(logfile,"Calling UpdateWindow\n");
		fflush(logfile);
#endif	
		UpdateWindow(gWindow);

#if _DEBUG
		fprintf(logfile,"Calling PeekMessage\n");
		fflush(logfile);
#endif	
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				// bail out, user cancelled
				*cancelled = TRUE;
			}
		}
	}

#if _DEBUG
	fprintf(logfile,"Calling InternetCloseHandle\n");
	fclose(logfile);
#endif
	
	fclose(fp);
	InternetCloseHandle(hdownload);
	InternetCloseHandle(hinet);

	return success;
}

LRESULT CALLBACK WinProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	HDC hdc;			// Drawing context
	PAINTSTRUCT ps;

	switch(message)
	{
	case WM_PAINT:
		{
			hdc = BeginPaint(hwnd, &ps);

			RECT rect;
			GetClientRect(hwnd, &rect);
			DrawText(hdc, gProgress, -1, &rect, 
				DT_SINGLELINE | DT_CENTER | DT_VCENTER);

			EndPaint(hwnd, &ps);
			return 0;
		}
	case WM_CLOSE:
	case WM_DESTROY:
		// Get out of full screen
		// full_screen_mode(false);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, message, wparam, lparam);
}

#define win_class_name L"FullScreen"

int parse_args(int argc, char **argv)
{
	int j;

	for (j = 1; j < argc; j++) 
	{
		if ((!strcmp(argv[j], "-url")) && (++j < argc)) 
		{
			gUpdateURL = argv[j];
		}
	}

	// If nothing was set, let the caller know.
	if (!gUpdateURL)
	{
		return 1;
	}
	return 0;
}
	
int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	// Parse the command line.
	LPSTR cmd_line_including_exe_name = GetCommandLineA();

	const int MAX_ARGS = 100;
	int argc = 0;
	char* argv[MAX_ARGS];		/* Flawfinder: ignore */

#if _DEBUG
	logfile = _wfopen(TEXT("updater.log"),TEXT("wt"));
	fprintf(logfile,"Parsing command arguments\n");
	fflush(logfile);
#endif
	
	char *token = NULL;
	if( cmd_line_including_exe_name[0] == '\"' )
	{
		// Exe name is enclosed in quotes
		token = strtok( cmd_line_including_exe_name, "\"" );
		argv[argc++] = token;
		token = strtok( NULL, " \t," );
	}
	else
	{
		// Exe name is not enclosed in quotes
		token = strtok( cmd_line_including_exe_name, " \t," );
	}

	while( (token != NULL) && (argc < MAX_ARGS) )
	{
		argv[argc++] = token;
		/* Get next token: */
		if (*(token + strlen(token) + 1) == '\"')		/* Flawfinder: ignore */
		{
			token = strtok( NULL, "\"");
		}
		else
		{
			token = strtok( NULL, " \t," );
		}
	}

	gUpdateURL = NULL;

	/////////////////////////////////////////
	//
	// Process command line arguments
	//

#if _DEBUG
	fprintf(logfile,"Processing command arguments\n");
	fflush(logfile);
#endif
	
	//
	// Parse the command line arguments
	//
	int parse_args_result = parse_args(argc, argv);
	
	WNDCLASSEX wndclassex = { 0 };
	DEVMODE dev_mode = { 0 };
	char update_exec_path[MAX_PATH];		/* Flawfinder: ignore */

	const int WINDOW_WIDTH = 250;
	const int WINDOW_HEIGHT = 100;

	wsprintf(gProgress, L"Connecting...");

	/* Init the WNDCLASSEX */
	wndclassex.cbSize = sizeof(WNDCLASSEX);
	wndclassex.style = CS_HREDRAW | CS_VREDRAW;
	wndclassex.hInstance = hInstance;
	wndclassex.lpfnWndProc = WinProc;
	wndclassex.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wndclassex.lpszClassName = win_class_name;
	
	RegisterClassEx(&wndclassex);
	
	// Get the size of the screen
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dev_mode);
	
	gWindow = CreateWindowEx(NULL, win_class_name, 
		L"Second Life Updater",
		WS_OVERLAPPEDWINDOW, 
		CW_USEDEFAULT, 
		CW_USEDEFAULT, 
		WINDOW_WIDTH, 
		WINDOW_HEIGHT,
		NULL, NULL, hInstance, NULL);

	ShowWindow(gWindow, nShowCmd);
	UpdateWindow(gWindow);

	if (parse_args_result)
	{
		MessageBox(gWindow, 
				L"Usage: updater -url <url> [-name <window_title>] [-program <program_name>] [-silent]",
				L"Usage", MB_OK);
		return parse_args_result;
	}

	// Did we get a userserver to work with?
	if (!gUpdateURL)
	{
		MessageBox(gWindow, L"Please specify the download url from the command line",
			L"Error", MB_OK);
		return 1;
	}

	// Can't feed GetTempPath into GetTempFile directly
	if (0 == GetTempPathA(MAX_PATH - 14, update_exec_path))
	{
		MessageBox(gWindow, L"Problem with GetTempPath()",
			L"Error", MB_OK);
		return 1;
	}
	strcat(update_exec_path, "Second_Life_Updater.exe");

	WCHAR update_uri[4096];
	mbstowcs(update_uri, gUpdateURL, 4096);

	int success = 0;
	int cancelled = 0;

	// Actually do the download
#if _DEBUG
	fprintf(logfile,"Calling get_url_into_file\n");
	fflush(logfile);
#endif	
	success = get_url_into_file(update_uri, update_exec_path, &cancelled);

	// WinInet can't tell us if we got a 404 or not.  Therefor, we check
	// for the size of the downloaded file, and assume that our installer
	// will always be greater than 1MB.
	if (gTotalBytesRead < (1024 * 1024) && ! cancelled)
	{
		MessageBox(gWindow,
			L"The Second Life auto-update has failed.\n"
			L"The problem may be caused by other software installed \n"
			L"on your computer, such as a firewall.\n"
			L"Please visit http://secondlife.com/download/ \n"
			L"to download the latest version of Second Life.\n",
			NULL, MB_OK);
		return 1;
	}

	if (cancelled)
	{
		// silently exit
		return 0;
	}

	if (!success)
	{
		MessageBox(gWindow, 
			L"Second Life download failed.\n"
			L"Please try again later.", 
			NULL, MB_OK);
		return 1;
	}

	// TODO: Make updates silent (with /S to NSIS)
	//char params[256];		/* Flawfinder: ignore */
	//sprintf(params, "/S");	/* Flawfinder: ignore */
	//MessageBox(gWindow, 
	//	L"Updating Second Life.\n\nSecond Life will automatically start once the update is complete.  This may take a minute...",
	//	L"Download Complete",
	//	MB_OK);
		
	if (32 >= (int) ShellExecuteA(gWindow, "open", update_exec_path, NULL, 
		"C:\\", SW_SHOWDEFAULT))
	{
		// Less than or equal to 32 means failure
		MessageBox(gWindow, L"Update failed.  Please try again later.", NULL, MB_OK);
		return 1;
	}

	// Give installer some time to open a window
	Sleep(1000);

	return 0;
}
