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

using SK_GetGameWindow_pfn = HWND (WINAPI *)(void);
SK_GetGameWindow_pfn SK_GetGameWindow;

unx_gamepad_s gamepad;

extern void UNX_InstallWindowHook (HWND hWnd);

  unx::ParameterStringW* texture_set;

///////////////////////////////////////////////////////////////////////////////
//
// DirectInput 8
//
///////////////////////////////////////////////////////////////////////////////
using IDirectInputDevice8_GetDeviceState_pfn = HRESULT (WINAPI *)(
  LPDIRECTINPUTDEVICE8W  This,
  DWORD                  cbData,
  LPVOID                 lpvData
);

struct SK_DI8_Keyboard {
  LPDIRECTINPUTDEVICE8W pDev = nullptr;
  uint8_t               state [512];
} *_dik;

typedef struct SK_DI8_Mouse {
  LPDIRECTINPUTDEVICE8W pDev = nullptr;
  DIMOUSESTATE2         state;
} *_dim;

using SK_Input_GetDI8Mouse_pfn = SK_DI8_Mouse*    (WINAPI *)   (void);
using SK_Input_GetDI8Keyboard_pfn = SK_DI8_Keyboard* (WINAPI *)(void);

SK_Input_GetDI8Mouse_pfn    SK_Input_GetDI8Mouse      = nullptr;
SK_Input_GetDI8Keyboard_pfn SK_Input_GetDI8Keyboard   = nullptr;

IDirectInputDevice8_GetDeviceState_pfn
        IDirectInputDevice8_GetDeviceState_Original   = nullptr;


// I don't care about joysticks, let them continue working while
//   the window does not have focus...

