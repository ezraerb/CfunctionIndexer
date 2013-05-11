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
using std::set;

typedef set<Token> TokenSet; // Group of tokens in namespace

// Output operator. Used mainly for debugging
ostream& operator<<(ostream& stream, const TokenSet& value);

// Namespace.h Declaration of the named symbol tables
class NameSpace {
  // Output operator. Used mainly for debugging
  friend ostream& operator<<(ostream& stream, const NameSpace& value);

 private:
  TokenSet _keyList; // List of C keywords and what their token values are
  TokenSet _globalList; // List of global/file scope symbols
  TokenSet _localList; // List of local (function) scope symbols

  // Returns true if token is related to variables
  bool haveVarToken(const Token& testToken);

  // Returns true if token is a declaration of a user defined type
  bool haveTypeToken(const Token& testToken);

  // This object is part of the parser, which can't be copied
  NameSpace(const NameSpace& other);
  NameSpace& operator=(const NameSpace& other);

 public:
  // Constructor
  NameSpace();

  // Destructor
  ~NameSpace();

  // Clears all user defined tokens from the namespace
  void clearGlobalNames();

  // Clears the namespace of all keywords with function scope
  void clearLocalNames();

  // If the token is a pre-defied keyword, changes token data to the keyword
  void checkForSymbol(Token& testToken);

  // Returns true if the given token is a keyword or userdefined name
  bool isKeyword(const Token& testToken);

  // Updates name space for a given parsed token. Mostly handles scope issues.
  void updateNameSpace(const Token& testToken);
};

// Clears the namespace of all keywords with function scope
inline void NameSpace::clearLocalNames()
{
    _localList.clear();
}

// Returns true if token is related to variables
inline bool NameSpace::haveVarToken(const Token& testToken)
{
  return ((testToken.getType() == Token::varname) ||
          (testToken.getType() == Token::typetoken));
}

// Returns true if token is a declaration of a user defined type
inline bool NameSpace::haveTypeToken(const Token& testToken)
{
  return ((testToken.getType() == Token::typetoken) ||
          (testToken.getType() == Token::functtypedef));
}

