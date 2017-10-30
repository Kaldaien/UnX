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

#define NOMINMAX

#include "config.h"
#include "parameter.h"
#include "ini.h"
#include "log.h"
#include "language.h"
#include "cheat.h"
#include "input.h"

#include "DLL_VERSION.H"

#include <string>
#include <algorithm>

#include <atlbase.h>

static
  iSK_INI*
             dll_ini         = nullptr;
static
  iSK_INI*
             language_ini    = nullptr;

static
  iSK_INI*
             booster_ini     = nullptr;

std::wstring UNX_VER_STR = UNX_VERSION_STR_W;
unx_config_s config;

typedef void (__stdcall *SK_PlugIn_ControlPanelWidget_pfn)(void);

typedef void (WINAPI *SK_DXGI_SetPreferredAdapter_pfn) (int);

SK_DXGI_SetPreferredAdapter_pfn SK_DXGI_SetPreferredAdapter = nullptr;

typedef void (WINAPI *SK_D3D11_SetResourceRoot_pfn)      (const wchar_t*);
typedef void (WINAPI *SK_D3D11_EnableTexDump_pfn)        (bool);
typedef void (WINAPI *SK_D3D11_EnableTexInjectFFX_pfn)   (bool);
typedef void (WINAPI *SK_D3D11_EnableTexCache_pfn)       (bool);
typedef void (WINAPI *SK_D3D11_AddTexHash_pfn)           (const wchar_t*, uint32_t, uint32_t);
typedef void (WINAPI *SK_D3D11_RemoveTexHash_pfn)        (uint32_t);
typedef void (WINAPI *SK_D3D11_PopulateResourceList_pfn) (void);

typedef void (WINAPI *SKX_D3D11_MarkTextures_pfn)      (bool,bool,bool);
typedef void (WINAPI *SKX_D3D11_EnableFullscreen_pfn)  (bool);

void __stdcall UNX_ControlPanelWidget (void);
SK_PlugIn_ControlPanelWidget_pfn SK_PlugIn_ControlPanelWidget_Original = nullptr;

SK_D3D11_SetResourceRoot_pfn      SK_D3D11_SetResourceRoot      = nullptr;
SK_D3D11_EnableTexDump_pfn        SK_D3D11_EnableTexDump        = nullptr;
SK_D3D11_EnableTexInjectFFX_pfn   SK_D3D11_EnableTexInject      = nullptr;
SK_D3D11_EnableTexCache_pfn       SK_D3D11_EnableTexCache       = nullptr;
SK_D3D11_AddTexHash_pfn           SK_D3D11_AddTexHash           = nullptr;
SK_D3D11_RemoveTexHash_pfn        SK_D3D11_RemoveTexHash        = nullptr;
SK_D3D11_PopulateResourceList_pfn SK_D3D11_PopulateResourceList = nullptr;

SKX_D3D11_MarkTextures_pfn      SKX_D3D11_MarkTextures     = nullptr;
SKX_D3D11_EnableFullscreen_pfn  SKX_D3D11_EnableFullscreen = nullptr;

extern wchar_t* UNX_GetExecutableName (void);

__declspec (dllimport) void __stdcall SK_ImGui_KeybindDialog (SK_Keybind* keybind);

std::string
UNX_WideCharToUTF8 (std::wstring in)
{
  int len = WideCharToMultiByte ( CP_UTF8, 0x00, in.c_str (), -1, nullptr, 0, nullptr, FALSE );

  std::string out;
              out.resize (len);

  WideCharToMultiByte           ( CP_UTF8, 0x00, in.c_str (), static_cast <int> (in.length ()), const_cast <char *> (out.data ()), len, nullptr, FALSE );

  return out;
}

struct {
  unx::ParameterBool*    bypass_intel;
} render;

struct {
} compatibility;

struct {
  struct {
    unx::ParameterBool*  entire_party_earns_ap;
    unx::ParameterBool*  playable_seymour;
    unx::ParameterBool*  permanent_sensor;
    unx::ParameterFloat* max_speed;
    unx::ParameterFloat* speed_step;
    unx::ParameterFloat* skip_dialog;
  } ffx;
} booster;

struct {
  unx::ParameterBool*    disable_dpi_scaling;
} display;

struct {
} colors;

struct {  
  unx::ParameterStringW* resource_root;
  unx::ParameterStringW* gamepad;
  unx::ParameterStringW* gamepad_hash_ffx;
  unx::ParameterStringW* gamepad_hash_ffx2;
  unx::ParameterBool*    dump;
  unx::ParameterBool*    inject;
  unx::ParameterBool*    mark;
  unx::ParameterBool*    cache;
} textures;

struct {
  unx::ParameterStringW* voice;
  unx::ParameterStringW* sfx;
  unx::ParameterStringW* video;
  unx::ParameterStringW* timing;
} language;

struct {
  unx::ParameterBool*    remap_dinput8;
  unx::ParameterInt*     gamepad_slot;
  unx::ParameterBool*    fix_bg_input;

  unx::ParameterBool*    block_left_alt;
  unx::ParameterBool*    block_left_ctrl;
  unx::ParameterBool*    block_windows;
  unx::ParameterBool*    block_all_keys;

  unx::ParameterBool*    four_finger_salute;

  unx::ParameterBool*    fast_exit;
  unx::ParameterBool*    trap_alt_tab;

  unx::ParameterBool*    filter_ime;
} input;

struct {
  unx::ParameterStringW* injector;
  unx::ParameterStringW* version;
} sys;


unx::ParameterFactory g_ParameterFactory;

