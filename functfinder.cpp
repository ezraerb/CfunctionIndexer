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
// Functfinder.cpp Support routines for creating function descriptions

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <algorithm>
#include <set>
#include <map>
#include"basetypes.h"
#include"errors.h"
#include"filebuffer.h"
#include"tokenizer.h"
#include"namespace.h"
#include"parser.h"
#include"functfinder.h"


using std::string;
using std::fstream;
using std::list;
using std::vector;

FunctHold::FunctHold()
{
    reset();
}

// Moves data from the hold map to function data cache
void FunctHold::moveHoldToCache(HoldMap::iterator first, HoldMap::iterator last,
                                Token::ScopeType wantScope)
{
    if (first != last) { // Have tokens to release
      // Convert to function data and add to cache
      HoldMap::iterator tokenIndex;
      for (tokenIndex = first; tokenIndex != last; tokenIndex++) {
        // Set scope
        Token temp(tokenIndex->first);
        temp.setScope(wantScope);
        _releaseData.push_back(FunctionData(temp, tokenIndex->second));
      } // Data conversion and cache
      _holdData.erase(first, last); // Remove it from the hold map
    } // Have function calls to process
}

// Holds a token if necessary
bool FunctHold::holdIfNeeded(const Token& testToken, const string& callFunct)
{
  // Only hold if scope for function call not known yet
  if ((testToken.getType() != Token::functcall) ||
      (testToken.getScope() != Token::noscope))
    return false;
  /* Attempting to hold while doing a release indicates a logic error. Holding
    functions can only happen while processing tokens. If there are functions
    to release, the caller should be doing that instead */
  else if (doingRelease())
    throw DouFuncRelException();
  else {
    _holdData.insert(make_pair(testToken, callFunct));
    return true;
  }
}

// Special processing for end of file. This releases all holds
FunctionData FunctHold::procEOF()
{
  /* Return all remaining function calls with global scope. If the call is
     still held at this point, the file does not have a declaration for the
     function, so it must be declared somewhere else */
  if (!_holdData.empty())
     // Convert the entire map to function data
     moveHoldToCache(_holdData.begin(), _holdData.end(), Token::globalscope);

  if (empty())
    return FunctionData(Token("", FilePosition("", 0), Token::notoken),
                        "NONE");
  else
    // Return next function in the cache
    return nextRelease();
}

FunctFinder::FunctFinder()
{
    reset();
}

// Returns the next function description in the input
FunctionData FunctFinder::nextFunction()
{
  bool haveFunct; // True have a function description to return
  Token functToken;

  if (_functCallsNoScope.doingRelease())
    return _functCallsNoScope.nextRelease();
  else {
      haveFunct = false;
      while ((!haveFunct) && (!_functBuffer.haveEOF())) {
        functToken = _functBuffer.nextFunction();
        if (functToken.getType() == Token::functdecl) {
            // Declaration. Now processing an new function
            _functCallsNoScope.releaseHold(functToken);
            _currFunction = functToken.getLexeme();
            haveFunct = true;
        }
        else if (!_functCallsNoScope.holdIfNeeded(functToken, _currFunction)) {
            haveFunct = true;
        }
      } // While loop
      if (haveFunct)
        return FunctionData(functToken, _currFunction);
      else
        return _functCallsNoScope.procEOF();
    } // Not releasing tokens
}
