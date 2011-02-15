/*
Secure Passphrase Generator
Copyright by Pawel Krawczyk 2010-2011
Home: http://ipsec.pl/passphrase
Licensing: http://creativecommons.org/licenses/BSD/
*/

#ifndef UNICODE
#define UNICODE
#endif

#define ABOUT_TEXT	L"Secure Passphrase Generator 1.1\r\n"   \
					L"Copyright by Pawel Krawczyk 2010-2011\r\n"  \
					L"Home: http://ipsec.pl/passphrase\r\n"  \
					L"Licensing: http://creativecommons.org/licenses/BSD/"
					
#define NUM_WORDS			4									/* how many words in passphrase? */
#define NUM_PASS			20									/* how many passphrases to display in one run */
#define ENTROPY_FORMULA		NUM_WORDS*(log(numWords)/log(2))	/* used for informational purposes only (status bar) */

/* This is how the passphrases list is going to look like (top 3):

	Oink Imbue Gulch Gaits
	Scolds+Spits+Spay+Mango
	Brier+Harlem+Juntas+Caged
*/

#include <windows.h>
#include <wincrypt.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <Shlwapi.h>
#include <CommCtrl.h>

// These Str* functions are defined and implemented in strsafe.h
#if defined(__GNUC__)
# include <string.h>
# include <stdlib.h>
# define StringCchPrintfW _snwprintf
# define StringCchCatW(dest, size, src) wcsncat(dest, src, size) // note swapped args
# ifndef StrDup
#  define StrDup wcsdup
# endif
# ifndef STATUSCLASSNAME
#  define STATUSCLASSNAME L"msctls_statusbar32"
# endif
#else
# include <strsafe.h>
#endif


#define ERRCHECK(ret, msg) if(ret) { \
					LPVOID lpMsgBuf; \
					DWORD dw = GetLastError(); \
					FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
						NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL ); \
					MessageBox(hwnd, lpMsgBuf, msg, MB_OK | MB_ICONERROR); \
					DestroyWindow(hwnd); \
					return 0; \
				}

const WCHAR g_szClassName[] = L"myWindowClass";

/* Dictionary for each language is embedded in the binary */
#include "polish.h"
#include "english.h"

#define ID_GENERATE		100
#define ID_LANGUAGE		200
#define ID_LANG_PL		210
#define ID_LANG_EN		220
#define ID_ABOUT		300
#define ID_INIT			400
#define ID_CLEANUP		500
#define ID_STATUS		600

#define IDC_MAIN_EDIT	1000
#define IDC_MAIN_STATUS	2000

/* This function implements B.5.1.1 Simple Discard Method from NIST SP800-90
 * http://csrc.nist.gov/publications/nistpubs/800-90/SP800-90revised_March2007.pdf
 */
/*
LONG rand_index(HWND hwnd, HCRYPTPROV hCryptProv, LONG max) {
	LONG randInput;	
	LONG upperLimitBytes;
	DOUBLE upperLimitBits;
	static LONG iCallsNum = 0, iTotalLoopsNum = 0;
	static DOUBLE iLoopsAvg = 0.0;
	LONG loops = 0;
	
	upperLimitBits = ceil(log(max) / log(2));
	upperLimitBytes = (LONG) ceil(upperLimitBits/8);

	while(1) {
		LONG retVal;
		randInput = 0L;
		retVal = CryptGenRandom(hCryptProv, upperLimitBytes, (BYTE *) &randInput);
		ERRCHECK((!retVal), L"CryptGenRandom")
		loops++;
		if(randInput < max) break; // found!
	}
	iCallsNum++;
	iTotalLoopsNum += loops;
	iLoopsAvg = iTotalLoopsNum/iCallsNum;
	// average number of iterations is counter purely for debugging purposes
	// and not displayed anywhere
	return randInput;
}
*/
LONG rand_index(IN HWND hwnd, IN HCRYPTPROV hCryptProv, IN LONG max) {
	OUT LONG randInput;	
	static LONG iCallsNum = 0, iTotalLoopsNum = 0;
	static DOUBLE iLoopsAvg = 0.0;
	static LONG iContinousRndTest = 0L;
	LONG upperLimitBytes;
	LONG loops = 0;
	DOUBLE upperLimitBits;
	
	upperLimitBits = ceil(log(max) / log(2));
	upperLimitBytes = (LONG) ceil(upperLimitBits/8);

	while(1) {
		LONG retVal;
		randInput = 0L;
		retVal = CryptGenRandom(hCryptProv, upperLimitBytes, (BYTE *) &randInput));
		ERRCHECK((!retVal), L"CryptGenRandom")
		/* FIPS 140-2 p. 44 Continuous random number generator test */
		if(upperLimitBits > 15 && randInput == iContinousRndTest) {
			MessageBox(hwnd, L"Random numbers are not sufficient quality!", L"FIPS 140-2 Continuous Random Test", MB_OK | MB_ICONERROR);
			DestroyWindow(hwnd);
		}
		iContinousRndTest = randInput;
		loops++;
		if(randInput < max) break; // found!
	}
	iCallsNum++;
	iTotalLoopsNum += loops;
	iLoopsAvg = iTotalLoopsNum/iCallsNum;
	// average number of iterations is counter purely for debugging purposes
	// and not displayed anywhere
	return randInput;
}

