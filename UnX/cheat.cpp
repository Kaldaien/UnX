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
#include "hook.h"

#include <Windows.h>
#include <cstdint>

enum unx_gametype_t {
  GAME_INVALID = 0x0,
  GAME_FFX     = 0x01,
  GAME_FFX2    = 0x02
} game_type = GAME_INVALID;


struct unx_ffx2_memory_s {
  struct {
    enum offset_t {
      Debug      = 0x9F78B8,
      PartyStats = 0xA016C0
    };
  } offsets;

  struct debug_s {
    struct {
      uint8_t allies;           // 0x9F78B8
      uint8_t enemies;          // 0x9F78B9
    } invincible;

    uint8_t unknown [4];        // 0x9F78BA

    struct {
      uint8_t enemies;          // 0x9F78BE
      uint8_t monsters;         // 0x9F78BF
    } control;

    uint8_t unknown1 [5];       // 0x9F78C0

    uint8_t mp_zero;            // 0x9F78C5
    uint8_t unknown2 [5];       // 0x9F78C6

    uint8_t debug_output;       // 0x9F78CB

    struct {
      uint8_t always_critical;  // 0x9F78CC
      uint8_t critical;         // 0x9F78CD
      uint8_t probability_100;  // 0x9F78CE
      uint8_t unknown3 [2];     // 0x9F78CF
      uint8_t damage_random;    // 0x9F78D1
      uint8_t damage_1;         // 0x9F78D2
      uint8_t damage_9999;      // 0x9F78D3
      uint8_t damage_99999;     // 0x9F78D4
    } damage;

    struct {
      uint8_t always_drop_rare; // 0x9F78D5
      uint8_t exp_100x;         // 0x9F78D6
      uint8_t gil_100x;         // 0x9F78D7
    } reward;

    uint8_t unknown4;           // 0x9F78D8

    uint8_t always_oversoul;    // 0x9F78D9

    uint8_t unknown5 [10];      // 0x9F78DA

    uint8_t first_attack;       // 0x9F78E4 (0xFF = OFF)
  } *debug_flags = nullptr;

  struct party_s {
    uint32_t unknown0;

    struct {
      uint32_t HP;
      uint32_t MP;
      uint8_t  strength;
      uint8_t  defense;
      uint8_t  magic;
      uint8_t  magic_defense;
      uint8_t  agility;
      uint8_t  accurcy;
      uint8_t  evasion;
      uint8_t  luck;
    } modifiers;

    struct {
      uint32_t current;
      uint32_t next_level;
    } exp;

    struct {
      struct {
        uint32_t HP;
        uint32_t MP;
      } current;

      struct {
        uint32_t HP;
        uint32_t MP;
      } max;
    } vitals;

    uint8_t unknown1;

    struct {
      uint8_t strength;
      uint8_t defense;
      uint8_t magic;
      uint8_t magic_defense;
      uint8_t agility;
      uint8_t accuracy;
      uint8_t evasion;
      uint8_t luck;
    } attributes;

    uint32_t unknown_blob [18];
  } *party = nullptr;
} ffx2;



struct unx_ffx_memory_s {
  struct {
    enum offset_t {
      PartyBase  = 0x0D32060,
      InBattle   = 0x1F10EA0,
      GainedAp   = 0x1F10EC4,
      Debug      = 0x0D2A8F8,
      BattleTrio = 0x0F3F7A4
    };
  } offsets;

  struct {
    enum character_t {
      Tidus   = 0,
      Yuna    = 1,
      Auron   = 2,
      Kimahri = 3,
      Wakka   = 4,
      Lulu    = 5,
      Rikku   = 6,
      Seymour = 7
    };
  } characters;


  struct debug_s {
    struct {
      uint8_t enemies;
      uint8_t party;
    } invincible;

    struct {
      uint8_t enemies;
      uint8_t unk3;
      uint8_t camera;
    } control;

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

    struct {
      uint8_t always_overdrive;
      uint8_t always_critical;
      uint8_t always_deal_1;
      uint8_t always_deal_10000;
      uint8_t always_deal_99999;
    } damage;

