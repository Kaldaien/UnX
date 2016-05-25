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
#include "parameter.h"

#include "input.h"

#include <mmsystem.h>
#pragma comment (lib, "winmm.lib")

typedef void (WINAPI *SK_D3D11_AddTexHash_pfn)(std::wstring, uint32_t);
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
  } f1, f2, f3, f4, f5, screenshot;

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
      return 1 << (idx - 1);
    }

    static int enumToIndex (unsigned int enum_val) {
      int idx = 0;

      while (enum_val > 0) {
        enum_val >>= 1;
        idx++;
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
  uint8_t             state [256]; // Handle overrun just in case
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
      if (GetForegroundWindow () != SK_GetGameWindow ()) {
        memset (out, 0, sizeof DIJOYSTATE2);

        out->rgdwPOV [0] = -1;
        out->rgdwPOV [1] = -1;
        out->rgdwPOV [2] = -1;
        out->rgdwPOV [3] = -1;
      }

      else {
        DIJOYSTATE2  in  = *out;

        for (int i = 0; i < 12; i++) {
          out->rgbButtons [ i ] = 
            in.rgbButtons [ gamepad.remap.map [ i ] ];
        }
      }
    }
  }

  if (This == _dik.pDev) {
    if (unx::window.active) {
      memcpy (_dik.state, lpvData, cbData);
    } else {
      memcpy (lpvData, _dik.state, cbData);

      ((uint8_t *)lpvData) [DIK_LALT]   = 0x0;
      ((uint8_t *)lpvData) [DIK_RALT]   = 0x0;
      ((uint8_t *)lpvData) [DIK_TAB]    = 0x0;
      ((uint8_t *)lpvData) [DIK_ESCAPE] = 0x0;
    }
  }
#if 0
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
  return hr;
#endif
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
    DWORD dwFlag = DISCL_FOREGROUND | DISCL_EXCLUSIVE;

    if (config.input.block_windows)
      dwFlag |= DISCL_NOWINKEY;

    (*lplpDirectInputDevice)->SetCooperativeLevel (SK_GetGameWindow (), dwFlag);
  }
#endif

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

typedef SHORT (WINAPI *GetAsyncKeyState_pfn)(
  _In_ int vKey
);

typedef UINT (WINAPI *GetRawInputData_pfn)(
  _In_      HRAWINPUT hRawInput,
  _In_      UINT      uiCommand,
  _Out_opt_ LPVOID    pData,
  _Inout_   PUINT     pcbSize,
  _In_      UINT      cbSizeHeader
);

GetAsyncKeyState_pfn GetAsyncKeyState_Original = nullptr;
GetRawInputData_pfn  GetRawInputData_Original  = nullptr;

SHORT
WINAPI
GetAsyncKeyState_Detour ( _In_ int vKey )
{
  if (unx::window.active)
    return GetAsyncKeyState_Original (vKey);

  GetAsyncKeyState_Original (vKey);

  return 0x00;
}

