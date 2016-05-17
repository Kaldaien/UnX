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
#define DIRECTINPUT_VERSION 0x0800

#include <Windows.h>
#include <cstdint>

#include <comdef.h>

#include <dinput.h>
#pragma comment (lib, "dxguid.lib")

#include "log.h"
#include "config.h"
#include "window.h"
#include "hook.h"

#include "input.h"

#include <mmsystem.h>
#pragma comment (lib, "winmm.lib")

///////////////////////////////////////////////////////////////////////////////
//
// DirectInput 8
//
///////////////////////////////////////////////////////////////////////////////
typedef HRESULT (WINAPI *IDirectInput8_CreateDevice_pfn)(
  IDirectInput8       *This,
  REFGUID              rguid,
  LPDIRECTINPUTDEVICE *lplpDirectInputDevice,
  LPUNKNOWN            pUnkOuter
);

typedef HRESULT (WINAPI *DirectInput8Create_pfn)(
  HINSTANCE  hinst,
  DWORD      dwVersion,
  REFIID     riidltf,
  LPVOID    *ppvOut,
  LPUNKNOWN  punkOuter
);

typedef HRESULT (WINAPI *IDirectInputDevice8_GetDeviceState_pfn)(
  LPDIRECTINPUTDEVICE  This,
  DWORD                cbData,
  LPVOID               lpvData
);

typedef HRESULT (WINAPI *IDirectInputDevice8_SetCooperativeLevel_pfn)(
  LPDIRECTINPUTDEVICE  This,
  HWND                 hwnd,
  DWORD                dwFlags
);

DirectInput8Create_pfn
        DirectInput8Create_Original                      = nullptr;
LPVOID
        DirectInput8Create_Hook                          = nullptr;
IDirectInput8_CreateDevice_pfn
        IDirectInput8_CreateDevice_Original              = nullptr;
IDirectInputDevice8_GetDeviceState_pfn
        IDirectInputDevice8_GetDeviceState_Original      = nullptr;
IDirectInputDevice8_SetCooperativeLevel_pfn
        IDirectInputDevice8_SetCooperativeLevel_Original = nullptr;



#define DINPUT8_CALL(_Ret, _Call) {                                     \
  dll_log.LogEx (true, L"[   Input  ]  Calling original function: ");   \
  (_Ret) = (_Call);                                                     \
  _com_error err ((_Ret));                                              \
  if ((_Ret) != S_OK)                                                   \
    dll_log.LogEx (false, L"(ret=0x%04x - %s)\n", err.WCode (),         \
                                                  err.ErrorMessage ()); \
  else                                                                  \
    dll_log.LogEx (false, L"(ret=S_OK)\n");                             \
}

#define __PTR_SIZE   sizeof LPCVOID 
#define __PAGE_PRIVS PAGE_EXECUTE_READWRITE 
 
#define DI8_VIRTUAL_OVERRIDE(_Base,_Index,_Name,_Override,_Original,_Type) {   \
   void** vftable = *(void***)*_Base;                                          \
                                                                               \
   if (vftable [_Index] != _Override) {                                        \
     DWORD dwProtect;                                                          \
                                                                               \
     VirtualProtect (&vftable [_Index], __PTR_SIZE, __PAGE_PRIVS, &dwProtect); \
                                                                               \
     /*if (_Original == NULL)                                                */\
       _Original = (##_Type)vftable [_Index];                                  \
                                                                               \
     vftable [_Index] = _Override;                                             \
                                                                               \
     VirtualProtect (&vftable [_Index], __PTR_SIZE, dwProtect, &dwProtect);    \
                                                                               \
  }                                                                            \
 }



struct di8_keyboard_s {
  LPDIRECTINPUTDEVICE pDev = nullptr;
  uint8_t             state [512]; // Handle overrun just in case
} _dik;

struct di8_mouse_s {
  LPDIRECTINPUTDEVICE pDev = nullptr;
  DIMOUSESTATE2       state;
} _dim;

// I don't care about joysticks, let them continue working while
//   the window does not have focus...

