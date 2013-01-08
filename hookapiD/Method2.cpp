//	Project: HookApi
//	File:    Method2.cpp
//	Author:  Paul Bludov
//	Date:    09/18/2001
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
#include "SampleDll.h" //implicit linking
#include <psapi.h>
#include <string>
#include <Dbghelp.h>

//@//////////////////////////////////////////////////////////////////////////
//	Method2: Override imports

HRESULT ApiHijackImports(
				HMODULE hModule,	//����� �� ��� ��������� (����� �������), � ��� ���������� ���������
				LPSTR szVictim,
				LPSTR szEntry,
				LPVOID pHijacker,	//���������� ���������
				LPVOID *ppOrig
				)
{
	// Check args
	if (::IsBadStringPtrA(szVictim, -1) ||
		(!IS_INTRESOURCE(szEntry) && ::IsBadStringPtrA(szEntry, -1)) ||
		::IsBadCodePtr(FARPROC(pHijacker)))
	{
		return E_INVALIDARG;
	}

	PIMAGE_DOS_HEADER pDosHeader = PIMAGE_DOS_HEADER(hModule); //DOS ��������� PE ����� (���� ���������)

	if (::IsBadReadPtr(pDosHeader, sizeof(IMAGE_DOS_HEADER)) ||
		IMAGE_DOS_SIGNATURE != pDosHeader->e_magic)
	{
		return E_INVALIDARG;
	}

	PIMAGE_NT_HEADERS pNTHeaders = 
		MakePtr(PIMAGE_NT_HEADERS, hModule, pDosHeader->e_lfanew); //PE ��������� (���� ���������)

	if (::IsBadReadPtr(pNTHeaders, sizeof(IMAGE_NT_HEADERS)) ||
		IMAGE_NT_SIGNATURE != pNTHeaders->Signature)
	{
		return E_INVALIDARG;
	}

	HRESULT hr = E_UNEXPECTED;

	// Locate the victim
	IMAGE_DATA_DIRECTORY& impDir = 
		pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]; //������� ���������� ������ ������������� ��������� (���������): ������� �������
	PIMAGE_IMPORT_DESCRIPTOR pImpDesc = 
		MakePtr(PIMAGE_IMPORT_DESCRIPTOR, hModule, impDir.VirtualAddress), //����� ������ ������� ������� (������ �������� IMAGE_IMPORT_DESCRIPTOR) 
			pEnd = pImpDesc + impDir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1; //����� ����� �������

	while(pImpDesc < pEnd)
	{
		char *p; //debug
		p=MakePtr(char*,hModule,pImpDesc->Name);
		
		if (0 == ::lstrcmpiA(MakePtr(LPSTR, hModule, pImpDesc->Name), szVictim)) //� ������� ������� szVictim
		{
			if (0 == pImpDesc->OriginalFirstThunk)
			{
				// no import names table
				return E_UNEXPECTED;
			}

			// Locate the entry
		    PIMAGE_THUNK_DATA pNamesTable =
				MakePtr(PIMAGE_THUNK_DATA, hModule, pImpDesc->OriginalFirstThunk);

			if (IS_INTRESOURCE(szEntry))
			{
				// By ordinal
				while(pNamesTable->u1.AddressOfData)
				{
					if (IMAGE_SNAP_BY_ORDINAL(pNamesTable->u1.Ordinal) &&
						WORD(szEntry) == IMAGE_ORDINAL(pNamesTable->u1.Ordinal))
					{
						hr = S_OK;
						break;
					}
					pNamesTable++;
				}
			}
			else
			{
				// By name
				while(pNamesTable->u1.AddressOfData)
				{
					if (!IMAGE_SNAP_BY_ORDINAL(pNamesTable->u1.Ordinal))
					{
						PIMAGE_IMPORT_BY_NAME pName = MakePtr(PIMAGE_IMPORT_BY_NAME,
											hModule, pNamesTable->u1.AddressOfData);

						if (0 == ::lstrcmpiA(LPSTR(pName->Name), szEntry))
						{
							hr = S_OK;
							break;
						}
					}
					pNamesTable++;
				}
			}

			if (SUCCEEDED(hr))
			{
				// Get address
				LPVOID *pProc = MakePtr(LPVOID *, pNamesTable,
							pImpDesc->FirstThunk - pImpDesc->OriginalFirstThunk);

				// Save original handler
				if (ppOrig)
					*ppOrig = *pProc;

				// write to write-protected memory
				return WriteProtectedMemory(pProc, &pHijacker, sizeof(LPVOID));
			}
			break;
		}
		pImpDesc++;
	}
	return hr;
}

