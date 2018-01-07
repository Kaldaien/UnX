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

#include <Windows.h>

#include "display.h"
#include "log.h"
#include "config.h"

void
UNX_DisableDPIScaling (void)
{
  DWORD   dwProcessSize = MAX_PATH;
  wchar_t wszProcessName [MAX_PATH + 2] = { };

  HANDLE hProc =
   GetCurrentProcess ();

  QueryFullProcessImageName (hProc, 0, wszProcessName, &dwProcessSize);

  DWORD dwDisposition = 0x00;
  HKEY  key           = nullptr;

  wchar_t wszKey [1024];
  lstrcpyW (wszKey, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers");

  LSTATUS status =
    RegCreateKeyExW ( HKEY_CURRENT_USER,
                        wszKey,
                          0, NULL, 0x00L,
                            KEY_READ | KEY_WRITE,
                               nullptr, &key, &dwDisposition );

  if (status == ERROR_SUCCESS && key != nullptr) {
    const wchar_t* wszKillDPI = L"~ HIGHDPIAWARE";

    RegSetValueExW (key, wszProcessName,  0, REG_SZ, (BYTE *)wszKillDPI, sizeof (wchar_t) * (lstrlenW (wszKillDPI) + 1));

    RegFlushKey (key);
    RegCloseKey (key);
  }
}

void
unx::DisplayFix::Init (void)
{
  if (config.display.disable_dpi_scaling)
    UNX_DisableDPIScaling ();
}

void
unx::DisplayFix::Shutdown (void)
{
}