UINT
WINAPI
GetRawInputData_Detour (
  _In_      HRAWINPUT hRawInput,
  _In_      UINT      uiCommand,
  _Out_opt_ LPVOID    pData,
  _Inout_   PUINT     pcbSize,
  _In_      UINT      cbSizeHeader )
{
  if (unx::window.active) {
    return GetRawInputData_Original (
             hRawInput,
               uiCommand,
                 pData,
                   pcbSize,
                     cbSizeHeader );
  }

  if (pData != nullptr && pcbSize != nullptr) {
    memset (pData, 0, *pcbSize);
    *pcbSize = 0;
  }

  return 0;
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
IsControllerPluggedIn (UINT uJoyID)
{
 if (uJoyID == (UINT)-1)
    return true;

  XINPUT_STATE xstate;

  static DWORD last_poll = timeGetTime ();
  static DWORD dwRet     = XInputGetState_Original (uJoyID, &xstate);

  // This function is actually a performance hazzard when no controllers
  //   are plugged in, so ... throttle the sucker.
  if (last_poll < timeGetTime () - 500UL)
    dwRet = XInputGetState_Original (uJoyID, &xstate);

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

  if (dwRet == ERROR_NOT_CONNECTED)
    dwRet = 0;

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

void
unx::InputManager::Init (void)
{
  SK_GetGameWindow =
    (SK_GetGameWindow_pfn)
      GetProcAddress ( GetModuleHandle (config.system.injector.c_str ()),
                         "SK_GetGameWindow" );

  unx::ParameterFactory factory;
  unx::INI::File* pad_cfg = new unx::INI::File (L"UnX_Gamepad.ini");
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

    dll_log.Log (L"[Button Map] Button Pack: %s", wszPadRoot);

    wchar_t wszPadIcons [MAX_PATH] = { L'\0' };
    lstrcatW (wszPadIcons, wszPadRoot);
    lstrcatW (wszPadIcons, L"ButtonMap.dds");

    dll_log.Log (L"[Button Map] Button Map:  %s", wszPadIcons);

    SK_D3D11_AddTexHash ( wszPadIcons,
                            config.textures.pad.icons.high );

    SK_D3D11_AddTexHash ( wszPadIcons,
                            config.textures.pad.icons.low );

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
            dll_log.LogEx
                        ( false, L"\n" );
          }

          dll_log.LogEx ( true, L"[Button Map] Button %10s: '%#38s' ( %08x :: ",
                          wszButtons [i / pad_lods],
                            wszPadButton,
                              hash );

          new_button = false;
        } else {
          dll_log.LogEx ( false, L"%08x )", hash );
        }

        SK_D3D11_AddTexHash (
              wszPadButton,
                hash
        );
      }
    }

    dll_log.LogEx ( false, L"\n" );
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

  unx::ParameterStringW* combo_ESC =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"ESC Buttons");

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

  unx::ParameterStringW* combo_SS =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"Screenshot Buttons");
  combo_SS->register_to_ini (pad_cfg, L"Gamepad.Steam", L"Screenshot");

  if (! combo_SS->load (gamepad.screenshot.unparsed)) {
    combo_SS->store (
      (gamepad.screenshot.unparsed = L"Select+R3")
    );
  }

  pad_cfg->write (L"UnX_Gamepad.ini");
  delete pad_cfg;

  if (config.input.remap_dinput8) {
    UNX_CreateDLLHook ( L"dinput8.dll", "DirectInput8Create",
                        DirectInput8Create_Detour,
              (LPVOID*)&DirectInput8Create_Original,
              (LPVOID*)&DirectInput8Create_Hook );
    UNX_EnableHook    (DirectInput8Create_Hook);
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

  if (config.input.fix_bg_input) {
    UNX_CreateDLLHook ( config.system.injector.c_str (),
                       "GetRawInputData_Detour",
                        GetRawInputData_Detour,
              (LPVOID*)&GetRawInputData_Original );

    UNX_CreateDLLHook ( config.system.injector.c_str (),
                       "GetAsyncKeyState_Detour",
                        GetAsyncKeyState_Detour,
              (LPVOID*)&GetAsyncKeyState_Original );
  }

  UNX_CreateDLLHook ( L"XInput9_1_0.dll",
                       "XInputGetState",
                        XInputGetState_Detour,
             (LPVOID *)&XInputGetState_Original );

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
    CreateThread ( NULL,
                     NULL,
                       Hooker::MessagePump,
                         nullptr,
                           NULL,
                             NULL );
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
    if ((! lstrcmpiW (wszTok, L"RB"))   || (! lstrcmpiW (wszTok, L"R1")))
      button = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    if ((! lstrcmpiW (wszTok, L"LT"))   || (! lstrcmpiW (wszTok, L"L2")))
      button = XINPUT_GAMEPAD_LEFT_TRIGGER;
    if ((! lstrcmpiW (wszTok, L"RT"))   || (! lstrcmpiW (wszTok, L"R2")))
      button = XINPUT_GAMEPAD_RIGHT_TRIGGER;
    if ((! lstrcmpiW (wszTok, L"LS"))   || (! lstrcmpiW (wszTok, L"L3")))
      button = XINPUT_GAMEPAD_LEFT_THUMB;
    if ((! lstrcmpiW (wszTok, L"RS"))   || (! lstrcmpiW (wszTok, L"R3")))
      button = XINPUT_GAMEPAD_RIGHT_THUMB;

    if ((! lstrcmpiW (wszTok, L"Start")))
      button = XINPUT_GAMEPAD_START;
    if ((! lstrcmpiW (wszTok, L"Back")) || (! lstrcmpiW (wszTok, L"Select")))
      button = XINPUT_GAMEPAD_BACK;
    if ((! lstrcmpiW (wszTok, L"A"))    || (! lstrcmpiW (wszTok, L"Cross")))
      button = XINPUT_GAMEPAD_A;
    if ((! lstrcmpiW (wszTok, L"B"))    || (! lstrcmpiW (wszTok, L"Circle")))
      button = XINPUT_GAMEPAD_B;
    if ((! lstrcmpiW (wszTok, L"X"))    || (! lstrcmpiW (wszTok, L"Square")))
      button = XINPUT_GAMEPAD_X;
    if ((! lstrcmpiW (wszTok, L"Y"))    || (! lstrcmpiW (wszTok, L"Triangle")))
      button = XINPUT_GAMEPAD_Y;

    if (button == XINPUT_GAMEPAD_LEFT_TRIGGER)
      state.lt = true;
    else if (button == XINPUT_GAMEPAD_RIGHT_TRIGGER)
      state.rt = true;
    else if (button != 0x00) {
      out [state.buttons++] = button;
    }

    //dll_log.Log (L"Button%lu: %s", state.buttons-1, wszTok);
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
}

