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

extern wchar_t* UNX_GetExecutableName (void);

void UNX_SetSensor (bool state);

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
      uint8_t enemies;                  // 0D2A8F8
      uint8_t party;                    // 0D2A8F9
    } invincible;

    struct {
      uint8_t enemies;                  // 0D2A8FA
      uint8_t unk3;                     // 0D2A8FB
      uint8_t camera;                   // 0D2A8FC
    } control;

    uint8_t unk4;                       // 0D2A8FD
    uint8_t unk5;                       // 0D2A8FE
    uint8_t unk6;                       // 0D2A8FF
    uint8_t unk7;                       // 0D2A900
    uint8_t unk8;                       // 0D2A901
    uint8_t unk9;                       // 0D2A902
    uint8_t unk10;                      // 0D2A903
    uint8_t unk11;                      // 0D2A904
    uint8_t unk12;                      // 0D2A905
    uint8_t unk13;                      // 0D2A906
    uint8_t unk14;                      // 0D2A907
    uint8_t unk15;                      // 0D2A908
    uint8_t unk16;                      // 0D2A909
    uint8_t unk17;                      // 0D2A90A
    uint8_t unk18;                      // 0D2A90B

    struct {
      uint8_t always_overdrive;         // 0D2A90C
      uint8_t always_critical;          // 0D2A90D
      uint8_t always_deal_1;            // 0D2A90E
      uint8_t always_deal_10000;        // 0D2A90F
      uint8_t always_deal_99999;        // 0D2A910
    } damage;

    struct {
      uint8_t always_rare_drop;         // 0D2A911
      uint8_t ap_100x;                  // 0D2A912
      uint8_t gil_100x;                 // 0D2A913
    } reward;

    uint8_t unk27;                      // 0D2A914
    uint8_t permanent_sensor;           // 0D2A915
    uint8_t unk28;                      // 0D2A916
    uint8_t unk29;                      // 0D2A917
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

struct {
  DWORD sensor   = 0;
  DWORD party_ap = 0;
  DWORD speed    = 0;
} last_changed;


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
float __UNX_speed_mod      = 1.0f;
bool  __UNX_skip_cutscenes = false;

extern LPVOID __UNX_base_img_addr;

void
UNX_FFX_AudioSkip (bool bSkip)
{
  static intptr_t pFMODSyncAddr = (intptr_t)((uint8_t *)__UNX_base_img_addr + 0x30AEC0);
  static uint8_t  orig_inst []  = { 0x55, 0x00, 0x00, 0x00 };

  static bool init = false;

  if (! init) {
    memcpy (orig_inst, (char *)pFMODSyncAddr, 3);
    init = true;
  }

  DWORD dwProtect;

  std::queue <DWORD> tids =
    UNX_SuspendAllOtherThreads ();

  VirtualProtect ((LPVOID)pFMODSyncAddr, 3, PAGE_READWRITE, &dwProtect);

  if (bSkip) {
    const uint8_t skip_inst [] = { 0xc2, 0x08, 0x00, 0x00 };
    memcpy ((void *)pFMODSyncAddr, skip_inst, 3);
    //SK_GetCommandProcessor ()->ProcessCommandFormatted ("mem t %p %s", pFMODSyncAddr, skip_inst);
  } else {
    memcpy ((void *)pFMODSyncAddr, orig_inst, 3);
    //SK_GetCommandProcessor ()->ProcessCommandFormatted ("mem t %p %s", pFMODSyncAddr, orig_inst);
  }

  __UNX_skip_cutscenes = bSkip;

  VirtualProtect ((LPVOID)pFMODSyncAddr, 3, dwProtect, &dwProtect);

  UNX_ResumeThreads (tids);
}

typedef float (__cdecl *FFX_GameTick_pfn)(float);
FFX_GameTick_pfn UNX_FFX_GameTick_Original = nullptr;

void
UNX_SpeedStep (void)
{
  last_changed.speed = timeGetTime ();

  if (__UNX_speed_mod < config.cheat.ffx.max_speed)
    __UNX_speed_mod *= config.cheat.ffx.speed_step;
  else
    __UNX_speed_mod = 1.0f;

  if (__UNX_speed_mod >= config.cheat.ffx.skip_dialog) {
    UNX_FFX_AudioSkip (true);
  } else {
    UNX_FFX_AudioSkip (false);
  }
}

float
__cdecl
UNX_FFX_GameTick (float x)
{
//  dll_log->Log ( L"[ FFXEvent ] Tick (%f)",
//s                   x );

  float tick = __UNX_speed_mod * x;

  static float last_tick = tick;

  if (isinf (tick) || isnan (tick))
    tick = last_tick;

  last_tick = tick;

  return UNX_FFX_GameTick_Original (tick);
}

