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
#define _CRT_NON_CONFORMING_SWPRINTFS

#include "parameter.h"

std::wstring
unx::ParameterInt::get_value_str (void)
{
  wchar_t str [32];
  _itow (value, str, 10);

  return std::wstring (str);
}

int
unx::ParameterInt::get_value (void)
{
  return value;
}

void
unx::ParameterInt::set_value (int val)
{
  value = val;
}


void
unx::ParameterInt::set_value_str (std::wstring str)
{
  value = _wtoi (str.c_str ());
}


void
unx::ParameterInt::store_str (std::wstring str)
{
  set_value_str  (str);
  iParameter::store ();
}

void
unx::ParameterInt::store (int val)
{
  set_value      (val);
  iParameter::store ();
}

bool
unx::ParameterInt::load (int& ref)
{
  bool bRet = 
    iParameter::load ();

  if (bRet)
    ref = get_value ();

  return bRet;
}



std::wstring
unx::ParameterInt64::get_value_str (void)
{
  wchar_t str [32];
  _i64tow (value, str, 10);

  return std::wstring (str);
}

int64_t
unx::ParameterInt64::get_value (void)
{
  return value;
}

void
unx::ParameterInt64::set_value (int64_t val)
{
  value = val;
}


void
unx::ParameterInt64::set_value_str (std::wstring str)
{
  value = _wtol (str.c_str ());
}


void
unx::ParameterInt64::store_str (std::wstring str)
{
  set_value_str  (str);
  iParameter::store ();
}

void
unx::ParameterInt64::store (int64_t val)
{
  set_value      (val);
  iParameter::store ();
}

bool
unx::ParameterInt64::load (int64_t& ref)
{
  bool bRet = 
    iParameter::load ();

  if (bRet)
    ref = get_value ();

  return bRet;
}



std::wstring
unx::ParameterBool::get_value_str (void)
{
  if (value == true)
    return L"true";

  return L"false";
}

bool
unx::ParameterBool::get_value (void)
{
  return value;
}

void
unx::ParameterBool::set_value (bool val)
{
  value = val;
}


void
unx::ParameterBool::set_value_str (std::wstring str)
{
  if (str.length () == 1 &&
      str [0] == L'1')
    value = true;

  else if (str.length () == 4 &&
      towlower (str [0]) == L't' &&
      towlower (str [1]) == L'r' &&
      towlower (str [2]) == L'u' &&
      towlower (str [3]) == L'e')
    value = true;

  else
    value = false;
}


void
unx::ParameterBool::store_str (std::wstring str)
{
  set_value_str  (str);
  iParameter::store ();
}

void
unx::ParameterBool::store (bool val)
{
  set_value      (val);
  iParameter::store ();
}

bool
unx::ParameterBool::load (bool& ref)
{
  bool bRet = 
    iParameter::load ();

  if (bRet)
    ref = get_value ();

  return bRet;
}



std::wstring
unx::ParameterFloat::get_value_str (void)
{
  wchar_t val_str [16];
  swprintf (val_str, L"%f", value);

  // Remove trailing 0's after the .
  size_t len = wcslen (val_str);
  for (size_t i = (len - 1); i > 1; i--) {
    if (val_str [i] == L'0' && val_str [i - 1] != L'.')
      len--;
    if (val_str [i] != L'0' && val_str [i] != L'\0')
      break;
  }

  val_str [len] = L'\0';

  return std::wstring (val_str);
}

float
unx::ParameterFloat::get_value (void)
{
  return value;
}

void
unx::ParameterFloat::set_value (float val)
{
  value = val;
}


void
unx::ParameterFloat::set_value_str (std::wstring str)
{
  value = (float)wcstod (str.c_str (), NULL);
}


void
unx::ParameterFloat::store_str (std::wstring str)
{
  set_value_str  (str);
  iParameter::store ();
}

void
unx::ParameterFloat::store (float val)
{
  set_value      (val);
  iParameter::store ();
}

bool
unx::ParameterFloat::load (float& ref)
{
  bool bRet = 
    iParameter::load ();

  if (bRet)
    ref = get_value ();

  return bRet;
}



std::wstring
unx::ParameterStringW::get_value_str (void)
{
  return value;
}

std::wstring
unx::ParameterStringW::get_value (void)
{
  return value;
}

void
unx::ParameterStringW::set_value (std::wstring val)
{
  value = val;
}


void
unx::ParameterStringW::set_value_str (std::wstring str)
{
  value = str;
}


void
unx::ParameterStringW::store_str (std::wstring str)
{
  set_value_str  (str);
  iParameter::store ();
}

void
unx::ParameterStringW::store (std::wstring val)
{
  set_value      (val);
  iParameter::store ();
}

bool
unx::ParameterStringW::load (std::wstring& ref)
{
  bool bRet = 
    iParameter::load ();

  if (bRet)
    ref = get_value ();

  return bRet;
}


template <>
unx::iParameter*
unx::ParameterFactory::create_parameter <int> (const wchar_t* name)
{
  iParameter* param = new ParameterInt ();
  params.push_back (param);

  return param;
}

template <>
unx::iParameter*
unx::ParameterFactory::create_parameter <int64_t> (const wchar_t* name)
{
  iParameter* param = new ParameterInt64 ();
  params.push_back (param);

  return param;
}

template <>
unx::iParameter*
unx::ParameterFactory::create_parameter <bool> (const wchar_t* name)
{
  iParameter* param = new ParameterBool ();
  params.push_back (param);

  return param;
}

template <>
unx::iParameter*
unx::ParameterFactory::create_parameter <float> (const wchar_t* name)
{
  iParameter* param = new ParameterFloat ();
  params.push_back (param);

  return param;
}

template <>
unx::iParameter*
unx::ParameterFactory::create_parameter <std::wstring> (const wchar_t* name)
{
  iParameter* param = new ParameterStringW ();
  params.push_back (param);

  return param;
}