//ReplaceIATEntryInOneMod(szVictim, , , hModule);
/*
void ReplaceIATEntryInOneMod(PCSTR pszCalleeModName, PROC pfnCurrent, PROC pfnNew, HMODULE hmodCaller) 
{
  //������� ����� ������ �������
  ULONG ulSize;
  PIMAGE_IMPORT_DESCRIPTOR pImportDesc = 
    (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(hmodCaller, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
  if (pImportDesc == NULL) 
    return;  //����� ��� ������ �������
  
  //����� ������ ������
  for (; pImportDesc->Name; pImportDesc++) 
  {
    PSTR pszModName = (PSTR)((PBYTE) hmodCaller + pImportDesc->Name);
    if (lstrcmpiA(pszModName, pszCalleeModName) == 0) 
    {
      //�����
      if (pImportDesc->Name == 0)
        return;  //�� ���� ������� �� �������������
      //������� ����� ������� �������
      PIMAGE_THUNK_DATA pThunk = 
        (PIMAGE_THUNK_DATA)((PBYTE) hmodCaller + pImportDesc->FirstThunk);
      //�������� ��� ������������� �������
      for (; pThunk->u1.Function; pThunk++) 
      {
        PROC* ppfn = (PROC*) &pThunk->u1.Function; //������� ����� �������
        BOOL fFound = (*ppfn == pfnCurrent);     //��� ����?
        if (!fFound && (*ppfn > sm_pvMaxAppAddr)) 
        {
          // ���� �� �����, �� ������ ��������. 
          // ���� �� � Win98 ��� ����������, ��
          // ����� ����� ���� push � ������� ����� �������
          PBYTE pbInFunc = (PBYTE) *ppfn;
          if (pbInFunc[0] == cPushOpCode) 
          {
            //��, ����� PUSH
            ppfn = (PROC*) &pbInFunc[1];
            //��� �����?
            fFound = (*ppfn == pfnCurrent);
          }
        }

        if (fFound) 
        { 
          //�����!!!
          DWORD dwDummy;
          //�������� ������ � ��� ��������
          VirtualProtect(ppfn, sizeof(ppfn), PAGE_EXECUTE_READWRITE, &dwDummy);
          //������ ����� �� ����
          WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, 
            sizeof(pfnNew), NULL);
          //����������� ��������
          VirtualProtect(ppfn, sizeof(ppfn), dwDummy , &dwDummy);
          //������!!!
          return;
        }
      }
    }
  }
  //����� ���� ������� �� �������
}
*/

void shortName(LPSTR strToChange)
{
	std::string path(strToChange);
	std::string filename;

	size_t pos = path.find_last_of("\\");
	if (pos != std::string::npos)
		filename.assign(path.begin() + pos + 1, path.end());
	else
		filename = path;

	lstrcpy(strToChange, filename.data());
}

//
//���� - OpenProcess, ReadProcessMemory
//
/*
void dump(LPSTR szVictimProcess)
{
	//������� ������� szVictimProcess
	DWORD aProcesses[1024], cbNeeded, nProcesses;
    unsigned int i;

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
		return;
    nProcesses = cbNeeded / sizeof(DWORD);

	HANDLE ProcHandle = 0;
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
	for (i = 0; i < nProcesses; i++)
	{
		ProcHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, aProcesses[i]);
		
		if (NULL != ProcHandle) 
		{
			HMODULE hMod;
			GetModuleBaseName(ProcHandle, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR)); // Get the process name
			if (0 == lstrcmpiA(szVictimProcess, szProcessName))
			{
				ReadProcessMemory(ProcHandle,hMod,
			}
		}

		CloseHandle(ProcHandle);
	}

	return;
}*/

