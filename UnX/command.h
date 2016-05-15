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

# include <unordered_map>

#include <locale> // tolower (...)

template <typename T>
class eTB_VarStub;// : public eTB_Variable

class eTB_Variable;
class eTB_Command;

class eTB_CommandResult
{
public:
  eTB_CommandResult (       std::string   word,
                            std::string   arguments = "",
                            std::string   result    = "",
                            int           status = false,
                      const eTB_Variable* var    = NULL,
                      const eTB_Command*  cmd    = NULL ) : word_   (word),
                                                            args_   (arguments),
                                                            result_ (result) {
    var_    = var;
    cmd_    = cmd;
    status_ = status;
  }

  std::string         getWord     (void) const { return word_;   }
  std::string         getArgs     (void) const { return args_;   }
  std::string         getResult   (void) const { return result_; }

  const eTB_Variable* getVariable (void) const { return var_;    }
  const eTB_Command*  getCommand  (void) const { return cmd_;    }

  int                 getStatus   (void) const { return status_; }

protected:

private:
  const eTB_Variable* var_;
  const eTB_Command*  cmd_;
        std::string   word_;
        std::string   args_;
        std::string   result_;
        int           status_;
};

class eTB_Command {
public:
  virtual eTB_CommandResult execute (const char* szArgs) = 0;

  virtual const char* getHelp            (void) { return "No Help Available"; }

  virtual int         getNumArgs         (void) { return 0; }
  virtual int         getNumOptionalArgs (void) { return 0; }
  virtual int         getNumRequiredArgs (void) {
    return getNumArgs () - getNumOptionalArgs ();
  }

protected:
private:
};

class eTB_Variable
{
friend class eTB_iVariableListener;
public:
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

  virtual VariableType  getType        (void) const = 0;
  virtual std::string   getValueString (void) const = 0;

protected:
  VariableType type_;
};

class eTB_iVariableListener
{
public:
  virtual bool OnVarChange (eTB_Variable* var, void* val = NULL) = 0;
protected:
};

template <typename T>
class eTB_VarStub : public eTB_Variable
{
friend class eTB_iVariableListener;
public:
  eTB_VarStub (void) : type_     (Unknown),
                       var_      (NULL),
                       listener_ (NULL)     { };

  eTB_VarStub ( T*                     var,
                eTB_iVariableListener* pListener = NULL );

  eTB_Variable::VariableType getType (void) const
  {
    return type_;
  }

  virtual std::string getValueString (void) const { return "(null)"; }

  const T& getValue (void) const { return *var_; }
  void     setValue (T& val)     {
    if (listener_ != NULL)
      listener_->OnVarChange (this, &val);
    else
      *var_ = val;
  }

  /// NOTE: Avoid doing this, as much as possible...
  T* getValuePtr (void) { return var_; }

  typedef  T _Tp;

protected:
  typename eTB_VarStub::_Tp* var_;

private:
  eTB_iVariableListener*     listener_;
};

#define eTB_CaseAdjust(ch,lower) ((lower) ? __ascii_tolower ((ch)) : (ch))

// Hash function for UTF8 strings
template < class _Kty, class _Pr = std::less <_Kty> >
class str_hash_compare
{
public:
  typedef typename _Kty::value_type value_type;
  typedef typename _Kty::size_type  size_type;  /* Was originally size_t ... */

  enum
  {
    bucket_size = 4,
    min_buckets = 8
  };

  str_hash_compare (void)      : comp ()      { };
  str_hash_compare (_Pr _Pred) : comp (_Pred) { };

  size_type operator() (const _Kty& _Keyval) const;
  bool      operator() (const _Kty& _Keyval1, const _Kty& _Keyval2) const;

  size_type hash_string (const _Kty& _Keyval) const;

private:
  _Pr comp;
};

typedef std::pair <std::string, eTB_Command*>  eTB_CommandRecord;
typedef std::pair <std::string, eTB_Variable*> eTB_VariableRecord;


class eTB_CommandProcessor
{
public:
  eTB_CommandProcessor (void);

  virtual ~eTB_CommandProcessor (void)
  {
  }

        eTB_Command* FindCommand   (const char* szCommand) const;

  const eTB_Command* AddCommand    ( const char*  szCommand,
                                     eTB_Command* pCommand );
  bool               RemoveCommand ( const char* szCommand );


  const eTB_Variable* FindVariable  (const char* szVariable) const;

  const eTB_Variable* AddVariable    ( const char*   szVariable,
                                       eTB_Variable* pVariable  );
  bool                RemoveVariable ( const char*   szVariable );


  eTB_CommandResult ProcessCommandLine      (const char* szCommandLine);
  eTB_CommandResult ProcessCommandFormatted (const char* szCommandFormat, ...);


protected:
private:
  std::unordered_map < std::string, eTB_Command*,
                       str_hash_compare <std::string> > commands_;
  std::unordered_map < std::string, eTB_Variable*,
                       str_hash_compare <std::string> > variables_;
};


typedef eTB_CommandProcessor* (__stdcall *SK_GetCommandProcessor_pfn)(void);
extern SK_GetCommandProcessor_pfn SK_GetCommandProcessor;

//extern eTB_CommandProcessor command;

#endif /* __EPSILON_TESTBED__COMMAND_H */
