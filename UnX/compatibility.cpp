/**
 * This file is part of UnX.
 *
 * UnX is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * UnX is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UnX.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/
#include <Windows.h>

#include "config.h"

#include "hook.h"
#include "log.h"

#define _NO_CVCONST_H
#include <dbghelp.h>

#pragma comment( lib, "dbghelp.lib" )

typedef HMODULE (WINAPI *LoadLibraryA_pfn)(LPCSTR  lpFileName);
typedef HMODULE (WINAPI *LoadLibraryW_pfn)(LPCWSTR lpFileName);

LoadLibraryA_pfn LoadLibraryA_Original = nullptr;
LoadLibraryW_pfn LoadLibraryW_Original = nullptr;

typedef HMODULE (WINAPI *LoadLibraryExA_pfn)
( _In_       LPCSTR  lpFileName,
  _Reserved_ HANDLE  hFile,
  _In_       DWORD   dwFlags
);

typedef HMODULE (WINAPI *LoadLibraryExW_pfn)
( _In_       LPCWSTR lpFileName,
  _Reserved_ HANDLE  hFile,
  _In_       DWORD   dwFlags
);

LoadLibraryExA_pfn LoadLibraryExA_Original = nullptr;
LoadLibraryExW_pfn LoadLibraryExW_Original = nullptr;

extern HMODULE hModSelf;

#include <Shlwapi.h>
#pragma comment (lib, "Shlwapi.lib")

BOOL
BlacklistLibraryW (LPCWSTR lpFileName)
{
  if (StrStrIW (lpFileName, L"ltc_help32") ||
      StrStrIW (lpFileName, L"ltc_game32")) {
    dll_log.Log (L"[Black List] Preventing Raptr's overlay, evil little thing must die!");
    return TRUE;
  }

  if (StrStrIW (lpFileName, L"PlayClaw")) {
    dll_log.Log (L"[Black List] Incompatible software: PlayClaw disabled");
    return TRUE;
  }

  if (StrStrIW (lpFileName, L"fraps")) {
    dll_log.Log (L"[Black List] FRAPS is not compatible with this software");
    return TRUE;
  }

#if 0
  if (StrStrIW (lpFileName, L"igdusc32")) {
    dll_log.Log (L"[Black List] Intel D3D11 Driver Bypassed");
    return TRUE;
  }
#endif

#if 0
  if (StrStrIW (lpFileName, L"ig75icd32")) {
    dll_log.Log (L"[Black List] Preventing Intel Integrated OpenGL driver from activating...");
    return TRUE;
  }
#endif

  return FALSE;
}

BOOL
BlacklistLibraryA (LPCSTR lpFileName)
{
  wchar_t wszWideLibName [MAX_PATH];

  MultiByteToWideChar (CP_OEMCP, 0x00, lpFileName, -1, wszWideLibName, MAX_PATH);

  return BlacklistLibraryW (wszWideLibName);
}

HMODULE
WINAPI
LoadLibraryA_Detour (LPCSTR lpFileName)
{
  if (lpFileName == nullptr)
    return NULL;

  HMODULE hModEarly = GetModuleHandleA (lpFileName);

  if (hModEarly == NULL && BlacklistLibraryA (lpFileName))
    return NULL;

  HMODULE hMod = LoadLibraryA_Original (lpFileName);

  if (hModEarly != hMod)
    dll_log.Log (L"[DLL Loader] Game loaded '%#64hs' <LoadLibraryA>", lpFileName);

  return hMod;
}

HMODULE
WINAPI
LoadLibraryW_Detour (LPCWSTR lpFileName)
{
  if (lpFileName == nullptr)
    return NULL;

  HMODULE hModEarly = GetModuleHandleW (lpFileName);

  if (hModEarly == NULL && BlacklistLibraryW (lpFileName))
    return NULL;

  HMODULE hMod = LoadLibraryW_Original (lpFileName);

  if (hModEarly != hMod)
    dll_log.Log (L"[DLL Loader] Game loaded '%#64s' <LoadLibraryW>", lpFileName);

  return hMod;
}

HMODULE
WINAPI
LoadLibraryExA_Detour (
  _In_       LPCSTR lpFileName,
  _Reserved_ HANDLE hFile,
  _In_       DWORD  dwFlags )
{
  if (lpFileName == nullptr)
    return NULL;

  HMODULE hModEarly = GetModuleHandleA (lpFileName);

  if (hModEarly == NULL && BlacklistLibraryA (lpFileName))
    return NULL;

  HMODULE hMod = LoadLibraryExA_Original (lpFileName, hFile, dwFlags);

  if (hModEarly != hMod && (! ((dwFlags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) ||
                               (dwFlags & LOAD_LIBRARY_AS_IMAGE_RESOURCE))))
    dll_log.Log (L"[DLL Loader] Game loaded '%#64hs' <LoadLibraryExA>", lpFileName);

  return hMod;
}

HMODULE
WINAPI
LoadLibraryExW_Detour (
  _In_       LPCWSTR lpFileName,
  _Reserved_ HANDLE  hFile,
  _In_       DWORD   dwFlags )
{
  if (lpFileName == nullptr)
    return NULL;

  HMODULE hModEarly = GetModuleHandleW (lpFileName);

  if (hModEarly == NULL && BlacklistLibraryW (lpFileName))
    return NULL;

  HMODULE hMod = LoadLibraryExW_Original (lpFileName, hFile, dwFlags);

  if (hModEarly != hMod && (! ((dwFlags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) ||
                               (dwFlags & LOAD_LIBRARY_AS_IMAGE_RESOURCE))))
    dll_log.Log (L"[DLL Loader] Game loaded '%#64s' <LoadLibraryExW>", lpFileName);

  return hMod;
}

#include "window.h"

LONG
WINAPI
UNX_TopLevelExceptionFilter ( _In_ struct _EXCEPTION_POINTERS *ExceptionInfo )
{
  static bool             last_chance = false;

  static CONTEXT          last_ctx = { 0 };
  static EXCEPTION_RECORD last_exc = { 0 };

  if (last_chance)
    return 0;

  // On second chance it's pretty clear that no exception handler exists,
  //   terminate the software.
  if (! memcmp (&last_ctx, ExceptionInfo->ContextRecord, sizeof CONTEXT)) {
    if (! memcmp (&last_exc, ExceptionInfo->ExceptionRecord, sizeof EXCEPTION_RECORD)) {
      extern HMODULE       hDLLMod;
      extern HMODULE       hInjectorDLL;
      extern BOOL APIENTRY DllMain (HMODULE hModule,
                                    DWORD   ul_reason_for_call,
                                    LPVOID  /* lpReserved */);

      last_chance = true;

      // Shutdown the module gracefully
      DllMain (hDLLMod, DLL_PROCESS_DETACH, nullptr);

      // Shutdown Special K
      FreeLibrary (hInjectorDLL); FreeLibrary (hInjectorDLL); FreeLibrary (hInjectorDLL);

      // Then the app
      SendMessage (unx::window.hwnd, WM_QUIT, 0, 0);
    }
  }

  last_ctx = *ExceptionInfo->ContextRecord;
  last_exc = *ExceptionInfo->ExceptionRecord;

  std::wstring desc;

  switch (ExceptionInfo->ExceptionRecord->ExceptionCode)
  {
    case EXCEPTION_ACCESS_VIOLATION:
      desc = L"\t<< EXCEPTION_ACCESS_VIOLATION >>";
             //L"The thread tried to read from or write to a virtual address "
             //L"for which it does not have the appropriate access.";
      break;

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      desc = L"\t<< EXCEPTION_ARRAY_BOUNDS_EXCEEDED >>";
             //L"The thread tried to access an array element that is out of "
             //L"bounds and the underlying hardware supports bounds checking.";
      break;

    case EXCEPTION_BREAKPOINT:
      desc = L"\t<< EXCEPTION_BREAKPOINT >>";
             //L"A breakpoint was encountered.";
      break;

    case EXCEPTION_DATATYPE_MISALIGNMENT:
      desc = L"\t<< EXCEPTION_DATATYPE_MISALIGNMENT >>";
             //L"The thread tried to read or write data that is misaligned on "
             //L"hardware that does not provide alignment.";
      break;

    case EXCEPTION_FLT_DENORMAL_OPERAND:
      desc = L"\t<< EXCEPTION_FLT_DENORMAL_OPERAND >>";
             //L"One of the operands in a floating-point operation is denormal.";
      break;

    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
      desc = L"\t<< EXCEPTION_FLT_DIVIDE_BY_ZERO >>";
             //L"The thread tried to divide a floating-point value by a "
             //L"floating-point divisor of zero.";
      break;

    case EXCEPTION_FLT_INEXACT_RESULT:
      desc = L"\t<< EXCEPTION_FLT_INEXACT_RESULT >>";
             //L"The result of a floating-point operation cannot be represented "
             //L"exactly as a decimal fraction.";
      break;

    case EXCEPTION_FLT_INVALID_OPERATION:
      desc = L"\t<< EXCEPTION_FLT_INVALID_OPERATION >>";
      break;

    case EXCEPTION_FLT_OVERFLOW:
      desc = L"\t<< EXCEPTION_FLT_OVERFLOW >>";
             //L"The exponent of a floating-point operation is greater than the "
             //L"magnitude allowed by the corresponding type.";
      break;

    case EXCEPTION_FLT_STACK_CHECK:
      desc = L"\t<< EXCEPTION_FLT_STACK_CHECK >>";
             //L"The stack overflowed or underflowed as the result of a "
             //L"floating-point operation.";
      break;

    case EXCEPTION_FLT_UNDERFLOW:
      desc = L"\t<< EXCEPTION_FLT_UNDERFLOW >>";
             //L"The exponent of a floating-point operation is less than the "
             //L"magnitude allowed by the corresponding type.";
      break;

    case EXCEPTION_ILLEGAL_INSTRUCTION:
      desc = L"\t<< EXCEPTION_ILLEGAL_INSTRUCTION >>";
             //L"The thread tried to execute an invalid instruction.";
      break;

    case EXCEPTION_IN_PAGE_ERROR:
      desc = L"\t<< EXCEPTION_IN_PAGE_ERROR >>";
             //L"The thread tried to access a page that was not present, "
             //L"and the system was unable to load the page.";
      break;

    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      desc = L"\t<< EXCEPTION_INT_DIVIDE_BY_ZERO >>";
             //L"The thread tried to divide an integer value by an integer "
             //L"divisor of zero.";
      break;

    case EXCEPTION_INT_OVERFLOW:
      desc = L"\t<< EXCEPTION_INT_OVERFLOW >>";
             //L"The result of an integer operation caused a carry out of the "
             //L"most significant bit of the result.";
      break;

    case EXCEPTION_INVALID_DISPOSITION:
      desc = L"\t<< EXCEPTION_INVALID_DISPOSITION >>";
             //L"An exception handler returned an invalid disposition to the "
             //L"exception dispatcher.";
      break;

    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      desc = L"\t<< EXCEPTION_NONCONTINUABLE_EXCEPTION >>";
             //L"The thread tried to continue execution after a noncontinuable "
             //L"exception occurred.";
      break;

    case EXCEPTION_PRIV_INSTRUCTION:
      desc = L"\t<< EXCEPTION_PRIV_INSTRUCTION >>";
             //L"The thread tried to execute an instruction whose operation is "
             //L"not allowed in the current machine mode.";
      break;

    case EXCEPTION_SINGLE_STEP:
      desc = L"\t<< EXCEPTION_SINGLE_STEP >>";
             //L"A trace trap or other single-instruction mechanism signaled "
             //L"that one instruction has been executed.";
      break;

    case EXCEPTION_STACK_OVERFLOW:
      desc = L"\t<< EXCEPTION_STACK_OVERFLOW >>";
             //L"The thread used up its stack.";
      break;
  }

  HMODULE hModSource;
  char    szModName [MAX_PATH] = { '\0' };
  DWORD64 eip                  = ExceptionInfo->ContextRecord->Eip;
  HANDLE  hProc                = GetCurrentProcess ();

  if (GetModuleHandleEx ( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            (LPCWSTR)eip,
                              &hModSource )) {
    GetModuleFileNameA (hModSource, szModName, MAX_PATH);
  }

  DWORD BaseAddr = 
    SymGetModuleBase ( hProc, eip );

  char* szDupName    = strdup (szModName);
  char* pszShortName = szDupName + lstrlenA (szDupName);

  while (  pszShortName      >  szDupName &&
    *(pszShortName - 1) != '\\')
    --pszShortName;

  dll_log.Log (L"-----------------------------------------------------------");
  dll_log.Log (L"[! Except !] %s", desc.c_str ());
  dll_log.Log (L"-----------------------------------------------------------");
  dll_log.Log (L"[ FaultMod ]  # File.....: '%hs'",  szModName);
  dll_log.Log (L"[ FaultMod ]  * EIP Addr.: %hs+%ph", pszShortName, eip-BaseAddr);

  dll_log.Log ( L"[StackFrame] <-> Eip=%08xh, Esp=%08xh, Ebp=%08xh",
                  eip,
                    ExceptionInfo->ContextRecord->Esp,
                      ExceptionInfo->ContextRecord->Ebp );
  dll_log.Log ( L"[StackFrame] >-< Esi=%08xh, Edi=%08xh",
                  ExceptionInfo->ContextRecord->Esi,
                    ExceptionInfo->ContextRecord->Edi );

  dll_log.Log ( L"[  GP Reg  ]       eax:     0x%08x",
                  ExceptionInfo->ContextRecord->Eax );
  dll_log.Log ( L"[  GP Reg  ]       ebx:     0x%08x",
                  ExceptionInfo->ContextRecord->Ebx );
  dll_log.Log ( L"[  GP Reg  ]       ecx:     0x%08x",
                  ExceptionInfo->ContextRecord->Ecx );
  dll_log.Log ( L"[  GP Reg  ]       edx:     0x%08x",
                  ExceptionInfo->ContextRecord->Edx );
  dll_log.Log ( L"[ GP Flags ]       EFlags:  0x%08x",
                  ExceptionInfo->ContextRecord->EFlags );

  SymLoadModule ( hProc,
                    nullptr,
                      szModName,
                        nullptr,
                          BaseAddr,
                            0 );

  SYMBOL_INFO_PACKAGE sip;
  sip.si.SizeOfStruct = sizeof SYMBOL_INFO;
  sip.si.MaxNameLen   = sizeof sip.name;

  DWORD64 Displacement = 0;

  if ( SymFromAddr ( hProc,
                       eip,
                         &Displacement,
                           &sip.si ) ) {
    dll_log.Log (
      L"-----------------------------------------------------------");

    IMAGEHLP_LINE64 ihl64;
    ihl64.SizeOfStruct = sizeof IMAGEHLP_LINE64;

    DWORD Disp;
    BOOL  bFileAndLine =
      SymGetLineFromAddr64 ( hProc, eip, &Disp, &ihl64 );

    if (bFileAndLine) {
      dll_log.Log ( L"[-(Source)-] [!] %hs  <%hs:%lu>",
                      sip.si.Name,
                        ihl64.FileName,
                          ihl64.LineNumber );
    } else {
      dll_log.Log ( L"[--(Name)--] [!] %hs",
                      sip.si.Name );
    }
  }
  dll_log.Log (L"-----------------------------------------------------------");

  SymUnloadModule (GetCurrentProcess (), BaseAddr);

  free (szDupName);

  if (ExceptionInfo->ExceptionRecord->ExceptionFlags != EXCEPTION_NONCONTINUABLE)
    return EXCEPTION_CONTINUE_EXECUTION;
  else {
    return UnhandledExceptionFilter (ExceptionInfo);
  }
}