    struct {
      uint8_t always_rare_drop;
      uint8_t ap_100x;
      uint8_t gil_100x;
    } reward;

    uint8_t unk27;
    uint8_t permanent_sensor;
    uint8_t unk28;
    uint8_t unk29;
  } *debug_flags = nullptr;

  struct party_s {
    struct {
      uint32_t HP;
      uint32_t MP;
      uint8_t  strength;
      uint8_t  defense;
      uint8_t  magic;
      uint8_t  magic_defense;
      uint8_t  agility;
      uint8_t  luck;
      uint8_t  evasion;
      uint8_t  accuracy;
    } base;

    struct {
      uint32_t total;
      uint32_t current;
    } AP;

    struct {
      struct {
        uint32_t HP;
        uint32_t MP;
      } current;

      struct {
        uint32_t HP;
        uint32_t MP;
      } max;
    } vitals;

    uint8_t  in_party;

    struct {
      uint8_t  weapon;
      uint8_t  armor;
    } equipment;

    struct {
      uint8_t  strength;
      uint8_t  defense;
      uint8_t  magic;
      uint8_t  magic_defense;
      uint8_t  agility;
      uint8_t  luck;
      uint8_t  evasion;
      uint8_t  accuracy;
    } current;

    uint8_t  unknown2;

    struct {
      uint8_t  mode;
      uint8_t  level;
      uint8_t  max;
    } overdrive;

    struct {
      uint8_t  banked;
      uint8_t  total;
    } sphere_level;

    uint8_t  unknown3;

    uint32_t skill_flags;

    struct {
      uint32_t battles;
      uint32_t kills;
    } record;

    uint8_t  unknown_blob [76];
  } *party = nullptr;

  struct battle_s {
    uint8_t  participation;
  } *battle = nullptr;

  struct battle_trio_s {
    uint32_t hp0;
    uint32_t hp1;
    uint8_t  unknown [136];
  } *trio = nullptr;

  struct ap_s {
    uint8_t  earn;
  } *ap = nullptr;
} ffx;


#include <windows.h>
#include <tlhelp32.h>
#include <queue>

std::queue <DWORD>
UNX_SuspendAllOtherThreads (void)
{
  std::queue <DWORD> threads;

  HANDLE hSnap =
    CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0);

  if (hSnap != INVALID_HANDLE_VALUE)
  {
    THREADENTRY32 tent;
    tent.dwSize = sizeof THREADENTRY32;

    if (Thread32First (hSnap, &tent))
    {
      do
      {
        if ( tent.dwSize >= FIELD_OFFSET (THREADENTRY32, th32OwnerProcessID) +
                                  sizeof (tent.th32OwnerProcessID) )
        {
          if ( tent.th32ThreadID       != GetCurrentThreadId  () &&
               tent.th32OwnerProcessID == GetCurrentProcessId () )
          {
            HANDLE hThread =
              OpenThread (THREAD_ALL_ACCESS, FALSE, tent.th32ThreadID);

            if (hThread != NULL)
            {
              threads.push (tent.th32ThreadID);
              SuspendThread (hThread);
              CloseHandle   (hThread);
            }
          }
        }

        tent.dwSize = sizeof (tent);
      } while (Thread32Next (hSnap, &tent));
    }

    CloseHandle (hSnap);
  }

  return threads;
}

void
UNX_ResumeThreads (std::queue <DWORD> threads)
{
  while (! threads.empty ())
  {
    DWORD tid = threads.front ();

    HANDLE hThread =
      OpenThread (THREAD_ALL_ACCESS, FALSE, tid);

    if (hThread != NULL)
    {
      ResumeThread (hThread);
      CloseHandle  (hThread);
    }

    threads.pop ();
  }
}


#include "log.h"
float __UNX_speed_mod = 1.0f;

typedef float (__cdecl *sub_820C00_pfn)(float);
sub_820C00_pfn UNX_FFX_StartEvent_Original = nullptr;

