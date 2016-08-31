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

#include "config.h"
#include "parameter.h"
#include "ini.h"
#include "log.h"

#include <string>

static
  iSK_INI*
             dll_ini         = nullptr;
static
  iSK_INI*
             language_ini    = nullptr;

static
  iSK_INI*
             booster_ini     = nullptr;

std::wstring UNX_VER_STR = L"0.6.3";
unx_config_s config;

typedef bool (WINAPI *SK_DXGI_EnableFlipMode_pfn)      (bool);
typedef void (WINAPI *SK_DXGI_SetPreferredAdapter_pfn) (int);

SK_DXGI_EnableFlipMode_pfn      SK_DXGI_EnableFlipMode      = nullptr;
SK_DXGI_SetPreferredAdapter_pfn SK_DXGI_SetPreferredAdapter = nullptr;

typedef void (WINAPI *SK_D3D11_SetResourceRoot_pfn)    (std::wstring);
typedef void (WINAPI *SK_D3D11_EnableTexDump_pfn)      (bool);
typedef void (WINAPI *SK_D3D11_EnableTexInjectFFX_pfn) (bool);
typedef void (WINAPI *SK_D3D11_EnableTexCache_pfn)     (bool);
typedef void (WINAPI *SK_D3D11_AddTexHash_pfn)         (std::wstring, uint32_t, uint32_t);
typedef void (WINAPI *SK_D3D11_RemoveTexHash_pfn)      (uint32_t);

typedef void (WINAPI *SKX_D3D11_MarkTextures_pfn)      (bool,bool,bool);
typedef void (WINAPI *SKX_D3D11_EnableFullscreen_pfn)  (bool);

SK_D3D11_SetResourceRoot_pfn    SK_D3D11_SetResourceRoot   = nullptr;
SK_D3D11_EnableTexDump_pfn      SK_D3D11_EnableTexDump     = nullptr;
SK_D3D11_EnableTexInjectFFX_pfn SK_D3D11_EnableTexInject   = nullptr;
SK_D3D11_EnableTexCache_pfn     SK_D3D11_EnableTexCache    = nullptr;
SK_D3D11_AddTexHash_pfn         SK_D3D11_AddTexHash        = nullptr;
SK_D3D11_RemoveTexHash_pfn      SK_D3D11_RemoveTexHash     = nullptr;

SKX_D3D11_MarkTextures_pfn      SKX_D3D11_MarkTextures     = nullptr;
SKX_D3D11_EnableFullscreen_pfn  SKX_D3D11_EnableFullscreen = nullptr;

extern wchar_t* UNX_GetExecutableName (void);

struct {
  unx::ParameterBool*    bypass_intel;
  unx::ParameterBool*    flip_mode;
} render;

struct {
  unx::ParameterBool*    mute_in_background;
} audio;

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
  unx::ParameterBool*    center;
} window;

struct {
  unx::ParameterBool*    disable_dpi_scaling;
  unx::ParameterBool*    enable_fullscreen;
} display;

struct {
  unx::ParameterBool*    reduce;
} stutter;

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

  unx::ParameterBool*    manage_cursor;
  unx::ParameterFloat*   cursor_timeout;
  unx::ParameterBool*    activate_on_kbd;

  unx::ParameterBool*    fast_exit;
} input;

struct {
  unx::ParameterStringW* injector;
  unx::ParameterStringW* version;
} sys;


unx::ParameterFactory g_ParameterFactory;

bool
UNX_SetupLowLevelRender (void)
{
  HMODULE hInjector =
    LoadLibrary (config.system.injector.c_str ());

  SK_DXGI_EnableFlipMode =
    (SK_DXGI_EnableFlipMode_pfn)
      GetProcAddress (hInjector, "SK_DXGI_EnableFlipMode");

  SK_DXGI_SetPreferredAdapter =
    (SK_DXGI_SetPreferredAdapter_pfn)
      GetProcAddress (hInjector, "SK_DXGI_SetPreferredAdapter");

  if ( SK_DXGI_EnableFlipMode      != nullptr &&
       SK_DXGI_SetPreferredAdapter != nullptr )
    return true;

  return false;
}

bool
UNX_SetupWindowMgmt (void)
{
  HMODULE hInjector =
    LoadLibrary (config.system.injector.c_str ());

  SKX_D3D11_EnableFullscreen =
    (SKX_D3D11_EnableFullscreen_pfn)
      GetProcAddress (hInjector, "SKX_D3D11_EnableFullscreen");

  return (SKX_D3D11_EnableFullscreen != nullptr);
}

