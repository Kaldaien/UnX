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
    bool    bypass_intel        = true;
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

  struct textures_s {
    std::wstring resource_root = L"UnX_Res";
    std::wstring gamepad       = L"PlayStation_Glossy";

    struct pad_hash_set_s {
      uint32_t high;
      uint32_t low;
    };

    struct pad_hash_s
    {
      pad_hash_set_s   icons = { 0x6d553cf3, 0xf35a7450 };

      struct pad_buttons_s {
        pad_hash_set_s A     = { 0x1fa21cd9, 0x1c4a8a04 };
        pad_hash_set_s B     = { 0x6599aaea, 0x45c61cee };
        pad_hash_set_s X     = { 0xaa50da09, 0x24af22f7 };
        pad_hash_set_s Y     = { 0xe4b753c4, 0xa91889c2 };

        pad_hash_set_s LB    = { 0x1248d62f, 0x4ccc411b };
        pad_hash_set_s RB    = { 0xa9ba5be2, 0xf5d5c951 };

        pad_hash_set_s LT    = { 0xaa6eca76, 0x6bc8a1bd };
        pad_hash_set_s RT    = { 0xdd8ae038, 0x539c0113 };

        pad_hash_set_s LS    = { 0xad5a3228, 0x59ec403b };
        pad_hash_set_s RS    = { 0x478fe0fd, 0xa0da91dd };

        pad_hash_set_s UP    = { 0x3149bde5, 0x21b21c9a };
        pad_hash_set_s RIGHT = { 0x95a1cfde, 0x8e936a13 };
        pad_hash_set_s DOWN  = { 0x7eec7211, 0xba866771 };
        pad_hash_set_s LEFT  = { 0x0d3d11bf, 0x0b8de699 };

        pad_hash_set_s START = { 0x8c5b52ca, 0x21ec28b6 };
        pad_hash_set_s BACK  = { 0xdf0201f5, 0x2a2aedd1 };
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
    std::wstring voice  = L"jp";
    std::wstring video  = L"jp";
    std::wstring sfx    = L"jp";
  } language;

  struct {
    bool block_left_alt     = false;
    bool block_left_ctrl    = false;
    bool block_windows      = false;
    bool block_all_keys     = false;

    bool four_finger_salute = true;

    bool cursor_mgmt        = true;
    int  cursor_timeout     = 1500;
    bool activate_on_kbd    = true;
    int  gamepad_slot       = 0;
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