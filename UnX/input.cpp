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
#define _CRT_NON_CONFORMING_WCSTOK
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
#include "parameter.h"

#include "input.h"

#include <mmsystem.h>
#pragma comment (lib, "winmm.lib")

#include <process.h>

typedef void (WINAPI *SK_D3D11_AddTexHash_pfn)(const wchar_t*, uint32_t, uint32_t);
extern SK_D3D11_AddTexHash_pfn SK_D3D11_AddTexHash;

typedef HWND (WINAPI *SK_GetGameWindow_pfn)(void);
SK_GetGameWindow_pfn SK_GetGameWindow;

struct unx_gamepad_s {
  std::wstring tex_set = L"PlayStation_Glossy";
  bool         legacy  = false;

  struct combo_s {
    std::wstring unparsed = L"";
    int          buttons  = 0;
    int          button0  = 0xffffffff;
    int          button1  = 0xffffffff;
    int          button2  = 0xffffffff;
    bool         lt       = false;
    bool         rt       = false;
  } f1, f2, f3, f4, f5, screenshot, fullscreen, esc, speedboost, kickstart;

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
} gamepad;

extern void UNX_InstallWindowHook (HWND hWnd);

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

IDirectInput8_CreateDevice_pfn
        IDirectInput8_CreateDevice_Original              = nullptr;
IDirectInputDevice8_GetDeviceState_pfn
        IDirectInputDevice8_GetDeviceState_Original      = nullptr;
IDirectInputDevice8_SetCooperativeLevel_pfn
        IDirectInputDevice8_SetCooperativeLevel_Original = nullptr;



#define DINPUT8_CALL(_Ret, _Call) {                                     \
  dll_log->LogEx (true, L"[   Input  ]  Calling original function: ");  \
  (_Ret) = (_Call);                                                     \
  _com_error err ((_Ret));                                              \
  if ((_Ret) != S_OK)                                                   \
    dll_log->LogEx (false, L"(ret=0x%04x - %s)\n", err.WCode (),        \
                                                  err.ErrorMessage ()); \
  else                                                                  \
    dll_log->LogEx (false, L"(ret=S_OK)\n");                            \
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
     if (_Original == NULL)                                                    \
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
  uint8_t             state [512];
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
  HRESULT hr;
  hr = IDirectInputDevice8_GetDeviceState_Original ( This,
                                                       cbData,
                                                         lpvData );

  if (SUCCEEDED (hr)) {
    if (cbData == sizeof DIJOYSTATE2) {
      DIJOYSTATE2* out = (DIJOYSTATE2 *)lpvData;

      // Fix Wonky Input Behavior When Window is Not In Foreground
      if (! unx::window.active/*GetForegroundWindow () != SK_GetGameWindow ()*/) {
        memset (out, 0, sizeof DIJOYSTATE2);

        out->rgdwPOV [0] = -1;
        out->rgdwPOV [1] = -1;
        out->rgdwPOV [2] = -1;
        out->rgdwPOV [3] = -1;
      }

      else {
        DIJOYSTATE2  in  = *out;

        for (int i = 0; i < 12; i++) {
          // Negative values are for axes, we cannot remap those yet
          if (gamepad.remap.map [ i ] >= 0) {
            out->rgbButtons [ i ] = 
              in.rgbButtons [ gamepad.remap.map [ i ] ];
          }
        }
      }
    }
  }

  //
  // Keyboard
  //
  if (This == _dik.pDev) {
    if (unx::window.active) {
      memcpy (_dik.state, lpvData, cbData);
    } else {
      memcpy (lpvData, _dik.state, cbData);

      ((uint8_t *)lpvData) [DIK_LALT]   = 0x0;
      ((uint8_t *)lpvData) [DIK_RALT]   = 0x0;
      ((uint8_t *)lpvData) [DIK_TAB]    = 0x0;
      ((uint8_t *)lpvData) [DIK_ESCAPE] = 0x0;
      ((uint8_t *)lpvData) [DIK_UP]     = 0x0;
      ((uint8_t *)lpvData) [DIK_DOWN]   = 0x0;
      ((uint8_t *)lpvData) [DIK_LEFT]   = 0x0;
      ((uint8_t *)lpvData) [DIK_RIGHT]  = 0x0;
      ((uint8_t *)lpvData) [DIK_RETURN] = 0x0;
      ((uint8_t *)lpvData) [DIK_LMENU]  = 0x0;
      ((uint8_t *)lpvData) [DIK_RMENU]  = 0x0;
    }
  }

  //
  // Mouse
  //
  if (This == _dim.pDev) {
    if (unx::window.active) {
      memcpy (&_dim.state, lpvData, cbData);
    } else {
      memcpy (lpvData, &_dim.state, cbData);
    }
  }

  return hr;
}

