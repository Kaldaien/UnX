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
#include <Windows.h>
#include <windowsx.h> // GET_X_LPARAM

#include "window.h"
#include "input.h"
#include "config.h"
#include "log.h"
#include "hook.h"

unx::window_state_s unx::window;

SetWindowLongA_pfn      SetWindowLongA_Original      = nullptr;

LRESULT
CALLBACK
DetourWindowProc ( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam );


bool windowed = false;

#include <dwmapi.h>
#pragma comment (lib, "dwmapi.lib")

typedef LRESULT (CALLBACK *DetourWindowProc_pfn)( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam
);

DetourWindowProc_pfn DetourWindowProc_Original = nullptr;


LRESULT
CALLBACK
DetourWindowProc ( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam )
{
  bool last_active = unx::window.active;

  //
  // The window activation state is changing, among other things we can take
  //   this opportunity to setup a special framerate limit.
  //
  if (unx::window.active != last_active) {
//    eTB_CommandProcessor* pCommandProc =
//      SK_GetCommandProcessor           ();
  }

#if 0
  // Block keyboard input to the game while the console is visible
  if (console_visible/* || background_render*/) {
    // Only prevent the mouse from working while the window is in the bg
    //if (background_render && uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
      //return DefWindowProc (hWnd, uMsg, wParam, lParam);

    if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);

    // Block RAW Input
    if (uMsg == WM_INPUT)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }
#endif

#if 0
  // Block the menu key from messing with stuff*
  if (config.input.block_left_alt &&
      (uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP)) {
    // Make an exception for Alt+Enter, for fullscreen mode toggle.
    //   F4 as well for exit
    if (wParam != VK_RETURN && wParam != VK_F4)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }
#endif

  // What an ugly mess, this is crazy :)
  if (config.input.cursor_mgmt) {
    extern bool IsControllerPluggedIn (UINT uJoyID);

    struct {
      POINTS pos      = { 0 }; // POINT (Short) - Not POINT plural ;)
      DWORD  sampled  = 0UL;
      bool   cursor   = true;

      int    init     = false;
      int    timer_id = 0x68992;
    } static last_mouse;

   auto ActivateCursor = [](bool changed = false)->
    bool
     {
       bool was_active = last_mouse.cursor;

       if (! last_mouse.cursor) {
         while (ShowCursor (TRUE) < 0) ;
         last_mouse.cursor = true;
       }

       if (changed)
         last_mouse.sampled = timeGetTime ();

       return (last_mouse.cursor != was_active);
     };

   auto DeactivateCursor = []()->
    bool
     {
       if (! last_mouse.cursor)
         return false;

       bool was_active = last_mouse.cursor;

       if (last_mouse.sampled <= timeGetTime () - config.input.cursor_timeout) {
         while (ShowCursor (FALSE) >= 0) ;
         last_mouse.cursor = false;
       }

       return (last_mouse.cursor != was_active);
     };

    if (! last_mouse.init) {
      SetTimer (hWnd, last_mouse.timer_id, config.input.cursor_timeout / 2, nullptr);
      last_mouse.init = true;
    }

    bool activation_event =
      (uMsg == WM_MOUSEMOVE);

    // Don't blindly accept that WM_MOUSEMOVE actually means the mouse moved...
    if (activation_event) {
      const short threshold = 2;

      // Filter out small movements
      if ( abs (last_mouse.pos.x - GET_X_LPARAM (lParam)) < threshold &&
           abs (last_mouse.pos.y - GET_Y_LPARAM (lParam)) < threshold )
        activation_event = false;

      last_mouse.pos = MAKEPOINTS (lParam);
    }

    if (config.input.activate_on_kbd)
      activation_event |= ( uMsg == WM_CHAR       ||
                            uMsg == WM_SYSKEYDOWN ||
                            uMsg == WM_SYSKEYUP );

    // If timeout is 0, just hide the thing indefinitely
    if (activation_event && config.input.cursor_timeout != 0)
      ActivateCursor (true);

    else if (uMsg == WM_TIMER && wParam == last_mouse.timer_id) {
      if (IsControllerPluggedIn (config.input.gamepad_slot))
        DeactivateCursor ();

      else
        ActivateCursor ();
    }
  }

  return DetourWindowProc_Original (hWnd, uMsg, wParam, lParam);
}

void
UNX_InstallWindowHook (HWND hWnd)
{
  unx::window.hwnd = hWnd;

  if (hWnd == 0) {
    UNX_CreateDLLHook ( config.system.injector.c_str (),
                        "SK_DetourWindowProc",
                         DetourWindowProc,
              (LPVOID *)&DetourWindowProc_Original );

    //DwmEnableMMCSS (TRUE);
  }
}





void
unx::WindowManager::Init (void)
{
//  CommandProcessor* comm_proc = CommandProcessor::getInstance ();
}

void
unx::WindowManager::Shutdown (void)
{
}


unx::WindowManager::
  CommandProcessor::CommandProcessor (void)
{
}

bool
  unx::WindowManager::
    CommandProcessor::OnVarChange (eTB_Variable* var, void* val)
{
  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();

  bool known = false;

  if (! known) {
    dll_log.Log ( L"[Window Mgr] UNKNOWN Variable Changed (%p --> %p)",
                    var,
                      val );
  }

  return false;
}

unx::WindowManager::CommandProcessor*
   unx::WindowManager::CommandProcessor::pCommProc = nullptr;