HRESULT
WINAPI
IDirectInputDevice8_GetDeviceState_Detour ( LPDIRECTINPUTDEVICE        This,
                                            DWORD                      cbData,
                                            LPVOID                     lpvData )
{
#if 0
  // For input faking (keyboard)
  if ((! unx::window.active) && config.render.allow_background && This == _dik.pDev) {
    memcpy (lpvData, _dik.state, cbData);
    return S_OK;
  }

  // For input faking (mouse)
  if ((! unx::window.active) && config.render.allow_background && This == _dim.pDev) {
    memcpy (lpvData, &_dim.state, cbData);
    return S_OK;
  }

  HRESULT hr;
  hr = IDirectInputDevice8_GetDeviceState_Original ( This,
                                                       cbData,
                                                         lpvData );

  if (SUCCEEDED (hr)) {
    if (unx::window.active && This == _dim.pDev) {
//
// This is only for mouselook, etc. That stuff works fine without aspect ratio correction.
//
//#define FIX_DINPUT8_MOUSE
#ifdef FIX_DINPUT8_MOUSE
      if (cbData == sizeof (DIMOUSESTATE) || cbData == sizeof (DIMOUSESTATE2)) {
        POINT mouse_pos { ((DIMOUSESTATE *)lpvData)->lX,
                          ((DIMOUSESTATE *)lpvData)->lY };

        unx::InputManager::CalcCursorPos (&mouse_pos);

        ((DIMOUSESTATE *)lpvData)->lX = mouse_pos.x;
        ((DIMOUSESTATE *)lpvData)->lY = mouse_pos.y;
      }
#endif
      memcpy (&_dim.state, lpvData, cbData);
    }

    else if (unx::window.active && This == _dik.pDev) {
      memcpy (&_dik.state, lpvData, cbData);
    }
  }

  return hr;
#else
  return S_OK;
#endif
}

HRESULT
WINAPI
IDirectInputDevice8_SetCooperativeLevel_Detour ( LPDIRECTINPUTDEVICE  This,
                                                 HWND                 hwnd,
                                                 DWORD                dwFlags )
{
#if 0
  if (config.input.block_windows)
    dwFlags |= DISCL_NOWINKEY;

  if (config.render.allow_background) {
    dwFlags &= ~DISCL_EXCLUSIVE;
    dwFlags &= ~DISCL_BACKGROUND;

    dwFlags |= DISCL_NONEXCLUSIVE;
    dwFlags |= DISCL_FOREGROUND;

    return IDirectInputDevice8_SetCooperativeLevel_Original (This, hwnd, dwFlags);
  }
#endif

  return IDirectInputDevice8_SetCooperativeLevel_Original (This, hwnd, dwFlags);
}

HRESULT
WINAPI
IDirectInput8_CreateDevice_Detour ( IDirectInput8       *This,
                                    REFGUID              rguid,
                                    LPDIRECTINPUTDEVICE *lplpDirectInputDevice,
                                    LPUNKNOWN            pUnkOuter )
{
  const wchar_t* wszDevice = (rguid == GUID_SysKeyboard) ? L"Default System Keyboard" :
                                (rguid == GUID_SysMouse) ? L"Default System Mouse" :
                                                           L"Other Device";

  dll_log.Log ( L"[   Input  ][!] IDirectInput8::CreateDevice (%08Xh, %s, %08Xh, %08Xh)",
                  This,
                    wszDevice,
                      lplpDirectInputDevice,
                        pUnkOuter );

  HRESULT hr;
  DINPUT8_CALL ( hr,
                  IDirectInput8_CreateDevice_Original ( This,
                                                         rguid,
                                                          lplpDirectInputDevice,
                                                           pUnkOuter ) );

  if (SUCCEEDED (hr)) {
#if 1
      void** vftable = *(void***)*lplpDirectInputDevice;

#if 0
      UNX_CreateFuncHook ( L"IDirectInputDevice8::GetDeviceState",
                           vftable [9],
                           IDirectInputDevice8_GetDeviceState_Detour,
                 (LPVOID*)&IDirectInputDevice8_GetDeviceState_Original );

      UNX_EnableHook (vftable [9]);
#endif

      UNX_CreateFuncHook ( L"IDirectInputDevice8::SetCooperativeLevel",
                           vftable [13],
                           IDirectInputDevice8_SetCooperativeLevel_Detour,
                 (LPVOID*)&IDirectInputDevice8_SetCooperativeLevel_Original );

      UNX_EnableHook (vftable [13]);
#else
     DI8_VIRTUAL_OVERRIDE ( lplpDirectInputDevice, 9,
                            L"IDirectInputDevice8::GetDeviceState",
                            IDirectInputDevice8_GetDeviceState_Detour,
                            IDirectInputDevice8_GetDeviceState_Original,
                            IDirectInputDevice8_GetDeviceState_pfn );

     DI8_VIRTUAL_OVERRIDE ( lplpDirectInputDevice, 13,
                            L"IDirectInputDevice8::SetCooperativeLevel",
                            IDirectInputDevice8_SetCooperativeLevel_Detour,
                            IDirectInputDevice8_SetCooperativeLevel_Original,
                            IDirectInputDevice8_SetCooperativeLevel_pfn );
#endif

    if (rguid == GUID_SysMouse)
      _dim.pDev = *lplpDirectInputDevice;
    else if (rguid == GUID_SysKeyboard)
      _dik.pDev = *lplpDirectInputDevice;
  }

  return hr;
}

