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

#include "sound.h"
#include "cheat.h"

#include <dxgi.h>
#include <d3d11.h>

typedef HRESULT (WINAPI *DXGISwap_ResizeBuffers_pfn)(
   IDXGISwapChain *This,
   UINT            BufferCount,
   UINT            Width,
   UINT            Height,
   DXGI_FORMAT     NewFormat,
   UINT            SwapChainFlags
);
DXGISwap_ResizeBuffers_pfn DXGISwap_ResizeBuffers_Original = nullptr;

typedef HRESULT (WINAPI *DXGISwap_ResizeTarget_pfn)(
   IDXGISwapChain *This,
   DXGI_MODE_DESC *desc
);
DXGISwap_ResizeTarget_pfn DXGISwap_ResizeTarget_Original = nullptr;

typedef HWND (WINAPI *SK_GetGameWindow_pfn)(void);
extern SK_GetGameWindow_pfn SK_GetGameWindow;

IDXGISwapChain* pGameSwapChain = nullptr;

enum unx_fullscreen_op_t {
  Fullscreen,
  Window,
  Restore
};

#include <atlbase.h>

bool
UNX_IsRenderThread (void)
{
  // Plugin not fully initialized yet ... we have no choice but
  //   to report this as false.
  if (unx::window.render_thread == 0)
    return false;

  if (GetCurrentThreadId () == unx::window.render_thread)
    return true;

  return false;
}

bool
UNX_IsWindowThread (void)
{
  // Plugin not fully initialized yet ... we have no choice but
  //   to report this as false.
  if (unx::window.hwnd == 0)
    return false;

  if ( GetCurrentThreadId       (                           ) ==
       GetWindowThreadProcessId ( unx::window.hwnd, nullptr ) )
    return true;

  return false;
}