HRESULT
WINAPI
IDirectInputDevice8_SetCooperativeLevel_Detour ( LPDIRECTINPUTDEVICE  This,
                                                 HWND                 hwnd,
                                                 DWORD                dwFlags )
{
  if (config.input.block_windows)
    dwFlags |= DISCL_NOWINKEY;

#if 0
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

  dll_log->Log ( L"[   Input  ][!] IDirectInput8::CreateDevice (%08Xh, %s, %08Xh, %08Xh)",
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

  static bool hooked = false;

  if (rguid != GUID_SysMouse && rguid != GUID_SysKeyboard && SUCCEEDED (hr) && (! hooked)) {
#if 1
      hooked = true;

      void** vftable = *(void***)*lplpDirectInputDevice;

      UNX_CreateFuncHook ( L"IDirectInputDevice8::GetDeviceState",
                           vftable [9],
                           IDirectInputDevice8_GetDeviceState_Detour,
                (LPVOID *)&IDirectInputDevice8_GetDeviceState_Original );

      UNX_EnableHook (vftable [9]);

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

#if 0
  if (SUCCEEDED (hr) && lplpDirectInputDevice != nullptr) {
    DWORD dwFlag = DISCL_FOREGROUND | DISCL_NONEXCLUSIVE;

    if (config.input.block_windows)
      dwFlag |= DISCL_NOWINKEY;

    (*lplpDirectInputDevice)->SetCooperativeLevel (SK_GetGameWindow (), dwFlag);
  }
#endif

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

XInputGetState_pfn XInputGetState_Original = nullptr;

bool
IsControllerPluggedIn (INT iJoyID)
{
  if (iJoyID == -1)
    return true;

  XINPUT_STATE xstate;

  static DWORD last_poll = timeGetTime ();
  static DWORD dwRet     = XInputGetState_Original (iJoyID, &xstate);

  // This function is actually a performance hazzard when no controllers
  //   are plugged in, so ... throttle the sucker.
  if (last_poll < timeGetTime () - 500UL)
    dwRet = XInputGetState_Original (iJoyID, &xstate);

  if (dwRet == ERROR_DEVICE_NOT_CONNECTED)
    return false;

  return true;
}

DWORD
WINAPI
XInputGetState_Detour ( _In_  DWORD         dwUserIndex,
                        _Out_ XINPUT_STATE *pState )
{
  if (! config.input.remap_dinput8)
    return XInputGetState_Original (dwUserIndex, pState);

  int slot = config.input.gamepad_slot == -1 ?
               0 : config.input.gamepad_slot;

  DWORD dwRet = XInputGetState_Original (slot, pState);

  //if (dwRet == ERROR_NOT_CONNECTED)
    //dwRet = 0;

  return dwRet;
}

///////////////////////////////////////////////////////////////////////////////
//
// User32 Input APIs
//
///////////////////////////////////////////////////////////////////////////////
void
WINAPI
SpinOrSleep (DWORD dwMilliseconds)
{
  DWORD dwEnd  = timeGetTime () + dwMilliseconds;

  while (timeGetTime () < dwEnd) {
    YieldProcessor ();
    Sleep (dwMilliseconds);
  }
}

typedef void (CALLBACK *SK_PluginKeyPress_pfn)( BOOL Control,
                        BOOL Shift,
                        BOOL Alt,
                        BYTE vkCode );
SK_PluginKeyPress_pfn SK_PluginKeyPress_Original = nullptr;


void
UNX_KickStart (void)
{
  SK_ICommandResult result = 
    SK_GetCommandProcessor ()->ProcessCommandLine ("Window.OverrideRes");

  if (result.getVariable () != nullptr) {
    RECT client;
    GetClientRect (unx::window.hwnd, &client);

    SK_GetCommandProcessor ()->ProcessCommandLine (
      "Window.OverrideRes 640x480"
    );
    SK_GetCommandProcessor ()->ProcessCommandFormatted (
      "Window.OverrideRes %lux%lu",
        client.right  - client.left,
        client.bottom - client.top );
    SK_GetCommandProcessor ()->ProcessCommandLine (
      "Window.OverrideRes 0x0"
    );
  }
}

void
CALLBACK
SK_UNX_PluginKeyPress ( BOOL Control,
                        BOOL Shift,
                        BOOL Alt,
                        BYTE vkCode )
{
  if (Control && Shift) {
    if (Alt && vkCode == 'K') {
      UNX_KickStart ();
    }

    else if (vkCode == 'H') {
      extern void UNX_SpeedStep (); 
      UNX_SpeedStep ();
    }

    else if (vkCode == 'P') {
      extern void UNX_TimeStop (void);
      UNX_TimeStop ();
    }

    else if (vkCode == 'U') {
      extern void UNX_FFX2_UnitTest (void);
      UNX_FFX2_UnitTest ();
    }

    else if (vkCode == 'Q') {
      extern void UNX_Quickie (void);
      UNX_Quickie ();
    }

    else if (vkCode == 'F') {
      extern void UNX_ToggleFreeLook (void);
      UNX_ToggleFreeLook ();
    }

    else if (vkCode == 'S') {
      extern void UNX_ToggleSensor (void);
      UNX_ToggleSensor ();
    }

    else if (vkCode == 'A') {
      extern void UNX_TogglePartyAP (void);
      UNX_TogglePartyAP ();
    }

    else if (vkCode == 'V') {
      SK_ICommandResult result = 
        SK_GetCommandProcessor ()->ProcessCommandLine ("PresentationInterval");

      uint32_t dwLen = 4;
      char szPresent [4];

      result.getVariable ()->getValueString (szPresent, &dwLen);

      if (strcmp (szPresent, "0"))
        SK_GetCommandProcessor ()->ProcessCommandLine ("PresentationInterval 0");
      else
        SK_GetCommandProcessor ()->ProcessCommandLine ("PresentationInterval 1");
    }

    // FFX Soft Reset
    else if (vkCode == VK_BACK) {
      extern bool UNX_KillMeNow (void);
      UNX_KillMeNow ();
    }
  }

  //SK_PluginKeyPress_Original (Control, Shift, Alt, vkCode);
}

typedef HRESULT (WINAPI *DirectInput8Create_pfn)(
 HINSTANCE hinst,
 DWORD     dwVersion,
 REFIID    riidltf,
 LPVOID*   ppvOut,
 LPUNKNOWN punkOuter
);

DirectInput8Create_pfn DirectInput8Create_Original = nullptr;

HRESULT
WINAPI
DirectInput8Create_Detour (
  HINSTANCE hinst,
  DWORD     dwVersion,
  REFIID    riidltf,
  LPVOID*   ppvOut,
  LPUNKNOWN punkOuter
)
{
  try {
    return DirectInput8Create_Original (hinst, dwVersion, riidltf, ppvOut, punkOuter);
  } catch (...) {
    dll_log->Log (L"[CrashPrvnt] Raptr tried and failed to initialize DInput8...");
    return E_FAIL;
  }
}

typedef const wchar_t* (__stdcall *SK_GetConfigPath_pfn)(void);
extern SK_GetConfigPath_pfn SK_GetConfigPath;

void
unx::InputManager::Init (void)
{
  SK_GetGameWindow =
    (SK_GetGameWindow_pfn)
      GetProcAddress ( GetModuleHandle (config.system.injector.c_str ()),
                         "SK_GetGameWindow" );

  unx::ParameterFactory factory;
  iSK_INI* pad_cfg =
    UNX_CreateINI (
      std::wstring (
        std::wstring (SK_GetConfigPath ()) + L"UnX_Gamepad.ini"
      ).c_str ()
    );
  pad_cfg->parse ();

  unx::ParameterStringW* texture_set =
    (unx::ParameterStringW *)factory.create_parameter <std::wstring> (L"Texture Set");
  texture_set->register_to_ini (pad_cfg, L"Gamepad.Type", L"TextureSet");

  if (! texture_set->load (gamepad.tex_set)) {
    gamepad.tex_set = L"PlayStation_Glossy";

    texture_set->store (gamepad.tex_set);
  }

  if ( texture_set->get_value ().length () )
  {
    config.textures.gamepad =
      texture_set->get_value ();

    //if (textures.gamepad_hash_ffx->load ())
      //wscanf (L"0x%x", &config.textures.pad.icons.high);

    //if (textures.gamepad_hash_ffx2->load ())
      //wscanf (L"0x%x", &config.textures.pad.icons.low);

    wchar_t wszPadRoot [MAX_PATH] = { L'\0' };
    lstrcatW (wszPadRoot, L"gamepads\\");
    lstrcatW (wszPadRoot, config.textures.gamepad.c_str ());
    lstrcatW (wszPadRoot, L"\\");

    dll_log->Log (L"[Button Map] Button Pack: %s", wszPadRoot);

    wchar_t wszPadIcons [MAX_PATH] = { L'\0' };
    lstrcatW (wszPadIcons, wszPadRoot);
    lstrcatW (wszPadIcons, L"ButtonMap.dds");

    dll_log->Log (L"[Button Map] Button Map:  %s", wszPadIcons);

    SK_D3D11_AddTexHash ( wszPadIcons,
                            config.textures.pad.icons.high,
                              0x00 );

    SK_D3D11_AddTexHash ( wszPadIcons,
                            config.textures.pad.icons.low,
                              0x00 );

    const size_t num_buttons = 16;
    const size_t pad_lods    = 2;

    const wchar_t*
      wszButtons [num_buttons] =
        {  L"A",     L"B",     L"X",    L"Y",
           L"LB",    L"RB",
           L"LT",    L"RT",
           L"LS",    L"RS",
           L"UP",    L"RIGHT", L"DOWN", L"LEFT",
           L"START", L"SELECT"                    };

    bool new_button = true;

    for (int i = 0; i < num_buttons * pad_lods; i++)
    {
      if (! (i % pad_lods))
        new_button = true;

      wchar_t wszPadButton [MAX_PATH] = { L'\0' };
      lstrcatW (wszPadButton, wszPadRoot);
      lstrcatW (wszPadButton, wszButtons [i / pad_lods]);
      lstrcatW (wszPadButton, L".dds");

      uint32_t hash =
        ((uint32_t *)&config.textures.pad.buttons) [i];

      if (hash != 0x00)
      {
        if (new_button) {
          if (i > 0) {
            dll_log->LogEx
                        ( false, L"\n" );
          }

          dll_log->LogEx ( true, L"[Button Map] Button %10s: '%#38s' ( %08x :: ",
                           wszButtons [i / pad_lods],
                             wszPadButton,
                               hash );

          new_button = false;
        } else {
          dll_log->LogEx ( false, L"%08x )", hash );
        }

        SK_D3D11_AddTexHash (
              wszPadButton,
                hash,
                  0x00
        );
      }
    }

    dll_log->LogEx ( false, L"\n" );
  }

  unx::ParameterBool* supports_XInput = 
    (unx::ParameterBool *)factory.create_parameter <bool> (L"Disable XInput?");
  supports_XInput->register_to_ini (pad_cfg, L"Gamepad.Type", L"UsesXInput");

  if (((unx::iParameter *)supports_XInput)->load ()) {
    gamepad.legacy = (! supports_XInput->get_value ());
  } else {
    supports_XInput->store (true);
  }

  //if (gamepad.legacy)
  {
    unx::ParameterInt* A     = (unx::ParameterInt *)factory.create_parameter <int> (L"A");
    A->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_A");

    if (((unx::iParameter *)A)->load ()) {
      gamepad.remap.buttons.A = (gamepad.remap.indexToEnum (A->get_value ()));
    } else {
      A->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.A));
    }

    unx::ParameterInt* B     = (unx::ParameterInt *)factory.create_parameter <int> (L"B");
    B->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_B");

    if (((unx::iParameter *)B)->load ()) {
      gamepad.remap.buttons.B = (gamepad.remap.indexToEnum (B->get_value ()));
    } else {
      B->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.B));
    }

    unx::ParameterInt* X     = (unx::ParameterInt *)factory.create_parameter <int> (L"X");
    X->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_X");

    if (((unx::iParameter *)X)->load ()) {
      gamepad.remap.buttons.X = (gamepad.remap.indexToEnum (X->get_value ()));
    } else {
      X->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.X));
    }

    unx::ParameterInt* Y     = (unx::ParameterInt *)factory.create_parameter <int> (L"Y");
    Y->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_Y");

    if (((unx::iParameter *)Y)->load ()) {
      gamepad.remap.buttons.Y = (gamepad.remap.indexToEnum (Y->get_value ()));
    } else {
      Y->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.Y));
    }

    unx::ParameterInt* START = (unx::ParameterInt *)factory.create_parameter <int> (L"START");
    START->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_Start");

    if (((unx::iParameter *)START)->load ()) {
      gamepad.remap.buttons.START = (gamepad.remap.indexToEnum (START->get_value ()));
    } else {
      START->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.START));
    }

    unx::ParameterInt* BACK  = (unx::ParameterInt *)factory.create_parameter <int> (L"BACK");
    BACK->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_Back");

    if (((unx::iParameter *)BACK)->load ()) {
      gamepad.remap.buttons.BACK = (gamepad.remap.indexToEnum (BACK->get_value ()));
    } else {
      BACK->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.BACK));
    }

    unx::ParameterInt* LB    = (unx::ParameterInt *)factory.create_parameter <int> (L"LB");
    LB->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_LB");

    if (((unx::iParameter *)LB)->load ()) {
      gamepad.remap.buttons.LB = (gamepad.remap.indexToEnum (LB->get_value ()));
    } else {
      LB->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.LB));
    }

    unx::ParameterInt* RB    = (unx::ParameterInt *)factory.create_parameter <int> (L"RB");
    RB->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_RB");

    if (((unx::iParameter *)RB)->load ()) {
      gamepad.remap.buttons.RB = (gamepad.remap.indexToEnum (RB->get_value ()));
    } else {
      RB->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.RB));
    }

    unx::ParameterInt* LT    = (unx::ParameterInt *)factory.create_parameter <int> (L"LT");
    LT->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_LT");

    if (((unx::iParameter *)LT)->load ()) {
      gamepad.remap.buttons.LT = (gamepad.remap.indexToEnum (LT->get_value ()));
    } else {
      LT->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.LT));
    }

    unx::ParameterInt* RT    = (unx::ParameterInt *)factory.create_parameter <int> (L"RT");
    RT->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_RT");

    if (((unx::iParameter *)RT)->load ()) {
      gamepad.remap.buttons.RT = (gamepad.remap.indexToEnum (RT->get_value ()));
    } else {
      RT->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.RT));
    }

    unx::ParameterInt* LS    = (unx::ParameterInt *)factory.create_parameter <int> (L"LS");
    LS->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_LS");

    if (((unx::iParameter *)LS)->load ()) {
      gamepad.remap.buttons.LS = (gamepad.remap.indexToEnum (LS->get_value ()));
    } else {
      LS->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.LS));
    }

    unx::ParameterInt* RS    = (unx::ParameterInt *)factory.create_parameter <int> (L"RS");
    RS->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_RS");

    if (((unx::iParameter *)RS)->load ()) {
      gamepad.remap.buttons.RS = (gamepad.remap.indexToEnum (RS->get_value ()));
    } else {
      RS->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.RS));
    }
  }

  unx::ParameterStringW* combo_Speed =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"SpeedBoost");
  combo_Speed->register_to_ini (pad_cfg, L"Gamepad.PC", L"SpeedBoost");

  if (! combo_Speed->load (gamepad.speedboost.unparsed)) {
    combo_Speed->store (
      (gamepad.speedboost.unparsed = L"Select+L2+Cross")
    );
  }

  unx::ParameterStringW* combo_Kickstart =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"KickStart");
  combo_Kickstart->register_to_ini (pad_cfg, L"Gamepad.PC", L"KickStart");

  if (! combo_Kickstart->load (gamepad.kickstart.unparsed)) {
    combo_Kickstart->store (
      (gamepad.kickstart.unparsed = L"L1+L2+Up")
    );
  }


  unx::ParameterStringW* combo_F1 =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F1 Buttons");
  combo_F1->register_to_ini (pad_cfg, L"Gamepad.PC", L"F1");

  if (! combo_F1->load (gamepad.f1.unparsed)) {
    combo_F1->store (
      (gamepad.f1.unparsed = L"Select+Cross")
    );
  }

  unx::ParameterStringW* combo_F2 =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F2 Buttons");
  combo_F2->register_to_ini (pad_cfg, L"Gamepad.PC", L"F2");

  if (! combo_F2->load (gamepad.f2.unparsed)) {
    combo_F2->store (
      (gamepad.f1.unparsed = L"Select+Circle")
    );
  }

  unx::ParameterStringW* combo_F3 =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F3 Buttons");
  combo_F3->register_to_ini (pad_cfg, L"Gamepad.PC", L"F3");

  if (! combo_F3->load (gamepad.f3.unparsed)) {
    combo_F3->store (
      (gamepad.f3.unparsed = L"Select+Square")
    );
  }

  unx::ParameterStringW* combo_F4 =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F4 Buttons");
  combo_F4->register_to_ini (pad_cfg, L"Gamepad.PC", L"F4");

  if (! combo_F4->load (gamepad.f4.unparsed)) {
    combo_F4->store (
      (gamepad.f4.unparsed = L"Select+L1")
    );
  }

  unx::ParameterStringW* combo_F5 =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F5 Buttons");
  combo_F5->register_to_ini (pad_cfg, L"Gamepad.PC", L"F5");

  if (! combo_F5->load (gamepad.f5.unparsed)) {
    combo_F5->store (
      (gamepad.f5.unparsed = L"Select+R1")
    );
  }

  unx::ParameterStringW* combo_ESC =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"Escape Buttons");
  combo_ESC->register_to_ini (pad_cfg, L"Gamepad.PC", L"ESC");

  if (! combo_ESC->load (gamepad.esc.unparsed)) {
    combo_ESC->store (
      (gamepad.esc.unparsed = L"L2+R2+Select")
    );
  }

  unx::ParameterStringW* combo_Fullscreen =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"Fullscreen Buttons");
  combo_Fullscreen->register_to_ini (pad_cfg, L"Gamepad.PC", L"Fullscreen");

  if (! combo_Fullscreen->load (gamepad.fullscreen.unparsed)) {
    combo_Fullscreen->store (
      (gamepad.fullscreen.unparsed = L"L2+L3")
    );
  }

  unx::ParameterStringW* combo_SS =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"Screenshot Buttons");
  combo_SS->register_to_ini (pad_cfg, L"Gamepad.Steam", L"Screenshot");

  if (! combo_SS->load (gamepad.screenshot.unparsed)) {
    combo_SS->store (
      (gamepad.screenshot.unparsed = L"Select+R3")
    );
  }

  pad_cfg->write ((std::wstring (SK_GetConfigPath ()) + L"UnX_Gamepad.ini").c_str ());
  delete pad_cfg;

  if (config.input.remap_dinput8) {
    HMODULE hModDontCare;

    // Fix problems with Raptr by pinning this DLL into memory constantly
    LoadLibrary (L"dinput8.dll");

    GetModuleHandleEx (
      GET_MODULE_HANDLE_EX_FLAG_PIN,
        L"dinput8.dll", 
          &hModDontCare );

    bool init = false;

    HRESULT com_init_hr =
      CoInitializeEx (nullptr, COINIT_MULTITHREADED);

    IDirectInput8W* pDInput8 = nullptr;

    HRESULT hr =
      CoCreateInstance ( CLSID_DirectInput8,
                           nullptr,
                             CLSCTX_INPROC_SERVER,
                               IID_IDirectInput8,
                                 (LPVOID *)&pDInput8 );

    if (SUCCEEDED (hr)) {
      void** vftable = *(void***)*&pDInput8;

      pDInput8->Initialize (GetModuleHandle (nullptr), DIRECTINPUT_VERSION);

      UNX_CreateFuncHook ( L"IDirectInput8::CreateDevice",
                           vftable [3],
                           IDirectInput8_CreateDevice_Detour,
                 (LPVOID*)&IDirectInput8_CreateDevice_Original );

      UNX_EnableHook (vftable [3]);

#if 0
      UNX_CreateDLLHook ( L"dinput8.dll",
                          "DirectInput8Create",
                          DirectInput8Create_Detour,
               (LPVOID *)&DirectInput8Create_Original );
#endif

      pDInput8->Release ();
    }

    //if (com_init_hr == S_OK)
      //CoUninitialize ();
  }


  // Post-Process our Remap Table
  for (int i = 0; i < 12; i++) {
    gamepad.remap.map [i] = 
      gamepad.remap.enumToIndex (
        ((int *)&gamepad.remap.buttons) [ i ]
      ) - 1;
  }


  //
  // Win32 API Input Hooks
  //

  //HookRawInput ();

  UNX_CreateDLLHook2 ( L"XInput9_1_0.dll",
                        "XInputGetState",
                         XInputGetState_Detour,
              (LPVOID *)&XInputGetState_Original );

  UNX_CreateDLLHook2 ( config.system.injector.c_str (),
                       "SK_PluginKeyPress",
                         SK_UNX_PluginKeyPress,
              (LPVOID *)&SK_PluginKeyPress_Original );

  UNX_ApplyQueuedHooks ();

  unx::InputManager::Hooker* pHook =
    unx::InputManager::Hooker::getInstance ();

  UNX_InstallWindowHook (NULL);

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
    (HANDLE)
      _beginthreadex ( nullptr,
                         0,
                           Hooker::MessagePump,
                             nullptr,
                              0,
                                nullptr );
}

