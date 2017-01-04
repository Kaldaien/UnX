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
#ifndef __EPSILON_TESTBED__COMMAND_H__
#define __EPSILON_TESTBED__COMMAND_H__

#include <Unknwnbase.h>

# include <unordered_map>

#include <locale> // tolower (...)

interface SK_IVariable;
interface SK_ICommand;

interface SK_ICommandResult
{
  SK_ICommandResult (       const char*   word,
                            const char*   arguments = "",
                            const char*   result    = "",
                            int           status = false,
                      const SK_IVariable*  var    = NULL,
                      const SK_ICommand*   cmd    = NULL ) : word_   (word),
                                                             args_   (arguments),
                                                             result_ (result) {
    var_    = var;
    cmd_    = cmd;
    status_ = status;
  }

  const char*          getWord     (void) const { return word_.c_str   (); }
  const char*          getArgs     (void) const { return args_.c_str   (); }
  const char*          getResult   (void) const { return result_.c_str (); }

  const SK_IVariable*  getVariable (void) const { return var_;    }
  const SK_ICommand*   getCommand  (void) const { return cmd_;    }

  int                  getStatus   (void) const { return status_; }

protected:

private:
  const SK_IVariable* var_;
  const SK_ICommand*  cmd_;
        std::string   word_;
        std::string   args_;
        std::string   result_;
        int           status_;
};

interface SK_ICommand
{
  virtual SK_ICommandResult execute (const char* szArgs) = 0;

  virtual const char* getHelp            (void) { return "No Help Available"; }

  virtual int         getNumArgs         (void) { return 0; }
  virtual int         getNumOptionalArgs (void) { return 0; }
  virtual int         getNumRequiredArgs (void) {
    return getNumArgs () - getNumOptionalArgs ();
  }
};

interface SK_IVariable
{
  friend interface SK_IVariableListener;

  enum VariableType {
    Float,
    Double,
    Boolean,
    Byte,
    Short,
    Int,
    LongInt,
    String,

    NUM_VAR_TYPES_,

    Unknown
  } VariableTypes;

  virtual VariableType  getType         (void)                        const = 0;
  virtual void          getValueString  ( _Out_opt_     char* szOut,
                                          _Inout_   uint32_t* dwLen ) const = 0;
  virtual void*         getValuePointer (void)                        const = 0;

protected:
  VariableType type_;
};

interface SK_IVariableListener
{
  virtual bool OnVarChange (SK_IVariable* var, void* val = NULL) = 0;
};

interface SK_ICommandProcessor
{
  SK_ICommandProcessor (void);

  virtual ~SK_ICommandProcessor (void)
  {
  }

  virtual SK_ICommand*       FindCommand   (const char* szCommand) const = 0;

  virtual const SK_ICommand* AddCommand    ( const char*  szCommand,
                                             SK_ICommand* pCommand ) = 0;
  virtual bool               RemoveCommand ( const char* szCommand ) = 0;


  virtual const SK_IVariable* FindVariable  (const char* szVariable) const = 0;

  virtual const SK_IVariable* AddVariable    ( const char*   szVariable,
                                               SK_IVariable* pVariable  ) = 0;
  virtual bool                RemoveVariable ( const char*   szVariable ) = 0;


  virtual SK_ICommandResult ProcessCommandLine      (const char* szCommandLine)        = 0;
  virtual SK_ICommandResult ProcessCommandFormatted (const char* szCommandFormat, ...) = 0;
};

typedef SK_ICommandProcessor* (__stdcall *SK_GetCommandProcessor_pfn)(void);
extern SK_GetCommandProcessor_pfn SK_GetCommandProcessor;

SK_IVariable*
UNX_CreateVar ( SK_IVariable::VariableType  type,
                void*                       var,
                SK_IVariableListener       *pListener = nullptr );

#endif /* __EPSILON_TESTBED__COMMAND_H */
