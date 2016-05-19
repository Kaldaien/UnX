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
  unx::INI::File* 
             dll_ini         = nullptr;
std::wstring UNX_VER_STR = L"0.2.0";
unx_config_s config;

typedef bool (WINAPI *SK_DXGI_EnableFlipMode_pfn)   (bool);

SK_DXGI_EnableFlipMode_pfn SK_DXGI_EnableFlipMode = nullptr;

typedef void (WINAPI *SK_D3D11_SetResourceRoot_pfn)  (std::wstring);
typedef void (WINAPI *SK_D3D11_EnableTexDump_pfn)    (bool);
typedef void (WINAPI *SK_D3D11_EnableTexInject_pfn)  (bool);
typedef void (WINAPI *SK_D3D11_AddTexHash_pfn)       (std::wstring, uint32_t);
typedef void (WINAPI *SK_D3D11_RemoveTexHash_pfn)    (uint32_t);

typedef void (WINAPI *SKX_D3D11_MarkTextures_pfn)    (bool,bool,bool);
typedef void (WINAPI *SKX_D3D11_EnableFullscreen_pfn)(bool);

SK_D3D11_SetResourceRoot_pfn   SK_D3D11_SetResourceRoot   = nullptr;
SK_D3D11_EnableTexDump_pfn     SK_D3D11_EnableTexDump     = nullptr;
SK_D3D11_EnableTexInject_pfn   SK_D3D11_EnableTexInject   = nullptr;
SK_D3D11_AddTexHash_pfn        SK_D3D11_AddTexHash        = nullptr;
SK_D3D11_RemoveTexHash_pfn     SK_D3D11_RemoveTexHash     = nullptr;

SKX_D3D11_MarkTextures_pfn     SKX_D3D11_MarkTextures     = nullptr;
SKX_D3D11_EnableFullscreen_pfn SKX_D3D11_EnableFullscreen = nullptr;

struct {
  unx::ParameterBool*    flip_mode;
} render;

struct {
} compatibility;

struct {
} window;

struct {
  unx::ParameterBool*    disable_dpi_scaling;
  unx::ParameterBool*    enable_fullscreen;
} display;

struct {
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
} textures;

struct {
  unx::ParameterStringW* voice;
  unx::ParameterStringW* sfx;
  unx::ParameterStringW* video;
} language;

struct {
  unx::ParameterBool*    block_left_alt;
  unx::ParameterBool*    block_left_ctrl;
  unx::ParameterBool*    block_windows;
  unx::ParameterBool*    block_all_keys;

  unx::ParameterBool*    four_finger_salute;
  unx::ParameterBool*    special_keys;

  unx::ParameterBool*    manage_cursor;
  unx::ParameterFloat*   cursor_timeout;
  unx::ParameterInt*     gamepad_slot;
  unx::ParameterBool*    activate_on_kbd;
  unx::ParameterBool*    alias_wasd;
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

  if (SK_DXGI_EnableFlipMode != nullptr)
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
    (SK_D3D11_EnableTexInject_pfn)
      GetProcAddress (hInjector, "SK_D3D11_EnableTexInject");

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
  if ( SK_D3D11_SetResourceRoot != nullptr && SK_D3D11_EnableTexDump != nullptr &&
       SK_D3D11_EnableTexInject != nullptr && SK_D3D11_AddTexHash    != nullptr &&
       SK_D3D11_RemoveTexHash   != nullptr ) {
    return true;
  }

  return false;
}