// Step 4: the Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
		case WM_CREATE:
			{
				// Create windows on first run
				HMENU hMenu = NULL, hSubMenu = NULL;
				HFONT hfDefault = NULL;
				HWND hEdit = NULL;
				HWND hStatus = NULL;

				// MENU
				hMenu = CreateMenu();
				ERRCHECK((hMenu==NULL), L"Cannot create menu")
				AppendMenu(hMenu, MF_STRING | MF_POPUP, ID_GENERATE, L"&Refresh");
				hSubMenu = CreatePopupMenu();
				ERRCHECK((hSubMenu==NULL), L"Cannot create submenu")
				AppendMenu(hSubMenu, MF_STRING, ID_LANG_PL, L"&Polish");
				AppendMenu(hSubMenu, MF_STRING, ID_LANG_EN, L"&English");
				AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT)hSubMenu, L"&Dictionary");
				AppendMenu(hMenu, MF_STRING | MF_POPUP, ID_ABOUT, L"&About");

				SetMenu(hwnd, hMenu);
				
				// "EDIT" CONTROL

				hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"Edit", NULL, 
					WS_CHILD | WS_VISIBLE | ES_MULTILINE , 0, 0, 100, 100, hwnd, 
					(HMENU)IDC_MAIN_EDIT, GetModuleHandle(NULL), NULL);
				ERRCHECK((hEdit == NULL), L"Could not create edit box")

				hfDefault = GetStockObject(SYSTEM_FIXED_FONT);
				ERRCHECK((hfDefault==NULL), L"Could not get system fixed font")

				SendMessage(hEdit, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE, 0));
						
				// STATUS BAR
				
				hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL, //* XXX This should be STATUSCLASSNAME */
					WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
									hwnd, (HMENU)IDC_MAIN_STATUS, GetModuleHandle(NULL), NULL);
				ERRCHECK((hStatus==NULL), L"Could not create status bar")
					
				SendMessage(hwnd, WM_SIZE, 0, 0);	
				SendMessage(hwnd, WM_COMMAND, ID_INIT, MAKELPARAM(FALSE, 0));
				SendMessage(hwnd, WM_COMMAND, ID_GENERATE, MAKELPARAM(FALSE, 0));
			}
		break;
		/////////////////////////// COMMAND
		case WM_COMMAND:
            switch(LOWORD(wParam))
            {
				static LONG language = ID_LANG_PL;
				static BOOLEAN shown = 0;
				static HCRYPTPROV hCryptProv;
				static size_t numWords;
				static LPCWSTR *words;
				HWND hStatus;
				
				case ID_INIT:
					// Initialize or re-initialize after language change
					// First try CryptoAPI RSA_AES provider, then RSA_FULL, fail if none is available
					if(!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, 0))  {
						if(!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 0))  {
							MessageBox(hwnd, L"CryptAcquireContext failed", L"CryptAcquireContext", MB_OK | MB_ICONERROR);
							DestroyWindow(hwnd);
						}
					}
					// Change status after (re)init
					if(language == ID_LANG_PL) {
						numWords = numPolish;
						words = wordsPolish;
						SendMessage(hwnd, WM_COMMAND, ID_STATUS, MAKELPARAM(FALSE, 0));
					}
					if(language == ID_LANG_EN) {
						numWords = numEnglish;
						words = wordsEnglish;
						SendMessage(hwnd, WM_COMMAND, ID_STATUS, MAKELPARAM(FALSE, 0));
					}
				break;
				
				case ID_STATUS:
					{
						// Update status bar with basic dictionary and entropy information
						WCHAR msg[1000];
						LPCWSTR lang;
						DOUBLE ent;
						LPCWSTR template = L"Dict: %s   Entropy: %.2f";
						
						if(language == ID_LANG_PL) lang=L"Polish";
						if(language == ID_LANG_EN) lang=L"English";
						
						// Information entropy calculation
						ent = ENTROPY_FORMULA; /* this is defined on top so can be audited easily */
						StringCchPrintfW(msg, sizeof(msg), template, lang, ent);

						hStatus = GetDlgItem(hwnd, IDC_MAIN_STATUS);
						ERRCHECK((hStatus == NULL), L"Could not update status bar")

						SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM) msg);
					}
				break;
				
				
				case ID_CLEANUP:
					CryptReleaseContext(hCryptProv, 0);
				break;
				
				case ID_LANG_PL:
					// Switch lang to PL
					language = ID_LANG_PL;
					SendMessage(hwnd, WM_COMMAND, ID_INIT, MAKELPARAM(FALSE, 0));
					SendMessage(hwnd, WM_COMMAND, ID_GENERATE, MAKELPARAM(FALSE, 0));
					shown = FALSE;
				break;
				
				case ID_LANG_EN:
					// Switch lang to EN
					language = ID_LANG_EN;
					SendMessage(hwnd, WM_COMMAND, ID_INIT, MAKELPARAM(FALSE, 0));
					SendMessage(hwnd, WM_COMMAND, ID_GENERATE, MAKELPARAM(FALSE, 0));
					shown = FALSE;
				break;
				
				// **************************** GENERATOR
				case ID_GENERATE:
				{
					// This is the actual passphrase generation loop
					// Passphrases are displayed as soon as they're generated
					int num_words,num_passphrases;
					WCHAR passphrase[1000];
					WCHAR window_output[1000*10];
					LPWSTR word;
					LPCWSTR separators = L" +-=:.,!#%&*_";
					WCHAR separator[sizeof(separators)];
					LONG rIndex;

					memset(window_output, 0, sizeof(window_output));
					/* Loop to generate a list of NP passphrases */
					for(num_passphrases=0; num_passphrases<NUM_PASS; num_passphrases++) {
						memset(passphrase, 0, sizeof(passphrase));
						StringCchPrintfW(separator, sizeof(separator), L"%c", separators[rand_index(hwnd, hCryptProv, sizeof(separators))]);
						/* Loop to generate one passphrase composed of NW words */
						for(num_words=0; num_words<NUM_WORDS; num_words++) {
							/* Produce random index between 0 and number of words in dictionary */
							rIndex = rand_index(hwnd, hCryptProv, numWords);
							/* Select random word from dictionary; result="word" */
							word = StrDup(words[rIndex]);
							/* Capitalise first letter; result="Word" */
							word[0] = towupper(word[0]);
							// Append new word to passphrase
							StringCchCatW(passphrase, sizeof(passphrase), word);
							LocalFree(word);
							// Append separator if any words to follow
							if(num_words< (NUM_WORDS-1))
								StringCchCatW(passphrase, sizeof(passphrase), separator);
						} /* end of single passphrase generation */
						// Append newly generated passphrase to end of list with CRLF
						StringCchCatW(window_output, sizeof(window_output), passphrase);
						StringCchCatW(window_output, sizeof(window_output), L"\r\n");
					} /* end of passphrase list generation */
					// Print to edit window
					SetDlgItemText(hwnd, IDC_MAIN_EDIT, window_output);
				}
				break;
				// **************************** end GENERATOR
				
				case ID_ABOUT:
					MessageBox(hwnd, ABOUT_TEXT, 
					L"About", MB_OK | MB_ICONINFORMATION);
				break;
            }
        break;
		/////////////////////////// END COMMAND
		case WM_SIZE:
			{
				// Window resize cosmetics
				RECT rcClient;
				
				HWND hEdit;
				int iEditHeight;
				
				HWND hStatus;
				RECT rcStatus;
				int iStatusHeight;
								
				hStatus = GetDlgItem(hwnd, IDC_MAIN_STATUS);
				SendMessage(hStatus, WM_SIZE, 0, 0);
				GetWindowRect(hStatus, &rcStatus);
				iStatusHeight = rcStatus.bottom - rcStatus.top;
		
				GetClientRect(hwnd, &rcClient);
				iEditHeight = rcClient.bottom - iStatusHeight;
				
				hEdit = GetDlgItem(hwnd, IDC_MAIN_EDIT);
				SetWindowPos(hEdit, NULL, 0, 0, rcClient.right, iEditHeight, SWP_NOZORDER);
			}
		break;
		case WM_CLOSE:
			// Run crypto cleanup and close window
			SendMessage(hwnd, WM_COMMAND, ID_CLEANUP, MAKELPARAM(FALSE, 0));
            DestroyWindow(hwnd);
        break;
        case WM_DESTROY:
            PostQuitMessage(0);
        break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    //Step 1: Registering the Window Class
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if(!RegisterClassEx(&wc))
    {
        MessageBox(NULL, L"Window Registration Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Step 2: Creating the Window
   hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
		g_szClassName,
        L"Secure Passphrase",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 550,
        NULL, NULL, hInstance, NULL);
   // Can't use ERRCHECK because there's no window yet
   if(hwnd == NULL) {
		LPVOID lpMsgBuf; \
		DWORD dw = GetLastError(); \
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
			NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL ); \
		MessageBox(hwnd, lpMsgBuf, L"Could not create application window", MB_OK | MB_ICONERROR); \
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Step 3: The Message Loop
    while(GetMessage(&Msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    return Msg.wParam;
}
