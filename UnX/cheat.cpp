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

#include "config.h"
#include "cheat.h"
#include "window.h"

#include <Windows.h>
#include <cstdint>

extern wchar_t* UNX_GetExecutableName (void);
extern LPVOID __UNX_base_img_addr;

void
unx::CheatManager::Init (void)
{
  wchar_t* pwszShortName =
    UNX_GetExecutableName ();

  if (! lstrcmpiW (pwszShortName, L"ffx.exe"))
    SetTimer (unx::window.hwnd, CHEAT_TIMER_FFX, 33, nullptr);

  //else if (! lstrcmpiW (pwszShortName, L"ffx-2.exe"))
    //SetTimer (unx::window.hwnd, CHEAT_TIMER_FFX2, 33, nullptr);
}

void
unx::CheatManager::Shutdown (void)
{
}

enum Offsets_FFX {
  OFFSET_PARTY_BASE = 0x0D32078,
  OFFSET_IN_BATTLE  = 0x1F10EA0,
  OFFSET_GAINED_AP  = 0x1F10EC4,

  OFFSET_PARTY_IS_MEMBER = 0x10
};

enum Sizes_FFX {
  SIZE_PARTY = 0x94
};

void
unx::CheatTimer_FFX (void)
{
  uint8_t* lpInParty  = (uint8_t *)__UNX_base_img_addr + OFFSET_PARTY_BASE + OFFSET_PARTY_IS_MEMBER;
  uint8_t* lpInBattle = (uint8_t *)__UNX_base_img_addr + OFFSET_IN_BATTLE;
  uint8_t* lpGainedAp = (uint8_t *)__UNX_base_img_addr + OFFSET_GAINED_AP;

  DWORD dwProtect;

  if (config.cheat.ffx.entire_party_earns_ap) {
    VirtualProtect (lpInBattle, 8, PAGE_READWRITE, &dwProtect);

      for (int i = 0; i < 8; i++) {
        if (*(lpInParty + (i * SIZE_PARTY)))
          lpInBattle [i] = 1;
      }

    VirtualProtect (lpInBattle, 8, dwProtect, &dwProtect);

    VirtualProtect (lpGainedAp, 8, PAGE_READWRITE, &dwProtect);

      memset (lpGainedAp, 1, 8);

    VirtualProtect (lpGainedAp, 8, dwProtect, &dwProtect);
  }
}

void
unx::CheatTimer_FFX2 (void)
{
}