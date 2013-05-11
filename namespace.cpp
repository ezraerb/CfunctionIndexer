/* This file is part of ProgramIndexer. It indexes function declarations,
    prototypes, and calls in C programs; and lists issues with functions
    including name collisions and shadow situations.

    Copyright (C) 2013   Ezra Erb

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 as published
    by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    I'd appreciate a note if you find this program useful or make
    updates. Please contact me through LinkedIn or github (my profile also has
    a link to the code depository)
*/
// namespace.cpp Code to mantain the keyword and defind symbols lists.

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include "basetypes.h"
#include "errors.h"
#include "namespace.h"

using std::vector;
using std::string;
using std::ostream;
using std::cout;
using std::endl;
using std::set;

// Prints out the token set. Main use is debugging
ostream& operator<<(ostream& stream, const TokenSet& testval)
{
  TokenSet::const_iterator index;
  for (index = testval.begin(); index != testval.end(); index++)
    stream << *index << endl;
  return stream;
}

ostream& operator<<(ostream& stream, const NameSpace& value)
{
  TokenSet::iterator index;
  stream << "Global symbols:" << endl;
  stream << value._globalList;
  stream << "Local symbols:" << endl;
  stream << value._localList;
  return stream;
}

// Constructor
NameSpace::NameSpace()
{
  // Create the list of default C keywords.
  _keyList.insert(Token("auto", Token::typetoken, Token::nomod));
  _keyList.insert(Token("break", Token::reserved, Token::nomod));
  _keyList.insert(Token("case", Token::reserved, Token::nomod));
  _keyList.insert(Token("char", Token::typetoken, Token::nomod));
  _keyList.insert(Token("const", Token::typetoken, Token::nomod));
  _keyList.insert(Token("continue", Token::reserved, Token::nomod));
  _keyList.insert(Token("default", Token::reserved, Token::nomod));
  _keyList.insert(Token("do", Token::reserved, Token::nomod));
  _keyList.insert(Token("double", Token::typetoken, Token::nomod));
  _keyList.insert(Token("else", Token::reserved, Token::nomod));
  _keyList.insert(Token("enum", Token::compound, Token::nomod));
  _keyList.insert(Token("extern", Token::typetoken, Token::nomod));
  _keyList.insert(Token("float", Token::typetoken, Token::nomod));
  _keyList.insert(Token("for", Token::control, Token::threearg));
  _keyList.insert(Token("goto", Token::reserved, Token::nomod));
  _keyList.insert(Token("if", Token::control, Token::onearg));
  _keyList.insert(Token("int", Token::typetoken, Token::nomod));
  _keyList.insert(Token("long", Token::typetoken, Token::nomod));
  _keyList.insert(Token("register", Token::typetoken, Token::nomod));
  _keyList.insert(Token("return", Token::reserved, Token::nomod));
  _keyList.insert(Token("short", Token::typetoken, Token::nomod));
  _keyList.insert(Token("signed", Token::typetoken, Token::nomod));
  _keyList.insert(Token("sizeof", Token::literal, Token::nomod)); // Close enough
  _keyList.insert(Token("static", Token::statictoken, Token::nomod));
  _keyList.insert(Token("struct", Token::compound, Token::nomod));
  _keyList.insert(Token("switch", Token::control, Token::onearg));
  _keyList.insert(Token("typedef", Token::typedeftoken, Token::nomod));
  _keyList.insert(Token("union", Token::compound, Token::nomod));
  _keyList.insert(Token("unsigned", Token::typetoken, Token::nomod));
  _keyList.insert(Token("void", Token::typetoken, Token::nomod));
  _keyList.insert(Token("volatile", Token::typetoken, Token::nomod));
  _keyList.insert(Token("while", Token::control, Token::onearg));
}

// Destructor
NameSpace::~NameSpace()
{
    clearGlobalNames();
}

// Clears all user defined tokens from the namespace
void NameSpace::clearGlobalNames()
{
  Token temp;

  clearLocalNames();
  /* A static prototype without a matching function declaration is an
     error. If find one here, it was never matched by a declaration */
  TokenSet::iterator index;
  for (index = _globalList.begin(); index != _globalList.end(); index++)
    if ((index->getType() == Token::functproto) &&
        (index->getScope() == Token::filescope))
        logTokenError(cout, *index, "Static prototype of ",
                      " has no matching declaration");
  _globalList.clear();
}