LPVOID FFX_LoadLevel_Original = nullptr;

__declspec (naked)
void
UNX_LoadLevel (char* szName)
{
  __asm { pushad
          pushfd
  }

  dll_log->Log ( L"[ FFXLevel ] FFX_LoadLevel (%hs)",
                   szName );

  __asm { popfd
          popad
          jmp FFX_LoadLevel_Original }
}

void
unx::CheatManager::Init (void)
{
  wchar_t* pwszShortName =
    UNX_GetExecutableName ();

  //
  // FFX
  //
  if (! lstrcmpiW (pwszShortName, L"ffx.exe")) {
    dll_log->LogEx (true, L"[Cheat Code]  Setting up FFX Cheat Engine...");

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

    UNX_CreateFuncHook ( L"FFX_GameTick",
        (LPVOID)((intptr_t)__UNX_base_img_addr + 0x420C00),
                             UNX_FFX_GameTick,
                  (LPVOID *)&UNX_FFX_GameTick_Original );
    UNX_EnableHook ((LPVOID)((intptr_t)__UNX_base_img_addr + 0x420C00));

#if 0
    UNX_CreateFuncHook ( L"FFX_LoadLevel",
        (LPVOID)((intptr_t)__UNX_base_img_addr + 0x241F60),
                             UNX_LoadLevel,
                  (LPVOID *)&FFX_LoadLevel_Original);
    UNX_EnableHook ((LPVOID)((intptr_t)__UNX_base_img_addr + 0x241F60));
#endif

    UNX_SetSensor (config.cheat.ffx.permanent_sensor);

    SetTimer (unx::window.hwnd, CHEAT_TIMER_FFX, 33, nullptr);

    dll_log->LogEx (false, L" done!\n");
  }

  //
  // FFX-2
  //
  else if (! lstrcmpiW (pwszShortName, L"ffx-2.exe")) {
    dll_log->LogEx (true, L"[Cheat Code]  Setting up FFX-2 Cheat Engine...");

    game_type = GAME_FFX2;

    ffx2.debug_flags =
      (unx_ffx2_memory_s::debug_s *)
        ((intptr_t)__UNX_base_img_addr + ffx2.offsets.Debug);

    ffx2.party =
      (unx_ffx2_memory_s::party_s *)
        ((intptr_t)__UNX_base_img_addr + ffx2.offsets.PartyStats);

    ffx2.debug_flags->debug_output = true;

    SetTimer (unx::window.hwnd, CHEAT_TIMER_FFX2, 33, nullptr);

    dll_log->LogEx (false, L" done!\n");
  }
}

void
unx::CheatManager::Shutdown (void)
{
}

#include "log.h"

void
UNX_TogglePartyAP (void)
{
  last_changed.party_ap = timeGetTime ();

  config.cheat.ffx.entire_party_earns_ap = (! config.cheat.ffx.entire_party_earns_ap);
}

void
UNX_ToggleFreeLook (void)
{
  if (game_type != GAME_FFX)
    return;

  ffx.debug_flags->control.camera =
    (! ffx.debug_flags->control.camera);
}

void
UNX_SetSensor (bool state)
{
  if (config.cheat.ffx.permanent_sensor != state)
    last_changed.sensor = timeGetTime ();

  config.cheat.ffx.permanent_sensor = state;

  ffx.debug_flags->permanent_sensor =
    (uint8_t)config.cheat.ffx.permanent_sensor;
}

void
UNX_ToggleSensor (void)
{
  if (game_type != GAME_FFX)
    return;

  UNX_SetSensor (! config.cheat.ffx.permanent_sensor);
}