void
UNX_SpeedStep (void)
{
  if (__UNX_speed_mod < config.cheat.ffx.max_speed)
    __UNX_speed_mod *= config.cheat.ffx.step_exp;
  else
    __UNX_speed_mod = 1.0f;
}

float
__cdecl
UNX_FFX_StartEvent (float x)
{
//  dll_log.Log ( L"[ FFXEvent ] Tick (%f)",
//s                  x );

  float tick = __UNX_speed_mod * x;

  //if (isinf (tick) || isnan (tick))
    //tick = last_tick;

  //last_tick = tick;

  return UNX_FFX_StartEvent_Original (tick);
}


extern wchar_t* UNX_GetExecutableName (void);
extern LPVOID __UNX_base_img_addr;

void
unx::CheatManager::Init (void)
{
  wchar_t* pwszShortName =
    UNX_GetExecutableName ();

  //
  // FFX
  //
  if (! lstrcmpiW (pwszShortName, L"ffx.exe")) {
    game_type = GAME_FFX;

    ffx.debug_flags =
      (unx_ffx_memory_s::debug_s *)
        ((intptr_t)__UNX_base_img_addr + ffx.offsets.Debug);

    ffx.party =
      (unx_ffx_memory_s::party_s *)
        ((intptr_t)__UNX_base_img_addr + ffx.offsets.PartyBase);

    ffx.battle =
      (unx_ffx_memory_s::battle_s *)
        ((intptr_t)__UNX_base_img_addr + ffx.offsets.InBattle);

    ffx.trio =
      (unx_ffx_memory_s::battle_trio_s *)
        ((intptr_t)__UNX_base_img_addr + ffx.offsets.BattleTrio);

    ffx.ap =
      (unx_ffx_memory_s::ap_s *)
        ((intptr_t)__UNX_base_img_addr + ffx.offsets.GainedAp);

    UNX_CreateFuncHook ( L"FFX_SetEventId",
        (LPVOID)((intptr_t)__UNX_base_img_addr + 0x420C00),
                             UNX_FFX_StartEvent,
                  (LPVOID *)&UNX_FFX_StartEvent_Original );
    UNX_EnableHook ((LPVOID)((intptr_t)__UNX_base_img_addr + 0x420C00));

    SetTimer (unx::window.hwnd, CHEAT_TIMER_FFX, 33, nullptr);
  }

  //
  // FFX-2
  //
  else if (! lstrcmpiW (pwszShortName, L"ffx-2.exe")) {
    game_type = GAME_FFX2;

    ffx2.debug_flags =
      (unx_ffx2_memory_s::debug_s *)
        ((intptr_t)__UNX_base_img_addr + ffx2.offsets.Debug);

    ffx2.party =
      (unx_ffx2_memory_s::party_s *)
        ((intptr_t)__UNX_base_img_addr + ffx2.offsets.PartyStats);

    SetTimer (unx::window.hwnd, CHEAT_TIMER_FFX2, 33, nullptr);

    ffx2.debug_flags->debug_output = true;
  }
}

void
unx::CheatManager::Shutdown (void)
{
}

#include "log.h"

void
UNX_ToggleFreeLook (void)
{
  if (game_type != GAME_FFX)
    return;

  ffx.debug_flags->control.camera =
    (! ffx.debug_flags->control.camera);
}

void
UNX_ToggleSensor (void)
{
  config.cheat.ffx.permanent_sensor =
    (! config.cheat.ffx.permanent_sensor);
}

// Quick Load
void
UNX_Quickie (void)
{
  if (game_type != GAME_FFX)
    return;

  extern bool schedule_load;
  schedule_load = true;
}

bool
UNX_IsInBattle (void)
{
  if (game_type != GAME_FFX)
    return false;

  for (int i = 0; i < 7; i++) {
    uint8_t state = ffx.party [i].in_party;

    if (state != 0x00 && state != 0x10) {
      if (ffx.battle [i].participation == 1)
        return true;
    }
  }

  return false;
}