bool
UNX_LoadConfig (std::wstring name) {
  // Load INI File
  std::wstring full_name = name + L".ini";
  dll_ini = new unx::INI::File ((wchar_t *)full_name.c_str ());

  bool empty = dll_ini->get_sections ().empty ();

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

  render.flip_mode =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Allow Flip Mode Rendering")
      );
  render.flip_mode->register_to_ini (
    dll_ini,
      L"UnX.Render",
        L"FlipMode" );


  language.voice =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Voiceover Language")
      );
  language.voice->register_to_ini (
    dll_ini,
      L"UnX.Language",
        L"Voice" );

  language.sfx =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Sound Effect Language")
      );
  language.sfx->register_to_ini (
    dll_ini,
      L"UnX.Language",
        L"SoundEffects" );

  language.video =
    static_cast <unx::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Video Language")
      );
  language.video->register_to_ini (
    dll_ini,
      L"UnX.Language",
        L"Video" );


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

  input.special_keys =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"F1-F5")
      );
  input.special_keys->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"SpecialKeys" );

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
  if (display.disable_dpi_scaling->load ())
    config.display.disable_dpi_scaling = display.disable_dpi_scaling->get_value ();


  if (language.voice->load ())
    config.language.voice = language.voice->get_value ();

  if (language.sfx->load ())
    config.language.sfx = language.sfx->get_value ();

  if (language.video->load ())
    config.language.video = language.video->get_value ();


  //if (input.block_left_alt->load ())
    //config.input.block_left_alt = input.block_left_alt->get_value ();

  //if (input.block_left_ctrl->load ())
    //config.input.block_left_ctrl = input.block_left_ctrl->get_value ();

  //if (input.block_windows->load ())
    //config.input.block_windows = input.block_windows->get_value ();

  //if (input.block_all_keys->load ())
    //config.input.block_all_keys = input.block_all_keys->get_value ();

  if (input.four_finger_salute->load ())
    config.input.four_finger_salute = input.four_finger_salute->get_value ();

  if (input.manage_cursor->load ())
    config.input.cursor_mgmt = input.manage_cursor->get_value ();

  if (input.cursor_timeout->load ())
    config.input.cursor_timeout = 
      static_cast <int>(input.cursor_timeout->get_value () * 1000UL);

  if (input.gamepad_slot->load ())
    config.input.gamepad_slot = input.gamepad_slot->get_value ();

  if (input.activate_on_kbd->load ())
    config.input.activate_on_kbd = input.activate_on_kbd->get_value ();


  if (sys.version->load ())
    config.system.version = sys.version->get_value ();

  if (sys.injector->load ())
    config.system.injector = sys.injector->get_value ();


  if (UNX_SetupLowLevelRender ()) {
    if (render.flip_mode->load ())
      config.render.flip_mode = render.flip_mode->get_value ();

    SK_DXGI_EnableFlipMode (config.render.flip_mode);
  }

  if (UNX_SetupTexMgmt ()) {
    if (textures.resource_root->load ())
      config.textures.resource_root = textures.resource_root->get_value ();

    if (textures.dump->load ())
      config.textures.dump =
        textures.dump->get_value ();

    if (textures.inject->load ())
      config.textures.inject =
        textures.inject->get_value ();

    SK_D3D11_SetResourceRoot (config.textures.resource_root);
    SK_D3D11_EnableTexDump   (config.textures.dump);
    SK_D3D11_EnableTexInject (config.textures.inject);
  }

  if (UNX_SetupWindowMgmt ()) {
    if (display.enable_fullscreen->load ()) {
      config.display.enable_fullscreen =
        display.enable_fullscreen->get_value ();
    }

    SKX_D3D11_EnableFullscreen (config.display.enable_fullscreen);
  }


  if (empty)
    return false;

  return true;
}

void
UNX_SaveConfig (std::wstring name, bool close_config) {
  input.manage_cursor->set_value    (config.input.cursor_mgmt);
  input.manage_cursor->store        ();

  input.cursor_timeout->set_value   ( (float)config.input.cursor_timeout /
                                      1000.0f );
  input.cursor_timeout->store       ();

  //input.gamepad_slot->set_value     (config.input.gamepad_slot);
  //input.gamepad_slot->store         ();

  input.activate_on_kbd->set_value  (config.input.activate_on_kbd);
  input.activate_on_kbd->store      ();

//  input.alias_wasd->set_value       (config.input.alias_wasd);
//  input.alias_wasd->store           ();


  sys.version->set_value  (UNX_VER_STR);
  sys.version->store      ();

  sys.injector->set_value (config.system.injector);
  sys.injector->store     ();

  dll_ini->write (name + L".ini");

  if (close_config) {
    if (dll_ini != nullptr) {
      delete dll_ini;
      dll_ini = nullptr;
    }
  }
}