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
#ifndef __UNX__HOOK_H__
#define __UNX__HOOK_H__

#pragma comment (lib, "MinHook/lib/libMinHook.x86.lib")
#include "MinHook/include/MinHook.h"

void
UNX_DrawCommandConsole (void);

MH_STATUS
WINAPI
UNX_CreateFuncHook ( LPCWSTR pwszFuncName,
                     LPVOID  pTarget,
                     LPVOID  pDetour,
                     LPVOID *ppOriginal );

MH_STATUS
WINAPI
UNX_CreateDLLHook ( LPCWSTR pwszModule, LPCSTR  pszProcName,
                    LPVOID  pDetour,    LPVOID *ppOriginal,
                    LPVOID* ppFuncAddr = nullptr );

MH_STATUS
WINAPI
UNX_EnableHook (LPVOID pTarget);

MH_STATUS
WINAPI
UNX_DisableHook (LPVOID pTarget);

MH_STATUS
WINAPI
UNX_RemoveHook (LPVOID pTarget);

MH_STATUS
WINAPI
UNX_Init_MinHook (void);

MH_STATUS
WINAPI
UNX_UnInit_MinHook (void);

#endif /* __UNX__HOOK_H__ */