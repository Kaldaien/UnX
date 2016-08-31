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
#ifndef __UNX__LOG_H__
#define __UNX__LOG_H__

#include <Unknwnbase.h>

// {A4BF1773-CAAB-48F3-AD88-C2AB5C23BD6F}
static const GUID IID_SK_Logger = 
{ 0xa4bf1773, 0xcaab, 0x48f3, { 0xad, 0x88, 0xc2, 0xab, 0x5c, 0x23, 0xbd, 0x6f } };

interface iSK_Logger : public IUnknown
{
  class AutoClose
  {
  friend interface iSK_Logger;
  public:
    ~AutoClose (void)
    {
      if (log_ != nullptr)
        log_->close ();

      log_ = nullptr;
    }

  protected:
    AutoClose (iSK_Logger* log) : log_ (log) { }

  private:
    iSK_Logger* log_;
  };

  AutoClose auto_close (void) {
    return AutoClose (this);
  }

  iSK_Logger (void) {
    AddRef ();
  }

  virtual ~iSK_Logger (void) {
    Release ();
  }

  /*** IUnknown methods ***/
  STDMETHOD  (       QueryInterface)(THIS_ REFIID riid, void** ppvObj) = 0;
  STDMETHOD_ (ULONG, AddRef)        (THIS)                             = 0;
  STDMETHOD_ (ULONG, Release)       (THIS)                             = 0;

  STDMETHOD_ (bool, init)(THIS_ const wchar_t* const wszFilename,
                                const wchar_t* const wszMode ) = 0;
  STDMETHOD_ (void, close)(THIS)                               = 0;

  STDMETHOD_ (void, LogEx)(THIS_ bool                 _Timestamp,
                              _In_z_ _Printf_format_string_
                                 wchar_t const* const _Format,
                                                      ... ) = 0;
  STDMETHOD_ (void, Log)  (THIS_ _In_z_ _Printf_format_string_
                                 wchar_t const* const _Format,
                                                      ... ) = 0;
  STDMETHOD_ (void, Log)  (THIS_ _In_z_ _Printf_format_string_
                                 char const* const    _Format,
                                                      ... ) = 0;
};

extern iSK_Logger* dll_log;

iSK_Logger*
UNX_CreateLog (const wchar_t* const wszName);

#endif /* __UNX__LOG_H__ */
