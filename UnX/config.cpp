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
#include "parameter.h"
#include "ini.h"
#include "log.h"

static
  unx::INI::File* 
             dll_ini         = nullptr;
std::wstring UNX_VER_STR = L"0.0.5";
unx_config_s config;

struct {
} render;

struct {
} compatibility;

struct {
} window;

struct {
  unx::ParameterBool*    disable_dpi_scaling;
} display;

struct {
} stutter;

struct {
} colors;

struct {
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

  input.alias_wasd =
    static_cast <unx::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Arrow keys alias to WASD")
      );
  input.alias_wasd->register_to_ini (
    dll_ini,
      L"UnX.Input",
        L"AliasArrowsToWASD" );


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

  if (input.manage_cursor->load ())
    config.input.cursor_mgmt = input.manage_cursor->get_value ();

  if (input.cursor_timeout->load ())
    config.input.cursor_timeout = 
      static_cast <int>(input.cursor_timeout->get_value () * 1000UL);

  if (input.gamepad_slot->load ())
    config.input.gamepad_slot = input.gamepad_slot->get_value ();

  if (input.activate_on_kbd->load ())
    config.input.activate_on_kbd = input.activate_on_kbd->get_value ();

  if (input.alias_wasd->load ())
    config.input.alias_wasd = input.alias_wasd->get_value ();


  if (sys.version->load ())
    config.system.version = sys.version->get_value ();

  if (sys.injector->load ())
    config.system.injector = sys.injector->get_value ();

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

  input.gamepad_slot->set_value     (config.input.gamepad_slot);
  input.gamepad_slot->store         ();

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