#include "ini.h"
#include "parameter.h"

#include <queue>

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

DWORD
WINAPI
unx::InputManager::Hooker::MessagePump (LPVOID hook_ptr)
{
  if ( ((! XInputGetState_Original) && (! gamepad.legacy)) )
    return 0;

  UNX_SetupSpecialButtons ();

  combo_button_s f1 ( &gamepad.f1 );
  combo_button_s f2 ( &gamepad.f2 );
  combo_button_s f3 ( &gamepad.f3 );
  combo_button_s f4 ( &gamepad.f4 );
  combo_button_s f5 ( &gamepad.f5 );
  combo_button_s ss ( &gamepad.screenshot );

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

  std::queue <WORD> pressed_scancodes;

  while (true) {
    int i = 1;

    while (pressed_scancodes.size ()) {
      keys [i++].ki.wScan = pressed_scancodes.back ();
                            pressed_scancodes.pop  ();

      if (i > 4) {
        SendInput (4, &keys [1], sizeof INPUT);
        i = 1;
      }
    }

    if (i > 1)
      SendInput (i-1, &keys [1], sizeof INPUT);

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
          xi_ret = 0;

          JOYINFOEX joy_ex { 0 };
          joy_ex.dwSize  = sizeof JOYINFOEX;
          joy_ex.dwFlags = JOY_RETURNALL      | JOY_RETURNPOVCTS |
                           JOY_RETURNCENTERED | JOY_USEDEADZONE;

          joyGetPosEx (JOYSTICKID1, &joy_ex);

          xi_state.Gamepad.bLeftTrigger  = 0;//joy_ex.dwUpos > 0;
          xi_state.Gamepad.bRightTrigger = 0;//joy_ex.dwVpos > 0;

          xi_state.Gamepad.wButtons = 0;

          if (joy_ex.dwButtons & gamepad.remap.buttons.A)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_A;
          if (joy_ex.dwButtons & gamepad.remap.buttons.B)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_B;
          if (joy_ex.dwButtons & gamepad.remap.buttons.X)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_X;
          if (joy_ex.dwButtons & gamepad.remap.buttons.Y)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_Y;

          if (joy_ex.dwButtons & gamepad.remap.buttons.START)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_START;
          if (joy_ex.dwButtons & gamepad.remap.buttons.BACK)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;

          if (joy_ex.dwButtons & gamepad.remap.buttons.LT)
            xi_state.Gamepad.bLeftTrigger  = 255;
          if (joy_ex.dwButtons & gamepad.remap.buttons.RT)
            xi_state.Gamepad.bRightTrigger = 255;

          if (joy_ex.dwButtons & gamepad.remap.buttons.LB)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
          if (joy_ex.dwButtons & gamepad.remap.buttons.RB)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

          if (joy_ex.dwButtons & gamepad.remap.buttons.LS)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
          if (joy_ex.dwButtons & gamepad.remap.buttons.RS)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

          if (joy_ex.dwPOV & JOY_POVFORWARD)
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
   if (xi_state.Gamepad.bLeftTrigger < 25)
     xi_state.Gamepad.bLeftTrigger = 0;

   if (xi_state.Gamepad.bRightTrigger < 25)
     xi_state.Gamepad.bRightTrigger = 0;

    bool esc_menu = (
        xi_ret == 0                          &&
        xi_state.Gamepad.bLeftTrigger        &&
        xi_state.Gamepad.bRightTrigger       &&
      ( xi_state.Gamepad.wButtons & ( XINPUT_GAMEPAD_START |
                                      XINPUT_GAMEPAD_BACK ) )
    );

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

    f1.poll (xi_ret, xi_state.Gamepad);
    f2.poll (xi_ret, xi_state.Gamepad);
    f3.poll (xi_ret, xi_state.Gamepad);
    f4.poll (xi_ret, xi_state.Gamepad);
    f5.poll (xi_ret, xi_state.Gamepad);
    ss.poll (xi_ret, xi_state.Gamepad);

    WORD scancode = 0;

#define UNX_SendScancode(x) { scancode          = (x);                \
                              keys [0].ki.wScan = scancode;           \
                                                                      \
                              SendInput (1, &keys [0], sizeof INPUT); \
                                                                      \
                              pressed_scancodes.push (scancode); }

    if (esc_menu) {
#if 0
      extern void*
      UNX_Scan (const uint8_t* pattern, size_t len, const uint8_t* mask);

      uint8_t inst [] = { 0x68, 0x88, 0x04, 0x00, 0x00,
                          0xe8, 0x66, 0xe7, 0xf3, 0xff,
                          0x83, 0xc4, 0x04, 0x85, 0xc0
      };
      uint8_t mask [] = { 0xff, 0xff, 0xff, 0xff, 0xff,
                          0x00, 0x00, 0x00, 0x00, 0x00,
                          0xff, 0xff, 0xff, 0xff, 0xff
      };

      typedef void (__fastcall *test_pfn)(void);
      test_pfn test =
        (test_pfn)(UNX_Scan (inst, 15, mask));

      uintptr_t expected = 0x6F1D50;
      uintptr_t offset   = (uintptr_t)test - expected;

      //dll_log.Log (L" Quicksave: %ph", test);

      DWORD quicksave = 0xAE0510 + offset;//0x00648190 + offset;

      //__asm { pushad 
              //call quicksave
              //popad };

      //typedef int (__cdecl *quickie_pfn)(void);
      //quickie_pfn quickie = (quickie_pfn)(0x657BE0/*0x6EFFF0*/ + offset);

      int x = 0;
      int y = 0;
      //quickie ()
#endif

      UNX_SendScancode (0x01);
    }

    if (f1.state)
      UNX_SendScancode (0x3b);

    if (f2.state)
      UNX_SendScancode (0x3c);

    if (f3.state)
      UNX_SendScancode (0x3d);

    if (f4.state)
      UNX_SendScancode (0x3e);

    if (f5.state)
      UNX_SendScancode (0x3f);

    if (four_finger) {
      SetFocus            (SK_GetGameWindow ());
      SetForegroundWindow (SK_GetGameWindow ());
      SetActiveWindow     (SK_GetGameWindow ());

      keys [0].ki.wScan = 0x38;
      SendInput (1, &keys [0], sizeof INPUT);

      Sleep (10);

      keys [0].ki.wScan = 0x3e;
      SendInput (1, &keys [0], sizeof INPUT);

      Sleep (10);
    }

    if (ss.wasJustPressed ()) {
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

    Sleep (15);
  }

  return 0;
}

unx::InputManager::Hooker* unx::InputManager::Hooker::pInputHook = nullptr;