void
unx::InputManager::Hooker::End (void)
{
  TerminateThread     (hMsgPump, 0);
}

#define XINPUT_GAMEPAD_DPAD_UP        0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN      0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT      0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT     0x0008

#define XINPUT_GAMEPAD_START          0x0010
#define XINPUT_GAMEPAD_BACK           0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB     0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB    0x0080

#define XINPUT_GAMEPAD_LEFT_SHOULDER  0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_LEFT_TRIGGER   0x10000
#define XINPUT_GAMEPAD_RIGHT_TRIGGER  0x20000

#define XINPUT_GAMEPAD_A              0x1000
#define XINPUT_GAMEPAD_B              0x2000
#define XINPUT_GAMEPAD_X              0x4000
#define XINPUT_GAMEPAD_Y              0x8000

struct button_map_state_s {
  int  buttons;
  bool lt, rt;
};

button_map_state_s
UNX_ParseButtonCombo (std::wstring combo, int* out)
{
  button_map_state_s state;
  state.lt      = false;
  state.rt      = false;
  state.buttons = 0;

  std::wstring map = combo;

  if (! map.length ())
    return state;

  wchar_t* wszMap  = _wcsdup (map.c_str ());
  wchar_t* wszTok  =  wcstok (wszMap, L"+");

  while (wszTok != nullptr) {
    int button = 0x00;

    if ((! lstrcmpiW (wszTok, L"LB"))   || (! lstrcmpiW (wszTok, L"L1")))
      button = XINPUT_GAMEPAD_LEFT_SHOULDER;
    else if ((! lstrcmpiW (wszTok, L"RB"))   || (! lstrcmpiW (wszTok, L"R1")))
      button = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    else if ((! lstrcmpiW (wszTok, L"LT"))   || (! lstrcmpiW (wszTok, L"L2")))
      button = XINPUT_GAMEPAD_LEFT_TRIGGER;
    else if ((! lstrcmpiW (wszTok, L"RT"))   || (! lstrcmpiW (wszTok, L"R2")))
      button = XINPUT_GAMEPAD_RIGHT_TRIGGER;
    else if ((! lstrcmpiW (wszTok, L"LS"))   || (! lstrcmpiW (wszTok, L"L3")))
      button = XINPUT_GAMEPAD_LEFT_THUMB;
    else if ((! lstrcmpiW (wszTok, L"RS"))   || (! lstrcmpiW (wszTok, L"R3")))
      button = XINPUT_GAMEPAD_RIGHT_THUMB;

    else if ((! lstrcmpiW (wszTok, L"Start")))
      button = XINPUT_GAMEPAD_START;
    else if ((! lstrcmpiW (wszTok, L"Back")) || (! lstrcmpiW (wszTok, L"Select")))
      button = XINPUT_GAMEPAD_BACK;
    else if ((! lstrcmpiW (wszTok, L"A"))    || (! lstrcmpiW (wszTok, L"Cross")))
      button = XINPUT_GAMEPAD_A;
    else if ((! lstrcmpiW (wszTok, L"B"))    || (! lstrcmpiW (wszTok, L"Circle")))
      button = XINPUT_GAMEPAD_B;
    else if ((! lstrcmpiW (wszTok, L"X"))    || (! lstrcmpiW (wszTok, L"Square")))
      button = XINPUT_GAMEPAD_X;
    else if ((! lstrcmpiW (wszTok, L"Y"))    || (! lstrcmpiW (wszTok, L"Triangle")))
      button = XINPUT_GAMEPAD_Y;

    else if ((! lstrcmpiW (wszTok, L"Up")))
      button = XINPUT_GAMEPAD_DPAD_UP;
    else if ((! lstrcmpiW (wszTok, L"Down")))
      button = XINPUT_GAMEPAD_DPAD_DOWN;
    else if ((! lstrcmpiW (wszTok, L"Left")))
      button = XINPUT_GAMEPAD_DPAD_LEFT;
    else if ((! lstrcmpiW (wszTok, L"Right")))
      button = XINPUT_GAMEPAD_DPAD_RIGHT;

    if (button == XINPUT_GAMEPAD_LEFT_TRIGGER)
      state.lt = true;
    else if (button == XINPUT_GAMEPAD_RIGHT_TRIGGER)
      state.rt = true;
    else if (button != 0x00) {
      out [state.buttons++] = button;
    }

    //dll_log->Log (L"Button%lu: %s", state.buttons-1, wszTok);
    wszTok = wcstok (nullptr, L"+");

    if (state.buttons > 1)
      break;
  }

  free (wszMap);

  return state;
}

