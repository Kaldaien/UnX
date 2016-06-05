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

#include "language.h"

#include <Windows.h>
#include "config.h"
#include "log.h"

extern void*
UNX_Scan (const uint8_t* pattern, size_t len, const uint8_t* mask);

enum asset_type_t {
  Voice,
  SoundEffect,
  Video
};

void
UNX_PatchLanguageRef (asset_type_t type, int idx, const char* jp, const char* us)
{
  const wchar_t* wszType = nullptr;

  bool jp_to_us = false;
  bool us_to_jp = false;

  switch (type)
  {
    case Voice:
      jp_to_us = config.language.voice == L"us";
      us_to_jp = config.language.voice == L"jp";
      wszType = L"Voice";
      break;

    case SoundEffect:
      jp_to_us = config.language.sfx == L"us";
      us_to_jp = config.language.sfx == L"jp";
      wszType = L"_SFX_";
      break;

    case Video:
      jp_to_us = config.language.video == L"us";
      us_to_jp = config.language.video == L"jp";
      wszType = L"_FMV_";
      break;
  }

  // Unsupported operation
  if (! (jp_to_us || us_to_jp))
    return;

  const uint8_t* sig  = jp_to_us ? (const uint8_t *)jp : (const uint8_t *)us;
  const uint8_t* src  = jp_to_us ? (const uint8_t *)us : (const uint8_t *)jp;

  void*          dst  = UNX_Scan ( sig, strlen ((const char *)sig), nullptr);

  DWORD dwOld;

  if (dst != nullptr) {
    dll_log.LogEx (true, L"[ Language ] %s%lu: %42hs ==> ", wszType, idx, dst);
    VirtualProtect (dst, strlen ((const char *)dst)+1, PAGE_READWRITE, &dwOld);
    strcpy ((char *)dst, (const char *)src);
    dll_log.LogEx (false, L"%hs\n", dst);
#if 0
    extern LPVOID __UNX_base_img_addr;

    if (idx == 4 && type == Voice) {
      uintptr_t expected = 0x00B4FAE0;
      __UNX_base_img_addr = (LPVOID)(src-expected);
    }

    dll_log.Log (L"[ Language ]   >> SRC addr: %ph :: Base addr: %ph, File addr: %ph", src, __UNX_base_img_addr, (uintptr_t)src-(uintptr_t)__UNX_base_img_addr);
#endif
  }
}

bool
UNX_PatchLanguageFFX (void)
{
  UNX_PatchLanguageRef ( Voice,
                           0,
                             "Voice/JP/ffx_jp_voice_btl.fev",
                             "Voice/US/ffx_us_voice_btl.fev" );

  UNX_PatchLanguageRef ( Voice,
                           1,
                             "Voice/JP/VoiceFevMapper.txt",
                             "Voice/US/VoiceFevMapper.txt" );

  UNX_PatchLanguageRef ( Voice,
                           2,
                             "Voice/JP/ffx_jp_voice_btl_iop_bank00.fsb",
                             "Voice/US/ffx_us_voice_btl_iop_bank00.fsb" );

  UNX_PatchLanguageRef ( Voice,
                           3,
                             "Voice/JP/",
                             "Voice/US/" );

  UNX_PatchLanguageRef ( Voice,
                           4,
                             "ffx_jp_voice01",
                             "ffx_us_voice01" );

  UNX_PatchLanguageRef ( Voice,
                           5,
                             "ffx_jp_voice270",
                             "ffx_us_voice270" );

  UNX_PatchLanguageRef ( SoundEffect,
                           0,
                             "SFX/JP/%04d.fev",
                             "SFX/US/%04d.fev" );

  UNX_PatchLanguageRef ( SoundEffect,
                           1,
                             "SFX/JP/9999.fev",
                             "SFX/US/9999.fev" );

  UNX_PatchLanguageRef ( Video,
                           0,
                             "JP/FFX_VideoList.txt",
                             "US/FFX_VideoList.txt" );

  if (config.language.video == L"us") {
    UNX_PatchLanguageRef ( Video,
                             1,
                               "Asia/FFX_VideoList.txt",
                               "US/FFX_VideoList.txt" );
  }

  if (config.language.video == L"jp") {
    UNX_PatchLanguageRef ( Video,
                             2,
                               "/MetaMenu/GameData/PS3Data/Video/JP/timestamp_JP.txt",
                               "/MetaMenu/GameData/PS3Data/Video/US/timestamp_%s.txt" );
  }

  return true;
}

