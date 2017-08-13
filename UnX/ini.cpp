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

#include <windows.h>

#include "ini.h"
iSK_INI*
__stdcall
UNX_CreateINI (const wchar_t* const wszName)
{
  extern HMODULE hInjectorDLL;

  typedef iSK_INI* (__stdcall *SK_CreateINI_pfn)(const wchar_t* const wszName);
  static SK_CreateINI_pfn SK_CreateINI = nullptr;

  if (SK_CreateINI == nullptr) {
    SK_CreateINI =
      (SK_CreateINI_pfn)
        GetProcAddress (
          hInjectorDLL,
            "SK_CreateINI"
        );
  }

  iSK_INI* pINI = SK_CreateINI (wszName);

  if (pINI != nullptr) {
    return pINI;
  } else {
    // ASSERT: WHAT THE HELL?!
    return nullptr;
  }
}