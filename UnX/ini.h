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
#ifndef __UNX__INI_H__
#define __UNX__INI_H__

#include <string>
#include <map>
#include <vector>

#include <Unknwnbase.h>

// {B526D074-2F4D-4BAE-B6EC-11CB3779B199}
static const GUID IID_SK_INISection = 
{ 0xb526d074, 0x2f4d, 0x4bae, { 0xb6, 0xec, 0x11, 0xcb, 0x37, 0x79, 0xb1, 0x99 } };

interface iSK_INISection : public IUnknown
{
public:
  iSK_INISection (void) {
    AddRef ();
  }

  iSK_INISection (std::wstring section_name) {
    Release ();
  }

  /*** IUnknown methods ***/
  STDMETHOD  (       QueryInterface)(THIS_ REFIID riid, void** ppvObj) = 0;
  STDMETHOD_ (ULONG, AddRef)        (THIS)                             = 0;
  STDMETHOD_ (ULONG, Release)       (THIS)                             = 0;

  STDMETHOD_ (std::wstring&, get_value)    (std::wstring key)  = 0;
  STDMETHOD_ (void,          set_name)     (std::wstring name) = 0;
  STDMETHOD_ (bool,          contains_key) (std::wstring key)  = 0;
  STDMETHOD_ (void,          add_key_value)(std::wstring key, std::wstring value) = 0;
};

// {DD2B1E00-6C14-4659-8B45-FCEF1BC2C724}
static const GUID IID_SK_INI = 
{ 0xdd2b1e00, 0x6c14, 0x4659, { 0x8b, 0x45, 0xfc, 0xef, 0x1b, 0xc2, 0xc7, 0x24 } };

interface iSK_INI : public IUnknown
{
  typedef const std::map <std::wstring, iSK_INISection> _TSectionMap;

           iSK_INI (const wchar_t* filename) {
    AddRef ();
  };

  virtual ~iSK_INI (void) {
    Release ();
  }

  /*** IUnknown methods ***/
  STDMETHOD  (       QueryInterface)(THIS_ REFIID riid, void** ppvObj) = 0;
  STDMETHOD_ (ULONG, AddRef)        (THIS)                             = 0;
  STDMETHOD_ (ULONG, Release)       (THIS)                             = 0;

  STDMETHOD_ (void, parse)  (THIS)                           = 0;
  STDMETHOD_ (void, import) (THIS_ std::wstring import_data) = 0;
  STDMETHOD_ (void, write)  (THIS_ std::wstring fname)       = 0;

  STDMETHOD_ (_TSectionMap&,   get_sections)    (THIS)                 = 0;
  STDMETHOD_ (iSK_INISection&, get_section)     (std::wstring section) = 0;
  STDMETHOD_ (bool,            contains_section)(std::wstring section) = 0;
};

iSK_INI*
UNX_CreateINI (const wchar_t* const wszName);

#endif