bool
UNX_SetupLowLevelRender (void)
{
  extern HMODULE hInjectorDLL;

  SK_DXGI_SetPreferredAdapter =
    (SK_DXGI_SetPreferredAdapter_pfn)
      GetProcAddress (hInjectorDLL, "SK_DXGI_SetPreferredAdapter");

  if ( SK_DXGI_SetPreferredAdapter != nullptr )
    return true;

  return false;
}

bool
UNX_SetupWindowMgmt (void)
{
  extern HMODULE hInjectorDLL;

  SKX_D3D11_EnableFullscreen =
    (SKX_D3D11_EnableFullscreen_pfn)
      GetProcAddress (hInjectorDLL, "SKX_D3D11_EnableFullscreen");

  return (SKX_D3D11_EnableFullscreen != nullptr);
}

bool
UNX_SetupTexMgmt (void)
{
  extern HMODULE hInjectorDLL;

  SK_D3D11_SetResourceRoot =
    (SK_D3D11_SetResourceRoot_pfn)
      GetProcAddress (hInjectorDLL, "SK_D3D11_SetResourceRoot");

  SK_D3D11_EnableTexDump =
    (SK_D3D11_EnableTexDump_pfn)
      GetProcAddress (hInjectorDLL, "SK_D3D11_EnableTexDump");

  SK_D3D11_EnableTexInject =
    (SK_D3D11_EnableTexInjectFFX_pfn)
      GetProcAddress (hInjectorDLL, "SK_D3D11_EnableTexInject_FFX");

  SK_D3D11_EnableTexCache =
    (SK_D3D11_EnableTexCache_pfn)
      GetProcAddress (hInjectorDLL, "SK_D3D11_EnableTexCache");

  SK_D3D11_AddTexHash =
    (SK_D3D11_AddTexHash_pfn)
      GetProcAddress (hInjectorDLL, "SK_D3D11_AddTexHash");

  SK_D3D11_RemoveTexHash =
    (SK_D3D11_RemoveTexHash_pfn)
      GetProcAddress (hInjectorDLL, "SK_D3D11_RemoveTexHash");

  SK_D3D11_PopulateResourceList =
    (SK_D3D11_PopulateResourceList_pfn)
      GetProcAddress (hInjectorDLL, "SK_D3D11_PopulateResourceList");

  // Ignore SKX_..., these are experimental things and the software
  //   must work even if SpecialK removes the function.
  if ( SK_D3D11_SetResourceRoot      != nullptr && SK_D3D11_EnableTexDump   != nullptr &&
       SK_D3D11_EnableTexInject      != nullptr && SK_D3D11_EnableTexCache  != nullptr &&
       SK_D3D11_AddTexHash           != nullptr && SK_D3D11_RemoveTexHash   != nullptr &&
       SK_D3D11_PopulateResourceList != nullptr ) {
    return true;
  }

  return false;
}

typedef const wchar_t* (__stdcall *SK_GetConfigPath_pfn)(void);
SK_GetConfigPath_pfn SK_GetConfigPath = nullptr;