HRESULT
WINAPI
DirectInput8Create_Detour ( HINSTANCE  hinst,
                            DWORD      dwVersion,
                            REFIID     riidltf,
                            LPVOID    *ppvOut,
                            LPUNKNOWN  punkOuter )
{
  dll_log.Log ( L"[   Input  ][!] DirectInput8Create (0x%X, %lu, ..., %08Xh, %08Xh)",
                  hinst, dwVersion, /*riidltf,*/ ppvOut, punkOuter );

  HRESULT hr;
  DINPUT8_CALL (hr,
    DirectInput8Create_Original ( hinst,
                                    dwVersion,
                                      riidltf,
                                        ppvOut,
                                          punkOuter ));

  if (hinst != GetModuleHandle (nullptr)) {
    dll_log.Log (L"[   Input  ] >> A third-party DLL is manipulating DirectInput 8; will not hook.");
    return hr;
  }

  // Avoid multiple hooks for third-party compatibility
  static bool hooked = false;

  if (SUCCEEDED (hr) && (! hooked)) {
#if 1
    void** vftable = *(void***)*ppvOut;

    UNX_CreateFuncHook ( L"IDirectInput8::CreateDevice",
                         vftable [3],
                         IDirectInput8_CreateDevice_Detour,
               (LPVOID*)&IDirectInput8_CreateDevice_Original );

    UNX_EnableHook (vftable [3]);
#else
     DI8_VIRTUAL_OVERRIDE ( ppvOut, 3,
                            L"IDirectInput8::CreateDevice",
                            IDirectInput8_CreateDevice_Detour,
                            IDirectInput8_CreateDevice_Original,
                            IDirectInput8_CreateDevice_pfn );
#endif
    hooked = true;
  }

  return hr;
}





