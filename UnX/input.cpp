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

unx::InputManager::gamepad_s gamepad;

typedef void (WINAPI *Sleep_pfn)(DWORD dwMilliseconds);
Sleep_pfn SK_Sleep = nullptr; // Avoid things that MaxDeltaTime does...

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
#if 0
  SK_Sleep =
    (Sleep_pfn)
      GetProcAddress ( GetModuleHandle ( config.system.injector.c_str () ),
                         "Sleep_Detour" );
#else
  SK_Sleep = SpinOrSleep;
#endif

  unx::ParameterFactory factory;
  unx::INI::File* pad_cfg = new unx::INI::File (L"UnX_Gamepad.ini");
  pad_cfg->parse ();

  unx::ParameterStringW* texture_set =
    (unx::ParameterStringW *)factory.create_parameter <std::wstring> (L"Texture Set");
  texture_set->register_to_ini (pad_cfg, L"Gamepad.Type", L"TextureSet");

  if (texture_set->load ())
    gamepad.tex_set = texture_set->get_value ();
  else {
    gamepad.tex_set = L"PlayStation_Glossy";

    texture_set->set_value (gamepad.tex_set);
    texture_set->store     ();
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
  supports_XInput->register_to_ini (pad_cfg, L"Gamepad.Type", L"UseXInput");

  if (supports_XInput->load ()) {
    gamepad.legacy = (! supports_XInput->get_value ());

    if (gamepad.legacy) {
      unx::ParameterInt* A     = (unx::ParameterInt *)factory.create_parameter <int> (L"A");
      A->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_A");

      if (A->load ()) {
        gamepad.legacy_map.A = (gamepad.legacy_map.indexToEnum (A->get_value ()));
      } else {
        A->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.A));
        A->store     ();
      }

      unx::ParameterInt* B     = (unx::ParameterInt *)factory.create_parameter <int> (L"B");
      B->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_B");

      if (B->load ()) {
        gamepad.legacy_map.B = (gamepad.legacy_map.indexToEnum (B->get_value ()));
      } else {
        B->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.B));
        B->store     ();
      }

      unx::ParameterInt* X     = (unx::ParameterInt *)factory.create_parameter <int> (L"X");
      X->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_X");

      if (X->load ()) {
        gamepad.legacy_map.X = (gamepad.legacy_map.indexToEnum (X->get_value ()));
      } else {
        X->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.X));
        X->store     ();
      }

      unx::ParameterInt* Y     = (unx::ParameterInt *)factory.create_parameter <int> (L"Y");
      Y->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_Y");

      if (Y->load ()) {
        gamepad.legacy_map.Y = (gamepad.legacy_map.indexToEnum (Y->get_value ()));
      } else {
        Y->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.Y));
        Y->store     ();
      }

      unx::ParameterInt* START = (unx::ParameterInt *)factory.create_parameter <int> (L"START");
      START->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_Start");

      if (START->load ()) {
        gamepad.legacy_map.START = (gamepad.legacy_map.indexToEnum (START->get_value ()));
      } else {
        START->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.START));
        START->store     ();
      }

      unx::ParameterInt* BACK  = (unx::ParameterInt *)factory.create_parameter <int> (L"BACK");
      BACK->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_Back");

      if (BACK->load ()) {
        gamepad.legacy_map.BACK = (gamepad.legacy_map.indexToEnum (BACK->get_value ()));
      } else {
        BACK->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.BACK));
        BACK->store     ();
      }

      unx::ParameterInt* LB    = (unx::ParameterInt *)factory.create_parameter <int> (L"LB");
      LB->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_LB");

      if (LB->load ()) {
        gamepad.legacy_map.LB = (gamepad.legacy_map.indexToEnum (LB->get_value ()));
      } else {
        LB->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.LB));
        LB->store     ();
      }

      unx::ParameterInt* RB    = (unx::ParameterInt *)factory.create_parameter <int> (L"RB");
      RB->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_RB");

      if (RB->load ()) {
        gamepad.legacy_map.RB = (gamepad.legacy_map.indexToEnum (RB->get_value ()));
      } else {
        RB->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.RB));
        RB->store     ();
      }

      unx::ParameterInt* LT    = (unx::ParameterInt *)factory.create_parameter <int> (L"LT");
      LT->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_LT");

      if (LT->load ()) {
        gamepad.legacy_map.LT = (gamepad.legacy_map.indexToEnum (LT->get_value ()));
      } else {
        LT->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.LT));
        LT->store     ();
      }

      unx::ParameterInt* RT    = (unx::ParameterInt *)factory.create_parameter <int> (L"RT");
      RT->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_RT");

      if (RT->load ()) {
        gamepad.legacy_map.RT = (gamepad.legacy_map.indexToEnum (RT->get_value ()));
      } else {
        RT->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.RT));
        RT->store     ();
      }

      unx::ParameterInt* LS    = (unx::ParameterInt *)factory.create_parameter <int> (L"LS");
      LS->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_LS");

      if (LS->load ()) {
        gamepad.legacy_map.LS = (gamepad.legacy_map.indexToEnum (LS->get_value ()));
      } else {
        LS->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.LS));
        LS->store     ();
      }

      unx::ParameterInt* RS    = (unx::ParameterInt *)factory.create_parameter <int> (L"RS");
      RS->register_to_ini (pad_cfg, L"Gamepad.Legacy", L"XInput_RS");

      if (RS->load ()) {
        gamepad.legacy_map.RS = (gamepad.legacy_map.indexToEnum (RS->get_value ()));
      } else {
        RS->set_value (gamepad.legacy_map.enumToIndex (gamepad.legacy_map.RS));
        RS->store     ();
      }
    }
  } else {
    supports_XInput->set_value (true);
    supports_XInput->store     ();
  }

  unx::iParameter* combo_ESC =
    factory.create_parameter <std::wstring> (L"ESC Buttons");

  unx::iParameter* combo_F1 =
    factory.create_parameter <std::wstring> (L"F1 Buttons");
  combo_F1->register_to_ini (pad_cfg, L"Gamepad.PC", L"F1");

  if (combo_F1->load ())
    gamepad.f1.unparsed = combo_F1->get_value_str ();
  else {
    gamepad.f1.unparsed = L"Select+Cross";

    combo_F1->set_value_str (gamepad.f1.unparsed);
    combo_F1->store         ();
  }

  unx::iParameter* combo_F2 =
    factory.create_parameter <std::wstring> (L"F2 Buttons");
  combo_F2->register_to_ini (pad_cfg, L"Gamepad.PC", L"F2");

  if (combo_F2->load ())
    gamepad.f2.unparsed = combo_F2->get_value_str ();
  else {
    gamepad.f2.unparsed = L"Select+Circle";

    combo_F2->set_value_str (gamepad.f2.unparsed);
    combo_F2->store         ();
  }

  unx::iParameter* combo_F3 =
    factory.create_parameter <std::wstring> (L"F3 Buttons");
  combo_F3->register_to_ini (pad_cfg, L"Gamepad.PC", L"F3");

  if (combo_F3->load ())
    gamepad.f3.unparsed = combo_F3->get_value_str ();
  else {
    gamepad.f3.unparsed = L"Select+Square";

    combo_F3->set_value_str (gamepad.f3.unparsed);
    combo_F3->store         ();
  }

  unx::iParameter* combo_F4 =
    factory.create_parameter <std::wstring> (L"F4 Buttons");
  combo_F4->register_to_ini (pad_cfg, L"Gamepad.PC", L"F4");

  if (combo_F4->load ())
    gamepad.f4.unparsed = combo_F4->get_value_str ();
  else {
    gamepad.f4.unparsed = L"Select+L1";

    combo_F4->set_value_str (gamepad.f4.unparsed);
    combo_F4->store         ();
  }

  unx::iParameter* combo_F5 =
    factory.create_parameter <std::wstring> (L"F5 Buttons");
  combo_F5->register_to_ini (pad_cfg, L"Gamepad.PC", L"F5");

  if (combo_F5->load ())
    gamepad.f5.unparsed = combo_F5->get_value_str ();
  else {
    gamepad.f5.unparsed = L"Select+R1";

    combo_F5->set_value_str (gamepad.f5.unparsed);
    combo_F5->store         ();
  }

  pad_cfg->write (L"UnX_Gamepad.ini");
  delete pad_cfg;

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