bool
UNX_LoadConfig (std::wstring name)
{
  extern HMODULE hInjectorDLL;

  SK_GetConfigPath =
    (SK_GetConfigPath_pfn)
      GetProcAddress (
        hInjectorDLL,
          "SK_GetConfigPath"
      );

  // Load INI File
  wchar_t wszFullName [ MAX_PATH + 2 ] = { L'\0' };
  wchar_t wszLanguage [ MAX_PATH + 2 ] = { L'\0' };
  wchar_t wszBooster  [ MAX_PATH + 2 ] = { L'\0' };

  lstrcatW (wszFullName, SK_GetConfigPath ());
  lstrcatW (wszFullName,       name.c_str ());
  lstrcatW (wszFullName,             L".ini");

  dll_ini = UNX_CreateINI (wszFullName);

  bool empty = dll_ini->get_sections ().empty ();

  lstrcatW (wszLanguage, SK_GetConfigPath ());
  lstrcatW (wszLanguage,       name.c_str ());
  lstrcatW (wszLanguage,    L"_Language.ini");

  language_ini = UNX_CreateINI (wszLanguage);

  lstrcatW (wszBooster, SK_GetConfigPath ());
  lstrcatW (wszBooster,       name.c_str ());
  lstrcatW (wszBooster,     L"_Booster.ini");

  booster_ini = UNX_CreateINI (wszBooster);

  //
  // Create Parameters
  //
  display.disable_dpi_scaling =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Disable DPI Scaling")
      );
  display.disable_dpi_scaling->register_to_ini (
    dll_ini,
      L"UnX.Display",
        L"DisableDPIScaling" );

  render.bypass_intel =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Bypass Intel GPUs")
      );
  render.bypass_intel->register_to_ini (
    dll_ini,
      L"UnX.Render",
        L"BypassIntel" );


  language.voice =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Voiceover Language")
      );
  language.voice->register_to_ini (
    language_ini,
      L"Language.Master",
        L"Voice" );

  language.sfx =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Sound Effect Language")
      );
  language.sfx->register_to_ini (
    language_ini,
      L"Language.Master",
        L"SoundEffects" );

  language.video =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Video Language")
      );
  language.video->register_to_ini (
    language_ini,
      L"Language.Master",
        L"Video" );



  input.remap_dinput8 =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Allow button remapping")
      );
  input.remap_dinput8->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"RemapDirectInput" );

  input.fix_bg_input =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Prevent DirectInput Nonsense in Background")
      );
  input.fix_bg_input->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"FixBackgroundInput" );

  input.block_left_alt =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Block Left Alt Key")
      );
  input.block_left_alt->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"BlockLeftAlt" );

  input.block_left_ctrl =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Block Left Ctrl Key")
      );
  input.block_left_ctrl->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"BlockLeftCtrl" );

  input.block_windows =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Block Windows Key")
      );
  input.block_windows->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"BlockWindows" );

  input.block_all_keys =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Block All Keys")
      );
  input.block_all_keys->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"BlockAllKeys" );

  input.four_finger_salute =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Classic Final Fantasy Four-Finger Salute Reinvisioned")
      );
  input.four_finger_salute->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"FourFingerSalute" );

  input.gamepad_slot =
    static_cast <unx::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Gamepad slot to test during cursor mgmt")
      );
  input.gamepad_slot->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"GamepadSlot" );

  input.fast_exit =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Exit Without Confirmation")
      );
  input.fast_exit->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"FastExit" );

  input.trap_alt_tab =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Prevent Game from Receiving Alt+Tab Info")
      );
  input.trap_alt_tab->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"TrapAltTab" );

  input.filter_ime =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Disable IME Messages, which appear to be the cause of some crashes.")
      );
  input.filter_ime->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"FilterIME" );


  textures.resource_root =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Root of all texture resources; possibly of all evil too...")
      );
  textures.resource_root->register_to_ini (
    dll_ini,
      L"UnX.Textures",
        L"ResourceRoot" );

  textures.dump =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Dump All Textures")
    );
  textures.dump->register_to_ini (
    dll_ini,
      L"UnX.Textures",
        L"Dump" );

  textures.inject =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Inject All Textures in <Res_Root>/inject/<hash>.dds")
    );
  textures.inject->register_to_ini (
    dll_ini,
      L"UnX.Textures",
        L"Inject" );

  textures.cache =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Cache all texture memory")
    );
  textures.cache->register_to_ini (
    dll_ini,
      L"UnX.Textures",
        L"Cache" );


  booster.ffx.entire_party_earns_ap =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Give everyone in the current party AP")
    );
  booster.ffx.entire_party_earns_ap->register_to_ini (
    booster_ini,
      L"Boost.FFX",
        L"EntirePartyEarnsAP" );

  booster.ffx.permanent_sensor =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Grant the Sensor ability no matter what weapon is equipped")
    );
  booster.ffx.permanent_sensor->register_to_ini (
    booster_ini,
      L"Boost.FFX",
        L"GrantPermanentSensor" );

  booster.ffx.max_speed =
    static_cast <unx::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Maximum Speed Boost")
    );
  booster.ffx.max_speed->register_to_ini (
    booster_ini,
      L"SpeedHack.FFX",
        L"MaxSpeed" );

  booster.ffx.speed_step =
    static_cast <unx::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Amount to multiply speed by")
    );
  booster.ffx.speed_step->register_to_ini (
    booster_ini,
      L"SpeedHack.FFX",
        L"SpeedStep" );

  booster.ffx.skip_dialog =
    static_cast <unx::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Speed at which dialog is skipped")
    );
  booster.ffx.skip_dialog->register_to_ini (
    booster_ini,
      L"SpeedHack.FFX",
        L"SkipDialogAtSpeed" );

  booster.ffx.playable_seymour =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Make Seymour a playable character")
    );
  booster.ffx.playable_seymour->register_to_ini (
    booster_ini,
      L"Fun.FFX",
        L"PlayableSeymour" );


  sys.version =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Software Version")
      );
  sys.version->register_to_ini (
    dll_ini,
      L"UnX.System",
        L"Version" );

  sys.injector =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Injector DLL")
      );
  sys.injector->register_to_ini (
    dll_ini,
      L"UnX.System",
        L"Injector" );


  //
  // Load Parameters
  //
  display.disable_dpi_scaling->load (config.display.disable_dpi_scaling);

  if (! language.voice->load (config.language.voice))
    language.voice->set_value (config.language.voice);

  unx::ParameterStringW* voice_override =
    (unx::ParameterStringW *)
      g_ParameterFactory.create_parameter <std::wstring> (
        L"App-Specific Voice Language Preference");

  voice_override->register_to_ini (
      language_ini,
        UNX_GetExecutableName (),
          L"Voice" );

  std::wstring tmp_str;

  if (voice_override->load (tmp_str)) {
    if (tmp_str.length ())   config.language.voice = tmp_str;
  } else {
    voice_override->store (L"");
  }


  if (! language.sfx->load (config.language.sfx))
    language.sfx->set_value (config.language.sfx);


  unx::ParameterStringW* sfx_override =
    (unx::ParameterStringW *)
      g_ParameterFactory.create_parameter <std::wstring> (
        L"App-Specific SFX Language Preference");

  sfx_override->register_to_ini (
      language_ini,
        UNX_GetExecutableName (),
          L"SoundEffects" );

  if (sfx_override->load (tmp_str)) {
    if (tmp_str.length ())   config.language.sfx = tmp_str;
  } else {
    sfx_override->store (L"");
  }


  if (! language.video->load (config.language.video))
    language.video->set_value (config.language.video);

  unx::ParameterStringW* fmv_override =
    (unx::ParameterStringW *)
      g_ParameterFactory.create_parameter <std::wstring> (
        L"App-Specific FMV Language Preference");

  fmv_override->register_to_ini (
      language_ini,
        UNX_GetExecutableName (),
          L"Video" );

  if (fmv_override->load (tmp_str)) {
    if (tmp_str.length ())   config.language.video = tmp_str;
  } else {
    fmv_override->store (L"");
  }

  input.remap_dinput8->load (config.input.remap_dinput8);
  input.gamepad_slot->load  (config.input.gamepad_slot);
  input.fix_bg_input->load  (config.input.fix_bg_input);

  input.block_windows->load (config.input.block_windows);
  //input.block_left_alt->load  (config.input.block_left_alt);
  //input.block_left_ctrl->load (config.input.block_left_ctrl);
  //input.block_all_keys->load  (config.input.block_all_keys);

  input.four_finger_salute->load (config.input.four_finger_salute);

  input.fast_exit->load    (config.input.fast_exit);
  input.trap_alt_tab->load (config.input.trap_alt_tab);
  input.filter_ime->load   (config.input.filter_ime);