//����� dll szVictim � �������� szVictimProcess
LPVOID imageBase(LPSTR szVictimProcess, LPSTR szVictim)
{
	//������� ������� szVictimProcess
	DWORD aProcesses[1024], cbNeeded, nProcesses;
    unsigned int i;

    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
		return NULL;
    nProcesses = cbNeeded / sizeof(DWORD);

	HANDLE ProcHandle = 0;
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
	for (i = 0; i < nProcesses; i++)
	{
		ProcHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, aProcesses[i]);
		
		if (NULL != ProcHandle) 
		{
			HMODULE hMod[1024];
			if ( EnumProcessModules(ProcHandle, hMod, sizeof(hMod), &cbNeeded) )
			{
				GetModuleBaseName(ProcHandle, hMod[0], szProcessName, sizeof(szProcessName)/sizeof(TCHAR)); // Get the process name
				if (0 == lstrcmpiA(szVictimProcess, szProcessName))
				{
					//������� ������ szVictim
					DWORD nModules = cbNeeded / sizeof(HMODULE);
					char szModName[MAX_PATH];
					for (unsigned int j = 0; j < nModules; j++)
					{
						if (GetModuleFileNameEx(ProcHandle, hMod[j], szModName, sizeof(szModName))) // Get the module name
						{
							shortName(szModName);
							if (0 == lstrcmpiA(szModName, szVictim)) 
							{
								MODULEINFO info;
								GetModuleInformation(ProcHandle, hMod[j], &info, sizeof(info));
								return info.lpBaseOfDll;

								//Equal To:
								//return hMod[j];
								
								//Debug:
								//LPSTR string = new char[256];
								//wsprintf(string,"\t%s (0x%08X)\n", szModName, hMod[j]);
							}
						}
					}
					break;
				}
			}
		}
		
		CloseHandle(ProcHandle);
	}

	return NULL; 
}

int MySymbolCounter(const char *a, char b, int c)
{
	FILE* pf = fopen("MySymbolCounter.txt","w+");
	fprintf(pf,"%xh, %c, %n",a,b,c);
	fclose(pf);
	return 10;
}

LRESULT CMainDlg::OnMethod2(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{	//��� ������� ������� - ��� ���������� (������) �������������� ���������

	//������� ����� dll (�� �����)
	//LPVOID pBase = imageBase("DllProj.exe", "SampleDll.dll");
	//if (!pBase) return 0;

	//ULONG ulSize;
	//PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(pBase, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
	
	//PIMAGE_DOS_HEADER pDosHeader = PIMAGE_DOS_HEADER((HMODULE)pBase); //DOS ��������� PE �����
	//WORD* pMagic = &(pDosHeader->e_magic);
	//BOOL test = ::IsBadReadPtr(pDosHeader, sizeof(IMAGE_DOS_HEADER));
	//bool test2 = IMAGE_DOS_SIGNATURE != pDosHeader->e_magic;

	//IMAGE_DOS_HEADER* image_dos_header = new(IMAGE_DOS_HEADER);
	//BOOL test = ::IsBadReadPtr(pBase, sizeof(IMAGE_DOS_HEADER));
	//memcpy(image_dos_header,pBase,sizeof(IMAGE_DOS_HEADER));
	//if(image_dos_header->e_magic!=IMAGE_DOS_SIGNATURE)// MZ
	//{
	//	int bad = true;
	//}

	//PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)RtlImageDirectoryEntryToData(
    //                    Entry->DllBase,
    //                    TRUE,
    //                    IMAGE_DIRECTORY_ENTRY_IMPORT,
    //                    &ImportSize
    //                    );

	LPVOID pOldHandler = NULL;
	//HRESULT hr = ApiHijackImports(::GetModuleHandle(NULL), "USER32.dll", "MessageBeep", MyMessageBeep, &pOldHandler);
	//HRESULT hr = ApiHijackImports(::GetModuleHandle(NULL), "SampleDll.dll", "NoArgs", MyNoArgs, &pOldHandler);
	HRESULT hr = ApiHijackImports(::GetModuleHandle(NULL), "USER32.dll", "MessageBoxA", MyHereMessageBoxA, &pOldHandler);
	if (SUCCEEDED(hr))
	{
		// Our handler will be used instead of original
		//::MessageBeep(m_uType);
		//BOOL p = NoArgs(m_uType); 
		MessageBoxA("Hello !");

		// Restore old handler
		hr = ApiHijackImports(::GetModuleHandle(NULL), "USER32.dll", "MessageBoxA", pOldHandler, NULL);
		//ApiHijackImports(::GetModuleHandle(NULL), "SampleDll.dll", "NoArgs", pOldHandler, NULL);
		if (!SUCCEEDED(hr))
			MessageBox(_T("Failed to hijack imports"), NULL, MB_OK | MB_ICONHAND);
	}
	else
		MessageBox(_T("Failed to hijack imports"), NULL, MB_OK | MB_ICONHAND);

	return 0;
}


//@//////////////////////////////////////////////////////////////////////////
//	End Of File Method2.cpp