HRESULT
WINAPI
IDirectInputDevice8_GetDeviceState_Detour ( LPDIRECTINPUTDEVICE8W      This,
                                            DWORD                      cbData,
                                            LPVOID                     lpvData )
{
  HRESULT hr;
  hr = IDirectInputDevice8_GetDeviceState_Original ( This,
                                                       cbData,
                                                         lpvData );

  if (SUCCEEDED (hr))
  {
    if (cbData == sizeof DIJOYSTATE2)
    {
      auto* out = (DIJOYSTATE2 *)lpvData;

      // Fix Wonky Input Behavior When Window is Not In Foreground
      if (! unx::window.active/*GetForegroundWindow () != SK_GetGameWindow ()*/)
      {
        memset (out, 0, sizeof DIJOYSTATE2);

        out->rgdwPOV [0] = static_cast <DWORD> (-1);
        out->rgdwPOV [1] = static_cast <DWORD> (-1);
        out->rgdwPOV [2] = static_cast <DWORD> (-1);
        out->rgdwPOV [3] = static_cast <DWORD> (-1);
      }

      else
      {
        DIJOYSTATE2  in  = *out;

        for (int i = 0; i < 12; i++)
        {
          // Negative values are for axes, we cannot remap those yet
          if (gamepad.remap.map [ i ] >= 0)
          {
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
  if (This == SK_Input_GetDI8Keyboard ()->pDev)
  {
    if (unx::window.active) memcpy (SK_Input_GetDI8Keyboard ()->state, lpvData, cbData);

    else
    {
      memcpy (lpvData, SK_Input_GetDI8Keyboard ()->state, cbData);

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
  if (This == SK_Input_GetDI8Mouse ()->pDev)
  {
    if (unx::window.active) memcpy (&SK_Input_GetDI8Mouse ()->state, lpvData, cbData);
    else                    memcpy (lpvData, &SK_Input_GetDI8Mouse ()->state, cbData);
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

using XInputGetState_pfn = DWORD (WINAPI *)(
  _In_  DWORD        dwUserIndex,
  _Out_ XINPUT_STATE *pState
);

XInputGetState_pfn XInputGetState_Original = nullptr;


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

using SK_XInput_PollController_pfn = bool (WINAPI *)( INT           iJoyID,
                                                      XINPUT_STATE* pState );
SK_XInput_PollController_pfn SK_XInput_PollController = nullptr;

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

using SK_PluginKeyPress_pfn = void (CALLBACK *)( BOOL Control,
                        BOOL Shift,
                        BOOL Alt,
                        BYTE vkCode );
SK_PluginKeyPress_pfn SK_PluginKeyPress_Original = nullptr;


void
UNX_KickStart (void)
{
  SK_ICommandResult result = 
    SK_GetCommandProcessor ()->ProcessCommandLine ("Window.OverrideRes");

  if (result.getVariable () != nullptr)
  {
    CreateThread (nullptr, 0, [] (LPVOID) ->
    DWORD
    {
      RECT client;
      GetClientRect (unx::window.hwnd, &client);

      SK_GetCommandProcessor ()->ProcessCommandLine (
        "Window.OverrideRes 640x480 "
      );
      SK_GetCommandProcessor ()->ProcessCommandFormatted (
        "Window.OverrideRes %lux%lu ",
          client.right  - client.left,
          client.bottom - client.top );
      SK_GetCommandProcessor ()->ProcessCommandLine (
        "Window.OverrideRes 0x0 "
      );

      CloseHandle (GetCurrentThread ());

      return 0;
    }, nullptr, 0x00, nullptr);
  }
}

UNX_Keybindings keybinds;


#include <unordered_map>

std::unordered_map <std::wstring, BYTE> humanKeyNameToVirtKeyCode;
std::unordered_map <BYTE, std::wstring> virtKeyCodeToHumanKeyName;

#include <queue>

#define SK_MakeKeyMask(vKey,ctrl,shift,alt) \
  (UINT)((vKey) | (((ctrl) != 0) <<  9) |   \
                  (((shift)!= 0) << 10) |   \
                  (((alt)  != 0) << 11))

void
SK_Keybind::update (void)
{
  human_readable = L"";

  std::wstring key_name = virtKeyCodeToHumanKeyName [(BYTE)(vKey & 0xFF)];

  if (! key_name.length ())
    return;

  std::queue <std::wstring> words;

  if (modifiers.ctrl)
    words.push (L"Ctrl");

  if (modifiers.alt)
    words.push (L"Alt");

  if (modifiers.shift)
    words.push (L"Shift");

  words.push (key_name);

  while (! words.empty ())
  {
    human_readable += words.front ();
    words.pop ();

    if (! words.empty ())
      human_readable += L"+";
  }

  masked_code = SK_MakeKeyMask (vKey & 0xFF, modifiers.ctrl, modifiers.shift, modifiers.alt);
}

void
SK_Keybind::parse (void)
{
  vKey = 0x00;

  static bool init = false;

  if (! init)
  {
    init = true;

    for (int i = 0; i < 0xFF; i++)
    {
      wchar_t name [32] = { };

      switch (i)
      {
        case VK_F1:     wcscat (name, L"F1");           break;
        case VK_F2:     wcscat (name, L"F2");           break;
        case VK_F3:     wcscat (name, L"F3");           break;
        case VK_F4:     wcscat (name, L"F4");           break;
        case VK_F5:     wcscat (name, L"F5");           break;
        case VK_F6:     wcscat (name, L"F6");           break;
        case VK_F7:     wcscat (name, L"F7");           break;
        case VK_F8:     wcscat (name, L"F8");           break;
        case VK_F9:     wcscat (name, L"F9");           break;
        case VK_F10:    wcscat (name, L"F10");          break;
        case VK_F11:    wcscat (name, L"F11");          break;
        case VK_F12:    wcscat (name, L"F12");          break;
        case VK_F13:    wcscat (name, L"F13");          break;
        case VK_F14:    wcscat (name, L"F14");          break;
        case VK_F15:    wcscat (name, L"F15");          break;
        case VK_F16:    wcscat (name, L"F16");          break;
        case VK_F17:    wcscat (name, L"F17");          break;
        case VK_F18:    wcscat (name, L"F18");          break;
        case VK_F19:    wcscat (name, L"F19");          break;
        case VK_F20:    wcscat (name, L"F20");          break;
        case VK_F21:    wcscat (name, L"F21");          break;
        case VK_F22:    wcscat (name, L"F22");          break;
        case VK_F23:    wcscat (name, L"F23");          break;
        case VK_F24:    wcscat (name, L"F24");          break;
        case VK_PRINT:  wcscat (name, L"Print Screen"); break;
        case VK_SCROLL: wcscat (name, L"Scroll Lock");  break;
        case VK_PAUSE:  wcscat (name, L"Pause Break");  break;

        default:
        {
          unsigned int scanCode =
            ( MapVirtualKey (i, 0) & 0xFF );

                        BYTE buf [256] = { };
          unsigned short int temp      =  0;
          
          bool asc = (i <= 32);

          if (! asc && i != VK_DIVIDE)
             asc = ToAscii ( i, scanCode, buf, &temp, 1 );

          scanCode            <<= 16;
          scanCode   |= ( 0x1 <<  25  );

          if (! asc)
            scanCode |= ( 0x1 << 24   );
    
          GetKeyNameText ( scanCode,
                             name,
                               32 );
        } break;
      }

    
      if ( i != VK_CONTROL  && i != VK_MENU     &&
           i != VK_SHIFT    && i != VK_OEM_PLUS && i != VK_OEM_MINUS &&
           i != VK_LSHIFT   && i != VK_RSHIFT   &&
           i != VK_LCONTROL && i != VK_RCONTROL &&
           i != VK_LMENU    && i != VK_RMENU )
      {

        humanKeyNameToVirtKeyCode.emplace (name, (BYTE)i);
        virtKeyCodeToHumanKeyName.emplace ((BYTE)i, name);
      }
    }
    
    humanKeyNameToVirtKeyCode.emplace (L"Plus",        (BYTE)VK_OEM_PLUS);
    humanKeyNameToVirtKeyCode.emplace (L"Minus",       (BYTE)VK_OEM_MINUS);
    humanKeyNameToVirtKeyCode.emplace (L"Ctrl",        (BYTE)VK_CONTROL);
    humanKeyNameToVirtKeyCode.emplace (L"Alt",         (BYTE)VK_MENU);
    humanKeyNameToVirtKeyCode.emplace (L"Shift",       (BYTE)VK_SHIFT);
    humanKeyNameToVirtKeyCode.emplace (L"Left Shift",  (BYTE)VK_LSHIFT);
    humanKeyNameToVirtKeyCode.emplace (L"Right Shift", (BYTE)VK_RSHIFT);
    humanKeyNameToVirtKeyCode.emplace (L"Left Alt",    (BYTE)VK_LMENU);
    humanKeyNameToVirtKeyCode.emplace (L"Right Alt",   (BYTE)VK_RMENU);
    humanKeyNameToVirtKeyCode.emplace (L"Left Ctrl",   (BYTE)VK_LCONTROL);
    humanKeyNameToVirtKeyCode.emplace (L"Right Ctrl",  (BYTE)VK_RCONTROL);
    
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_CONTROL,   L"Ctrl");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_MENU,      L"Alt");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_SHIFT,     L"Shift");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_OEM_PLUS,  L"Plus");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_OEM_MINUS, L"Minus");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_LSHIFT,    L"Left Shift");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_RSHIFT,    L"Right Shift");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_LMENU,     L"Left Alt");
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_RMENU,     L"Right Alt");  
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_LCONTROL,  L"Left Ctrl"); 
    virtKeyCodeToHumanKeyName.emplace ((BYTE)VK_RCONTROL,  L"Right Ctrl");

    init = true;
  }

  wchar_t wszKeyBind [128] = { };

  lstrcatW (wszKeyBind, human_readable.c_str ());

  wchar_t* wszBuf = nullptr;
  wchar_t* wszTok = std::wcstok (wszKeyBind, L"+", &wszBuf);

  modifiers.ctrl  = false;
  modifiers.alt   = false;
  modifiers.shift = false;

  if (wszTok == nullptr)
  {
    vKey = humanKeyNameToVirtKeyCode [wszKeyBind];
  }

  while (wszTok)
  {
    BYTE vKey_ = humanKeyNameToVirtKeyCode [wszTok];

    if (vKey_ == VK_CONTROL)
      modifiers.ctrl  = true;
    else if (vKey_ == VK_SHIFT)
      modifiers.shift = true;
    else if (vKey_ == VK_MENU)
      modifiers.alt   = true;
    else
      vKey = vKey_;

    wszTok = std::wcstok (nullptr, L"+", &wszBuf);
  }

  masked_code = SK_MakeKeyMask (vKey & 0xFF, modifiers.ctrl, modifiers.shift, modifiers.alt);
}