bool
UNX_PatchLanguageFFX2 (void)
{
  UNX_PatchLanguageRef ( Voice,
                           0,
                             "Voice/JP/ffx2_jp_voice_btl.fev",
                             "Voice/US/ffx2_us_voice_btl.fev" );

  UNX_PatchLanguageRef ( Voice,
                           1,
                             "Voice/JP/VoiceFevMapper.txt",
                             "Voice/US/VoiceFevMapper.txt" );

  UNX_PatchLanguageRef ( Voice,
                           2,
                             "Voice/JP/ffx2_jp_voice_btl_iop_bank00.fsb",
                             "Voice/US/ffx2_us_voice_btl_iop_bank00.fsb" );

  UNX_PatchLanguageRef ( Voice,
                           3,
                             "Voice/JP/",
                             "Voice/US/" );

  UNX_PatchLanguageRef ( Voice,
                           4,
                             "ffx2_jp_voice00_2",
                             "ffx2_us_voice00_2" );

  UNX_PatchLanguageRef ( Voice,
                           5,
                             "ffx2_jp_voice02_2",
                             "ffx2_us_voice02_2" );

  UNX_PatchLanguageRef ( Voice,
                           6,
                             "ffx2_jp_voice03_2",
                             "ffx2_us_voice03_2" );

  UNX_PatchLanguageRef ( Voice,
                           7,
                             "ffx2_jp_voice04_2",
                             "ffx2_us_voice04_2" );

  UNX_PatchLanguageRef ( Voice,
                           8,
                             "ffx2_jp_voice06_1",
                             "ffx2_us_voice06_1" );

  UNX_PatchLanguageRef ( SoundEffect,
                           0,
                             "SFX/JP/%04d.fev",
                             "SFX/US/%04d.fev" );

  UNX_PatchLanguageRef ( Video,
                           0,
                             "JP/FFX_VideoList.txt",
                             "US/FFX_VideoList.txt" );

  if (config.language.video == L"us") {
    UNX_PatchLanguageRef ( Video,
                             1,
                               "Asia/FFX_VideoList.txt",
                               "US/FFX_VideoList.txt" );
  }

  if (config.language.video == L"jp") {
    UNX_PatchLanguageRef ( Video,
                             2,
                               "/MetaMenu/GameData/PSVitaData/Video/JP/timestamp_JP.txt",
                               "/MetaMenu/GameData/PSVitaData/Video/US/timestamp_%s.txt" );
  }

  return true;
}

bool
UNX_PatchLanguageFFX_Will (void)
{
  UNX_PatchLanguageRef ( Voice,
                           0,
                             "Voice/JP/ffx_jp_voice_btl.fev",
                             "Voice/US/ffx_us_voice_btl.fev" );

  UNX_PatchLanguageRef ( Voice,
                           1,
                             "Voice/JP/VoiceFevMapper.txt",
                             "Voice/US/VoiceFevMapper.txt" );

  UNX_PatchLanguageRef ( Voice,
                           2,
                             "Voice/JP/ffx_jp_voice_btl_iop_bank00.fsb",
                             "Voice/US/ffx_us_voice_btl_iop_bank00.fsb" );

  UNX_PatchLanguageRef ( Voice,
                           3,
                             "Voice/JP/",
                             "Voice/US/" );

  UNX_PatchLanguageRef ( Voice,
                           4,
                             "ffx_jp_voice01",
                             "ffx_us_voice01" );

  UNX_PatchLanguageRef ( Voice,
                           5,
                             "ffx_jp_voice270",
                             "ffx_us_voice270" );

  UNX_PatchLanguageRef ( SoundEffect,
                           0,
                             "SFX/JP/%04d.fev",
                             "SFX/US/%04d.fev" );

  UNX_PatchLanguageRef ( SoundEffect,
                           1,
                             "SFX/JP/9999.fev",
                             "SFX/US/9999.fev" );

  UNX_PatchLanguageRef ( Video,
                           0,
                             "JP/FFX_VideoList.txt",
                             "US/FFX_VideoList.txt" );

  if (config.language.video == L"us") {
    UNX_PatchLanguageRef ( Video,
                             1,
                               "Asia/FFX_VideoList.txt",
                               "US/FFX_VideoList.txt" );
  }

  UNX_PatchLanguageRef ( Video,
                           1,
                             "/MetaMenu/GameData/PS3Data/Video/JP/SideStory.webm",
                             "/MetaMenu/GameData/PS3Data/Video/US/SideStory.webm" );

  if (config.language.video == L"jp") {
    UNX_PatchLanguageRef ( Video,
                             2,
                               "/MetaMenu/GameData/PS3Data/Video/JP/timestamp_JP.txt",
                               "/MetaMenu/GameData/PS3Data/Video/US/timestamp_%s.txt" );
  }

  return true;
}

wchar_t*
UNX_GetExecutableName (void)
{
  static wchar_t*
    pwszExec = nullptr;

  wchar_t wszProcessName [MAX_PATH] = { 0 };
  DWORD   dwProcessSize = MAX_PATH;

  HANDLE hProc = GetCurrentProcess ();

  QueryFullProcessImageName (hProc, 0, wszProcessName, &dwProcessSize);

  wchar_t* pwszShortName = wszProcessName + lstrlenW (wszProcessName);

  while (  pwszShortName      >  wszProcessName &&
    *(pwszShortName - 1) != L'\\')
    --pwszShortName;

  pwszExec = wcsdup (pwszShortName);
  return pwszExec;
}

//typedef __cdecl
bool
UNX_PatchLanguage (void)
{
  wchar_t* pwszShortName = UNX_GetExecutableName ();

  if (! lstrcmpiW (pwszShortName, L"ffx.exe")) {
    return UNX_PatchLanguageFFX ();
  }

  else if (! lstrcmpiW (pwszShortName, L"ffx-2.exe"))
    return UNX_PatchLanguageFFX2 ();

  else if (! lstrcmpiW (pwszShortName, L"FFX&X-2_Will.exe"))
    return UNX_PatchLanguageFFX_Will ();

  return true;
}

void
unx::LanguageManager::Init (void)
{
  // Initialize memory addresses
  UNX_Scan ((const uint8_t *)"XYZ123", strlen ("XYZ123"), nullptr);

  UNX_PatchLanguage ();
}

void
unx::LanguageManager::Shutdown (void)
{
  return;
}


//mov     ax, word_112D67C
//retn