bool
UNX_SetupTexMgmt (void)
{
  HMODULE hInjector =
    LoadLibrary (config.system.injector.c_str ());

  SK_D3D11_SetResourceRoot =
    (SK_D3D11_SetResourceRoot_pfn)
      GetProcAddress (hInjector, "SK_D3D11_SetResourceRoot");

  SK_D3D11_EnableTexDump =
    (SK_D3D11_EnableTexDump_pfn)
      GetProcAddress (hInjector, "SK_D3D11_EnableTexDump");

  SK_D3D11_EnableTexInject =
    (SK_D3D11_EnableTexInjectFFX_pfn)
      GetProcAddress (hInjector, "SK_D3D11_EnableTexInject_FFX");

  SK_D3D11_EnableTexCache =
    (SK_D3D11_EnableTexCache_pfn)
      GetProcAddress (hInjector, "SK_D3D11_EnableTexCache");

  SK_D3D11_AddTexHash =
    (SK_D3D11_AddTexHash_pfn)
      GetProcAddress (hInjector, "SK_D3D11_AddTexHash");

  SK_D3D11_RemoveTexHash =
    (SK_D3D11_RemoveTexHash_pfn)
      GetProcAddress (hInjector, "SK_D3D11_RemoveTexHash");

  SKX_D3D11_MarkTextures =
    (SKX_D3D11_MarkTextures_pfn)
      GetProcAddress (hInjector, "SKX_D3D11_MarkTextures");

  // Ignore SKX_..., these are experimental things and the software
  //   must work even if SpecialK removes the function.
  if ( SK_D3D11_SetResourceRoot != nullptr && SK_D3D11_EnableTexDump   != nullptr &&
       SK_D3D11_EnableTexInject != nullptr && SK_D3D11_EnableTexCache  != nullptr &&
       SK_D3D11_AddTexHash      != nullptr && SK_D3D11_RemoveTexHash   != nullptr ) {
    return true;
  }

  return false;
}

typedef std::wstring (__stdcall *SK_GetConfigPath_pfn)(void);
SK_GetConfigPath_pfn SK_GetConfigPath = nullptr;

bool
UNX_LoadConfig (std::wstring name)
{
  SK_GetConfigPath =
    (SK_GetConfigPath_pfn)
      GetProcAddress (
        GetModuleHandle ( L"dxgi.dll" ),
          "SK_GetConfigPath"
      );

  // Load INI File
  std::wstring full_name = SK_GetConfigPath () + name + L".ini";
  dll_ini = UNX_CreateINI ((wchar_t *)full_name.c_str ());

  bool empty = dll_ini->get_sections ().empty ();

  std::wstring language_file = SK_GetConfigPath () + name + L"_Language.ini";
  language_ini = UNX_CreateINI ((wchar_t *)language_file.c_str ());

  std::wstring booster_file = SK_GetConfigPath () + name + L"_Booster.ini";
  booster_ini = UNX_CreateINI ((wchar_t *)booster_file.c_str ());

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

  display.enable_fullscreen =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Enable Fullscreen Mode [Alt+Enter]")
      );
  display.enable_fullscreen->register_to_ini (
    dll_ini,
      L"UnX.Display",
        L"EnableFullscreen" );


  window.center =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Center the Render Window")
      );
  window.center->register_to_ini (
    dll_ini,
      L"UnX.Window",
        L"Center" );


  render.bypass_intel =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Bypass Intel GPUs")
      );
  render.bypass_intel->register_to_ini (
    dll_ini,
      L"UnX.Render",
        L"BypassIntel" );

  render.flip_mode =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Allow Flip Mode Rendering")
      );
  render.flip_mode->register_to_ini (
    dll_ini,
      L"UnX.Render",
        L"FlipMode" );


  audio.mute_in_background =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Mute When Game Window Is In Background")
      );
  audio.mute_in_background->register_to_ini (
    dll_ini,
      L"UnX.Audio",
        L"BackgroundMute" );


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


  stutter.reduce =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Prevent Sleep (5) from killing performance")
      );
  stutter.reduce->register_to_ini (
    dll_ini,
      L"UnX.Stutter",
        L"Reduce"
  );


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

  input.manage_cursor = 
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Hide the mouse cursor after a period of inactivity")
      );
  input.manage_cursor->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"ManageCursor" );

  input.cursor_timeout = 
    static_cast <unx::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Hide the mouse cursor after a period of inactivity")
      );
  input.cursor_timeout->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"CursorTimeout" );

  input.gamepad_slot =
    static_cast <unx::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Gamepad slot to test during cursor mgmt")
      );
  input.gamepad_slot->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"GamepadSlot" );

  input.activate_on_kbd =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Keyboard activates mouse cursor")
      );
  input.activate_on_kbd->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"KeysActivateCursor" );

  input.fast_exit =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Exit Without Confirmation")
      );
  input.fast_exit->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"FastExit" );


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


  audio.mute_in_background->load (config.audio.mute_in_background);

  stutter.reduce->load (config.stutter.reduce);

  window.center->load   (config.window.center);

  input.remap_dinput8->load (config.input.remap_dinput8);
  input.gamepad_slot->load  (config.input.gamepad_slot);
  input.fix_bg_input->load  (config.input.fix_bg_input);

  input.block_windows->load (config.input.block_windows);
  //input.block_left_alt->load  (config.input.block_left_alt);
  //input.block_left_ctrl->load (config.input.block_left_ctrl);
  //input.block_all_keys->load  (config.input.block_all_keys);

  input.four_finger_salute->load (config.input.four_finger_salute);
  input.manage_cursor->load      (config.input.cursor_mgmt);

  float timeout;

  if (input.cursor_timeout->load (timeout))
    config.input.cursor_timeout = 
      static_cast <int>(timeout * 1000UL);

  input.activate_on_kbd->load (config.input.activate_on_kbd);

  input.fast_exit->load (config.input.fast_exit);


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
  if (! lstrcmpiW (UNX_GetExecutableName (), L"ffx.exe")) {
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
  sys.injector->load (config.system.injector);

  if (UNX_SetupWindowMgmt ()) {
    display.enable_fullscreen->load (config.display.enable_fullscreen);

    SKX_D3D11_EnableFullscreen (config.display.enable_fullscreen);
  }

  if (UNX_SetupLowLevelRender ()) {
    render.bypass_intel->load (config.render.bypass_intel);
    render.flip_mode->load    (config.render.flip_mode);

    SK_DXGI_EnableFlipMode ( config.render.flip_mode       &&
                          (! config.display.enable_fullscreen ) );

    // Rather dumb logic that assumes the Intel GPU will be Adapter 0.
    //
    //   This isn't dangerous, my DXGI DLL prevents overriding to select
    //     either Intel or Microsoft adapters.
    SK_DXGI_SetPreferredAdapter (config.render.bypass_intel ? 1 : 0);
  }

  if (UNX_SetupTexMgmt ()) {
    textures.resource_root->load (config.textures.resource_root);
    textures.dump->load          (config.textures.dump);
    textures.inject->load        (config.textures.inject);
    textures.cache->load         (config.textures.cache);

    SK_D3D11_SetResourceRoot (config.textures.resource_root);
    SK_D3D11_EnableTexDump   (config.textures.dump);
    SK_D3D11_EnableTexInject (config.textures.inject);
    SK_D3D11_EnableTexCache  (config.textures.cache);

    if (config.textures.inject)
      SK_D3D11_AddTexHash (L"Title.dds", 0xA4FFC068, 0x00);
  }

  if (empty)
    return false;

  return true;
}

