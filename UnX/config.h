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
  } render;

  struct {
    bool    disable_dpi_scaling  = true;
  } display;

  struct {
  } compatibility;

  struct {
  } window;

  struct {
  } stutter;

  struct {
  } textures;

  struct {
  } shaders;

  struct {
    int  num_frames = 0;
    bool shaders    = false;
    bool ui         = false; // General-purpose UI stuff
  } trace;

  struct {
    std::wstring voice;
    std::wstring video;
    std::wstring sfx;
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