// If the token is a keyword, changes token data to that of the keyword
void NameSpace::checkForSymbol(Token& testToken)
{
  TokenSet::iterator symbolIter;
  bool localVar = false; // True, name is a variable in local scope

  symbolIter = _keyList.find(testToken);
  if (symbolIter != _keyList.end()) // Identifier is a reserved word
    testToken.setToTokenMeaning(*symbolIter);
  else { // Check for local symbol
    symbolIter = _localList.find(testToken);
    if (symbolIter != _localList.end()) {
      if (symbolIter->getType() == Token::typetoken) // Locally defined typedef
        testToken.setToTokenMeaning(*symbolIter);
      else
        localVar = true;
    }
    /* Local vars can shadow function names. If the name is then used as a
       function call, it is an error. This program is biased toward believing
       a function call was intended in such cases, so get scope info even if
       the name is a local variable */
    if ((symbolIter == _localList.end()) || localVar) {
      symbolIter = _globalList.find(testToken);
      if (symbolIter == _globalList.end())
        testToken.setScope(Token::noscope); // Can't determine its scope yet
      // Check for defined typedef. Need to check if it is shadowed
      else if (haveTypeToken(*symbolIter)) {
        if (!localVar)
            testToken.setToTokenMeaning(*symbolIter);
        // Else it is shadowed; don't do anything
      }
      else if (!haveVarToken(*symbolIter)) {
        // Have a potential function call. Set its scope
        /* Static prototypes are overridden by the scope of the actual
           function declaration, so can't resolve calls for these yet.
           (Note that not having a declaration somewhere in the file is an
            error; this is handled elsewhere) */
        if ((symbolIter->getType() != Token::functproto) ||
            (symbolIter->getScope() != Token::filescope))
            testToken.setScope(symbolIter->getScope());
        else
            testToken.setScope(Token::noscope);
      } // Potential function call
    }
  }
}

bool NameSpace::isKeyword(const Token& testToken)
{
  /* Names are originally tokenized as identifiers. If the token is an
     identifier, need to look it up to find out what it really is. For
     everything else, base the result on the token type */
  if (testToken.getType() != Token::identifier)
    return ((testToken.getType() == Token::literal) || (testToken.getType() == Token::functdecl) ||
            (testToken.getType() == Token::functproto) || (testToken.getType() == Token::functcall) ||
            (testToken.getType() == Token::functtypedef) || (testToken.getType() == Token::typetoken) ||
            (testToken.getType() == Token::typedeftoken) || (testToken.getType() == Token::statictoken) ||
            (testToken.getType() == Token::compound) || (testToken.getType() == Token::control) ||
            (testToken.getType() == Token::reserved));
  else {
    // Identifier, so need to look it up in symbol table. Its a keyword if its not a variable name
    TokenSet::iterator temp;
    temp = _keyList.find(testToken);
    if ((temp != _keyList.end()) && (temp->getType() != Token::varname))
        return true;
    else {
        temp = _globalList.find(testToken);
        if ((temp != _globalList.end()) && (temp->getType() != Token::varname))
            return true;
        else {
            temp = _localList.find(testToken);
            return ((temp != _localList.end()) &&
                    (temp->getType() != Token::varname));
        } // Not in global symbol list
    } // Not in language keyword list
  } // Identifer
}

/* Update the name space for a given parsed token. Report errors in the case
   of symbol collisions which can affect the program results. */