void
CALLBACK
SK_UNX_PluginKeyPress ( BOOL Control,
                        BOOL Shift,
                        BOOL Alt,
                        BYTE vkCode )
{
  extern __declspec (dllimport) bool SK_ImGui_Visible;

  // Don't trigger keybindings while we are setting them ;)
  if (SK_ImGui_Visible)
    return;


  auto masked_key =
    SK_MakeKeyMask (vkCode & 0xFF, Control, Shift, Alt);

  if (masked_key == keybinds.SpeedStep.masked_code)
  {
    extern void UNX_SpeedStep (); 
                UNX_SpeedStep ();
  }

  else if (masked_key == keybinds.KickStart.masked_code)
  {
    UNX_KickStart ();
  }

  else if (masked_key == keybinds.TimeStop.masked_code)
  {
    extern void UNX_TimeStop (void);
                UNX_TimeStop ();
  }

  else if (masked_key == keybinds.FullAP.masked_code)
  {
    extern void UNX_TogglePartyAP (void);
                UNX_TogglePartyAP ();
  }

  else if (masked_key == keybinds.Sensor.masked_code)
  {
    extern void UNX_ToggleSensor (void);
                UNX_ToggleSensor ();
  }

  else if (masked_key == keybinds.FreeLook.masked_code)
  {
    extern void UNX_ToggleFreeLook (void);
                UNX_ToggleFreeLook ();
  }

  // FFX Soft Reset
  else if (masked_key == keybinds.SoftReset.masked_code)
  {
    extern bool UNX_KillMeNow (void);
                UNX_KillMeNow ();
  }

  else if (masked_key == keybinds.VSYNC.masked_code)
  {
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

  //else if (vkCode == 'U') {
  //  extern void UNX_FFX2_UnitTest (void);
  //  UNX_FFX2_UnitTest ();
  //}

  else if (Control && Shift && vkCode == 'Q')
  {
    extern void UNX_Quickie (void);
    UNX_Quickie ();
  }

  SK_PluginKeyPress_Original (Control, Shift, Alt, vkCode);
}

using  SK_GetConfigPath_pfn = const wchar_t* (__stdcall *)(void);
extern SK_GetConfigPath_pfn SK_GetConfigPath;

iSK_INI* pad_cfg = nullptr;

extern unx::ParameterStringW* unx_speedstep;
extern unx::ParameterStringW* unx_kickstart;
extern unx::ParameterStringW* unx_timestop;
extern unx::ParameterStringW* unx_freelook;
extern unx::ParameterStringW* unx_sensor;
extern unx::ParameterStringW* unx_fullap;
extern unx::ParameterStringW* unx_VSYNC;
extern unx::ParameterStringW* unx_soft_reset;

void
unx::InputManager::Init (void)
{
  SK_GetGameWindow =
    (SK_GetGameWindow_pfn)
      GetProcAddress ( GetModuleHandle (config.system.injector.c_str ()),
                         "SK_GetGameWindow" );

  unx::ParameterFactory factory;
  pad_cfg =
    UNX_CreateINI (
      std::wstring (
        std::wstring (SK_GetConfigPath ()) + L"UnX_Gamepad.ini"
      ).c_str ()
    );

  auto LoadKeybind =
    [&](SK_Keybind* binding, wchar_t* ini_name) ->
      auto
      {
        unx::ParameterStringW* ret =
         dynamic_cast <unx::ParameterStringW *>
          (factory.create_parameter <std::wstring> (L"DESCRIPTION HERE"));

        ret->register_to_ini ( pad_cfg, L"UNX.Keybinds", ini_name );

        if (! ret->load (binding->human_readable))
        {
          binding->parse ();
          ret->store     (binding->human_readable);
        }

        binding->human_readable = ret->get_value ();
        binding->parse ();

        return ret;
      };

  unx_speedstep  = LoadKeybind (&keybinds.SpeedStep,       L"CycleSpeedBoost");
  unx_kickstart  = LoadKeybind (&keybinds.KickStart,       L"KickStart");
  unx_timestop   = LoadKeybind (&keybinds.TimeStop,        L"ToggleTimeStop");
  unx_freelook   = LoadKeybind (&keybinds.FreeLook,        L"ToggleFreeLook");
  unx_sensor     = LoadKeybind (&keybinds.Sensor,          L"TogglePermanentSensor");
  unx_fullap     = LoadKeybind (&keybinds.FullAP,          L"ToggleFullPartyAP");
  unx_VSYNC      = LoadKeybind (&keybinds.VSYNC,           L"ToggleVSYNC");
  unx_soft_reset = LoadKeybind (&keybinds.SoftReset,       L"SoftReset");

  texture_set =
    (unx::ParameterStringW *)factory.create_parameter <std::wstring> (L"Texture Set");
  texture_set->register_to_ini (pad_cfg, L"Gamepad.Type", L"TextureSet");

  if (! texture_set->load (gamepad.tex_set))
  {
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

    wchar_t wszPadRoot [MAX_PATH] = { };

    lstrcatW (wszPadRoot, L"gamepads\\");
    lstrcatW (wszPadRoot, config.textures.gamepad.c_str ());
    lstrcatW (wszPadRoot, L"\\");

    dll_log->Log (L"[Button Map] Button Pack: %s", wszPadRoot);

    wchar_t wszPadIcons [MAX_PATH] = { };

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
        if (new_button)
        {
          if (i > 0)
          {
            dll_log->LogEx
                        ( false, L"\n" );
          }

          dll_log->LogEx ( true, L"[Button Map] Button %10s: '%#38s' ( %08x :: ",
                           wszButtons [i / pad_lods],
                             wszPadButton,
                               hash );

          new_button = false;
        }

        else
        {
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


  gamepad.f1.combo_name         = "F1";
  gamepad.f2.combo_name         = "F2";
  gamepad.f3.combo_name         = "F3";
  gamepad.f4.combo_name         = "F4";
  gamepad.f5.combo_name         = "F5";
  gamepad.screenshot.combo_name = "Steam Screenshot";
  gamepad.fullscreen.combo_name = "Fullscreen Toggle";
  gamepad.esc.combo_name        = "PC Game Menu";
  gamepad.speedboost.combo_name = "Speed Hack";
  gamepad.kickstart.combo_name  = "Kickstart";
  gamepad.softreset.combo_name  = "Soft Game Reset";


  auto* supports_XInput = 
    (unx::ParameterBool *)factory.create_parameter <bool> (L"Disable XInput?");
  supports_XInput->register_to_ini (pad_cfg, L"Gamepad.Type", L"UsesXInput");

  if (((unx::iParameter *)supports_XInput)->load ())
  {
    gamepad.legacy = (! supports_XInput->get_value ());
  } else {
    supports_XInput->store (true);
  }

  //if (gamepad.legacy)
  {
    auto* A     = (unx::ParameterInt *)factory.create_parameter <int> (L"A");
    A->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_A");

    if (((unx::iParameter *)A)->load ())
    {
      gamepad.remap.buttons.A = (gamepad.remap.indexToEnum (A->get_value ()));
    } else {
      A->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.A));
    }

    auto* B     = (unx::ParameterInt *)factory.create_parameter <int> (L"B");
    B->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_B");

    if (((unx::iParameter *)B)->load ())
    {
      gamepad.remap.buttons.B = (gamepad.remap.indexToEnum (B->get_value ()));
    } else {
      B->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.B));
    }

    auto* X     = (unx::ParameterInt *)factory.create_parameter <int> (L"X");
    X->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_X");

    if (((unx::iParameter *)X)->load ())
    {
      gamepad.remap.buttons.X = (gamepad.remap.indexToEnum (X->get_value ()));
    } else {
      X->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.X));
    }

    auto* Y     = (unx::ParameterInt *)factory.create_parameter <int> (L"Y");
    Y->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_Y");

    if (((unx::iParameter *)Y)->load ())
    {
      gamepad.remap.buttons.Y = (gamepad.remap.indexToEnum (Y->get_value ()));
    } else {
      Y->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.Y));
    }

    auto* START = (unx::ParameterInt *)factory.create_parameter <int> (L"START");
    START->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_Start");

    if (((unx::iParameter *)START)->load ())
    {
      gamepad.remap.buttons.START = (gamepad.remap.indexToEnum (START->get_value ()));
    } else {
      START->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.START));
    }

    auto* BACK  = (unx::ParameterInt *)factory.create_parameter <int> (L"BACK");
    BACK->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_Back");

    if (((unx::iParameter *)BACK)->load ())
    {
      gamepad.remap.buttons.BACK = (gamepad.remap.indexToEnum (BACK->get_value ()));
    } else {
      BACK->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.BACK));
    }

    auto* LB    = (unx::ParameterInt *)factory.create_parameter <int> (L"LB");
    LB->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_LB");

    if (((unx::iParameter *)LB)->load ())
    {
      gamepad.remap.buttons.LB = (gamepad.remap.indexToEnum (LB->get_value ()));
    } else {
      LB->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.LB));
    }

    auto* RB    = (unx::ParameterInt *)factory.create_parameter <int> (L"RB");
    RB->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_RB");

    if (((unx::iParameter *)RB)->load ())
    {
      gamepad.remap.buttons.RB = (gamepad.remap.indexToEnum (RB->get_value ()));
    } else {
      RB->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.RB));
    }

    auto* LT    = (unx::ParameterInt *)factory.create_parameter <int> (L"LT");
    LT->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_LT");

    if (((unx::iParameter *)LT)->load ())
    {
      gamepad.remap.buttons.LT = (gamepad.remap.indexToEnum (LT->get_value ()));
    } else {
      LT->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.LT));
    }

    auto* RT    = (unx::ParameterInt *)factory.create_parameter <int> (L"RT");
    RT->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_RT");

    if (((unx::iParameter *)RT)->load ())
    {
      gamepad.remap.buttons.RT = (gamepad.remap.indexToEnum (RT->get_value ()));
    } else {
      RT->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.RT));
    }

    auto* LS    = (unx::ParameterInt *)factory.create_parameter <int> (L"LS");
    LS->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_LS");

    if (((unx::iParameter *)LS)->load ())
    {
      gamepad.remap.buttons.LS = (gamepad.remap.indexToEnum (LS->get_value ()));
    } else {
      LS->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.LS));
    }

    auto* RS    = (unx::ParameterInt *)factory.create_parameter <int> (L"RS");
    RS->register_to_ini (pad_cfg, L"Gamepad.Remap", L"XInput_RS");

    if (((unx::iParameter *)RS)->load ())
    {
      gamepad.remap.buttons.RS = (gamepad.remap.indexToEnum (RS->get_value ()));
    } else {
      RS->store (gamepad.remap.enumToIndex (gamepad.remap.buttons.RS));
    }
  }


  gamepad.speedboost.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"SpeedBoost");

  gamepad.kickstart.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"KickStart");

  gamepad.f1.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F1 Buttons");

  gamepad.f2.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F2 Buttons");

  gamepad.f3.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F3 Buttons");

  gamepad.f4.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F4 Buttons");

  gamepad.f5.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"F5 Buttons");

  gamepad.esc.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"Escape Buttons");

  gamepad.fullscreen.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"Fullscreen Buttons");

  gamepad.screenshot.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"Screenshot Buttons");

  gamepad.softreset.config_parameter =
    (unx::ParameterStringW *)
      factory.create_parameter <std::wstring> (L"Soft Reset Buttons");


  unx::ParameterStringW* combo_Speed       =
    (unx::ParameterStringW *)gamepad.speedboost.config_parameter;

  unx::ParameterStringW* combo_Kickstart   =
    (unx::ParameterStringW *)gamepad.kickstart.config_parameter;

  unx::ParameterStringW* combo_F1          =
    (unx::ParameterStringW *)gamepad.f1.config_parameter;

  unx::ParameterStringW* combo_F2          =
    (unx::ParameterStringW *)gamepad.f2.config_parameter;

  unx::ParameterStringW*  combo_F3         =
    (unx::ParameterStringW *)gamepad.f3.config_parameter;

  unx::ParameterStringW*  combo_F4         =
    (unx::ParameterStringW *)gamepad.f4.config_parameter;

  unx::ParameterStringW*  combo_F5         =
    (unx::ParameterStringW *)gamepad.f5.config_parameter;

  unx::ParameterStringW*  combo_ESC        =
    (unx::ParameterStringW *)gamepad.esc.config_parameter;

  unx::ParameterStringW*  combo_Fullscreen =
    (unx::ParameterStringW *)gamepad.fullscreen.config_parameter;

  unx::ParameterStringW*  combo_SS         =
    (unx::ParameterStringW *)gamepad.screenshot.config_parameter;

  unx::ParameterStringW*  combo_SoftReset  =
    (unx::ParameterStringW *)gamepad.softreset.config_parameter;


  combo_Speed->register_to_ini (pad_cfg, L"Gamepad.PC", L"SpeedBoost");

  if (! combo_Speed->load (gamepad.speedboost.unparsed))
  {
    combo_Speed->store (
      (gamepad.speedboost.unparsed = L"Select+L2+Cross")
    );
  }

  combo_Kickstart->register_to_ini (pad_cfg, L"Gamepad.PC", L"KickStart");

  if (! combo_Kickstart->load (gamepad.kickstart.unparsed))
  {
    combo_Kickstart->store (
      (gamepad.kickstart.unparsed = L"L1+L2+Up")
    );
  }


  combo_F1->register_to_ini (pad_cfg, L"Gamepad.PC", L"F1");

  if (! combo_F1->load (gamepad.f1.unparsed))
  {
    combo_F1->store (
      (gamepad.f1.unparsed = L"Select+Cross")
    );
  }


  combo_F2->register_to_ini (pad_cfg, L"Gamepad.PC", L"F2");

  if (! combo_F2->load (gamepad.f2.unparsed))
  {
    combo_F2->store (
      (gamepad.f1.unparsed = L"Select+Circle")
    );
  }


  combo_F3->register_to_ini (pad_cfg, L"Gamepad.PC", L"F3");

  if (! combo_F3->load (gamepad.f3.unparsed))
  {
    combo_F3->store (
      (gamepad.f3.unparsed = L"Select+Square")
    );
  }


  combo_F4->register_to_ini (pad_cfg, L"Gamepad.PC", L"F4");

  if (! combo_F4->load (gamepad.f4.unparsed))
  {
    combo_F4->store (
      (gamepad.f4.unparsed = L"Select+L1")
    );
  }


  combo_F5->register_to_ini (pad_cfg, L"Gamepad.PC", L"F5");

  if (! combo_F5->load (gamepad.f5.unparsed))
  {
    combo_F5->store (
      (gamepad.f5.unparsed = L"Select+R1")
    );
  }


  combo_ESC->register_to_ini (pad_cfg, L"Gamepad.PC", L"ESC");

  if (! combo_ESC->load (gamepad.esc.unparsed))
  {
    combo_ESC->store (
      (gamepad.esc.unparsed = L"L2+R2+Select")
    );
  }


  combo_Fullscreen->register_to_ini (pad_cfg, L"Gamepad.PC", L"Fullscreen");

  if (! combo_Fullscreen->load (gamepad.fullscreen.unparsed))
  {
    combo_Fullscreen->store (
      (gamepad.fullscreen.unparsed = L"L2+L3")
    );
  }


  combo_SS->register_to_ini (pad_cfg, L"Gamepad.Steam", L"Screenshot");

  if (! combo_SS->load (gamepad.screenshot.unparsed))
  {
    combo_SS->store (
      (gamepad.screenshot.unparsed = L"Select+R3")
    );
  }

  combo_SoftReset->register_to_ini (pad_cfg, L"Gamepad.PC", L"SoftReset");

  if (! combo_SoftReset->load (gamepad.softreset.unparsed))
  {
    combo_SoftReset->store (
      (gamepad.softreset.unparsed = L"L1+L2+R1+R2+Select+Start")
    );
  }


  pad_cfg->write ((std::wstring (SK_GetConfigPath ()) + L"UnX_Gamepad.ini").c_str ());