#include <mmsystem.h>

using namespace unx::InputManager;

void
UNX_SetupSpecialButtons (void)
{
  button_map_state_s state =
    UNX_ParseButtonCombo ( gamepad.f1.unparsed,
                             &gamepad.f1.button0 );

  gamepad.f1.lt      = state.lt;
  gamepad.f1.rt      = state.rt;
  gamepad.f1.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.f2.unparsed,
                             &gamepad.f2.button0 );

  gamepad.f2.lt      = state.lt;
  gamepad.f2.rt      = state.rt;
  gamepad.f2.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.f3.unparsed,
                             &gamepad.f3.button0 );

  gamepad.f3.lt      = state.lt;
  gamepad.f3.rt      = state.rt;
  gamepad.f3.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.f4.unparsed,
                             &gamepad.f4.button0 );

  gamepad.f4.lt      = state.lt;
  gamepad.f4.rt      = state.rt;
  gamepad.f4.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.f5.unparsed,
                             &gamepad.f5.button0 );

  gamepad.f5.lt      = state.lt;
  gamepad.f5.rt      = state.rt;
  gamepad.f5.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.screenshot.unparsed,
                             &gamepad.screenshot.button0 );

  gamepad.screenshot.lt      = state.lt;
  gamepad.screenshot.rt      = state.rt;
  gamepad.screenshot.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.esc.unparsed,
                             &gamepad.esc.button0 );

  gamepad.esc.lt      = state.lt;
  gamepad.esc.rt      = state.rt;
  gamepad.esc.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.fullscreen.unparsed,
                             &gamepad.fullscreen.button0 );

  gamepad.fullscreen.lt      = state.lt;
  gamepad.fullscreen.rt      = state.rt;
  gamepad.fullscreen.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.speedboost.unparsed,
                             &gamepad.speedboost.button0 );

  gamepad.speedboost.lt      = state.lt;
  gamepad.speedboost.rt      = state.rt;
  gamepad.speedboost.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo ( gamepad.kickstart.unparsed,
                             &gamepad.kickstart.button0 );

  gamepad.kickstart.lt      = state.lt;
  gamepad.kickstart.rt      = state.rt;
  gamepad.kickstart.buttons = state.buttons;

}

