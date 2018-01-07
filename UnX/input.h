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
#ifndef __UNX__INPUT_H__
#define __UNX__INPUT_H__

#include "command.h"

#include <string>

struct SK_Keybind
{
  const char*  bind_name;
  std::wstring human_readable;

  struct {
    BOOL ctrl,
         shift,
         alt;
  } modifiers;

  SHORT vKey;

  UINT  masked_code; // For fast comparison

  void parse  (void);
  void update (void);
};

struct UNX_Keybindings
{
  UNX_Keybindings (void)
  {
    vec.emplace_back (&VSYNC);
    vec.emplace_back (&KickStart);

    ffx_vec.emplace_back (&VSYNC);
    ffx_vec.emplace_back (&KickStart);
    ffx_vec.emplace_back (&SpeedStep);
    ffx_vec.emplace_back (&TimeStop);
    ffx_vec.emplace_back (&FreeLook);
    ffx_vec.emplace_back (&Sensor);
    ffx_vec.emplace_back (&FullAP),
    ffx_vec.emplace_back (&SoftReset);
  }

  SK_Keybind VSYNC     { "Toggle VSYNC",                  L"Ctrl+Shift+V",
                         true, true, false,  'V' };
  SK_Keybind KickStart { "Kickstart (fix stuck loading)", L"Ctrl+Alt+Shift+K",
                         true, true, true,   'K' };
  SK_Keybind SpeedStep { "Speed Boost",                   L"Ctrl+Shift+H",
                         true, true, false,  'H' };
  SK_Keybind TimeStop  { "Toggle Time Stop",              L"Ctrl+Shift+P",
                          true, true, false, 'P' };
  SK_Keybind FreeLook  { "Toggle Freelook",               L"Ctrl+Shift+F",
                          true, true, false, 'F' };
  SK_Keybind Sensor    { "Toggle Permanent Sensor",       L"Ctrl+Shift+S",
                          true, true, false, 'S' };
  SK_Keybind FullAP    { "Toggle Full Party AP",          L"Ctrl+Shift+A",
                          true, true, false, 'A' };
  SK_Keybind SoftReset { "Soft Reset (FFX)",              L"Ctrl+Shift+Delete",
                         true, true, false,   VK_DELETE };

  std::vector <SK_Keybind *> vec;
  std::vector <SK_Keybind *> ffx_vec;
} extern keybinds;

struct SK_GamepadCombo_V0 {
  const wchar_t** button_names = nullptr;
  std::string     combo_name   =  "";
  std::wstring    unparsed     = L"";
  int             buttons      = 0;
};

#include "parameter.h"

struct UNX_GamepadCombo : SK_GamepadCombo_V0
{
  UNX_GamepadCombo (const char* shorthand) { combo_name = shorthand; }

  unx::ParameterStringW* config_parameter;
};

struct unx_gamepad_s {
  std::wstring tex_set = L"PlayStation_Glossy";
  bool         legacy  = false;

  UNX_GamepadCombo f1         { "F1"                };
  UNX_GamepadCombo f2         { "F2"                };
  UNX_GamepadCombo f3         { "F3"                };
  UNX_GamepadCombo f4         { "F4"                };
  UNX_GamepadCombo f5         { "F5"                };
  UNX_GamepadCombo screenshot { "Steam Screenshot"  };
  UNX_GamepadCombo fullscreen { "Fullscreen Toggle" }; 
  UNX_GamepadCombo esc        { "Escape"            };
  UNX_GamepadCombo speedboost { "Speedboost"        };
  UNX_GamepadCombo kickstart  { "Kickstart"         };
  UNX_GamepadCombo softreset  { "Soft Reset"        };

  struct names_s
  {
    const wchar_t*
      Xbox [18] =
        {  L"Up",    L"Down",     L"Left",    L"Right",
           L"Start", L"Back",
           L"LS",    L"RS",
           L"LB",    L"RB",
           L"Guide", L"Unused",
           L"A",     L"B",     L"X",    L"Y",
           L"LT",    L"RT",      };
    
    const wchar_t*
      PlayStation [18] =
        {  L"Up",    L"Down",     L"Left",    L"Right",
           L"Start", L"Select",
           L"L3",    L"R3",
           L"L1",    L"R1",
           L"Home",  L"Unused",
           L"Cross", L"Circle",   L"Square",  L"Triangle",
           L"L2",    L"R2",      };
  } names;

  struct remap_s {
    struct buttons_s {
      int X     = JOY_BUTTON1;
      int A     = JOY_BUTTON2;
      int B     = JOY_BUTTON3;
      int Y     = JOY_BUTTON4;
      int LB    = JOY_BUTTON5;
      int RB    = JOY_BUTTON6;
      int LT    = JOY_BUTTON7;
      int RT    = JOY_BUTTON8;
      int BACK  = JOY_BUTTON9;
      int START = JOY_BUTTON10;
      int LS    = JOY_BUTTON11;
      int RS    = JOY_BUTTON12;
    } buttons;

    // Post-Process the button map above so we do not
    //   have to constantly perform the operations
    //     below when polling a controller...
    int map [12];

    static int indexToEnum (int idx) {
      // For Axes
      if (idx <= 0) {
        idx = (16 - idx);
      }

      // Possibility for sign-related problems exists if
      //   0 is passed, but button 0 is not valid anyway.
      return 1 << (idx - 1);
    }

    static int enumToIndex (unsigned int enum_val) {
      int idx = 0;

      // For Axes
      bool axis = false;

      if (enum_val >= (1 << 16))
        axis = true;

      while (enum_val > 0) {
        enum_val >>= 1;
        idx++;
      }

      if (axis) {
        idx -= 16;
        idx  = -idx;
      }

      return idx;
    }
  } remap;
} extern gamepad;


namespace unx
{
  namespace InputManager
  {
    void Init     ();
    void Shutdown ();

    class Hooker {
    private:
      HANDLE          hMsgPump;

      static Hooker*  pInputHook;
    protected:
      Hooker (void) { }

    public:
      static Hooker* getInstance (void)
      {
        if (pInputHook == nullptr)
          pInputHook = new Hooker ();

        return pInputHook;
      }

      void Start (void);
      void End   (void);

      HANDLE GetThread (void) {
        return hMsgPump;
      }

      static unsigned int
        __stdcall
        MessagePump (LPVOID hook_ptr);
    };
  }
}

#endif /* __UNX__INPUT_H__ */