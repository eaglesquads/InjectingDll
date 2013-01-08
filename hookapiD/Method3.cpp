//	Project: HookApi
//	File:    Method3.cpp
//	Author:  Paul Bludov
//	Date:    09/19/2001
//
//	Description:
//		NONE
//
//	Update History:
//		NONE
//
//@//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MainDlg.h"

//@//////////////////////////////////////////////////////////////////////////
//	Method3: Override exports

HRESULT ApiHijackExports(
				HMODULE hModule,	//����� �� dll (����� ��������)
				LPSTR szEntry,
				LPVOID pHijacker,
				LPVOID *ppOrig
				)
{
	// Check args
	if ((!IS_INTRESOURCE(szEntry) && ::IsBadStringPtrA(szEntry, -1))
		|| ::IsBadCodePtr(FARPROC(pHijacker)))
	{
		return E_INVALIDARG;
	}

	PIMAGE_DOS_HEADER pDosHeader = PIMAGE_DOS_HEADER(hModule);

	if (::IsBadReadPtr(pDosHeader, sizeof(IMAGE_DOS_HEADER)) ||
		IMAGE_DOS_SIGNATURE != pDosHeader->e_magic)
	{
		return E_INVALIDARG;
	}

	PIMAGE_NT_HEADERS pNTHeaders =
		MakePtr(PIMAGE_NT_HEADERS, hModule, pDosHeader->e_lfanew);
	
	if (::IsBadReadPtr(pNTHeaders, sizeof(IMAGE_NT_HEADERS)) ||
		IMAGE_NT_SIGNATURE != pNTHeaders->Signature)
	{
		return E_INVALIDARG;
	}

	HRESULT hr = E_UNEXPECTED;

	IMAGE_DATA_DIRECTORY& expDir =
		pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	PIMAGE_EXPORT_DIRECTORY pExpDir =
		MakePtr(PIMAGE_EXPORT_DIRECTORY, hModule, expDir.VirtualAddress);

	LPDWORD pdwAddrs = MakePtr(LPDWORD, hModule, pExpDir->AddressOfFunctions);
	LPWORD pdwOrd = MakePtr(LPWORD, hModule, pExpDir->AddressOfNameOrdinals);
	DWORD dwAddrIndex = -1;

	if (IS_INTRESOURCE(szEntry))
	{
		// By ordinal

		dwAddrIndex = WORD(szEntry) - pExpDir->Base;
		hr = S_OK;
	}
	else
	{
		// By name
		LPDWORD pdwNames = MakePtr(LPDWORD, hModule, pExpDir->AddressOfNames);
		for (DWORD iName = 0; iName < pExpDir->NumberOfNames; iName++)
		{
			if (0 == ::lstrcmpiA(MakePtr(LPSTR, hModule, pdwNames[iName]), szEntry))
			{
				dwAddrIndex = pdwOrd[iName];
				hr = S_OK;
				break;
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		if (pdwAddrs[dwAddrIndex] >= expDir.VirtualAddress &&
			pdwAddrs[dwAddrIndex] < expDir.VirtualAddress + expDir.Size)
		{
			// We have a redirection
			LPSTR azRedir = MakePtr(LPSTR, hModule, pdwAddrs[dwAddrIndex]);
			ATLASSERT(!IsBadStringPtrA(azRedir, -1));

			LPSTR azDot = strchr(azRedir, '.');
			int nLen = azDot - azRedir;
			LPSTR azModule = (LPSTR)alloca(nLen);
			memcpy(azModule, azRedir, nLen);
			azModule[nLen] = '\x0';

			// Try to patch redirected function
			return ApiHijackExports(
				::GetModuleHandle(azModule), azDot + 1, pHijacker, ppOrig);
		}

		if (ppOrig)
			*ppOrig = MakePtr(LPVOID, hModule, pdwAddrs[dwAddrIndex]);

		DWORD dwOffset = DWORD_PTR(pHijacker) - DWORD_PTR(hModule);

		// write to write-protected memory
		hr = WriteProtectedMemory(pdwAddrs + dwAddrIndex, &dwOffset, sizeof(LPVOID));
	}

	return hr;
}

LRESULT CMainDlg::OnMethod3(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LPVOID pOldHandler = NULL;
	//HRESULT hr = ApiHijackExports(::GetModuleHandle("USER32.dll"), "MessageBeep", MyMessageBeep, &pOldHandler);
	HRESULT hr = ApiHijackExports(::GetModuleHandle("SampleDll.dll"), "NoArgs", MyNoArgs, &pOldHandler);
	if (SUCCEEDED(hr))
	{
		//BOOL (WINAPI *pMessageBeep)(IN UINT);
		//pMessageBeep = (BOOL (WINAPI *)(IN UINT))
		//		::GetProcAddress(::GetModuleHandle("USER32.dll"), "MessageBeep");
		//pMessageBeep(m_uType);

		//BOOL (*pFunctionNoArgs)(int);
		//pFunctionNoArgs = (BOOL (*)(int))
		//		::GetProcAddress(::GetModuleHandle("SampleDll.dll"), "NoArgs");
		//pFunctionNoArgs(m_uType);

		HINSTANCE hDllInstance = LoadLibraryEx(_T("SampleDll.dll"), 0, DONT_RESOLVE_DLL_REFERENCES);
		BOOL (*pFunctionNoArgs)(int);
		pFunctionNoArgs = (BOOL (*)(int))GetProcAddress(hDllInstance,"NoArgs");
		pFunctionNoArgs(m_uType);

		// Restore old handler
		//ApiHijackExports(::GetModuleHandle("USER32.dll"), "MessageBeep", pOldHandler, NULL);
		ApiHijackExports(::GetModuleHandle("SampleDll.dll"), "NoArgs", pOldHandler, NULL);
	}
	else
		MessageBox(_T("Failed to hijack ::MessageBeep() exports"), NULL, MB_OK | MB_ICONHAND);

	return 0;
}


//@//////////////////////////////////////////////////////////////////////////
//	End Of File Method3.cpp