#include "ini.h"
#include "parameter.h"

#include <deque>

struct combo_button_s {
  combo_button_s (
      unx_gamepad_s::combo_s* combo_
  ) : combo (combo_) {
    state      = false;
    last_state = false;
  };


  bool poll ( DWORD          xi_ret,
              XINPUT_GAMEPAD xi_gamepad )
  {
    last_state = state;

    state =
      (xi_ret == 0                                  &&
       combo->buttons != 0                          &&
       ((! combo->lt) || xi_gamepad.bLeftTrigger)   &&
       ((! combo->rt) || xi_gamepad.bRightTrigger)  &&
       xi_gamepad.wButtons & (WORD)combo->button0   &&
       xi_gamepad.wButtons & (WORD)combo->button1   &&
       xi_gamepad.wButtons & (WORD)combo->button2);

    return state;
  }

  bool wasJustPressed (void) {
    return state && (! last_state);
  }


  unx_gamepad_s::combo_s*
       combo;

  bool state;
  bool last_state;
};

BYTE
UNX_PollAxis (int axis, const JOYINFOEX& joy_ex, const JOYCAPSW& caps)
{
#pragma warning (push)
#pragma warning (disable: 4244)

  switch (unx_gamepad_s::remap_s::enumToIndex (axis))
  {
    case -1:
      return 255 * ((float)(joy_ex.dwXpos - caps.wXmin) /
                       (float)(caps.wXmax - caps.wXmin));
    case -2:
      return 255 * ((float)(joy_ex.dwYpos - caps.wYmin) /
                       (float)(caps.wYmax - caps.wYmin));
    case -3:
      return 255 * ((float)(joy_ex.dwZpos - caps.wZmin) /
                       (float)(caps.wZmax - caps.wZmin));
    case -4:
      return 255 * ((float)(joy_ex.dwUpos - caps.wUmin) /
                       (float)(caps.wUmax - caps.wUmin));
    case -5:
      return 255 * ((float)(joy_ex.dwVpos - caps.wVmin) /
                       (float)(caps.wVmax - caps.wVmin));
    case -6:
      return 255 * ((float)(joy_ex.dwRpos - caps.wRmin) /
                       (float)(caps.wRmax - caps.wRmin));
    default:
      return 255 * ((joy_ex.dwButtons & axis) ? 1 : 0);
  }
#pragma warning (pop)
}