extern LONG WINAPI
UNX_TopLevelExceptionFilter ( _In_ struct _EXCEPTION_POINTERS *ExceptionInfo );


typedef LPTOP_LEVEL_EXCEPTION_FILTER (WINAPI *SetUnhandledExceptionFilter_pfn)(
  _In_opt_ LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter
);
SetUnhandledExceptionFilter_pfn SetUnhandledExceptionFilter_Original = nullptr;


LPTOP_LEVEL_EXCEPTION_FILTER
WINAPI
SetUnhandledExceptionFilter_Detour (
  _In_opt_ LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter
)
{
  //if (lpTopLevelExceptionFilter != 0)
    //return SetUnhandledExceptionFilter_Original (UNX_TopLevelExceptionFilter);

  return SetUnhandledExceptionFilter_Original (lpTopLevelExceptionFilter);
}



void
UNX_InitCompatBlacklist (void)
{
  SymInitialize (
    GetCurrentProcess (),
      NULL,
        TRUE );

  SymRefreshModuleList (GetCurrentProcess ());

  SetErrorMode (SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
  SetUnhandledExceptionFilter (UNX_TopLevelExceptionFilter);

  UNX_CreateDLLHook ( L"kernel32.dll", "SetUnhandledExceptionFilter",
                      SetUnhandledExceptionFilter_Detour,
           (LPVOID *)&SetUnhandledExceptionFilter_Original );

  UNX_CreateDLLHook ( L"kernel32.dll", "LoadLibraryA",
                      LoadLibraryA_Detour,
            (LPVOID*)&LoadLibraryA_Original );

  UNX_CreateDLLHook ( L"kernel32.dll", "LoadLibraryW",
                      LoadLibraryW_Detour,
            (LPVOID*)&LoadLibraryW_Original );

  UNX_CreateDLLHook ( L"kernel32.dll", "LoadLibraryExA",
                      LoadLibraryExA_Detour,
            (LPVOID*)&LoadLibraryExA_Original );

  UNX_CreateDLLHook ( L"kernel32.dll", "LoadLibraryExW",
                      LoadLibraryExW_Detour,
            (LPVOID*)&LoadLibraryExW_Original );

  if (GetModuleHandleW (L"fraps.dll") != NULL) {
    dll_log.Log (L"[Black List] FRAPS detected; expect the game to crash.");
    FreeLibrary (GetModuleHandleW (L"fraps.dll"));
  }
}

LPVOID __UNX_base_img_addr = nullptr;
LPVOID __UNX_end_img_addr  = nullptr;

void*
UNX_Scan (const uint8_t* pattern, size_t len, const uint8_t* mask)
{
  uint8_t* base_addr = (uint8_t *)GetModuleHandle (nullptr);

  MEMORY_BASIC_INFORMATION mem_info;
  VirtualQuery (base_addr, &mem_info, sizeof mem_info);

  //
  // VMProtect kills this, so let's do something else...
  //
#ifdef VMPROTECT_IS_NOT_A_FACTOR
  IMAGE_DOS_HEADER* pDOS =
    (IMAGE_DOS_HEADER *)mem_info.AllocationBase;
  IMAGE_NT_HEADERS* pNT  =
    (IMAGE_NT_HEADERS *)((intptr_t)(pDOS + pDOS->e_lfanew));

  uint8_t* end_addr = base_addr + pNT->OptionalHeader.SizeOfImage;
#else
           base_addr = (uint8_t *)mem_info.BaseAddress;//AllocationBase;
  uint8_t* end_addr  = (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;

#if 0
  if (base_addr != (uint8_t *)0x400000) {
    dll_log.Log ( L"[ Sig Scan ] Expected module base addr. 40000h, but got: %ph",
                    base_addr );
  }
#endif

  size_t pages = 0;

// Scan up to 32 MiB worth of data
#define PAGE_WALK_LIMIT (base_addr) + (1 << 26)

  //
  // For practical purposes, let's just assume that all valid games have less than 32 MiB of
  //   committed executable image data.
  //
  while (VirtualQuery (end_addr, &mem_info, sizeof mem_info) && end_addr < PAGE_WALK_LIMIT) {
    if (mem_info.Protect & PAGE_NOACCESS || (! (mem_info.Type & MEM_IMAGE)))
      break;

    pages += VirtualQuery (end_addr, &mem_info, sizeof mem_info);

    end_addr = (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;
  } 

  if (end_addr > PAGE_WALK_LIMIT) {
#if 0
    dll_log.Log ( L"[ Sig Scan ] Module page walk resulted in end addr. out-of-range: %ph",
                    end_addr );
    dll_log.Log ( L"[ Sig Scan ]  >> Restricting to %ph",
                    PAGE_WALK_LIMIT );
#endif
    end_addr = PAGE_WALK_LIMIT;
  }

#if 0
  dll_log.Log ( L"[ Sig Scan ] Module image consists of %lu pages, from %ph to %ph",
                  pages,
                    base_addr,
                      end_addr );
#endif
#endif

  __UNX_base_img_addr = base_addr;
  __UNX_end_img_addr  = end_addr;

  uint8_t*  begin = (uint8_t *)base_addr;
  uint8_t*  it    = begin;
  int       idx   = 0;

  while (it < end_addr)
  {
    VirtualQuery (it, &mem_info, sizeof mem_info);

    // Bail-out once we walk into an address range that is not resident, because
    //   it does not belong to the original executable.
    if (mem_info.RegionSize == 0)
      break;

    uint8_t* next_rgn =
     (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;

    if ( (! (mem_info.Type    & MEM_IMAGE))  ||
         (! (mem_info.State   & MEM_COMMIT)) ||
             mem_info.Protect & PAGE_NOACCESS ) {
      it    = next_rgn;
      idx   = 0;
      begin = it;
      continue;
    }

    // Do not search past the end of the module image!
    if (next_rgn >= end_addr)
      break;

    while (it < next_rgn) {
      uint8_t* scan_addr = it;

      bool match = (*scan_addr == pattern [idx]);

      // For portions we do not care about... treat them
      //   as matching.
      if (mask != nullptr && (! mask [idx]))
        match = true;

      if (match) {
        if (++idx == len)
          return (void *)begin;

        ++it;
      }

      else {
        // No match?!
        if (it > end_addr - len)
          break;

        it  = ++begin;
        idx = 0;
      }
    }
  }

  return nullptr;
}

void
UNX_FlushInstructionCache ( LPCVOID base_addr,
                           size_t  code_size )
{
  FlushInstructionCache ( GetCurrentProcess (),
                            base_addr,
                              code_size );
}

void
UNX_InjectMachineCode ( LPVOID   base_addr,
                        uint8_t* new_code,
                        size_t   code_size,
                        DWORD    permissions,
                        uint8_t* old_code = nullptr )
{
  DWORD dwOld;

  if (VirtualProtect (base_addr, code_size, permissions, &dwOld))
  {
    if (old_code != nullptr)
      memcpy (old_code, base_addr, code_size);

    memcpy (base_addr, new_code, code_size);

    VirtualProtect (base_addr, code_size, dwOld, &dwOld);

    UNX_FlushInstructionCache (base_addr, code_size);
  }
}