//  delete pad_cfg;

  if (config.input.remap_dinput8)
  {
    SK_Input_GetDI8Keyboard =
      (SK_Input_GetDI8Keyboard_pfn)
        GetProcAddress ( GetModuleHandle (config.system.injector.c_str ()),
                           "SK_Input_GetDI8Keyboard" );
    SK_Input_GetDI8Mouse =
      (SK_Input_GetDI8Mouse_pfn)
        GetProcAddress ( GetModuleHandle (config.system.injector.c_str ()),
                           "SK_Input_GetDI8Mouse" );

    UNX_CreateDLLHook2 ( config.system.injector.c_str (),
                         "DI8_GetDeviceState_Override",
                         IDirectInputDevice8_GetDeviceState_Detour,
              (LPVOID *)&IDirectInputDevice8_GetDeviceState_Original );
  }


  // Post-Process our Remap Table
  for (int i = 0; i < 12; i++)
  {
    gamepad.remap.map [i] = 
      gamepad.remap.enumToIndex (
        ((int *)&gamepad.remap.buttons) [ i ]
      ) - 1;
  }

  SK_XInput_PollController =
    (SK_XInput_PollController_pfn)
      GetProcAddress ( GetModuleHandle (config.system.injector.c_str ()),
                         "SK_XInput_PollController" );

  UNX_CreateDLLHook2 ( config.system.injector.c_str (),
                       "SK_PluginKeyPress",
                         SK_UNX_PluginKeyPress,
              (LPVOID *)&SK_PluginKeyPress_Original );

  UNX_ApplyQueuedHooks ();

  unx::InputManager::Hooker* pHook =
    unx::InputManager::Hooker::getInstance ();

  UNX_InstallWindowHook (SK_GetGameWindow ());

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
}