bool
UNX_KillMeNow (void)
{
  switch (game_type)
  {
    case GAME_FFX:
    {
      if (UNX_IsInBattle ()) {
        uint8_t* inst = (uint8_t*)((intptr_t)__UNX_base_img_addr + 0x392930);

        const uint8_t die  [] = { 0xEB, 0x1D };
              uint8_t live [] = { 0x75, 0x1D };

        DWORD dwProtect;

        std::queue <DWORD> suspended_tids =
          UNX_SuspendAllOtherThreads ();
        {
          VirtualProtect (inst, 2, PAGE_EXECUTE_READWRITE, &dwProtect);
          memcpy         (live, inst, 2);
          memcpy         (inst, die,  2);
        }
        UNX_ResumeThreads (suspended_tids);

        Sleep (133);

        suspended_tids =
          UNX_SuspendAllOtherThreads ();
        {
          memcpy         (inst, live, 2);
          VirtualProtect (inst, 2, dwProtect, &dwProtect);
        }
        UNX_ResumeThreads (suspended_tids);

        return true;
      }

      for (int i = 0; i < 8; i++) {
        ffx.party [i].vitals.current.HP = 0UL;
      }

      extern bool queue_death;
      queue_death = true;

      Sleep (33);
    } break;

    case GAME_FFX2:
    {
      for (int i = 0; i < 8; i++) {
        ffx2.party [i].vitals.current.HP = 0UL;
      }

      *(uint8_t *)((intptr_t)__UNX_base_img_addr+0x9F7880) = 0x1;
      Sleep (33);
      *(uint8_t *)((intptr_t)__UNX_base_img_addr+0x9F7880) = 0x0;
      Sleep (0);
    } break;
  }

  return true;
}

void
unx::CheatTimer_FFX (void)
{
  if (game_type != GAME_FFX)
    return;

  DWORD dwProtect;

  ffx.debug_flags->permanent_sensor =
    config.cheat.ffx.permanent_sensor;

  if (config.cheat.ffx.playable_seymour) {
    ffx.party [ffx.characters.Seymour].in_party = 0x11;
  } else {
    ffx.party [ffx.characters.Seymour].in_party = 0x10;
  }

  if (config.cheat.ffx.entire_party_earns_ap && UNX_IsInBattle ()) {
    VirtualProtect (&ffx.battle->participation, 8, PAGE_READWRITE, &dwProtect);

      for (int i = 0; i < 7; i++) {
        uint8_t state = ffx.party [i].in_party;

        if (state != 0x00 && state != 0x10) {
          if (ffx.battle [i].participation != 1)
            ffx.battle [i].participation = 2;
        } else {
          ffx.battle [i].participation = 0;
        }
      }

    VirtualProtect (&ffx.battle->participation, 8, dwProtect,      &dwProtect);
    VirtualProtect (&ffx.ap->earn,              8, PAGE_READWRITE, &dwProtect);

      for (int i = 0; i < 7; i++) {
        uint8_t state = ffx.party [i].in_party;

        if (state != 0x00 && state != 0x10)
          ffx.ap [i].earn = 1;
        else
          ffx.ap [i].earn = 0;
      }

    VirtualProtect (&ffx.ap->earn, 8, dwProtect, &dwProtect);
  }
}

void
unx::CheatTimer_FFX2 (void)
{
  if (game_type != GAME_FFX2)
    return;
}

void
UNX_FFX2_UnitTest (void)
{
  if (game_type != GAME_FFX2)
    return;

  //ffx2.party->exp.current != 0;
  //ffx2.party->vitals.current.HP <= ffx2.party_stats->vitals.max.HP;
  for (int i = 0; i < 3; i++) {
    dll_log.Log ( L"[UnitTest] %lu / %lu HP :: %lu / %lu MP",
                    ffx2.party [i].vitals.current.HP,
                    ffx2.party [i].vitals.max.HP,
                      ffx2.party [i].vitals.current.MP,
                      ffx2.party [i].vitals.max.MP );
  }
}