#if 1
  // Force BG input fix off if this is disabled
  if (! config.input.fast_exit)
    config.input.fix_bg_input = false;
#else
  // Force fast exit on if this is enabled
  if (config.input.fix_bg_input)
    config.input.fast_exit = true;
#endif


  extern wchar_t* UNX_GetExecutableName (void);
  if (StrStrIW (UNX_GetExecutableName (), L"ffx.exe"))
  {
    booster.ffx.entire_party_earns_ap->load (config.cheat.ffx.entire_party_earns_ap);
    booster.ffx.permanent_sensor->load      (config.cheat.ffx.permanent_sensor);
    booster.ffx.playable_seymour->load      (config.cheat.ffx.playable_seymour);

    booster.ffx.max_speed->load   (config.cheat.ffx.max_speed);
    booster.ffx.speed_step->load  (config.cheat.ffx.speed_step);
    booster.ffx.skip_dialog->load (config.cheat.ffx.skip_dialog);
} else {
    config.cheat.ffx.speed_step = 0.0f;
    config.cheat.ffx.max_speed  = 1.0f;
  }

  sys.version->load  (config.system.version);
  //sys.injector->load (config.system.injector);

  render.bypass_intel->load (config.render.bypass_intel);

  if ( (config.render.bypass_intel) &&
       UNX_SetupLowLevelRender () )
  {
    // Rather dumb logic that assumes the Intel GPU will be Adapter 0.
    //
    //   This isn't dangerous, my DXGI DLL prevents overriding to select
    //     either Intel or Microsoft adapters.
    SK_DXGI_SetPreferredAdapter (config.render.bypass_intel ? 1 : -1);
  }

  if (UNX_SetupTexMgmt ())
  {
    textures.resource_root->load (config.textures.resource_root);
    textures.dump->load          (config.textures.dump);
    textures.inject->load        (config.textures.inject);
    textures.cache->load         (config.textures.cache);

    SK_D3D11_SetResourceRoot (config.textures.resource_root.c_str ());
    SK_D3D11_EnableTexDump   (config.textures.dump);
    SK_D3D11_EnableTexInject (config.textures.inject);
    SK_D3D11_EnableTexCache  (config.textures.cache);

    if (config.textures.inject)
      SK_D3D11_AddTexHash (L"Title.dds", 0xA4FFC068, 0x00);

    if (config.textures.inject || config.textures.dump)
      SK_D3D11_PopulateResourceList ();
  }

  UNX_SaveConfig (name);

  if (empty)
    return false;

  return true;
}

#include <imgui/imgui.h>
#pragma comment (lib, "F:\\SteamLibrary\\steamapps\\common\\FINAL FANTASY FFX&FFX-2 HD Remaster\\dxgi.lib")

void
UNX_SaveConfig (std::wstring name, bool close_config)
{
  input.remap_dinput8->store        (config.input.remap_dinput8);

  //input.block_windows->store        (config.input.block_windows);
  input.fix_bg_input->store         (config.input.fix_bg_input);

  input.trap_alt_tab->store         (config.input.trap_alt_tab);
  input.filter_ime->store           (config.input.filter_ime);



  ((unx::iParameter *)language.sfx)->set_value_str (config.language.sfx);
  ((unx::iParameter *)language.sfx)->store         (                   );

  ((unx::iParameter *)language.voice)->set_value_str (config.language.voice);
  ((unx::iParameter *)language.voice)->store         (                     );

  ((unx::iParameter *)language.video)->set_value_str (config.language.video);
  ((unx::iParameter *)language.video)->store         (                     );

  extern wchar_t* UNX_GetExecutableName (void);
  if (StrStrIW (UNX_GetExecutableName (), L"ffx.exe"))
  {
    booster.ffx.entire_party_earns_ap->store (config.cheat.ffx.entire_party_earns_ap);
    booster.ffx.permanent_sensor->store      (config.cheat.ffx.permanent_sensor);
    booster.ffx.playable_seymour->store      (config.cheat.ffx.playable_seymour);
    booster.ffx.max_speed->store             (config.cheat.ffx.max_speed);
    booster.ffx.speed_step->store            (config.cheat.ffx.speed_step);
    booster.ffx.skip_dialog->store           (config.cheat.ffx.skip_dialog);
  }

  sys.version->store      (UNX_VER_STR);
  sys.injector->store     (config.system.injector);

  wchar_t wszFullName [ MAX_PATH + 2 ] = { L'\0' };
  wchar_t wszLanguage [ MAX_PATH + 2 ] = { L'\0' };
  wchar_t wszBooster  [ MAX_PATH + 2 ] = { L'\0' };

  lstrcatW (wszFullName, SK_GetConfigPath ());
  lstrcatW (wszFullName,       name.c_str ());
  lstrcatW (wszFullName,             L".ini");

  dll_ini->write (wszFullName);

  lstrcatW (wszLanguage, SK_GetConfigPath ());
  lstrcatW (wszLanguage,       name.c_str ());
  lstrcatW (wszLanguage,    L"_Language.ini");

  language_ini->write (wszLanguage);

  lstrcatW (wszBooster, SK_GetConfigPath ());
  lstrcatW (wszBooster,       name.c_str ());
  lstrcatW (wszBooster,     L"_Booster.ini");

  booster_ini->write (wszBooster);

  if (close_config)
  {
    // Actually, this is somewhat invalid -- DO NOT FREE MEMORY THAT WAS
    //                                       ALLOCATED IN A DIFFERENT DLL
    //                             (unless compiled under the EXACT same environment)!
    if (dll_ini != nullptr) {
      //delete dll_ini;
      dll_ini = nullptr;
    }

    if (language_ini != nullptr) {
      //delete language_ini;
      language_ini = nullptr;
    }

    if (booster_ini != nullptr) {
      //delete booster_ini;
      booster_ini = nullptr;
    }
  }
}

