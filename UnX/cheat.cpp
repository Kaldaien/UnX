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

enum unx_gametype_t {
  GAME_INVALID = 0x0,
  GAME_FFX     = 0x01,
  GAME_FFX2    = 0x02
} game_type = GAME_INVALID;

enum Offsets_FFX {
  OFFSET_PARTY_BASE = 0x0D32078,
  OFFSET_IN_BATTLE  = 0x1F10EA0,
  OFFSET_GAINED_AP  = 0x1F10EC4,

  OFFSET_PARTY_IS_MEMBER = 0x10
};

enum Sizes_FFX {
  SIZE_PARTY = 0x94
};

struct unx_debug_ffx2_s {
  uint8_t invincible_allies;  // 0x9F78B8
  uint8_t invincible_enemies; // 0x9F78B9
  uint8_t unknown [4];        // 0x9F78BA
  uint8_t control_enemies;    // 0x9F78BE
  uint8_t control_monsters;   // 0x9F78BF
  uint8_t unknown1 [5];       // 0x9F78C0
  uint8_t mp_zero;            // 0x9F78C5
  uint8_t unknown2 [5];       // 0x9F78C6
  uint8_t always_critical;    // 0x9F78CC
  uint8_t critical;           // 0x9F78CD
  uint8_t probability_100;    // 0x9F78CE
  uint8_t unknown3 [2];       // 0x9F78CF
  uint8_t damage_random;      // 0x9F78D1
  uint8_t damage_1;           // 0x9F78D2
  uint8_t damage_9999;        // 0x9F78D3
  uint8_t damage_99999;       // 0x9F78D4
  uint8_t rare_drop_100;      // 0x9F78D5
  uint8_t exp_100x;           // 0x9F78D6
  uint8_t gil_100x;           // 0x9F78D7
  uint8_t unknown4;           // 0x9F78D8
  uint8_t always_oversoul;    // 0x9F78D9
  uint8_t unknown5 [10];      // 0x9F78DA
  uint8_t first_attack;       // 0x9F78E4 (0xFF = OFF)
} *ffx2_debug_flags = nullptr;


extern wchar_t* UNX_GetExecutableName (void);
extern LPVOID __UNX_base_img_addr;

void
unx::CheatManager::Init (void)
{
  wchar_t* pwszShortName =
    UNX_GetExecutableName ();

  if (! lstrcmpiW (pwszShortName, L"ffx.exe")) {
    game_type = GAME_FFX;
    SetTimer (unx::window.hwnd, CHEAT_TIMER_FFX, 33, nullptr);
  }

  else if (! lstrcmpiW (pwszShortName, L"ffx-2.exe")) {
    game_type = GAME_FFX2;
    ffx2_debug_flags = (unx_debug_ffx2_s *)((uint8_t *)__UNX_base_img_addr + 0x9F78B8);
    //ffx2_debug_flags->always_critical = 1;
    SetTimer (unx::window.hwnd, CHEAT_TIMER_FFX2, 33, nullptr);
  }
}

void
unx::CheatManager::Shutdown (void)
{
}

#include "log.h"

struct unx_party_s {
  uint32_t HP_cur;
  uint32_t MP_cur;

  uint32_t HP_max;
  uint32_t MP_max;

  uint8_t  in_party;

  uint8_t  unknown0;
  uint8_t  unknown1;

  uint8_t  strength;
  uint8_t  defense;
  uint8_t  magic;
  uint8_t  magic_defense;
  uint8_t  agility;
  uint8_t  luck;
  uint8_t  evasion;
  uint8_t  accuracy;

  uint8_t  unknown2;

  uint8_t  overdrive_mode;
  uint8_t  overdrive_level;
  uint8_t  overdrive_max;

  uint8_t  sphere_level_banked;
  uint8_t  sphere_level_net;

  uint8_t  unknown3;

  uint32_t skill_flags;

  uint8_t  unknown_blob [108];
};

const intptr_t unx_debug_offset = 0xD2A8F8;