DWORD
WINAPI
UNX_ToggleFullscreenThread (LPVOID user)
{
  BOOL                  fullscreen = FALSE;
  CComPtr <IDXGIOutput> pOutput    = nullptr;

  if (SUCCEEDED (pGameSwapChain->GetFullscreenState (&fullscreen, &pOutput))) {
    if (fullscreen) {
      pGameSwapChain->SetFullscreenState (FALSE, nullptr);
    } else {
      DXGI_SWAP_CHAIN_DESC swap_desc;
      pGameSwapChain->GetDesc (&swap_desc);

      DXGI_MODE_DESC mode = swap_desc.BufferDesc;

      pGameSwapChain->ResizeTarget       (&mode);
      pGameSwapChain->SetFullscreenState (TRUE, pOutput);

      mode.RefreshRate.Denominator = 0;
      mode.RefreshRate.Numerator   = 0;

      pGameSwapChain->ResizeTarget  (&mode);
      pGameSwapChain->ResizeBuffers (0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    }
  }

  return 0;
}

void
UNX_ToggleFullscreen (void)
{
  // Mod is not yet fully initialized...
  if (! pGameSwapChain)
    return;

  // It is not safe to issue DXGI commands from the thread that drives
  //   the Windows message pump... so spawn a separate thread to do this.
  CreateThread (nullptr, 0, UNX_ToggleFullscreenThread, nullptr, 0, nullptr);
}


void UNX_SetFullscreenState (unx_fullscreen_op_t op);

void
UNX_SetFullscreenState (unx_fullscreen_op_t op)
{
  static BOOL last_fullscreen = FALSE;

  // Mod is not yet fully initialized...
  if (pGameSwapChain == nullptr)
    return;

  BOOL fs;
  pGameSwapChain->GetFullscreenState (&fs, nullptr);

  if (op == Fullscreen) {
    if (fs != TRUE) {
      dll_log.Log (L"[Fullscreen] Transition: Window -> Full");

      last_fullscreen = fs;

      UNX_ToggleFullscreen ();
    }
  } else {
    if (op == Restore) {
      dll_log.Log (L"[Fullscreen] Operation: *Restore");
      UNX_SetFullscreenState (last_fullscreen ? Fullscreen : Window);
      last_fullscreen = fs;
    }

    if (op == Window) {
      if (fs == TRUE) {
        dll_log.Log (L"[Fullscreen] Transition: Full -> Window");

        last_fullscreen = fs;

        UNX_ToggleFullscreen ();
      }
    }
  }
}


unx::window_state_s unx::window;

SetWindowLongA_pfn      SetWindowLongA_Original      = nullptr;

LRESULT
CALLBACK
DetourWindowProc ( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam );


bool windowed = false;

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
  static bool shutting_down = false;
  static bool last_active   = unx::window.active;


  if (GetForegroundWindow () == hWnd)
    unx::window.active = true;
  else
    unx::window.active = false;


  //
  // Setup the Cheat Manager on the first message received
  //   while the render window is active
  //
  static bool init_cheats = false;
  if (unx::window.active && (! init_cheats)) {
    unx::window.hwnd = hWnd;
    unx::CheatManager::Init ();
    init_cheats      = true;
  }



  // This state is persistent and we do not want Alt+F4 to remember a muted
  //   state.
  if (uMsg == WM_DESTROY || uMsg == WM_QUIT || (config.input.fast_exit && uMsg == WM_CLOSE)) {
    shutting_down = true;

    // Don't change the active window when shutting down
    SetForegroundWindow (SK_GetGameWindow ());

    // The game would deadlock and never shutdown if we tried to Alt+F4 in
    //   fullscreen mode... oh the quirks of D3D :(
    if (pGameSwapChain != nullptr && config.display.enable_fullscreen) {
      UNX_SetFullscreenState (Window);
    }

    UNX_SetGameMute (FALSE);

    // Without this, some Windows functions wouldn't work correctly
    extern void UNX_ReleaseESCKey (void);
    UNX_ReleaseESCKey ();

    // Don't trigger the code below that handles window deactivation in fullscreen mode
    unx::window.active = last_active;
  }

  //
  // The window activation state is changing, among other things we can take
  //   this opportunity to setup a special framerate limit.
  //
  if ((! shutting_down) && (unx::window.active != last_active || (uMsg == WM_ACTIVATEAPP && unx::window.active != last_active))) {
    bool deactivate = ! (unx::window.active);

    if (uMsg == WM_ACTIVATEAPP) {
      deactivate = ! (bool)wParam;
      unx::window.active = ! deactivate;
    }

    last_active = unx::window.active;

    if (config.audio.mute_in_background) {
      BOOL bMute = FALSE;

      if (deactivate)
        bMute = TRUE;

      UNX_SetGameMute (bMute);
    }

    dll_log.Log ( L"[Window Mgr] Activation: %s",
                    unx::window.active ? L"ACTIVE" :
                                         L"INACTIVE" );

    //
    // Allow Alt+Tab to work
    //
    if (pGameSwapChain != nullptr && config.display.enable_fullscreen) {
      if (! deactivate) {
        UNX_SetFullscreenState (Restore);
      } else {
        UNX_SetFullscreenState (Window);
      }
    }
  }

  const bool fix_background_input = true;

  if (fix_background_input) {
    if ( uMsg == WM_NCACTIVATE ) {
      return 0;
    }
  }


  if (uMsg == WM_TIMER) {
    if (wParam == unx::CHEAT_TIMER_FFX)
      unx::CheatTimer_FFX ();
    else if (wParam == unx::CHEAT_TIMER_FFX2)
      unx::CheatTimer_FFX2 ();
  }



  if (unx::window.active != last_active) {
//    eTB_CommandProcessor* pCommandProc =
//      SK_GetCommandProcessor           ();
  }


  if (config.input.fix_bg_input) {
    // Block keyboard input to the game while the console is visible
    if (! (unx::window.active)/* || background_render*/) {
      if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

      // Block RAW Input
      if (uMsg == WM_INPUT)
        return DefWindowProc (hWnd, uMsg, wParam, lParam);
    }
  }


  // Block the menu key from messing with stuff*
  if ((uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP)) {

    // Alt + Enter (Fullscreen toggle)
    if (uMsg == WM_SYSKEYDOWN && wParam == VK_RETURN) {
      if (pGameSwapChain && config.display.enable_fullscreen) {
        UNX_ToggleFullscreen ();
        return DefWindowProc (hWnd, uMsg, wParam, lParam);
      }
    }

    // Actually, just block Alt+F4
    if (config.input.fast_exit || wParam != VK_F4)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

  // What an ugly mess, this is crazy :)
  if (config.input.cursor_mgmt) {
    extern bool IsControllerPluggedIn (INT iJoyID);

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
      if (config.input.cursor_timeout != 0)
        SetTimer (hWnd, last_mouse.timer_id, config.input.cursor_timeout / 2, nullptr);
      else
        SetTimer (hWnd, last_mouse.timer_id, 250/*USER_TIMER_MINIMUM*/, nullptr);

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

HRESULT
WINAPI
DXGISwap_ResizeBuffers_Detour (
   IDXGISwapChain *This,
   UINT            BufferCount,
   UINT            Width,
   UINT            Height,
   DXGI_FORMAT     NewFormat,
   UINT            SwapChainFlags
)
{
  DXGI_SWAP_CHAIN_DESC desc;

  if (SUCCEEDED (This->GetDesc (&desc))) {
    if (desc.OutputWindow == SK_GetGameWindow ())
      pGameSwapChain = This;
  }

  bool should_center = pGameSwapChain == This && config.window.center;

  if (config.display.enable_fullscreen) {
    BOOL fullscreen = FALSE;

    if (pGameSwapChain) {
      if (SUCCEEDED (pGameSwapChain->GetFullscreenState (&fullscreen, nullptr))) {
        if (fullscreen) // Moving the window in fullscreen mode is bad mojo
          should_center = false;
      }
    }
  }


  if (pGameSwapChain == This /*&& config.display.enable_fullscreen*/) {
    //
    // Allow Alt+Enter
    //

    CComPtr <ID3D11Device> pDev;
    CComPtr <IDXGIDevice>  pDevDXGI;
    CComPtr <IDXGIAdapter> pAdapter;
    CComPtr <IDXGIFactory> pFactory;

    if ( SUCCEEDED (pGameSwapChain->GetDevice  (IID_PPV_ARGS (&pDev))     ) &&
         SUCCEEDED (pDev->QueryInterface       (IID_PPV_ARGS (&pDevDXGI)) ) &&
         SUCCEEDED (pDevDXGI->GetAdapter                     (&pAdapter)  ) &&
         SUCCEEDED (      pAdapter->GetParent  (IID_PPV_ARGS (&pFactory)) ) )
    {
      dll_log.Log ( L"[Fullscreen] Setting DXGI Window Association "
                    L"(HWND: Game=%X,SwapChain=%X - flags=DXGI_MWA_NO_WINDOW_CHANGES)",
                      SK_GetGameWindow (), desc.OutputWindow );

      pFactory->MakeWindowAssociation (desc.OutputWindow, DXGI_MWA_NO_WINDOW_CHANGES |
                                                          DXGI_MWA_NO_ALT_ENTER );

      // Allow Fullscreen
      if (config.display.enable_fullscreen)
        SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
      else
        SwapChainFlags = 0x00;
    }
  }

  HRESULT hr =
    DXGISwap_ResizeBuffers_Original (This, BufferCount, Width, Height, NewFormat, SwapChainFlags);

  if (should_center) {
    MONITORINFO moninfo;
    moninfo.cbSize = sizeof MONITORINFO;

    GetMonitorInfo (MonitorFromWindow (SK_GetGameWindow (), MONITOR_DEFAULTTONEAREST), &moninfo);

    int mon_width  = moninfo.rcMonitor.right  - moninfo.rcMonitor.left;
    int mon_height = moninfo.rcMonitor.bottom - moninfo.rcMonitor.top;

    if (Width <= mon_width && Height <= mon_height) {
      SetWindowPos ( SK_GetGameWindow (), HWND_NOTOPMOST, (mon_width  - Width)  / 2,
                                                          (mon_height - Height) / 2,
                       Width, Height,
                        SWP_NOSIZE | SWP_NOSENDCHANGING );
    }
  }

  return hr;
}

typedef HRESULT (WINAPI *SK_BeginBufferSwap_pfn)(void);
SK_BeginBufferSwap_pfn SK_BeginBufferSwap_Original = nullptr;

HRESULT
WINAPI
SK_BeginBufferSwap_Detour (void)
{
  if (unx::window.render_thread == 0)
    unx::window.render_thread = GetCurrentThreadId ();

#if 0
  CComPtr <IDXGIOutput> pOut;

  if (            pGameSwapChain != nullptr &&
       SUCCEEDED (pGameSwapChain->GetContainingOutput (&pOut)) ) {
    pOut->WaitForVBlank ();
  }
#endif

  return SK_BeginBufferSwap_Original ();
}

void
UNX_InstallWindowHook (HWND hWnd)
{
  unx::window.hwnd = hWnd;

  UNX_CreateDLLHook ( config.system.injector.c_str (),
                        "SK_BeginBufferSwap",
                         SK_BeginBufferSwap_Detour,
              (LPVOID *)&SK_BeginBufferSwap_Original );

  UNX_CreateDLLHook ( config.system.injector.c_str (),
                      "SK_DetourWindowProc",
                       DetourWindowProc,
            (LPVOID *)&DetourWindowProc_Original );

  UNX_CreateDLLHook ( config.system.injector.c_str (),
                      "DXGISwap_ResizeBuffers_Override",
                       DXGISwap_ResizeBuffers_Detour,
            (LPVOID *)&DXGISwap_ResizeBuffers_Original );
}





void
unx::WindowManager::Init (void)
{
//  CommandProcessor* comm_proc = CommandProcessor::getInstance ();
}

void
unx::WindowManager::Shutdown (void)
{
  unx::CheatManager::Shutdown ();
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