#define XINPUT_GAMEPAD_DPAD_UP    0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN  0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT  0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008

#define XINPUT_GAMEPAD_START       0x0010
#define XINPUT_GAMEPAD_BACK        0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB  0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080

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
//#include <joystickapi.h>

using namespace unx::InputManager;

void
UNX_SetupSpecialButtons (void)
{
  button_map_state_s state =
    UNX_ParseButtonCombo (gamepad.f1.unparsed, &gamepad.f1.button0);

  gamepad.f1.lt      = state.lt;
  gamepad.f1.rt      = state.rt;
  gamepad.f1.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo (gamepad.f2.unparsed, &gamepad.f2.button0);

  gamepad.f2.lt      = state.lt;
  gamepad.f2.rt      = state.rt;
  gamepad.f2.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo (gamepad.f3.unparsed, &gamepad.f3.button0);

  gamepad.f3.lt      = state.lt;
  gamepad.f3.rt      = state.rt;
  gamepad.f3.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo (gamepad.f4.unparsed, &gamepad.f4.button0);

  gamepad.f4.lt      = state.lt;
  gamepad.f4.rt      = state.rt;
  gamepad.f4.buttons = state.buttons;

  state =
    UNX_ParseButtonCombo (gamepad.f5.unparsed, &gamepad.f5.button0);

  gamepad.f5.lt      = state.lt;
  gamepad.f5.rt      = state.rt;
  gamepad.f5.buttons = state.buttons;
}

