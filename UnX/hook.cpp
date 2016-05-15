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
WINAPI
UNX_CreateFuncHook ( LPCWSTR pwszFuncName,
                     LPVOID  pTarget,
                     LPVOID  pDetour,
                     LPVOID *ppOriginal )
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (WINAPI *BMF_CreateFuncHook_t)
      ( LPCWSTR pwszFuncName, LPVOID  pTarget,
        LPVOID  pDetour,      LPVOID *ppOriginal );
  static BMF_CreateFuncHook_t BMF_CreateFuncHook =
    (BMF_CreateFuncHook_t)GetProcAddress (hParent, "BMF_CreateFuncHook");

  return
    BMF_CreateFuncHook (pwszFuncName, pTarget, pDetour, ppOriginal);
}

MH_STATUS
WINAPI
UNX_CreateDLLHook ( LPCWSTR pwszModule, LPCSTR  pszProcName,
                    LPVOID  pDetour,    LPVOID *ppOriginal,
                    LPVOID *ppFuncAddr )
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (WINAPI *BMF_CreateDLLHook_t)(
        LPCWSTR pwszModule, LPCSTR  pszProcName,
        LPVOID  pDetour,    LPVOID *ppOriginal, 
        LPVOID *ppFuncAddr );
  static BMF_CreateDLLHook_t BMF_CreateDLLHook =
    (BMF_CreateDLLHook_t)GetProcAddress (hParent, "BMF_CreateDLLHook");

  return
    BMF_CreateDLLHook (pwszModule,pszProcName,pDetour,ppOriginal,ppFuncAddr);
}








MH_STATUS
WINAPI
UNX_EnableHook (LPVOID pTarget)
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (WINAPI *BMF_EnableHook_t)(LPVOID pTarget);
  static BMF_EnableHook_t BMF_EnableHook =
    (BMF_EnableHook_t)GetProcAddress (hParent, "BMF_EnableHook");

  return BMF_EnableHook (pTarget);
}

MH_STATUS
WINAPI
UNX_DisableHook (LPVOID pTarget)
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (WINAPI *BMF_DisableHook_t)(LPVOID pTarget);
  static BMF_DisableHook_t BMF_DisableHook =
    (BMF_DisableHook_t)GetProcAddress (hParent, "BMF_DisableHook");

  return BMF_DisableHook (pTarget);
}

MH_STATUS
WINAPI
UNX_RemoveHook (LPVOID pTarget)
{
  static HMODULE hParent =
    GetModuleHandle (config.system.injector.c_str ());

  typedef MH_STATUS (WINAPI *BMF_RemoveHook_t)(LPVOID pTarget);
  static BMF_RemoveHook_t BMF_RemoveHook =
    (BMF_RemoveHook_t)GetProcAddress (hParent, "BMF_RemoveHook");

  return BMF_RemoveHook (pTarget);
}

MH_STATUS
WINAPI
UNX_Init_MinHook (void)
{
  MH_STATUS status = MH_OK;

  return status;
}

MH_STATUS
WINAPI
UNX_UnInit_MinHook (void)
{
  MH_STATUS status = MH_OK;

  return status;
}