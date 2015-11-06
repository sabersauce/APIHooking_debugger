//surprise calc
//hookmain

//using debugger

#include <Windows.h>
#include <stdio.h>

wchar_t oriStr[] = L"5201314";
wchar_t surpriseStr[] = L"( ©b- ©b)¤Ä¥í";

HANDLE hDebuggeeProc;
HANDLE hDebuggeeThread;
BYTE backup;
BYTE int3 = '\xcc';
FARPROC pSetWindowTextW;

BOOL
OnCreateProcessDebugEvent(DEBUG_EVENT de) {
	pSetWindowTextW = GetProcAddress(LoadLibraryA("user32.dll"), "SetWindowTextW");
	if (pSetWindowTextW == NULL) {
		printf("OnCreateProcessDebugEvent:GetProcAddress failed.\n");
		return FALSE;
	}
	hDebuggeeProc = de.u.CreateProcessInfo.hProcess;
	hDebuggeeThread = de.u.CreateProcessInfo.hThread;

	BOOL result;
	result = ReadProcessMemory(hDebuggeeProc, pSetWindowTextW, &backup, sizeof(BYTE), NULL);
	if (result == FALSE) {
		printf("OnCreateProcessDebugEvent:ReadProcessMemory failed.\n");
		return FALSE;
	}
	
	result = WriteProcessMemory(hDebuggeeProc, pSetWindowTextW, &int3, sizeof(BYTE), NULL);
	if (result == FALSE) {
		printf("OnCreateProcessDebugEvent:WriteProcessMemory failed.\n");
		return FALSE;
	}

	ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
	return TRUE;
}

BOOL
OnBreakPointDebugEvent(DEBUG_EVENT de) {
	CONTEXT threadCtx;
	BOOL result;
	DWORD remoteThreadStrAddr;
	wchar_t strBuffer[100] = { 0 };
	if (de.u.Exception.ExceptionRecord.ExceptionAddress == pSetWindowTextW) {
		result = WriteProcessMemory(hDebuggeeProc, pSetWindowTextW, &backup, sizeof(BYTE), NULL);
		if (result == FALSE) {
			printf("OnBreakPointDebugEvent:(unhook)WriteProcessMemory failed.\n");
			return FALSE;
		}

		threadCtx.ContextFlags = CONTEXT_CONTROL;
		GetThreadContext(hDebuggeeThread, &threadCtx);

		result = ReadProcessMemory(hDebuggeeProc, (LPCVOID)(threadCtx.Esp + 8), &remoteThreadStrAddr, sizeof(DWORD), NULL);
		if (result == FALSE) {
			printf("OnBreakPointDebugEvent:(read str address)ReadProcessMemory failed.\n");
			return FALSE;
		}

		result = ReadProcessMemory(hDebuggeeProc, (LPCVOID)remoteThreadStrAddr, strBuffer, sizeof(wchar_t) * 100, NULL);
		if (result == FALSE) {
			printf("OnBreakPointDebugEvent:(read str)ReadProcessMemory failed.\n");
			return FALSE;
		}

		if (wcscmp(strBuffer, oriStr) == 0) {
			printf("hit!\n");
			result = WriteProcessMemory(hDebuggeeProc, (LPVOID)remoteThreadStrAddr, surpriseStr, sizeof(wchar_t)*(wcslen(surpriseStr) + 1), NULL);
			if (result == FALSE) {
				printf("OnBreakPointDebugEvent:(set surprise str)WriteProcessMemory failed.\n");
				return FALSE;
			}
		}
		
		threadCtx.Eip = (DWORD)pSetWindowTextW;
		result = SetThreadContext(hDebuggeeThread, &threadCtx);
		if (result == FALSE) {
			printf("OnBreakPointDebugEvent:SetThreadContext failed.\n");
			return FALSE;
		}
		ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
		Sleep(0);

		result = WriteProcessMemory(hDebuggeeProc, pSetWindowTextW, &int3, sizeof(BYTE), NULL);
		if (result == FALSE) {
			printf("OnBreakPointDebugEvent:(rehook)WriteProcessMemory failed.\n");
		}
		return TRUE;  
	} 
	return FALSE;
}

int
main(int argc, char *argv[]) {
	if (argc != 2) {
		if (argc != 1) printf("Wrong parameters.\n\n");
		printf("Usage:hookmain.exe <ProcessID>");
		return FALSE;
	}

	DWORD pID = atoi(argv[1]);

	BOOL result;
	result = DebugActiveProcess(pID);
	if (result == FALSE) {
		printf("Attach to the process failed.\n");
		return FALSE;
	}

	DEBUG_EVENT de;
	while (WaitForDebugEvent(&de, INFINITE)) {
		if (de.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
			if (OnCreateProcessDebugEvent(de))
				continue;
			else
				break;
		}
		else if (de.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
			if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) {
				if(OnBreakPointDebugEvent(de)) 
					continue;
			}
		}
		else if (de.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
			break;
		}

		ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
	}
	return 0;
}