void
unx::InputManager::Hooker::End (void)
{
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
};


button_map_state_s
UNX_ParseButtonCombo (std::wstring combo, int* out)
{
  *out = 0x0;

  button_map_state_s state         = { };
                     state.buttons =  0;

  std::wstring map = combo;

  if (! map.length ())
    return state;

  wchar_t* wszMap  = _wcsdup (map.c_str ());
  wchar_t* wszTok  =  wcstok (wszMap, L"+");

  while (wszTok != nullptr)
  {
    int button = 0x00;

    if ((! _wcsicmp (wszTok, L"LB"))        || (! _wcsicmp (wszTok, L"L1")))
      button = XINPUT_GAMEPAD_LEFT_SHOULDER;
    else if ((! _wcsicmp (wszTok, L"RB"))   || (! _wcsicmp (wszTok, L"R1")))
      button = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    else if ((! _wcsicmp (wszTok, L"LT"))   || (! _wcsicmp (wszTok, L"L2")))
      button = XINPUT_GAMEPAD_LEFT_TRIGGER;
    else if ((! _wcsicmp (wszTok, L"RT"))   || (! _wcsicmp (wszTok, L"R2")))
      button = XINPUT_GAMEPAD_RIGHT_TRIGGER;
    else if ((! _wcsicmp (wszTok, L"LS"))   || (! _wcsicmp (wszTok, L"L3")))
      button = XINPUT_GAMEPAD_LEFT_THUMB; 
    else if ((! _wcsicmp (wszTok, L"RS"))   || (! _wcsicmp (wszTok, L"R3")))
      button = XINPUT_GAMEPAD_RIGHT_THUMB;

    else if ((! _wcsicmp (wszTok, L"Start")))
      button = XINPUT_GAMEPAD_START;
    else if ((! _wcsicmp (wszTok, L"Back")) || (! _wcsicmp (wszTok, L"Select")))
      button = XINPUT_GAMEPAD_BACK;
    else if ((! _wcsicmp (wszTok, L"A"))    || (! _wcsicmp (wszTok, L"Cross")))
      button = XINPUT_GAMEPAD_A;
    else if ((! _wcsicmp (wszTok, L"B"))    || (! _wcsicmp (wszTok, L"Circle")))
      button = XINPUT_GAMEPAD_B;
    else if ((! _wcsicmp (wszTok, L"X"))    || (! _wcsicmp (wszTok, L"Square")))
      button = XINPUT_GAMEPAD_X;
    else if ((! _wcsicmp (wszTok, L"Y"))    || (! _wcsicmp (wszTok, L"Triangle")))
      button = XINPUT_GAMEPAD_Y;

    else if ((! _wcsicmp (wszTok, L"Up")))
      button = XINPUT_GAMEPAD_DPAD_UP;
    else if ((! _wcsicmp (wszTok, L"Down")))
      button = XINPUT_GAMEPAD_DPAD_DOWN;
    else if ((! _wcsicmp (wszTok, L"Left")))
      button = XINPUT_GAMEPAD_DPAD_LEFT;
    else if ((! _wcsicmp (wszTok, L"Right")))
      button = XINPUT_GAMEPAD_DPAD_RIGHT;

    if (button != 0x00)
    {
      *out          |= button;
      state.buttons |= button;
      //out [state.buttons++] = button;
    }

    //dll_log->Log (L"Button%lu: %s", state.buttons-1, wszTok);
    wszTok = wcstok (nullptr, L"+");
  }

  free (wszMap);

  return state;
}

