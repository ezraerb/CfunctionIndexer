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
// Functfinder.h Description of object to hold descriptions until they are complete
using std::multimap;
using std::pair;
using std::make_pair;

class FunctHold {

/* This class holds data for function calls with unknown scope. They are released
    when the scope is known, which is set per function. This natually leads to a
    multimap indexed by token to hold the data. When released, the data for the
    matching tokens is converted to FunctionData and cached. Callers expect it to
    be returned one at a time, and using a cache takes up the same amount of space
    as leaving the data in the map, and is cleaner to implement. The end of token
    processing triggers a release of the remaining data */

private:
  typedef multimap<Token, string> HoldMap;

  HoldMap _holdData;

  vector<FunctionData> _releaseData;

  // Moves data from the hold map to function data cache
  void moveHoldToCache(HoldMap::iterator begin, HoldMap::iterator end,
                       Token::ScopeType wantScope);

  // This object is part of an object based on a file, so it can't be copied
  FunctHold(const FunctHold& other);
  FunctHold& operator=(const FunctHold& other);

public:
  FunctHold();

  // This object uses the default destructor

  // Initializes the object
  void reset();

  // Returns next token released from hold
  FunctionData nextRelease();

  // Update scope of a function, and release any holds on it
  void releaseHold(const Token& declToken);

  bool doingRelease();

  // Returns true if all functions have been released
  bool empty();

  // Holds a token if necessary
  bool holdIfNeeded(const Token& testToken, const string& callFunct);

  // Special processing for end of file. This releases all holds
  FunctionData procEOF();
};

// Initializes the object
inline void FunctHold::reset()
{
  _holdData.clear();
  _releaseData.clear();
}

// Returns function description of next token to release from hold
inline FunctionData FunctHold::nextRelease()
{
    FunctionData newData(_releaseData.back());
    _releaseData.pop_back();
    return newData;
}

inline bool FunctHold::doingRelease()
{
    return (!_releaseData.empty());
}

inline bool FunctHold::empty()
{
    return (_holdData.empty() && (!doingRelease()));
}

// Update scope of a function, and release any holds on it
inline void FunctHold::releaseHold(const Token& declToken)
{
  if (declToken.getType() == Token::functdecl) {
    pair<HoldMap::iterator, HoldMap::iterator> tokens = _holdData.equal_range(declToken);
    moveHoldToCache(tokens.first, tokens.second, declToken.getScope());
  } // Token is actually a function declaration
}

// Object to produce function descriptions
class FunctFinder {
 private:
  Parser _functBuffer; // Source of data
  string _currFunction; // Current function declaration being processed
  FunctHold _functCallsNoScope; // Object to hold calls with incompelete scope
  // This object is based on a file, so it can't be copied
  FunctFinder(const FunctFinder& other);
  FunctFinder& operator=(const FunctFinder& other);

 public:
  FunctFinder();

  // Resets processing state
  void reset();

  // Starts the function finder on the give file
  void start(const string& fileName);

  // Returns true if all functions have been processed
  bool haveEOF();

  // Returns the next function description in the input
  FunctionData nextFunction();
};

// Resets processing state
inline void FunctFinder::reset()
{
    _currFunction = "NONE";
    _functCallsNoScope.reset();
}

// Starts the function finder on the give file
inline void FunctFinder::start(const string& fileName)
{
    reset();
    _functBuffer.start(fileName);
}

inline bool FunctFinder::haveEOF()
{
    // At end when source file processed and hold list is empty
    return (_functBuffer.haveEOF() && _functCallsNoScope.empty());
}