void NameSpace::updateNameSpace(const Token& testToken)
{
  TokenSet::iterator globalIter; // Pointer to existing global record
  TokenSet::iterator localiter; // Pointer to existing local record

  globalIter = _globalList.find(testToken);
  localiter = _localList.find(testToken);

  if (testToken.getScope() == Token::localscope) { // Local scope
    /* Local scope is updated if the symbol is new, or a typedef collided
       with a varname */
    if ((localiter == _localList.end()) ||
        ((localiter->getType() == Token::varname) &&
         (testToken.getType() == Token::typetoken))) {
      /* If the symbol collides with a global symbol, a shadow situation now
         exists. Issue a warning if a the global symbol is a function.
         Shadowing by type is more serious than shadowing by a variable,
         because it is much harder to check if a type symbol was meant to be
         used as a function */
      if ((globalIter != _globalList.end()) && (!haveVarToken(*globalIter))) {
        if (testToken.getType() == Token::typetoken) {
            if (globalIter->getType() == Token::functtypedef)
                logTokenError(cout, testToken, "Declaration of type ",
                              " shadows function typedef with same name in outer scope");
            else
                logTokenError(cout, testToken, "Declaration of type ",
                              " shadows function with same name in outer scope");
        }
        else if (globalIter->getType() == Token::functtypedef)
            logTokenError(cout, testToken, "Local variable ",
                          " shadows function typedef with same name in outer scope");
        else
            logTokenError(cout, testToken, "Local variable ",
                          " shadows function with same name in outer scope");
      }
      if (localiter != _localList.end())
        _localList.erase(localiter);
      _localList.insert(testToken);
    } // Need to update local scope
  }
  // Symbol must be file or global scope
  else if (haveVarToken(testToken)) {
    if (globalIter == _globalList.end())
      _globalList.insert(testToken);
    // Report colision of a var with a function
    else if (!haveVarToken(*globalIter)) {
      if (globalIter->getType() == Token::functtypedef) {
        if (testToken.getType() == Token::varname)
            logTokenError(cout, testToken, "Variable ",
                          " uses name previosly used as typedef for function");
        else
            logTokenError(cout, testToken, "Type declarion ",
                          " uses name previosly used as typedef for function");
      }
      else if (testToken.getType() == Token::varname)
        logTokenError(cout, testToken, "Variable ",
                      " uses name previously used as a function");
      else
        logTokenError(cout, testToken, "Type declaration ",
                      " uses name previously used as a function");
    }
    // If a var collides with a typedef, take the typedef
    else if ((globalIter->getType() == Token::varname) &&
             (testToken.getType() == Token::typetoken)) {
      _globalList.erase(globalIter);
      _globalList.insert(testToken);
    }
  }
  else { // Function typedef or function
    if (localiter != _localList.end()) {
      // Collision with a local name
      /* If have either a function call that was not previously declared, or a
         type that was ignored due to a shadow, assume the conflict is due
         to the misuse of the local symbol */
      if (((globalIter != _globalList.end()) && haveTypeToken(*globalIter)) ||
          ((testToken.getType() == Token::functcall) &&
           ((globalIter == _globalList.end()) || haveVarToken(*globalIter)))) {
        if (testToken.getType() == Token::functtypedef)
            logTokenError(cout, testToken, "Typedef for function ",
                          " uses name previously used as a local variable");
        else
            logTokenError(cout, testToken, "Function ",
                          " uses name previously used as a local variable");
      }
      // Collision is a shadow. Issue a warning if the shadow is new
      else if ((globalIter == _globalList.end()) ||
               haveVarToken(*globalIter)) {
        if (localiter->getType() == Token::typetoken) {
            if (testToken.getType() == Token::functtypedef)
                logTokenError(cout, testToken, "Declaration of type ",
                              " shadows function typedef with same name in outer scope");
            else
                logTokenError(cout, testToken, "Declaration of type ",
                              " shadows function with same name in outer scope");
        }
        else if (testToken.getType() == Token::functtypedef)
            logTokenError(cout, *localiter, "Local variable ",
                          " shadows function typedef with same name in outer scope");
        else
            logTokenError(cout, *localiter, "Local variable ",
                          " shadows function with same name in outer scope");
        }
    }
    if (testToken.getType() == Token::functcall) {
      /* If function call collides with a type, ignore it. Issue a warning if
         the collision was not due to a local shadow */
      if ((globalIter != _globalList.end()) && haveTypeToken(*globalIter)) {
        if (localiter == _localList.end())
            logTokenError(cout, *globalIter, "Type declaration ",
                          " uses name previously used as a function");
      }
      /* If name of function is not in stack as a function prototype or
         declaration, have an undeclared function call. Issue a warning and
         update the namespace if the function call is not already there */
      else if ((globalIter == _globalList.end()) ||
               ((globalIter->getType() != Token::functproto) &&
                (globalIter->getType() != Token::functdecl))) {
        logTokenError(cout, testToken, "Function call ",
                      " has no prototype");
        if (globalIter == _globalList.end())
            _globalList.insert(testToken);
        else if (globalIter->getType() != Token::functcall) {
            // Compain if symbol was not shadowed
            if (localiter == _localList.end())
                logTokenError(cout, *globalIter, "Variable ",
                              " uses name previously used as a function");
            _globalList.erase(globalIter);
            _globalList.insert(testToken);
            }
        }
    } // Function call
    // Function prototype or declaration
    else if (globalIter == _globalList.end())
      _globalList.insert(testToken);
    /* If collide with a typedef, have a dedefiniton of a local variable,
       which shadowed the typedef, as a function declaration. This requies
       the function declaration to have been made in local scope, which is
       almost certainly an error. Lose the declaration */
    else if (haveTypeToken(*globalIter)) {
      if (localiter == _localList.end()) { // Shadow handled above
        if (testToken.getType() == Token::functtypedef) {
            if (globalIter->getType() == Token::functtypedef)
                logTokenError(cout, testToken, "Duplicate declaration of function typedef ", "");
            else
                logTokenError(cout, *globalIter, "Type declarion ",
                              " uses name previosly used as typedef for function");
        }
        else
            logTokenError(cout, *globalIter, "Type declaration ",
                          " uses name previously used as a function");
      }
    }
    // If a function collides with a var, believe the function was intended
    else if (haveVarToken(*globalIter)) {
      if (testToken.getType() == Token::functtypedef)
        logTokenError(cout, *globalIter, "Variable ",
                      " uses name previosly used as typedef for function");
      else
        logTokenError(cout, *globalIter, "Variable ",
                      " uses name previously used as a function");
      // Overwrite the variable symbol with the token
      _globalList.erase(globalIter);
      _globalList.insert(testToken);
    }
    /* If function typedef collides with a function declaration, believe the
       function declaration was intended */
    else if (testToken.getType() == Token::functtypedef)
        logTokenError(cout, testToken, "Type declaration ",
                      " uses name previously used as a function");
    /* If a function call collides with a declaration, have the declaration
       for a previously undeclared function */
    else if (globalIter->getType() == Token::functcall) {
      _globalList.erase(globalIter);
      _globalList.insert(testToken);
    }
    /* If get to there, a function or prototype collided with another
       declaration or prototype. Now file vs. global scope needs to be
       considered */
    else if (testToken.getType() == Token::functproto) {
      // Prototype collided with prototype
      /* If scope narrows, some function calls may have the wrong scope. Issue
         a warning about this */
      if (globalIter->getType() == Token::functproto) {
        if ((testToken.getScope() == Token::filescope) &&
            (globalIter->getScope() == Token::globalscope)) {
            logTokenError(cout, testToken, "Static function ",
                          "occurs after global prototype in same file.");
            _globalList.erase(globalIter);
            _globalList.insert(testToken);
        }
        else
            logTokenError(cout, testToken, "Duplicate prototype of ", "");
      }
      else
        // Prototype collided with declaration
        logTokenError(cout, testToken, "Prototype for ",
                      " occurs after declaration");
    }
    else if (globalIter->getType() == Token::functproto) {
      // Declaration collided with prototype
      if ((testToken.getScope() == Token::filescope) &&
          (globalIter->getScope() == Token::globalscope))
        logTokenError(cout, testToken, "Static function ",
                      "occurs after global prototype in same file.");
      _globalList.erase(globalIter);
      _globalList.insert(testToken);
    }
    // Declaration collided with declaration
    else {
      if (testToken.getScope() == globalIter->getScope())
        logTokenError(cout, testToken, "Duplicate declaration of ", "");
      else {
        logTokenError(cout, testToken, "Duplicate declaration of ",
                      ", with different scope. File scope assumed.");
        // Assume file scope is the one wanted for calls in the file
        if (globalIter->getScope() == Token::globalscope) {
            _globalList.erase(globalIter);
            _globalList.insert(testToken);
        }
      }
    } // Function or typedef of function
  } // Global scope symbol
}