void
UNX_SerializeButtonCombo (UNX_GamepadCombo* combo)
{
  combo->unparsed = L"";

  std::queue <const wchar_t*> buttons;

  //if (combo->lt)
  //  buttons.push (combo->button_names [16]);
  //
  //if (combo->rt)
  //  buttons.push (combo->button_names [17]);

  for (int i = 0; i < 18; i++)
  {
    if (combo->buttons & ( 1 << i ))
    {
      buttons.push (combo->button_names [i]);
    }
  }

  combo->unparsed = L"";

  while (! buttons.empty ())
  {
    combo->unparsed += buttons.front ();
                       buttons.pop   ();

    if (! buttons.empty ())
      combo->unparsed += L"+";
  }
}

#include <Shlwapi.h>
#include <mmsystem.h>

using namespace unx::InputManager;

void
UNX_SetupSpecialButtons (void)
{
  bool ps_map = StrStrIW (gamepad.tex_set.c_str (), L"PlayStation") ||
                StrStrIW (gamepad.tex_set.c_str (), L"PS");

  const wchar_t** name_map = ps_map ? gamepad.names.PlayStation :
                                      gamepad.names.Xbox;

  button_map_state_s state =
    UNX_ParseButtonCombo ( gamepad.f1.unparsed,
                             &gamepad.f1.buttons );

  gamepad.f1.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.f1);

  state =
    UNX_ParseButtonCombo ( gamepad.f2.unparsed,
                             &gamepad.f2.buttons );

  gamepad.f2.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.f2);

  state =
    UNX_ParseButtonCombo ( gamepad.f3.unparsed,
                             &gamepad.f3.buttons );

  gamepad.f3.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.f3);

  state =
    UNX_ParseButtonCombo ( gamepad.f4.unparsed,
                             &gamepad.f4.buttons );

  gamepad.f4.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.f4);

  state =
    UNX_ParseButtonCombo ( gamepad.f5.unparsed,
                             &gamepad.f5.buttons );

  gamepad.f5.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.f5);

  state =
    UNX_ParseButtonCombo ( gamepad.screenshot.unparsed,
                             &gamepad.screenshot.buttons );

  gamepad.screenshot.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.screenshot);

  state =
    UNX_ParseButtonCombo ( gamepad.esc.unparsed,
                             &gamepad.esc.buttons );

  gamepad.esc.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.esc);

  state =
    UNX_ParseButtonCombo ( gamepad.fullscreen.unparsed,
                             &gamepad.fullscreen.buttons );

  gamepad.fullscreen.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.fullscreen);

  state =
    UNX_ParseButtonCombo ( gamepad.speedboost.unparsed,
                             &gamepad.speedboost.buttons );

  gamepad.speedboost.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.speedboost);

  state =
    UNX_ParseButtonCombo ( gamepad.kickstart.unparsed,
                             &gamepad.kickstart.buttons );

  gamepad.kickstart.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.kickstart);

  state =
    UNX_ParseButtonCombo ( gamepad.softreset.unparsed,
                             &gamepad.softreset.buttons );

  gamepad.softreset.button_names = name_map;

  UNX_SerializeButtonCombo (&gamepad.softreset);
}

