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
#ifndef __UNX__WINDOW_H__
#define __UNX__WINDOW_H__

#include "command.h"

namespace unx
{
  // State for window activation faking
  struct window_state_s {
    bool    init             = false;

    WNDPROC WndProc_Original = nullptr;

    bool    active           = true;
    bool    activating       = false;

    DWORD   proc_id       = 0;
    DWORD   render_thread = 0;
    HWND    hwnd          = nullptr;

    bool    fullscreen    = false;
    RECT    window_rect;
    RECT    cursor_clip;
    POINT   cursor_pos; // The cursor position when the game isn't screwing
                        //   with it...

    HICON   large_icon;
    HICON   small_icon;

    DWORD   style    = 0;//WS_VISIBLE | WS_POPUP | WS_MINIMIZEBOX; // Style before we removed the border
    DWORD   style_ex = 0;//WS_EX_APPWINDOW;                        // StyleEX before removing the border

    bool    borderless = false;
  } extern window;

  namespace WindowManager
  {
    void Init     ();
    void Shutdown ();

    class CommandProcessor : public SK_IVariableListener {
    public:
      CommandProcessor (void);

      virtual bool OnVarChange (SK_IVariable* var, void* val = NULL);

      static CommandProcessor* getInstance (void)
      {
        if (pCommProc == nullptr)
          pCommProc = new CommandProcessor ();

        return pCommProc;
      }

    protected:
      SK_IVariable* foreground_fps_;
      SK_IVariable* background_fps_;

    private:
      static CommandProcessor* pCommProc;
    };
  }
}

#endif /* __UNX__WINDOW_H__ */