void
UNX_TimeStop (void)
{
  if (game_type != GAME_FFX)
    return;

  DWORD dwProtect;

  // Timestop actually (0x12FBB63)

  uint8_t* skip = (uint8_t *)((intptr_t)__UNX_base_img_addr + 0x12FBB63 - 0x400000);

  VirtualProtect ((LPVOID)skip, 0x1, PAGE_READWRITE, &dwProtect);
    *skip = ! (*skip);
  VirtualProtect ((LPVOID)skip, 0x1, dwProtect,      &dwProtect);

}
// Quick Load
void
UNX_Quickie (void)
{
#if 0
      DWORD     dwProtect;
#if 0
      uint32_t* skip = (uint32_t *)((intptr_t)__UNX_base_img_addr + 0x12FB7C0 - 0x400000);

      VirtualProtect ((LPVOID)skip, 4, PAGE_READWRITE, &dwProtect);
        *skip = 1;
      VirtualProtect ((LPVOID)skip, 4, dwProtect,      &dwProtect);
#else
      // Timestop actually (0x12FBB63)

      uint8_t* skip = (uint8_t *)((intptr_t)__UNX_base_img_addr + 0x12FBB60 - 0x400000);

      VirtualProtect ((LPVOID)skip, 0x13, PAGE_READWRITE, &dwProtect);
        for (int i = 0; i < 0x13; i++) {
          //*(skip+i) = ! *(skip+i);
        }
      VirtualProtect ((LPVOID)skip, 0x13, dwProtect,      &dwProtect);
#endif

  typedef int (*sub_786BC0_pfn)(void);
  sub_786BC0_pfn QuickSave = nullptr;

  QuickSave = (sub_786BC0_pfn)((intptr_t)__UNX_base_img_addr + 0x786BC0 - 0x400000);
  QuickSave ();

#else
  if (game_type != GAME_FFX)
    return;

  typedef int (__cdecl *sub_7C8650_pfn)(int);
  sub_7C8650_pfn Menu =
    (sub_7C8650_pfn)((intptr_t)__UNX_base_img_addr + 0x3C8650);
  Menu (0x6);

  extern volatile ULONG schedule_load;
  InterlockedExchange (&schedule_load, TRUE);
#endif
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

        static const uint8_t die  [] = { 0xEB, 0x1D, 0 };
              static uint8_t live [] = { 0x75, 0x1D, 0 };

        static volatile LONG first = TRUE;

        if (InterlockedCompareExchange (&first, FALSE, TRUE)) {
          // Backup the original instructions
          memcpy (live, inst, 2);
        }

        for (int i = 0; i < 8; i++) {
          ffx.party [i].vitals.current.HP = 0UL;
        }

        std::queue <DWORD> suspended_tids =
          UNX_SuspendAllOtherThreads ();
        {
          SK_GetCommandProcessor ()->ProcessCommandFormatted ("mem t 392930 %s", (const char *)die);
        }
        UNX_ResumeThreads (suspended_tids);

        Sleep (133);

        suspended_tids =
          UNX_SuspendAllOtherThreads ();
        {
          SK_GetCommandProcessor ()->ProcessCommandFormatted ("mem t 392930 %s", (const char *)live);
        }
        UNX_ResumeThreads (suspended_tids);

        return true;
      }

      extern volatile ULONG queue_death;
      InterlockedExchange (&queue_death, TRUE);

      Sleep (33);
    } break;

    case GAME_FFX2:
    {
      for (int i = 0; i < 8; i++) {
        ffx2.party [i].vitals.current.HP = 0UL;
      }

      SK_GetCommandProcessor ()->ProcessCommandLine ("mem b 9F7880 1");
      Sleep (33);
      SK_GetCommandProcessor ()->ProcessCommandLine ("mem b 9F7880 0");
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

  if (config.cheat.ffx.permanent_sensor)
    UNX_SetSensor (config.cheat.ffx.permanent_sensor);

  DWORD dwProtect;

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
    dll_log->Log ( L"[UnitTest] %lu / %lu HP :: %lu / %lu MP",
                     ffx2.party [i].vitals.current.HP,
                     ffx2.party [i].vitals.max.HP,
                       ffx2.party [i].vitals.current.MP,
                       ffx2.party [i].vitals.max.MP );
  }
}


std::string
UNX_SummarizeCheats (DWORD dwTime)
{
  std::string summary = "";

  const DWORD status_duration = 2500UL;

  switch (game_type)
  {
    case GAME_FFX:
    {
      if (last_changed.party_ap > dwTime - status_duration) {
        summary += "Full Party AP:    ";
        summary += config.cheat.ffx.entire_party_earns_ap ?
                     "ON\n" : "OFF\n";
      }

      if (last_changed.sensor > dwTime - status_duration) {
        summary += "Permanent Sensor: ";
        summary += config.cheat.ffx.permanent_sensor ?
                     "ON\n" : "OFF\n";
      }

      if (last_changed.speed > dwTime - (status_duration * 2)) {
        char szGameSpeed [64] = { '\0' };

        sprintf ( szGameSpeed, "Game Speed:       %4.1fx\n",
                    __UNX_speed_mod );
        summary += szGameSpeed;
      }

      uint8_t* skip = (uint8_t *)((intptr_t)__UNX_base_img_addr + 0x12FBB63 - 0x400000);

      if (ffx.debug_flags->control.camera || *skip || __UNX_skip_cutscenes) {
        summary += "SPECIAL MODE:     ";

        if (ffx.debug_flags->control.camera)
          summary += "(Free Look) ";

        if (*skip)
          summary += "(Timestop) ";

        if (__UNX_skip_cutscenes)
          summary += "(Cutscene Skip) ";

        summary += "\n";
      }
    } break;

    default:
      break;
  }

  return summary;
}