void
UNX_SaveConfig (std::wstring name, bool close_config) {
  audio.mute_in_background->store   (config.audio.mute_in_background);

  window.center->store              (config.window.center);

  stutter.reduce->store             (config.stutter.reduce);

  input.remap_dinput8->store        (config.input.remap_dinput8);
  input.manage_cursor->store        (config.input.cursor_mgmt);

  input.cursor_timeout->store       ( (float)config.input.cursor_timeout /
                                      1000.0f );

//input.gamepad_slot->store         (config.input.gamepad_slot);
  input.activate_on_kbd->store      (config.input.activate_on_kbd);
  //input.block_windows->store        (config.input.block_windows);
  input.fix_bg_input->store         (config.input.fix_bg_input);


  ((unx::iParameter *)language.sfx)->store   ();
  ((unx::iParameter *)language.voice)->store ();
  ((unx::iParameter *)language.video)->store ();

  extern wchar_t* UNX_GetExecutableName (void);
  if (! lstrcmpiW (UNX_GetExecutableName (), L"ffx.exe")) {
    booster.ffx.entire_party_earns_ap->store (config.cheat.ffx.entire_party_earns_ap);
    booster.ffx.permanent_sensor->store      (config.cheat.ffx.permanent_sensor);
    booster.ffx.playable_seymour->store      (config.cheat.ffx.playable_seymour);
    booster.ffx.max_speed->store             (config.cheat.ffx.max_speed);
    booster.ffx.speed_step->store            (config.cheat.ffx.speed_step);
    booster.ffx.skip_dialog->store           (config.cheat.ffx.skip_dialog);
  }

  sys.version->store      (UNX_VER_STR);
  sys.injector->store     (config.system.injector);

  dll_ini->write      (SK_GetConfigPath () + name + L".ini");
  language_ini->write (SK_GetConfigPath () + name + L"_Language.ini");
  booster_ini->write  (SK_GetConfigPath () + name + L"_Booster.ini");

  if (close_config) {
    if (dll_ini != nullptr) {
      delete dll_ini;
      dll_ini = nullptr;
    }

    if (language_ini != nullptr) {
      delete language_ini;
      language_ini = nullptr;
    }

    if (booster_ini != nullptr) {
      delete booster_ini;
      booster_ini = nullptr;
    }
  }
}