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


struct SK_Keybind
{
  const char*  bind_name;
  std::wstring human_readable;

  struct {
    BOOL ctrl,
         shift,
         alt;
  };

  SHORT vKey;

  UINT  masked_code; // For fast comparison

  void parse  (void);
  void update (void);
};

struct UNX_Keybindings
{
  SK_Keybind SpeedStep { "Speed Boost",                   L"Ctrl+Shift+H",
                         true, true, false,  'H' };
  SK_Keybind KickStart { "Kickstart (fix stuck loading)", L"Ctrl+Alt+Shift+K",
                         true, true, true,   'K' };
  SK_Keybind TimeStop  { "TimeStop",                      L"Ctrl+Shift+P",
                          true, true, false, 'P' };
  SK_Keybind FreeLook  { "FreeLook",                      L"Ctrl+Shift+F",
                          true, true, false, 'F' };
  SK_Keybind Sensor    { "Permanent Sensor",              L"Ctrl+Shift+S",
                          true, true, false, 'S' };
  SK_Keybind FullAP    { "Full Party Earns AP",           L"Ctrl+Shift+A",
                          true, true, false, 'A' };
  SK_Keybind VSYNC     { "VSYNC",                         L"Ctrl+Shift+V",
                         true, true, false,  'V' };
  SK_Keybind SoftReset { "Soft Reset (FFX)",              L"Ctrl+Shift+Delete",
                         true, true, false,   VK_DELETE };
} extern keybinds;


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