#include "ini.h"
#include "parameter.h"

DWORD
WINAPI
unx::InputManager::Hooker::MessagePump (LPVOID hook_ptr)
{
  //factory.create_parameter (L"")

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
      SK_Sleep (500);
      continue;
    }

    break;
  }

  SK_GetCommandProcessor ()->ProcessCommandLine ("TargetFPS 0.0");

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

    SK_Sleep (1);
  }
#endif

  dll_log.Log ( L"[   Input  ] * Installed keyboard hook for command console... "
                      L"%lu %s (%lu ms!)",
                hits,
                  hits > 1 ? L"tries" : L"try",
                    timeGetTime () - dwTime );

  if ( ((! XInputGetState) && (! gamepad.legacy)) )
    return 0;

  UNX_SetupSpecialButtons ();

#define XI_POLL_INTERVAL 500UL

  DWORD  xi_ret       = 0;
  DWORD  last_xi_poll = timeGetTime () - (XI_POLL_INTERVAL + 1UL);

  BOOL need_release = FALSE;

  INPUT* keys  =
     new INPUT [2];

  while (true) {
    if (need_release) {
      SendInput (1, &keys [1], sizeof INPUT);
    }

    int slot = config.input.gamepad_slot;
    if (slot == -1)
      slot = 0;

    XINPUT_STATE xi_state { 0 };

    if (XInputGetState != nullptr && (! gamepad.legacy)) {
      // This function is actually a performance hazzard when no controllers
      //   are plugged in, so ... throttle the sucker.
      if (  xi_ret != ERROR_DEVICE_NOT_CONNECTED ||
           (last_xi_poll < timeGetTime () - XI_POLL_INTERVAL) ) {
        xi_ret       = XInputGetState (slot, &xi_state);
        last_xi_poll = timeGetTime    ();
      }
    } else {
      if (  xi_ret != ERROR_DEVICE_NOT_CONNECTED ||
           (last_xi_poll < timeGetTime () - XI_POLL_INTERVAL) ) {
        if (! joyGetNumDevs ())
          xi_ret = ERROR_DEVICE_NOT_CONNECTED;
        else {
          xi_ret = 0;//

          //JOYINFO joy;
          JOYINFOEX joy_ex { 0 };
          joy_ex.dwSize  = sizeof JOYINFOEX;
          joy_ex.dwFlags = JOY_RETURNALL      | JOY_RETURNPOVCTS |
                           JOY_RETURNCENTERED | JOY_USEDEADZONE;

          joyGetPosEx (JOYSTICKID1, &joy_ex);

          xi_state.Gamepad.bLeftTrigger  = 0;//joy_ex.dwUpos > 0;
          xi_state.Gamepad.bRightTrigger = 0;//joy_ex.dwVpos > 0;

          xi_state.Gamepad.wButtons = 0;

          if (joy_ex.dwButtons & gamepad.legacy_map.A)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_A;
          if (joy_ex.dwButtons & gamepad.legacy_map.B)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_B;
          if (joy_ex.dwButtons & gamepad.legacy_map.X)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_X;
          if (joy_ex.dwButtons & gamepad.legacy_map.Y)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_Y;

          if (joy_ex.dwButtons & gamepad.legacy_map.START)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_START;
          if (joy_ex.dwButtons & gamepad.legacy_map.BACK)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;

          if (joy_ex.dwButtons & gamepad.legacy_map.LT)
            xi_state.Gamepad.bLeftTrigger  = 1;
          if (joy_ex.dwButtons & gamepad.legacy_map.RT)
            xi_state.Gamepad.bRightTrigger = 1;

          if (joy_ex.dwButtons & gamepad.legacy_map.LB)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
          if (joy_ex.dwButtons & gamepad.legacy_map.RB)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

          if (joy_ex.dwButtons & gamepad.legacy_map.LS)
            xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
          if (joy_ex.dwButtons & gamepad.legacy_map.RS)
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

    bool four_finger =
        config.input.four_finger_salute &&
        xi_state.Gamepad.bLeftTrigger   &&
        xi_state.Gamepad.bRightTrigger  &&
      ( xi_state.Gamepad.wButtons & ( XINPUT_GAMEPAD_START |
                                      XINPUT_GAMEPAD_BACK ) );

    bool f1 =
        gamepad.f1.buttons != 0                                &&
        ((! gamepad.f1.lt) || xi_state.Gamepad.bLeftTrigger)   &&
        ((! gamepad.f1.rt) || xi_state.Gamepad.bRightTrigger)  &&
        xi_state.Gamepad.wButtons & gamepad.f1.button0         &&
        xi_state.Gamepad.wButtons & gamepad.f1.button1         &&
        xi_state.Gamepad.wButtons & gamepad.f1.button2;

    bool f2 =
        gamepad.f2.buttons != 0                                &&
        ((! gamepad.f2.lt) || xi_state.Gamepad.bLeftTrigger)   &&
        ((! gamepad.f2.rt) || xi_state.Gamepad.bRightTrigger)  &&
        xi_state.Gamepad.wButtons & gamepad.f2.button0         &&
        xi_state.Gamepad.wButtons & gamepad.f2.button1         &&
        xi_state.Gamepad.wButtons & gamepad.f2.button2;

    bool f3 =
        gamepad.f3.buttons != 0                                &&
        ((! gamepad.f3.lt) || xi_state.Gamepad.bLeftTrigger)   &&
        ((! gamepad.f3.rt) || xi_state.Gamepad.bRightTrigger)  &&
        xi_state.Gamepad.wButtons & gamepad.f3.button0         &&
        xi_state.Gamepad.wButtons & gamepad.f3.button1         &&
        xi_state.Gamepad.wButtons & gamepad.f3.button2;

    bool f4 =
        gamepad.f4.buttons != 0                                &&
        ((! gamepad.f4.lt) || xi_state.Gamepad.bLeftTrigger)   &&
        ((! gamepad.f4.rt) || xi_state.Gamepad.bRightTrigger)  &&
        xi_state.Gamepad.wButtons & gamepad.f4.button0         &&
        xi_state.Gamepad.wButtons & gamepad.f4.button1         &&
        xi_state.Gamepad.wButtons & gamepad.f4.button2;

    bool f5 =
        gamepad.f5.buttons != 0                                &&
        ((! gamepad.f5.lt) || xi_state.Gamepad.bLeftTrigger)   &&
        ((! gamepad.f5.rt) || xi_state.Gamepad.bRightTrigger)  &&
        xi_state.Gamepad.wButtons & gamepad.f5.button0         &&
        xi_state.Gamepad.wButtons & gamepad.f5.button1         &&
        xi_state.Gamepad.wButtons & gamepad.f5.button2;

    WORD scancode = 0;

    bool hotkey = true;

    if (four_finger) {
      extern LPVOID __UNX_base_img_addr;
      uint16_t* scenario_flag = (uint16_t *)((uintptr_t)__UNX_base_img_addr + (0x12FB784-0x00401000-0x04));
      dll_log.Log (L"Scenario Flag: %u", *scenario_flag);

      //typedef int (__fastcall *ffxDebugMenu_pfn)(void);
      //extern LPVOID __UNX_base_img_addr;
      //ffxDebugMenu_pfn ffxDebug =
        //(ffxDebugMenu_pfn)((uint8_t *)(uint8_t *)GetModuleHandle (nullptr) + 0x257BE0);
      //ffxDebug ();

      scancode = 0x01;
    }
    else if (f1)
      scancode = 0x3b;
    else if (f2)
      scancode = 0x3c;
    else if (f3)
      scancode = 0x3d;
    else if (f4)
      scancode = 0x3e;
    else if (f5)
      scancode = 0x3f;
    else
      hotkey = false;

    if (hotkey) {
      keys [0].type           = INPUT_KEYBOARD;
      keys [0].ki.wVk         = 0;

      keys [0].ki.wScan       = scancode;
      keys [0].ki.dwFlags     = KEYEVENTF_SCANCODE;
      keys [0].ki.time        = 0;
      keys [0].ki.dwExtraInfo = 0;

      SendInput (1, &keys [0], sizeof INPUT);

      keys [1].type           = INPUT_KEYBOARD;
      keys [1].ki.wVk         = 0;

      keys [1].ki.wScan       = scancode;
      keys [1].ki.dwFlags     = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
      keys [1].ki.time        = 0;
      keys [1].ki.dwExtraInfo = 0;

      need_release = TRUE;
    } else {
      need_release = FALSE;
    }

    SK_Sleep (10);
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