unsigned int
__stdcall
unx::InputManager::Hooker::MessagePump (LPVOID hook_ptr)
{
  if ( ((! XInputGetState_Original) && (! gamepad.legacy)) ) {
    CloseHandle (GetCurrentThread ());
    return 0;
  }

  UNX_SetupSpecialButtons ();

  combo_button_s f1         ( &gamepad.f1 );
  combo_button_s f2         ( &gamepad.f2 );
  combo_button_s f3         ( &gamepad.f3 );
  combo_button_s f4         ( &gamepad.f4 );
  combo_button_s f5         ( &gamepad.f5 );
  combo_button_s esc        ( &gamepad.esc );
  combo_button_s full       ( &gamepad.fullscreen );
  combo_button_s sshot      ( &gamepad.screenshot );
  combo_button_s speedboost ( &gamepad.speedboost );
  combo_button_s kickstart  ( &gamepad.kickstart  );

#define XI_POLL_INTERVAL 500UL

  DWORD  xi_ret       = ERROR_DEVICE_NOT_CONNECTED;
  DWORD  last_xi_poll = timeGetTime ();// - (XI_POLL_INTERVAL + 1UL);

  INPUT keys [5];

  keys [0].type           = INPUT_KEYBOARD;
  keys [0].ki.wVk         = 0;
  keys [0].ki.dwFlags     = KEYEVENTF_SCANCODE;
  keys [0].ki.time        = 0;
  keys [0].ki.dwExtraInfo = 0;

  for (int i = 1; i < 5; i++) {
    keys [i].type           = INPUT_KEYBOARD;
    keys [i].ki.wVk         = 0;
    keys [i].ki.dwFlags     = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    keys [i].ki.time        = 0;
    keys [i].ki.dwExtraInfo = 0;
  }

  std::deque <WORD> pressed_scancodes;

  while (true) {
    int slot = config.input.gamepad_slot;
    if (slot == -1)
      slot = 0;

    // Do Not Handle Input While We Do Not Have Focus
    if (GetForegroundWindow () != SK_GetGameWindow ()) {
      Sleep (15);
      continue;
    }

    XINPUT_STATE xi_state = { 0 };

    if (XInputGetState_Original != nullptr && (! gamepad.legacy)) {
      // This function is actually a performance hazzard when no controllers
      //   are plugged in, so ... throttle the sucker.
      if (  xi_ret == 0 ||
           (last_xi_poll < timeGetTime () - XI_POLL_INTERVAL) ) {
        xi_ret       = XInputGetState_Original (slot, &xi_state);
        last_xi_poll = timeGetTime    ();

        if (xi_ret != 0)
          xi_state = { 0 };
      }
    } else {
      if (  xi_ret == 0 ||
           (last_xi_poll < timeGetTime () - XI_POLL_INTERVAL) ) {
        if (! joyGetNumDevs ()) {
          xi_ret = ERROR_DEVICE_NOT_CONNECTED;
        } else {
          static JOYCAPSW caps = { 0 };

          if (! caps.wMaxButtons)
            joyGetDevCaps (JOYSTICKID1, &caps, sizeof JOYCAPSW);

          xi_ret = 0;

          JOYINFOEX joy_ex { 0 };
          joy_ex.dwSize  = sizeof JOYINFOEX;
          joy_ex.dwFlags = JOY_RETURNALL      | JOY_RETURNPOVCTS |
                           JOY_RETURNCENTERED | JOY_USEDEADZONE;

          joyGetPosEx (JOYSTICKID1, &joy_ex);

          xi_state.Gamepad.bLeftTrigger  = 0;//joy_ex.dwUpos > 0;
          xi_state.Gamepad.bRightTrigger = 0;//joy_ex.dwVpos > 0;

          xi_state.Gamepad.wButtons = 0;

          if (UNX_PollAxis (gamepad.remap.buttons.A, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_A;
          if (UNX_PollAxis (gamepad.remap.buttons.B, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_B;
          if (UNX_PollAxis (gamepad.remap.buttons.X, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_X;
          if (UNX_PollAxis (gamepad.remap.buttons.Y, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_Y;

          if (UNX_PollAxis (gamepad.remap.buttons.START, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_START;
          if (UNX_PollAxis (gamepad.remap.buttons.BACK, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;

          xi_state.Gamepad.bLeftTrigger =
            UNX_PollAxis (gamepad.remap.buttons.LT, joy_ex, caps);

          xi_state.Gamepad.bRightTrigger =
            UNX_PollAxis (gamepad.remap.buttons.RT, joy_ex, caps);

          if (UNX_PollAxis (gamepad.remap.buttons.LB, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
          if (UNX_PollAxis (gamepad.remap.buttons.RB, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

          if (UNX_PollAxis (gamepad.remap.buttons.LS, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
          if (UNX_PollAxis (gamepad.remap.buttons.RS, joy_ex, caps) > 190)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

          if (joy_ex.dwPOV == JOY_POVFORWARD)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
          if (joy_ex.dwPOV & JOY_POVBACKWARD)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
          if (joy_ex.dwPOV & JOY_POVLEFT)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
          if (joy_ex.dwPOV & JOY_POVRIGHT)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        }
        last_xi_poll = timeGetTime    ();
      }
    }

   //
   // Analog deadzone compensation
   //
   if (xi_state.Gamepad.bLeftTrigger < 130)
     xi_state.Gamepad.bLeftTrigger = 0;

   if (xi_state.Gamepad.bRightTrigger < 130)
     xi_state.Gamepad.bRightTrigger = 0;

    //SetFocus (SK_GetGameWindow ());

    bool four_finger = (
        xi_ret == 0                          &&
        config.input.four_finger_salute      &&
        xi_state.Gamepad.bLeftTrigger        &&
        xi_state.Gamepad.bRightTrigger       &&
        xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_LEFT_SHOULDER)  &&
        xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_RIGHT_SHOULDER) &&
        xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_START)          &&
        xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_BACK)
    );

    f1.poll         (xi_ret, xi_state.Gamepad);
    f2.poll         (xi_ret, xi_state.Gamepad);
    f3.poll         (xi_ret, xi_state.Gamepad);
    f4.poll         (xi_ret, xi_state.Gamepad);
    f5.poll         (xi_ret, xi_state.Gamepad);
    esc.poll        (xi_ret, xi_state.Gamepad);
    full.poll       (xi_ret, xi_state.Gamepad);
    sshot.poll      (xi_ret, xi_state.Gamepad);
    speedboost.poll (xi_ret, xi_state.Gamepad);
    kickstart.poll  (xi_ret, xi_state.Gamepad);

    WORD scancode = 0;

#define UNX_SendScancode(x) { scancode          = (x);                \
                              keys [0].ki.wScan = scancode;           \
                                                                      \
                              SendInput (1, &keys [0], sizeof INPUT); \
                                                                      \
                              pressed_scancodes.push_back (scancode); }

    if (four_finger) {
      extern bool UNX_KillMeNow (void);

      // If in battle, trigger a Game Over screen, otherwise restart the
      //   entire game.
      if (! UNX_KillMeNow ()) {
        SendMessage (SK_GetGameWindow (), WM_CLOSE, 0, 0);
      }

      Sleep (15);
      continue;
    }

    // Filter out what appears to be the beginning of a "four finger salute"
    //   so that menus and various other things are not activated.
    if ( xi_state.Gamepad.bRightTrigger && xi_state.Gamepad.bLeftTrigger &&
         xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_LEFT_SHOULDER)      &&
         xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_RIGHT_SHOULDER) ) {
      Sleep (15);
      continue;
    }

    bool full_sleep = true;

    if (esc.state) {
      UNX_SendScancode (0x01);
    }

    else if (speedboost.state) {
      if (speedboost.wasJustPressed ()) {
        extern void UNX_SpeedStep (); 
        UNX_SpeedStep ();
      }
    }

    else if (f1.state) {
      UNX_SendScancode (0x3b);
    }

    else if (f2.state) {
      UNX_SendScancode (0x3c);
    }

    else if (f3.state) {
      UNX_SendScancode (0x3d);
    }

    else if (f4.state) {
      UNX_SendScancode (0x3e);
    }

    else if (f5.state) {
      UNX_SendScancode (0x3f);
    }

    else if (full.wasJustPressed ()) {
      UNX_SendScancode (0x38);
      UNX_SendScancode (0x1c);
      full_sleep = false;
    }

    else if (kickstart.wasJustPressed ()) {
      UNX_KickStart ();
    }

    else if (sshot.wasJustPressed ()) {
      typedef bool (__stdcall *SK_SteamAPI_TakeScreenshot_pfn)(void);

      static SK_SteamAPI_TakeScreenshot_pfn
        SK_SteamAPI_TakeScreenshot =
          (SK_SteamAPI_TakeScreenshot_pfn)
            GetProcAddress (
                     GetModuleHandle ( config.system.injector.c_str () ),
                       "SK_SteamAPI_TakeScreenshot"
            );

      if (SK_SteamAPI_TakeScreenshot != nullptr)
        SK_SteamAPI_TakeScreenshot ();
    }

    Sleep (full_sleep ? 15 : 0);

    if (! full_sleep)
      SetForegroundWindow (SK_GetGameWindow ());

    int i = 1;

    //SetFocus (SK_GetGameWindow ());

    while (pressed_scancodes.size ()) {
      keys [i++].ki.wScan = pressed_scancodes.front     ();
                            pressed_scancodes.pop_front ();

      if (i > 4) {
        SendInput (4, &keys [1], sizeof INPUT);
        i = 1;
      }
    }

    if (i > 1)
      SendInput (i-1, &keys [1], sizeof INPUT);

    //SetFocus (SK_GetGameWindow ());
  }

  return 0;
}

void
UNX_ReleaseESCKey (void)
{
return;

  INPUT keys [2];

  keys [0].type           = INPUT_KEYBOARD;
  keys [0].ki.wVk         = 0;
  keys [0].ki.wScan       = 0x01;
  keys [0].ki.dwFlags     = KEYEVENTF_SCANCODE;
  keys [0].ki.time        = 0;
  keys [0].ki.dwExtraInfo = 0;

  SendInput (1, &keys [0], sizeof INPUT);
  Sleep     (66);

  keys [1].type           = INPUT_KEYBOARD;
  keys [1].ki.wVk         = 0;
  keys [1].ki.wScan       = 0x01;
  keys [1].ki.dwFlags     = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
  keys [1].ki.time        = 0;
  keys [1].ki.dwExtraInfo = 0;

  SendInput (1, &keys [1], sizeof INPUT);
  Sleep     (66);
}

unx::InputManager::Hooker*
  unx::InputManager::Hooker::pInputHook = nullptr;