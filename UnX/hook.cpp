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
#define _CRT_SECURE_NO_WARNINGS

#include <string>

#include "hook.h"
#include "config.h"


MH_STATUS
__stdcall
UNX_CreateFuncHook ( const wchar_t  *pwszFuncName,
                           void     *pTarget,
                           void     *pDetour,
                           void    **ppOriginal )
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (__stdcall *SK_CreateFuncHook_pfn)( const wchar_t  *pwszFuncName,
                                                              void     *pTarget,
                                                              void     *pDetour,
                                                              void    **ppOriginal );

  static SK_CreateFuncHook_pfn SK_CreateFuncHook =
    (SK_CreateFuncHook_pfn)GetProcAddress (hParent, "SK_CreateFuncHook");

  return
    SK_CreateFuncHook (pwszFuncName, pTarget, pDetour, ppOriginal);
}

MH_STATUS
__stdcall
UNX_CreateDLLHook ( const wchar_t  *pwszModule, const char  *pszProcName,
                          void     *pDetour,          void **ppOriginal,
                          void    **ppFuncAddr )
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (__stdcall *SK_CreateDLLHook_pfn)( const wchar_t  *pwszModule, const char  *pszProcName,
                                                             void     *pDetour,          void **ppOriginal,
                                                             void    **ppFuncAddr );
  static SK_CreateDLLHook_pfn SK_CreateDLLHook =
    (SK_CreateDLLHook_pfn)GetProcAddress (hParent, "SK_CreateDLLHook");

  return
    SK_CreateDLLHook (pwszModule,pszProcName,pDetour,ppOriginal,ppFuncAddr);
}

MH_STATUS
__stdcall
UNX_CreateDLLHook2 ( const wchar_t  *pwszModule, const char  *pszProcName,
                           void     *pDetour,          void **ppOriginal,
                           void    **ppFuncAddr )
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (__stdcall *SK_CreateDLLHook2_pfn)( const wchar_t  *pwszModule, const char  *pszProcName,
                                                              void     *pDetour,          void **ppOriginal,
                                                              void    **ppFuncAddr );

  static SK_CreateDLLHook2_pfn SK_CreateDLLHook2 =
    (SK_CreateDLLHook2_pfn)GetProcAddress (hParent, "SK_CreateDLLHook2");

  return
    SK_CreateDLLHook2 (pwszModule,pszProcName,pDetour,ppOriginal,ppFuncAddr);
}

MH_STATUS
__stdcall
UNX_ApplyQueuedHooks (void)
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (__stdcall *SK_ApplyQueuedHooks_pfn)(void);

  static SK_ApplyQueuedHooks_pfn SK_ApplyQueuedHooks =
    (SK_ApplyQueuedHooks_pfn)GetProcAddress (hParent, "SK_ApplyQueuedHooks");

  return SK_ApplyQueuedHooks ();
}








MH_STATUS
__stdcall
UNX_EnableHook (void *pTarget)
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (__stdcall *SK_EnableHook_pfn)(void *pTarget);

  static SK_EnableHook_pfn SK_EnableHook =
    (SK_EnableHook_pfn)GetProcAddress (hParent, "SK_EnableHook");

  return SK_EnableHook (pTarget);
}

MH_STATUS
__stdcall
UNX_DisableHook (void *pTarget)
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (__stdcall *SK_DisableHook_pfn)(void *pTarget);

  static SK_DisableHook_pfn SK_DisableHook =
    (SK_DisableHook_pfn)GetProcAddress (hParent, "SK_DisableHook");

  return SK_DisableHook (pTarget);
}

MH_STATUS
__stdcall
UNX_RemoveHook (void *pTarget)
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (__stdcall *SK_RemoveHook_pfn)(void *pTarget);

  static SK_RemoveHook_pfn SK_RemoveHook =
    (SK_RemoveHook_pfn)GetProcAddress (hParent, "SK_RemoveHook");

  return SK_RemoveHook (pTarget);
}

MH_STATUS
__stdcall
UNX_Init_MinHook (void)
{
  MH_STATUS status = MH_OK;

  return status;
}

MH_STATUS
__stdcall
UNX_UnInit_MinHook (void)
{
  MH_STATUS status = MH_OK;

  return status;
}