struct unx_debug_s {
  uint8_t unk0;
  uint8_t unk1;
  uint8_t unk2;
  uint8_t unk3;
  uint8_t free_camera;
  uint8_t unk4;
  uint8_t unk5;
  uint8_t unk6;
  uint8_t unk7;
  uint8_t unk8;
  uint8_t unk9;
  uint8_t unk10;
  uint8_t unk11;
  uint8_t unk12;
  uint8_t unk13;
  uint8_t unk14;
  uint8_t unk15;
  uint8_t unk16;
  uint8_t unk17;
  uint8_t unk18;
  uint8_t unk19;
  uint8_t unk20;
  uint8_t unk21;
  uint8_t unk22;
  uint8_t unk23;
  uint8_t unk24;
  uint8_t unk25;
  uint8_t unk26;
  uint8_t unk27;
  uint8_t permanent_sensor;
  uint8_t unk28;
  uint8_t unk29;
};

void
UNX_ToggleFreeLook (void)
{
  if (game_type != GAME_FFX)
    return;

  unx_debug_s*
    debug = (unx_debug_s *)((uint8_t *)__UNX_base_img_addr + unx_debug_offset);

  debug->free_camera = (! debug->free_camera);
}

void
UNX_ToggleSensor (void)
{
  config.cheat.ffx.permanent_sensor = (! config.cheat.ffx.permanent_sensor);
}

bool
UNX_IsInBattle (void)
{
  if (game_type != GAME_FFX)
    return false;

  unx_party_s* party =
    (unx_party_s *)((uint8_t *)__UNX_base_img_addr + OFFSET_PARTY_BASE);

  uint8_t* lpInBattle = (uint8_t *)__UNX_base_img_addr + OFFSET_IN_BATTLE;

  for (int i = 0; i < 7; i++) {
    uint8_t state = party [i].in_party;

    if (state != 0x00 && state != 0x10) {
      if (lpInBattle [i] == 1)
        return true;
    }
  }

  return false;
}

bool
UNX_KillMeNow (void)
{
  return false;

  unx_party_s* party =
    (unx_party_s *)((uint8_t *)__UNX_base_img_addr + OFFSET_PARTY_BASE);

  if (! UNX_IsInBattle ())
    return false;

  else {
    for (int i = 0; i < 8; i++) {
      party [i].HP_cur = 0UL;
    }
  }

  return true;
}

void
unx::CheatTimer_FFX (void)
{
  if (game_type != GAME_FFX)
    return;

  unx_party_s* party =
    (unx_party_s *)((uint8_t *)__UNX_base_img_addr + OFFSET_PARTY_BASE);

  uint8_t* lpInBattle = (uint8_t *)__UNX_base_img_addr + OFFSET_IN_BATTLE;
  uint8_t* lpGainedAp = (uint8_t *)__UNX_base_img_addr + OFFSET_GAINED_AP;

  DWORD dwProtect;


  unx_debug_s*
    debug = (unx_debug_s *)((uint8_t *)__UNX_base_img_addr + unx_debug_offset);

  debug->permanent_sensor = config.cheat.ffx.permanent_sensor;


  if (config.cheat.ffx.playable_seymour) {
    party [7].in_party = 0x11;
  } else {
    party [7].in_party = 0x10;
  }

  if (config.cheat.ffx.entire_party_earns_ap && UNX_IsInBattle ()) {
    VirtualProtect (lpInBattle, 8, PAGE_READWRITE, &dwProtect);

      for (int i = 0; i < 7; i++) {
        uint8_t state = party [i].in_party;

        if (state != 0x00 && state != 0x10) {
          if (lpInBattle [i] != 1)
            lpInBattle [i] = 2;
        } else {
          lpInBattle [i] = 0;
        }
      }

    VirtualProtect (lpInBattle, 8, dwProtect, &dwProtect);
    VirtualProtect (lpGainedAp, 8, PAGE_READWRITE, &dwProtect);

      for (int i = 0; i < 7; i++) {
        uint8_t state = party [i].in_party;

        if (state != 0x00 && state != 0x10)
          lpGainedAp [i] = 1;
        else
          lpGainedAp [i] = 0;
      }

    VirtualProtect (lpGainedAp, 8, dwProtect, &dwProtect);
  }
}

void
unx::CheatTimer_FFX2 (void)
{
  if (game_type != GAME_FFX2)
    return;
}