#include "ini.h"
#include "parameter.h"

#include <deque>

struct combo_button_s {
  combo_button_s (
      UNX_GamepadCombo* combo_
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
       ((! (combo->buttons & XINPUT_GAMEPAD_LEFT_TRIGGER))  || xi_gamepad.bLeftTrigger  > 130) &&
       ((! (combo->buttons & XINPUT_GAMEPAD_RIGHT_TRIGGER)) || xi_gamepad.bRightTrigger > 130) &&
       xi_gamepad.wButtons == (WORD)combo->buttons);

    if (wasJustPressed ())
      time_activated = timeGetTime ();
    else if (wasJustReleased ())
      time_released  = timeGetTime ();

    return state;
  }

  bool wasJustPressed (void) {
    return state && (! last_state);
  }

  bool wasJustReleased (void) {
    return last_state && (! state);
  }


  UNX_GamepadCombo*
       combo;

  bool state;
  bool last_state;

  DWORD time_activated;
  DWORD time_released;
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

__declspec (dllimport)
bool SK_ImGui_Visible;


std::deque <WORD> pressed_scancodes;

void
UNX_PollInput (void)
{
  if ( ((! SK_XInput_PollController) && (! gamepad.legacy)) )
  {
    return;
  }

  static bool init  = false;

  if (! init)
  {
    UNX_SetupSpecialButtons ();
  }

  static combo_button_s f1         ( &gamepad.f1 );
  static combo_button_s f2         ( &gamepad.f2 );
  static combo_button_s f3         ( &gamepad.f3 );
  static combo_button_s f4         ( &gamepad.f4 );
  static combo_button_s f5         ( &gamepad.f5 );
  static combo_button_s esc        ( &gamepad.esc );
  static combo_button_s full       ( &gamepad.fullscreen );
  static combo_button_s sshot      ( &gamepad.screenshot );
  static combo_button_s speedboost ( &gamepad.speedboost );
  static combo_button_s kickstart  ( &gamepad.kickstart  );
  static combo_button_s softreset  ( &gamepad.softreset  );

#define XI_POLL_INTERVAL 500UL

  static DWORD  xi_ret       = ERROR_DEVICE_NOT_CONNECTED;
  static DWORD  last_xi_poll = timeGetTime ();// - (XI_POLL_INTERVAL + 1UL);

  static INPUT keys [5];

  if (! init)
  {
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

    init = true;
  }


  int slot = config.input.gamepad_slot;
  if (slot == -1)
      slot = 0;

//  // Do Not Handle Input While We Do Not Have Focus
//  if (GetForegroundWindow () != SK_GetGameWindow ())
//  {
//    return;
//  }


  XINPUT_STATE xi_state = {   };
  bool         success  = false;

  if (SK_XInput_PollController != nullptr && (! gamepad.legacy))
  {
    // This function is actually a performance hazard when no controllers
    //   are plugged in, so ... throttle the sucker.
    if (  xi_ret == 0 ||
         (last_xi_poll < timeGetTime () - XI_POLL_INTERVAL) )
    {
           success = SK_XInput_PollController (slot, &xi_state);
      last_xi_poll = timeGetTime    ();

      if (! success)
      {
        xi_state.Gamepad = {                        };
        xi_ret           = ERROR_DEVICE_NOT_CONNECTED;
      } else
        xi_ret   =   0;
    }
  }

  else
  {
    if (  xi_ret == 0 ||
         (last_xi_poll < timeGetTime () - XI_POLL_INTERVAL) )
    {
      if (! joyGetNumDevs ())
      {
        xi_ret = ERROR_DEVICE_NOT_CONNECTED;
      }

      else
      {
        static JOYCAPSW caps = { };

        if (! caps.wMaxButtons)
          joyGetDevCaps (JOYSTICKID1, &caps, sizeof JOYCAPSW);

        xi_ret = 0;

        JOYINFOEX joy_ex { };
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

  if (xi_state.Gamepad.bLeftTrigger > 30)
    xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_TRIGGER;
  else
    xi_state.Gamepad.wButtons &= ~XINPUT_GAMEPAD_RIGHT_TRIGGER;

  if (xi_state.Gamepad.bRightTrigger > 30)
    xi_state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_TRIGGER;
  else
    xi_state.Gamepad.wButtons &= ~XINPUT_GAMEPAD_RIGHT_TRIGGER;

 //
 // Analog deadzone compensation
 //
 if (xi_state.Gamepad.bLeftTrigger < 130)
   xi_state.Gamepad.bLeftTrigger = 0;

 if (xi_state.Gamepad.bRightTrigger < 130)
   xi_state.Gamepad.bRightTrigger = 0;

  //SetFocus (SK_GetGameWindow ());

 softreset.poll (xi_ret, xi_state.Gamepad);

 bool four_finger = softreset.wasJustPressed ();
  //bool four_finger = (
  //    xi_ret == 0                          &&
  //    config.input.four_finger_salute      &&
  //    xi_state.Gamepad.bLeftTrigger        &&
  //    xi_state.Gamepad.bRightTrigger       &&
  //    xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_LEFT_SHOULDER)  &&
  //    xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_RIGHT_SHOULDER) &&
  //    xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_START)          &&
  //    xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_BACK)
  //);

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

#define UNX_SendScancodeMake(vk,x)   { keybd_event ((vk), (x), KEYEVENTF_SCANCODE,                   0); }
#define UNX_SendScancodeBreak(vk, y) { keybd_event ((vk), (y), KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0); }

#define UNX_SendScancode(vk,x,y) {                         UNX_SendScancodeMake  ((vk), (x));                          \
          CreateThread (nullptr, 0, [](LPVOID) -> DWORD {             SleepEx (66, TRUE);                              \
                                                           UNX_SendScancodeBreak ((vk), (x));                          \
                                                           CloseHandle (GetCurrentThread ());                          \
                                                                                  return 0; }, nullptr, 0x0, nullptr); \
};

  if (four_finger)
  {
    extern bool UNX_KillMeNow (void);

    // If in battle, trigger a Game Over screen, otherwise restart the
    //   entire game.
    if (! UNX_KillMeNow ())
    {
      SendMessage (SK_GetGameWindow (), WM_CLOSE, 0, 0);
    }

    return;
  }

  // Filter out what appears to be the beginning of a "four finger salute"
  //   so that menus and various other things are not activated.
  if ( xi_state.Gamepad.bRightTrigger && xi_state.Gamepad.bLeftTrigger &&
       xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_LEFT_SHOULDER)      &&
       xi_state.Gamepad.wButtons & (XINPUT_GAMEPAD_RIGHT_SHOULDER) )
  {
    return;
  }

  bool        full_sleep = true;
  static bool long_press = false;

  if (esc.wasJustPressed ())
  {
    UNX_SendScancode (VK_ESCAPE, 0x01, 0x81);
  }

  else if (esc.wasJustReleased ())
  {
    long_press         = false;
    esc.time_activated = MAXDWORD;
  }


  if (speedboost.state)
  {
    if (speedboost.wasJustPressed ())
    {
      extern void UNX_SpeedStep (); 
      UNX_SpeedStep ();
    }
  }

  else if (f1.wasJustPressed ()) { UNX_SendScancode (VK_F1, 0x3b, 0xbb); }
  else if (f2.wasJustPressed ()) { UNX_SendScancode (VK_F2, 0x3c, 0xbc); }
  else if (f3.wasJustPressed ()) { UNX_SendScancode (VK_F3, 0x3d, 0xbd); }
  else if (f4.wasJustPressed ()) { UNX_SendScancode (VK_F4, 0x3e, 0xbe); }
  else if (f5.wasJustPressed ()) { UNX_SendScancode (VK_F5, 0x3f, 0xbf); }

  else if (full.wasJustPressed ())
  {
    UNX_SendScancode (VK_MENU,   0x38, 0xb8);
    UNX_SendScancode (VK_RETURN, 0x1c, 0x9c);

    full_sleep = false;
  }

  else if (kickstart.wasJustPressed ())
  {
    UNX_KickStart ();
  }

  else if (sshot.wasJustPressed ())
  {
    using SK_SteamAPI_TakeScreenshot_pfn = bool (__stdcall *)(void);

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
}

unsigned int
__stdcall
unx::InputManager::Hooker::MessagePump (LPVOID)
{
  return 0;
}

void
UNX_ReleaseESCKey (void)
{
return;

  ///INPUT keys [2];
  ///
  ///keys [0].type           = INPUT_KEYBOARD;
  ///keys [0].ki.wVk         = 0;
  ///keys [0].ki.wScan       = 0x01;
  ///keys [0].ki.dwFlags     = KEYEVENTF_SCANCODE;
  ///keys [0].ki.time        = 0;
  ///keys [0].ki.dwExtraInfo = 0;
  ///
  ///SendInput (1, &keys [0], sizeof INPUT);
  ///Sleep     (66);
  ///
  ///keys [1].type           = INPUT_KEYBOARD;
  ///keys [1].ki.wVk         = 0;
  ///keys [1].ki.wScan       = 0x01;
  ///keys [1].ki.dwFlags     = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
  ///keys [1].ki.time        = 0;
  ///keys [1].ki.dwExtraInfo = 0;
  ///
  ///SendInput (1, &keys [1], sizeof INPUT);
  ///Sleep     (66);
}

unx::InputManager::Hooker*
  unx::InputManager::Hooker::pInputHook = nullptr;