unx::ParameterStringW* unx_speedstep;
unx::ParameterStringW* unx_kickstart;
unx::ParameterStringW* unx_timestop;
unx::ParameterStringW* unx_freelook;
unx::ParameterStringW* unx_sensor;
unx::ParameterStringW* unx_fullap;
unx::ParameterStringW* unx_VSYNC;
unx::ParameterStringW* unx_soft_reset;

#include <d3d11.h>

typedef enum D3DX11_IMAGE_FILE_FORMAT {
  D3DX11_IFF_BMP          = 0,
  D3DX11_IFF_JPG          = 1,
  D3DX11_IFF_PNG          = 3,
  D3DX11_IFF_DDS          = 4,
  D3DX11_IFF_TIFF         = 10,
  D3DX11_IFF_GIF          = 11,
  D3DX11_IFF_WMP          = 12,
  D3DX11_IFF_FORCE_DWORD  = 0x7fffffff
} D3DX11_IMAGE_FILE_FORMAT, *LPD3DX11_IMAGE_FILE_FORMAT;

typedef struct D3DX11_IMAGE_INFO {
  UINT                     Width;
  UINT                     Height;
  UINT                     Depth;
  UINT                     ArraySize;
  UINT                     MipLevels;
  UINT                     MiscFlags;
  DXGI_FORMAT              Format;
  D3D11_RESOURCE_DIMENSION ResourceDimension;
  D3DX11_IMAGE_FILE_FORMAT ImageFileFormat;
} D3DX11_IMAGE_INFO, *LPD3DX11_IMAGE_INFO;


typedef struct D3DX11_IMAGE_LOAD_INFO {
  UINT              Width;
  UINT              Height;
  UINT              Depth;
  UINT              FirstMipLevel;
  UINT              MipLevels;
  D3D11_USAGE       Usage;
  UINT              BindFlags;
  UINT              CpuAccessFlags;
  UINT              MiscFlags;
  DXGI_FORMAT       Format;
  UINT              Filter;
  UINT              MipFilter;
  D3DX11_IMAGE_INFO *pSrcInfo;
} D3DX11_IMAGE_LOAD_INFO, *LPD3DX11_IMAGE_LOAD_INFO;

interface ID3DX11ThreadPump;

typedef HRESULT (WINAPI *D3DX11CreateTextureFromFileW_pfn)(
  _In_  ID3D11Device           *pDevice,
  _In_  LPCWSTR                pSrcFile,
  _In_  D3DX11_IMAGE_LOAD_INFO *pLoadInfo,
  _In_  IUnknown               *pPump,
  _Out_ ID3D11Resource         **ppTexture,
  _Out_ HRESULT                *pHResult
);

interface ID3DX11ThreadPump;

typedef HRESULT (WINAPI *D3DX11GetImageInfoFromFileW_pfn)(
  _In_  LPCWSTR           pSrcFile,
  _In_  ID3DX11ThreadPump *pPump,
  _In_  D3DX11_IMAGE_INFO *pSrcInfo,
  _Out_ HRESULT           *pHResult
);

__declspec (dllimport)
IUnknown* __stdcall SK_Render_GetDevice (void);

__declspec (dllimport)
IUnknown* __stdcall SK_Render_GetSwapChain (void);


#include "cheat.h"
#include "hook.h"

struct ButtonRef_s {
  ID3D11Texture2D* pTex;
  std::wstring     name;
  std::string      name_utf8; // Lazy optimization
};

static std::vector <ButtonRef_s> gamepads;


__declspec (dllimport)
void
__stdcall
SKX_ImGui_RegisterDiscardableResource (IUnknown* pRes);

typedef void (__stdcall *SK_ImGui_ResetCallback_pfn)(void);

__declspec (dllimport)
void
__stdcall
SKX_ImGui_RegisterResource (IUnknown* pRes);

__declspec (dllimport)
void
__stdcall
SKX_ImGui_RegisterResetCallback (SK_ImGui_ResetCallback_pfn pCallback);

__declspec (dllimport)
void
__stdcall
SKX_ImGui_UnregisterResetCallback (SK_ImGui_ResetCallback_pfn pCallback);

void
__stdcall
UNX_ImGui_ResetCallback (void)
{
  gamepads.clear ();
}

__declspec (dllimport)
INT
__stdcall
SK_ImGui_GamepadComboDialog0 (SK_GamepadCombo_V0* combo);

