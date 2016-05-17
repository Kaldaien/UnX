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
#ifndef __UNX__CONFIG_H__
#define __UNX__CONFIG_H__

#include <Windows.h>
#include <string>

extern std::wstring UNX_VER_STR;

struct unx_config_s
{
  struct {
    bool    flip_mode           = true;
  } render;

  struct {
    bool    disable_dpi_scaling = true;
    bool    enable_fullscreen   = false;
  } display;

  struct {
  } compatibility;

  struct {
  } window;

  struct {
  } stutter;

  struct {
    std::wstring resource_root = L"UnX_Res";
    std::wstring gamepad       = L"PlayStation_Glossy";

    struct pad_hash_s
    {
      struct pad_icons_s {
        uint32_t ffx  = 0x6d553cf3;
        uint32_t ffx2 = 0xf35a7450;
      } icons;

      struct pad_buttons_s {
        struct pad_button_hashes_ffx_s {
          uint32_t A     = 0x1fa21cd9;
          uint32_t B     = 0x6599aaea;
          uint32_t X     = 0xaa50da09;
          uint32_t Y     = 0xe4b753c4;

          uint32_t LB    = 0x1248d62f;
          uint32_t RB    = 0xa9ba5be2;

          uint32_t LT    = 0xaa6eca76;
          uint32_t RT    = 0xdd8ae038;

          uint32_t LS    = 0xad5a3228;
          uint32_t RS    = 0x478fe0fd;

          uint32_t UP    = 0x3149bde5;
          uint32_t RIGHT = 0x95a1cfde;
          uint32_t DOWN  = 0x7eec7211;
          uint32_t LEFT  = 0x0d3d11bf;

          uint32_t START = 0x8c5b52ca;
          uint32_t BACK  = 0xdf0201f5;
        } hashes0;

#if 0
        struct pad_button_hashes_ffx2_s {
          uint32_t A     = 0x1fa21cd9;
          uint32_t B     = 0x6599aaea;
          uint32_t X     = 0xaa50da09;
          uint32_t Y     = 0xe4b753c4;

          uint32_t LB    = 0x1248d62f;
          uint32_t RB    = 0xa9ba5be2;

          uint32_t LT    = 0xaa6eca76;
          uint32_t RT    = 0xdd8ae038;

          uint32_t LS    = 0xad5a3228;
          uint32_t RS    = 0x478fe0fd;

          uint32_t UP    = 0x3149bde5;
          uint32_t RIGHT = 0x95a1cfde;
          uint32_t DOWN  = 0x7eec7211;
          uint32_t LEFT  = 0x0d3d11bf;

          uint32_t START = 0x8c5b52ca;
          uint32_t BACK  = 0xdf0201f5;
        } hashes1;
#endif
      } buttons;
    } pad;

    bool         dump              = false;
    bool         inject            = false;
    bool         mark              = false;
  } textures;

  struct {
  } shaders;

  struct {
    int  num_frames = 0;
    bool shaders    = false;
    bool ui         = false; // General-purpose UI stuff
  } trace;

  struct {
    std::wstring voice = L"jp";
    std::wstring video = L"jp";
    std::wstring sfx   = L"jp";
  } language;

  struct {
    bool block_left_alt  = false;
    bool block_left_ctrl = false;
    bool block_windows   = false;
    bool block_all_keys  = false;
    bool cursor_mgmt     = true;
    int  cursor_timeout  = 1500;
    bool activate_on_kbd = true;
    bool alias_wasd      = true;
    int  gamepad_slot    = 0;
  } input;

  struct {
    std::wstring
            version            = UNX_VER_STR;
    std::wstring
            injector           = L"dxgi.dll";
  } system;
};

extern unx_config_s config;

bool UNX_LoadConfig (std::wstring name         = L"UnX");
void UNX_SaveConfig (std::wstring name         = L"UnX",
                     bool         close_config = false);

#endif /* __UNX__CONFIG_H__ */