typedef struct _XINPUT_GAMEPAD {
  WORD  wButtons;
  BYTE  bLeftTrigger;
  BYTE  bRightTrigger;
  SHORT sThumbLX;
  SHORT sThumbLY;
  SHORT sThumbRX;
  SHORT sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE {
  DWORD          dwPacketNumber;
  XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef DWORD (WINAPI *XInputGetState_pfn)(
  _In_  DWORD        dwUserIndex,
  _Out_ XINPUT_STATE *pState
);

XInputGetState_pfn XInputGetState = nullptr;

bool
IsControllerPluggedIn (UINT uJoyID)
{
 if (uJoyID == (UINT)-1)
    return true;

  XINPUT_STATE xstate;

  static DWORD last_poll = timeGetTime ();
  static DWORD dwRet     = XInputGetState (uJoyID, &xstate);

  // This function is actually a performance hazzard when no controllers
  //   are plugged in, so ... throttle the sucker.
  if (last_poll < timeGetTime () - 500UL)
    dwRet = XInputGetState (uJoyID, &xstate);

  if (dwRet == ERROR_DEVICE_NOT_CONNECTED)
    return false;

  return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// User32 Input APIs
//
///////////////////////////////////////////////////////////////////////////////
typedef UINT (WINAPI *GetRawInputData_pfn)(
  _In_      HRAWINPUT hRawInput,
  _In_      UINT      uiCommand,
  _Out_opt_ LPVOID    pData,
  _Inout_   PUINT     pcbSize,
  _In_      UINT      cbSizeHeader
);

GetAsyncKeyState_pfn GetAsyncKeyState_Original = nullptr;
GetRawInputData_pfn  GetRawInputData_Original  = nullptr;

UINT
WINAPI
GetRawInputData_Detour (_In_      HRAWINPUT hRawInput,
                        _In_      UINT      uiCommand,
                        _Out_opt_ LPVOID    pData,
                        _Inout_   PUINT     pcbSize,
                        _In_      UINT      cbSizeHeader)
{
  if (config.input.block_all_keys) {
    *pcbSize = 0;
    return 0;
  }

  int size = GetRawInputData_Original (hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);

  // Block keyboard input to the game while the console is active
  if (unx::InputManager::Hooker::getInstance ()->isVisible () && uiCommand == RID_INPUT) {
    *pcbSize = 0;
    return 0;
  }

  return size;
}

SHORT
WINAPI
GetAsyncKeyState_Detour (_In_ int vKey)
{
#define UNX_ConsumeVKey(vKey) { GetAsyncKeyState_Original(vKey); return 0; }

#if 0
  // Window is not active, but we are faking it...
  if ((! unx::window.active) && config.render.allow_background)
    UNX_ConsumeVKey (vKey);
#endif

  // Block keyboard input to the game while the console is active
  if (unx::InputManager::Hooker::getInstance ()->isVisible ()) {
    UNX_ConsumeVKey (vKey);
  }

#if 0
  // Block Left Alt
  if (vKey == VK_LMENU)
    if (config.input.block_left_alt)
      UNX_ConsumeVKey (vKey);

  // Block Left Ctrl
  if (vKey == VK_LCONTROL)
    if (config.input.block_left_ctrl)
      UNX_ConsumeVKey (vKey);
#endif

  return GetAsyncKeyState_Original (vKey);
}

void
HookRawInput (void)
{
#if 0
  // Defer installation of this hook until DirectInput8 is setup
  if (GetRawInputData_Original == nullptr) {
    dll_log.LogEx (true, L"[   Input  ] Installing Deferred Hook: \"GetRawInputData (...)\"... ");
    MH_STATUS status =
      UNX_CreateDLLHook ( L"user32.dll", "GetRawInputData",
                          GetRawInputData_Detour,
                (LPVOID*)&GetRawInputData_Original );
   dll_log.LogEx (false, L"%hs\n", MH_StatusToString (status));
  }
#endif
}



void
unx::InputManager::Init (void)
{
#if 0
  //
  // For this game, the only reason we hook this is to block the Windows key.
  //
  if (config.input.block_windows) {
    //
    // We only hook one DLL export from DInput8, all other DInput stuff is
    //   handled through virtual function table overrides
    //
    UNX_CreateDLLHook ( L"dinput8.dll", "DirectInput8Create",
                        DirectInput8Create_Detour,
              (LPVOID*)&DirectInput8Create_Original,
              (LPVOID*)&DirectInput8Create_Hook );
    UNX_EnableHook    (DirectInput8Create_Hook);
  }

  //
  // Win32 API Input Hooks
  //

  HookRawInput ();

  UNX_CreateDLLHook ( L"user32.dll", "GetAsyncKeyState",
                      GetAsyncKeyState_Detour,
            (LPVOID*)&GetAsyncKeyState_Original );
#endif

  HMODULE hModXInput13 =
    LoadLibraryW (L"XInput1_3.dll");

  XInputGetState =
    (XInputGetState_pfn)
      GetProcAddress (hModXInput13, "XInputGetState");

  unx::InputManager::Hooker* pHook =
    unx::InputManager::Hooker::getInstance ();

  pHook->Start ();
}

void
unx::InputManager::Shutdown (void)
{
  unx::InputManager::Hooker* pHook =
    unx::InputManager::Hooker::getInstance ();

  pHook->End ();
}


void
unx::InputManager::Hooker::Start (void)
{
  hMsgPump =
    CreateThread ( NULL,
                     NULL,
                       Hooker::MessagePump,
                         &hooks,
                           NULL,
                             NULL );
}

void
unx::InputManager::Hooker::End (void)
{
  TerminateThread     (hMsgPump, 0);
  UnhookWindowsHookEx (hooks.keyboard);
  UnhookWindowsHookEx (hooks.mouse);
}

std::string console_text;
std::string mod_text ("");

void
unx::InputManager::Hooker::Draw (void)
{
  typedef BOOL (__stdcall *BMF_DrawExternalOSD_t)(std::string app_name, std::string text);

  static HMODULE               hMod =
    GetModuleHandle (config.system.injector.c_str ());
  static BMF_DrawExternalOSD_t BMF_DrawExternalOSD
    =
    (BMF_DrawExternalOSD_t)GetProcAddress (hMod, "BMF_DrawExternalOSD");

  std::string output;

  static DWORD last_time = timeGetTime ();
  static bool  carret    = true;

  if (visible) {
    output += text;

    // Blink the Carret
    if (timeGetTime () - last_time > 333) {
      carret = ! carret;

      last_time = timeGetTime ();
    }

    if (carret)
      output += "-";

    // Show Command Results
    if (command_issued) {
      output += "\n";
      output += result_str;
    }
  }

  console_text = output;

  output += "\n";
  output += mod_text;

  BMF_DrawExternalOSD ("UnX", output.c_str ());
}

DWORD
WINAPI
unx::InputManager::Hooker::MessagePump (LPVOID hook_ptr)
{
  hooks_s* pHooks = (hooks_s *)hook_ptr;

  ZeroMemory (text, 4096);

  text [0] = '>';

  extern    HMODULE hDLLMod;

  DWORD dwThreadId;

  int hits = 0;

  DWORD dwTime = timeGetTime ();

  while (true) {
    DWORD dwProc;

    dwThreadId =
      GetWindowThreadProcessId (GetForegroundWindow (), &dwProc);

    // Ugly hack, but a different window might be in the foreground...
    if (dwProc != GetCurrentProcessId ()) {
      //dll_log.Log (L" *** Tried to hook the wrong process!!!");
      Sleep (500);
      continue;
    }

    break;
  }

#if 0
  // Defer initialization of the Window Message redirection stuff until
  //   we have an actual window!
  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();
  pCommandProc->ProcessCommandFormatted ("TargetFPS %f", config.window.foreground_fps);
#endif

  dll_log.Log ( L"[   Input  ] # Found window in %03.01f seconds, "
                    L"installing keyboard hook...",
                  (float)(timeGetTime () - dwTime) / 1000.0f );

  extern void UNX_InstallWindowHook (HWND hWnd);
  UNX_InstallWindowHook (GetForegroundWindow ());

  dwTime = timeGetTime ();
  hits   = 1;

#if 0
  while (! (pHooks->keyboard = SetWindowsHookEx ( WH_KEYBOARD,
                                                    KeyboardProc,
                                                      hDLLMod,
                                                        dwThreadId ))) {
    _com_error err (HRESULT_FROM_WIN32 (GetLastError ()));

    dll_log.Log ( L"[   Input  ] @ SetWindowsHookEx failed: 0x%04X (%s)",
                  err.WCode (), err.ErrorMessage () );

    ++hits;

    if (hits >= 5) {
      dll_log.Log ( L"[   Input  ] * Failed to install keyboard hook after %lu tries... "
        L"bailing out!",
        hits );
      return 0;
    }

    Sleep (1);
  }
#endif

  dll_log.Log ( L"[   Input  ] * Installed keyboard hook for command console... "
                      L"%lu %s (%lu ms!)",
                hits,
                  hits > 1 ? L"tries" : L"try",
                    timeGetTime () - dwTime );

  if ((! config.input.four_finger_salute) || (! XInputGetState))
    return 0;

#define XI_POLL_INTERVAL 500UL

  DWORD  xi_ret       = 0;
  DWORD  last_xi_poll = timeGetTime () - (XI_POLL_INTERVAL + 1UL);

  BOOL need_release = FALSE;

  INPUT* keys  =
     new INPUT [2];

  while (true) {
    if (need_release) {
      keys [1].type           = INPUT_KEYBOARD;
      keys [1].ki.wVk         = 0;

      keys [1].ki.wScan       = 0x1;
      keys [1].ki.dwFlags     = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
      keys [1].ki.time        = 0;
      keys [1].ki.dwExtraInfo = 0;

      SendInput (1, &keys [1], sizeof INPUT);
    }

    int slot = config.input.gamepad_slot;
    if (slot == -1)
      slot = 0;

    XINPUT_STATE xi_state;

    // This function is actually a performance hazzard when no controllers
    //   are plugged in, so ... throttle the sucker.
    if (  xi_ret != ERROR_DEVICE_NOT_CONNECTED ||
         (last_xi_poll < timeGetTime () - XI_POLL_INTERVAL) ) {
      xi_ret       = XInputGetState (slot, &xi_state);
      last_xi_poll = timeGetTime    ();

#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK  0x0020
    }

    bool four_finger =
        xi_state.Gamepad.bLeftTrigger  &&
        xi_state.Gamepad.bRightTrigger &&
      ( xi_state.Gamepad.wButtons & ( XINPUT_GAMEPAD_START |
                                      XINPUT_GAMEPAD_BACK ) );

    if (four_finger) {
      keys [0].type           = INPUT_KEYBOARD;
      keys [0].ki.wVk         = 0;

      keys [0].ki.wScan       = 0x1;
      keys [0].ki.dwFlags     = KEYEVENTF_SCANCODE;
      keys [0].ki.time        = 0;
      keys [0].ki.dwExtraInfo = 0;

      SendInput (1, &keys [0], sizeof INPUT);

      need_release = TRUE;
      Sleep (10);
      continue;
    }

    need_release = FALSE;
    Sleep (10);
  }

  delete [] keys;

  return 0;
}

LRESULT
CALLBACK
unx::InputManager::Hooker::MouseProc (int nCode, WPARAM wParam, LPARAM lParam)
{
  MOUSEHOOKSTRUCT* pmh = (MOUSEHOOKSTRUCT *)lParam;

  return CallNextHookEx (Hooker::getInstance ()->hooks.mouse, nCode, wParam, lParam);
}

LRESULT
CALLBACK
unx::InputManager::Hooker::KeyboardProc (int nCode, WPARAM wParam, LPARAM lParam)
{
  if (nCode == 0 /*nCode >= 0 && (nCode != HC_NOREMOVE)*/) {
    BYTE    vkCode   = LOWORD (wParam) & 0xFF;
    BYTE    scanCode = HIWORD (lParam) & 0x7F;
    SHORT   repeated = LOWORD (lParam);
    bool    keyDown  = ! (lParam & 0x80000000);

    if (visible && vkCode == VK_BACK) {
      if (keyDown) {
        size_t len = strlen (text);
                len--;

        if (len < 1)
          len = 1;

        text [len] = '\0';
      }
    }

    // We don't want to distinguish between left and right on these keys, so alias the stuff
    else if ((vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT)) {
      if (keyDown) keys_ [VK_SHIFT] = 0x81; else keys_ [VK_SHIFT] = 0x00;
    }

    else if ((vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_MENU)) {
      if (keyDown) keys_ [VK_MENU] = 0x81; else keys_ [VK_MENU] = 0x00;
    }

    else if ((!repeated) && vkCode == VK_CAPITAL) {
      if (keyDown) if (keys_ [VK_CAPITAL] == 0x00) keys_ [VK_CAPITAL] = 0x81; else keys_ [VK_CAPITAL] = 0x00;
    }

    else if ((vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL)) {
      if (keyDown) keys_ [VK_CONTROL] = 0x81; else keys_ [VK_CONTROL] = 0x00;
    }

    else if ((vkCode == VK_UP) || (vkCode == VK_DOWN)) {
      if (keyDown && visible) {
        if (vkCode == VK_UP)
          commands.idx--;
        else
          commands.idx++;

        // Clamp the index
        if (commands.idx < 0)
          commands.idx = 0;
        else if (commands.idx >= commands.history.size ())
          commands.idx = commands.history.size () - 1;

        if (commands.history.size ()) {
          strcpy (&text [1], commands.history [commands.idx].c_str ());
          command_issued = false;
        }
      }
    }

    else if (visible && vkCode == VK_RETURN) {
      if (keyDown && LOWORD (lParam) < 2) {
        size_t len = strlen (text+1);
        // Don't process empty or pure whitespace command lines
        if (len > 0 && strspn (text+1, " ") != len) {
          eTB_CommandResult result = SK_GetCommandProcessor ()->ProcessCommandLine (text+1);

          if (result.getStatus ()) {
            // Don't repeat the same command over and over
            if (commands.history.size () == 0 ||
                commands.history.back () != &text [1]) {
              commands.history.push_back (&text [1]);
            }

            commands.idx = commands.history.size ();

            text [1] = '\0';

            command_issued = true;
          }
          else {
            command_issued = false;
          }

          result_str = result.getWord   () + std::string (" ")   +
                       result.getArgs   () + std::string (":  ") +
                       result.getResult ();
        }
      }
    }

    else if (keyDown) {
      eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();

      bool new_press = keys_ [vkCode] != 0x81;

      keys_ [vkCode] = 0x81;

      if (keys_ [VK_CONTROL] && keys_ [VK_SHIFT]) {
        if ( keys_ [VK_TAB]         && 
               ( vkCode == VK_CONTROL ||
                 vkCode == VK_SHIFT   ||
                 vkCode == VK_TAB ) &&
             new_press ) {
          visible = ! visible;

          // Avoid duplicating a BMF feature
          static HMODULE hD3D9 = GetModuleHandle (config.system.injector.c_str ());

          typedef void (__stdcall *BMF_SteamAPI_SetOverlayState_t)(bool);
          static BMF_SteamAPI_SetOverlayState_t BMF_SteamAPI_SetOverlayState =
              (BMF_SteamAPI_SetOverlayState_t)
                GetProcAddress ( hD3D9,
                                    "BMF_SteamAPI_SetOverlayState" );

          BMF_SteamAPI_SetOverlayState (visible);

          // Prevent the Steam Overlay from being a real pain
          return -1;
        }

        if (keys_ [VK_MENU] && vkCode == 'L' && new_press) {
          pCommandProc->ProcessCommandLine ("Trace.NumFrames 1");
          pCommandProc->ProcessCommandLine ("Trace.Enable true");
        }

// Not really that useful, and we want the 'B' key for something else
#if 0
        else if (keys_ [VK_MENU] && vkCode == 'B' && new_press) {
          pCommandProc->ProcessCommandLine ("Render.AllowBG toggle");
        }
#endif
        else if (vkCode == 'H' && new_press) {
          pCommandProc->ProcessCommandLine ("Render.HighPrecisionSSAO toggle");
        }

        else if (vkCode == VK_OEM_COMMA && new_press) {
          pCommandProc->ProcessCommandLine ("Render.MSAA toggle");
        }

        else if (vkCode == 'Z' && new_press) {
          pCommandProc->ProcessCommandLine ("Render.TaskTiming toggle");
        }

        else if (vkCode == 'X' && new_press) {
          pCommandProc->ProcessCommandLine ("Render.AggressiveOpt toggle");
        }

        else if (vkCode == '1' && new_press) {
          pCommandProc->ProcessCommandLine ("Window.ForegroundFPS 60.0");
        }

        else if (vkCode == '2' && new_press) {
          pCommandProc->ProcessCommandLine ("Window.ForegroundFPS 30.0");
        }

        else if (vkCode == '3' && new_press) {
          pCommandProc->ProcessCommandLine ("Window.ForegroundFPS 0.0");
        }

        else if (vkCode == VK_OEM_PERIOD && new_press) {
          pCommandProc->ProcessCommandLine ("Render.FringeRemoval toggle");
        }
      }

      // Don't print the tab character, it's pretty useless.
      if (visible && vkCode != VK_TAB) {
        char key_str [2];
        key_str [1] = '\0';

        if (1 == ToAsciiEx ( vkCode,
                              scanCode,
                              keys_,
                            (LPWORD)key_str,
                              0,
                              GetKeyboardLayout (0) ) &&
             isprint (*key_str) ) {
          strncat (text, key_str, 1);
          command_issued = false;
        }
      }
    }

    else if ((! keyDown)) {
      bool new_release = keys_ [vkCode] != 0x00;

      keys_ [vkCode] = 0x00;
    }

    if (visible) return -1;
  }

  return CallNextHookEx (Hooker::getInstance ()->hooks.keyboard, nCode, wParam, lParam);
};


void
UNX_DrawCommandConsole (void)
{
  static int draws = 0;

  // Skip the first several frames, so that the console appears below the
  //  other OSD.
  if (draws++ > 20) {
    unx::InputManager::Hooker* pHook =
      unx::InputManager::Hooker::getInstance ();
    pHook->Draw ();
  }
}


unx::InputManager::Hooker* unx::InputManager::Hooker::pInputHook = nullptr;

char                       unx::InputManager::Hooker::text [4096];

BYTE                       unx::InputManager::Hooker::keys_ [256]    = { 0 };
bool                       unx::InputManager::Hooker::visible        = false;

bool                       unx::InputManager::Hooker::command_issued = false;
std::string                unx::InputManager::Hooker::result_str;

unx::InputManager::Hooker::command_history_s
                          unx::InputManager::Hooker::commands;