void
__stdcall
UNX_ControlPanelWidget (void)
{
  static bool first = true;

  if (first)
  {
    SKX_ImGui_RegisterResetCallback (UNX_ImGui_ResetCallback);
    first = false;
  }


  static 
    const char* szLabel = (game_type & GAME_FFX) ? "Final Fantasy X HD Remaster" :
                                                   "Final Fantasy X-2 HD Remaster";

  if (ImGui::CollapsingHeader (szLabel, ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.40f, 0.40f, 0.45f));
    ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.45f, 0.45f, 0.80f));
    ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.53f, 0.53f, 0.80f));
    ImGui::TreePush       ("");

    if (ImGui::CollapsingHeader ("Language", ImGuiTreeNodeFlags_DefaultOpen))
    {
      ImGui::TreePush ("");

      const char* szLanguageOptions = "Game Default\0English\0Japanese\0\0";

      int voice_lang = (config.language.voice == L"us" ? 1 : config.language.voice == L"jp" ? 2 : 0);
      int sfx_lang   = (config.language.sfx   == L"us" ? 1 : config.language.sfx   == L"jp" ? 2 : 0);
      int fmv_lang   = (config.language.video == L"us" ? 1 : config.language.video == L"jp" ? 2 : 0);

      int orig_voice = voice_lang;
      int orig_sfx   = sfx_lang;
      int orig_fmv   = fmv_lang;

      ImGui::PushItemWidth (ImGui::GetWindowWidth () * 0.7f);

             bool changed_now = false;
      static bool changed     = false;

      if ( ImGui::Combo ( "Dialogue", &voice_lang,
                          szLanguageOptions ) )
      {
             if (voice_lang == 1) config.language.voice = L"us";
        else if (voice_lang == 2) config.language.voice = L"jp";
        else                      config.language.voice = L"default";
      
        if (voice_lang != orig_voice)
        {
          changed_now = true;
          unx::LanguageManager::ApplyPatch (Voice);
        }
      }

      if ( ImGui::Combo ( "Sound Effects", &sfx_lang,
                          szLanguageOptions ) )
      {
             if (sfx_lang == 1) config.language.sfx = L"us";
        else if (sfx_lang == 2) config.language.sfx = L"jp";
        else                    config.language.sfx = L"default";

        if (sfx_lang != orig_sfx)
        {
          changed_now = true;
          unx::LanguageManager::ApplyPatch (SoundEffect);
        }
      }

      if ( ImGui::Combo ( "Full Motion Video", &fmv_lang,
                          szLanguageOptions ) )
      {
             if (fmv_lang == 1) config.language.video = L"us";
        else if (fmv_lang == 2) config.language.video = L"jp";
        else                    config.language.video = L"default";
      
        if (fmv_lang != orig_fmv)
        {
          changed_now = true;
          unx::LanguageManager::ApplyPatch (Video);
        }
      }

      ImGui::PopItemWidth (  );

      if (changed_now)
      {
        UNX_SaveConfig ();
        changed = true;
      }

      if (changed)
      {
        ImGui::PushStyleColor (ImGuiCol_Text, ImColor::HSV (0.4f, 1.0f, 0.94f));
        ImGui::BulletText     ("Game Restart Required");
        ImGui::PopStyleColor  ();
      }

      ImGui::TreePop      (  );
    }

    if (ImGui::CollapsingHeader ("Key Bindings"))
    {
      auto Keybinding = [](SK_Keybind* binding, unx::ParameterStringW* param) ->
      auto
      {
          std::string label  = "##UnX_Keybind_";
                      label += binding->bind_name;

          ImGui::PushID   ((int)&binding);
          bool selected =
            ImGui::Selectable (label.c_str (), false);
          ImGui::PopID    ();

        ImGui::SameLine   ();
        ImGui::Text       (UNX_WideCharToUTF8 (binding->human_readable).c_str ());

        if (selected)
        {
          ImGui::OpenPopup (binding->bind_name);
        }

        std::wstring original_binding = binding->human_readable;

        SK_ImGui_KeybindDialog (binding);

        if (original_binding != binding->human_readable)
        {
          param->store   (binding->human_readable);

          extern iSK_INI* pad_cfg;

          pad_cfg->write (pad_cfg->get_filename ());

          return true;
        }

        return false;
      };

      ImGui::TreePush   ("");
      ImGui::BeginGroup (  );
        ImGui::Text     ("Toggle VSYNC");
        ImGui::Text     ("Kickstart (Fix Stuck Loading)");

        if (game_type & GAME_FFX)
        {
          ImGui::Text   ("Speed Boost");
          ImGui::Text   ("Toggle Time Stop");
          ImGui::Text   ("Toggle Freelook");
          ImGui::Text   ("Toggle Permanent Sensor");
          ImGui::Text   ("Toggle Full Party AP");
          ImGui::Text   ("Soft Reset");
        }
      ImGui::EndGroup   (  );
      ImGui::SameLine   (  );

      ImGui::BeginGroup (  );
        Keybinding        (&keybinds.VSYNC,     unx_VSYNC);
        Keybinding        (&keybinds.KickStart, unx_kickstart);

        if (game_type & GAME_FFX)
        {
          Keybinding      (&keybinds.SpeedStep, unx_speedstep);
          Keybinding      (&keybinds.TimeStop,  unx_timestop);
          Keybinding      (&keybinds.FreeLook,  unx_freelook);
          Keybinding      (&keybinds.Sensor,    unx_sensor);
          Keybinding      (&keybinds.FullAP,    unx_fullap);
          Keybinding      (&keybinds.SoftReset, unx_soft_reset);
        }
      ImGui::EndGroup   (  );
      ImGui::TreePop    (  );
    }


    if (ImGui::CollapsingHeader ("Gamepad Config"))
    {
      ImGui::TreePush ("");

      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));

      bool gamepad_binding = ImGui::CollapsingHeader ("Gamepad Bindings");

      if (ImGui::IsItemHovered ())
        ImGui::SetTooltip ("Press and hold all buttons in your combo for half a second.");

      if (gamepad_binding)
      {
        bool ps_map = StrStrIW (gamepad.tex_set.c_str (), L"PlayStation") ||
                      StrStrIW (gamepad.tex_set.c_str (), L"PS");

        auto GamepadCombo = [&](UNX_GamepadCombo* combo) ->
        auto
        {
          std::string label  = "###";
                      label += combo->combo_name;

          ImGui::PushID   ((int)&combo);
          bool selected =
            ImGui::Selectable (label.c_str (), false);
          ImGui::PopID    ();
          ImGui::SameLine ();

          ImGui::Text     (UNX_WideCharToUTF8 (combo->unparsed).c_str ());

          if (selected)
          {
            combo->button_names =
              ps_map ? gamepad.names.PlayStation :
                       gamepad.names.Xbox;
          
            ImGui::OpenPopup (combo->combo_name.c_str ());
          }

          if (SK_ImGui_GamepadComboDialog0 (combo))
          {
            extern void UNX_SetupSpecialButtons (void);
            UNX_SetupSpecialButtons ();

            ((unx::ParameterStringW *)combo->config_parameter)->store (combo->unparsed);

            extern iSK_INI* pad_cfg;
            
            pad_cfg->write (pad_cfg->get_filename ());
          
            return true;
          }

          return false;
        };

        ImGui::TreePush   ("");
        ImGui::BeginGroup (  );
          ImGui::Text     ("F1");
          ImGui::Text     ("F2");
          ImGui::Text     ("F3");
          ImGui::Text     ("F4");
          ImGui::Text     ("F5");
          ImGui::Text     ("Screenshot");
          ImGui::Text     ("Fullscreen");
          ImGui::Text     ("Escape");
          ImGui::Text     ("Kickstart");

          if (game_type & GAME_FFX)
          {
            ImGui::Text   ("Speed Boost");
            ImGui::Text   ("Soft Reset");
          }
        ImGui::EndGroup   (  );
        ImGui::SameLine   (  );

        ImGui::BeginGroup (  );
        GamepadCombo (&gamepad.f1);
        GamepadCombo (&gamepad.f2);
        GamepadCombo (&gamepad.f3);
        GamepadCombo (&gamepad.f4);
        GamepadCombo (&gamepad.f5);
        GamepadCombo (&gamepad.screenshot);
        GamepadCombo (&gamepad.fullscreen);
        GamepadCombo (&gamepad.esc);
        GamepadCombo (&gamepad.kickstart);

        if (game_type & GAME_FFX)
        {
          GamepadCombo (&gamepad.speedboost);
          GamepadCombo (&gamepad.softreset);
          if (ImGui::IsItemHovered () || ImGui::IsItemFocused ())
            ImGui::SetTooltip ("It is STRONGLY recommended that you do not change this gamepad combo!");
        }
        ImGui::EndGroup   (  );
        ImGui::TreePop    (  );
      }

      if (ImGui::CollapsingHeader ("Gamepad Button Icons"))
      {
        static int  max_width = 0;
        static bool changed   = false;

        ImGui::TreePush ("");

        if (gamepads.empty ())
        {
          WIN32_FIND_DATA fd     = {  };
          HANDLE          hFind  = INVALID_HANDLE_VALUE;

          std::wstring resource_dir =
            config.textures.resource_root;
          resource_dir += L"\\gamepads\\*";

          hFind =
            FindFirstFileW (resource_dir.c_str (), &fd);

          static D3DX11GetImageInfoFromFileW_pfn
            D3DX11GetImageInfoFromFileW = nullptr;

          static D3DX11CreateTextureFromFileW_pfn
            D3DX11CreateTextureFromFileW = nullptr;

          if (D3DX11CreateTextureFromFileW == nullptr)
          {
            D3DX11CreateTextureFromFileW =
              (D3DX11CreateTextureFromFileW_pfn)
                GetProcAddress (GetModuleHandle (L"d3dx11_43.dll"), "D3DX11CreateTextureFromFileW");
          }

          if (D3DX11GetImageInfoFromFileW == nullptr)
          {
            D3DX11GetImageInfoFromFileW =
              (D3DX11GetImageInfoFromFileW_pfn)
                GetProcAddress (GetModuleHandle (L"d3dx11_43.dll"), "D3DX11GetImageInfoFromFileW");
          }

          if (hFind != INVALID_HANDLE_VALUE)
          {
            do
            {
              wchar_t wszGamepadTex [MAX_PATH * 2] = { };

              lstrcatW (wszGamepadTex, config.textures.resource_root.c_str ());
              lstrcatW (wszGamepadTex, L"\\gamepads\\");
              lstrcatW (wszGamepadTex, fd.cFileName);
              lstrcatW (wszGamepadTex, L"\\ButtonMap.dds");

              if (GetFileAttributes (wszGamepadTex) != INVALID_FILE_ATTRIBUTES)
              {
                ID3D11Texture2D* pTexture2D = nullptr;

                D3DX11_IMAGE_INFO      img_info   = {   };
                D3DX11_IMAGE_LOAD_INFO load_info  = {   };

                if ( D3DX11GetImageInfoFromFileW && SUCCEEDED (
                       D3DX11GetImageInfoFromFileW ( wszGamepadTex,
                                                      nullptr,
                                                       &img_info,
                                                        nullptr )
                               )
                    )
                {
                  load_info.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
                  load_info.CpuAccessFlags = 0;
                  load_info.Depth          = img_info.Depth;
                  load_info.Filter         = (UINT)-1;
                  load_info.FirstMipLevel  = 0;
                  load_info.Format         = img_info.Format;
                  load_info.Height         = img_info.Height;
                  load_info.MipFilter      = (UINT)-1;
                  load_info.MipLevels      = img_info.MipLevels;
                  load_info.MiscFlags      = img_info.MiscFlags;
                  load_info.pSrcInfo       = &img_info;
                  load_info.Usage          = D3D11_USAGE_IMMUTABLE;
                  load_info.Width          = img_info.Width;

                  if ( D3DX11CreateTextureFromFileW && SUCCEEDED ( D3DX11CreateTextureFromFileW (
                     reinterpret_cast <ID3D11Device *> (SK_Render_GetDevice ()), wszGamepadTex,
                                       &load_info, nullptr,
                       (ID3D11Resource **)&pTexture2D, nullptr )
                                 )
                     )
                  {
                    int width =
                      (int)ImGui::CalcTextSize (UNX_WideCharToUTF8 (fd.cFileName).c_str ()).x;

                    if (width > max_width)
                      max_width = width;

                    gamepads.emplace_back ( ButtonRef_s { pTexture2D, fd.cFileName, UNX_WideCharToUTF8 (fd.cFileName) } );

                    SKX_ImGui_RegisterResource (pTexture2D);
                  }
                }
              }
            } while (FindNextFileW (hFind, &fd) != 0);

            FindClose (hFind);
          }
        }

        ID3D11Texture2D* pTex = nullptr;

        ImGui::Columns    (2);

        for ( auto it : gamepads )
        {
          extern unx::ParameterStringW* texture_set;
          extern iSK_INI*               pad_cfg;
          static std::wstring           original = texture_set->get_value ();

          bool selected =
            texture_set->get_value () == it.name;

          if (selected && (! pTex))
            pTex = it.pTex;

          if (ImGui::Selectable (it.name_utf8.c_str (), &selected))
          {
            if (selected)
            {
              gamepad.tex_set   = it.name;
              texture_set->store (it.name);
              pad_cfg->write     (pad_cfg->get_filename ());

              changed = (original != it.name);

              extern void
              UNX_SetupSpecialButtons (void);
              UNX_SetupSpecialButtons ();
            }
          }

          if (ImGui::IsItemHovered ())
          {
            pTex = it.pTex;
          }
        }

        if (changed)
        {
          ImGui::PushStyleColor (ImGuiCol_Text, ImColor::HSV (0.125f, 1.0f, 1.0f));
          ImGui::BulletText     ("Game Restart Required");
          ImGui::PopStyleColor  (                       );
        }

        ImGui::NextColumn ();

      //float width  = ImGui::GetItemRectSize ().x;
        float height = ImGui::GetItemRectSize ().y;

        if (pTex != nullptr)
        {
          D3D11_TEXTURE2D_DESC desc = { };
               pTex->GetDesc (&desc);

          ID3D11ShaderResourceView*       pSRV     = nullptr;
          D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
  
          srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
          srv_desc.Format                    = desc.Format;
          srv_desc.Texture2D.MipLevels       = 1;
          srv_desc.Texture2D.MostDetailedMip = 0;

          ID3D11Device* pDev = reinterpret_cast <ID3D11Device *> (
                                 SK_Render_GetDevice ()
                               );

          bool success = (pDev != nullptr) &&
            SUCCEEDED (   pDev->CreateShaderResourceView (pTex, &srv_desc, &pSRV) );

          if (success && pSRV != nullptr)
          {
            ImGui::Image ( pSRV,    ImVec2  ( ((float)desc.Width / (float)desc.Height) * std::max (height, (float)desc.Height),
                                                                                         std::max (height, (float)desc.Height) ),
                                       ImVec2  (0,1),             ImVec2  (1,0),
                                       ImColor (255,255,255,255), ImColor (242,242,13,255) );

            SKX_ImGui_RegisterDiscardableResource (pSRV);
          }
        }

        ImGui::NextColumn ( );
        ImGui::Columns    (1);

        ImGui::TreePop ();
      }

      ImGui::PopStyleColor (3);
      ImGui::TreePop       ( );
    }

    if (game_type & GAME_FFX)
    {
      if (ImGui::CollapsingHeader ("Game Boosters", ImGuiTreeNodeFlags_DefaultOpen))
      {
        bool dirty = false;

        ImGui::TreePush ("");

        ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.02f, 0.68f, 0.90f, 0.45f));
        ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.07f, 0.72f, 0.90f, 0.80f));
        ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.14f, 0.78f, 0.87f, 0.80f));

        if (ImGui::CollapsingHeader ("Speed Boost"))
        {
          ImGui::TreePush    ("");

          bool changed = false;

          changed |= ImGui::InputFloat  ("Step Multiplier", &config.cheat.ffx.speed_step,  0, 0, 1);
          changed |= ImGui::InputFloat  ("Speed Limit",     &config.cheat.ffx.max_speed,   0, 0, 1);
          changed |= ImGui::InputFloat  ("Dialog Skip At",  &config.cheat.ffx.skip_dialog, 0, 0, 1);

          if (changed)
          {
            dirty = true;
            UNX_UpdateSpeedLimit ();
          }

          ImGui::TreePop     (  );
        }

        if (ImGui::CollapsingHeader ("Sensor / Party AP"))
        {
          ImGui::TreePush     ("");

          if (ImGui::Checkbox ("Entire Party Earns AP", &config.cheat.ffx.entire_party_earns_ap))
          {
            dirty = true;
            UNX_TogglePartyAP (  );
            UNX_TogglePartyAP (  );
          }

          if (ImGui::Checkbox ("Grant Permanent Sensor", &config.cheat.ffx.permanent_sensor))
          {
            dirty = true;
            UNX_ToggleSensor  (  );
            UNX_ToggleSensor  (  );
          }

          ImGui::TreePop      (  );
        }

        if (ImGui::CollapsingHeader ("Misc."))
        {
          dirty = true;
          ImGui::TreePush ("");
          ImGui::Checkbox ("Seymour As Playable Character", &config.cheat.ffx.playable_seymour);
          ImGui::TreePop  (  );
        }

        ImGui::PopStyleColor (3);
        ImGui::TreePop       ( );

        if (dirty)
          UNX_SaveConfig ();
      }
    }

    ImGui::PopStyleColor (3);
    ImGui::TreePop       ( );
  }
}
