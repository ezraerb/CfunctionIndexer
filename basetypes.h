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
// Common structures used to parse files
/* NOTE: Deliberately not including standard headers since all callers
    need them and should include the headers themselves */
using std::string;
using std::ostream;
using std::vector;

// Describes where in a file a piece of data came from
class FilePosition {
private:
  string _fileName;
  int _lineNo;
public:
  // Constructor to set at a given position in a file
  FilePosition(const string& fileName, int lineNo);

  /* Uses the defalt copy constructor, assignment operator,
     equality operator, and destructor */

  // Moves to next line
  void incrLine();

  bool operator==(const FilePosition& other) const;

  // Less than operator. Needed to sort the class
  bool operator<(const FilePosition& other) const;

  const string& getFileName() const;

  int getLineNo() const;
};

// Debug output operator.
ostream& operator<<(ostream& stream, const FilePosition& value);

  // Moves to next line
inline void FilePosition::incrLine()
{
    _lineNo++;
}

inline bool FilePosition::operator==(const FilePosition& other) const
{
    return ((_fileName == other._fileName) &&
            (_lineNo == other._lineNo));
}

// Less than operator. Needed to sort the class
inline bool FilePosition::operator<(const FilePosition& other) const
{
    if (_fileName < other._fileName)
        return true;
    else if (_fileName > other._fileName)
        return false;
    else
        return (_lineNo < other._lineNo);
}

inline const string& FilePosition::getFileName() const
{
    return _fileName;
}

inline int FilePosition::getLineNo() const
{
    return _lineNo;
}

class Token {
  friend ostream& operator<<(ostream& stream, const Token& data);

// Describes a C languate element
public:
    typedef enum { notoken, identifier, literal, varname, functdecl, functproto,
                    functcall, functtypedef, typetoken, typedeftoken, statictoken,
                    compound, control, reserved, openparen, closeparen, openbrace,
                    closebrace, ampersand, fieldaccess, semicolon, declsymbol,
                    othersymbol, tokenEOF } TokenType;
    typedef enum { noscope, keyword, globalscope, filescope,
                    localscope } ScopeType;
    typedef enum { nomod, funcref, onearg, twoarg, threearg } ModType;

private:
  string _lexeme; // Actual data from the file
  FilePosition _location; // where it was found
  TokenType _type; // What it means.
  ScopeType _scope; // Scope it falls in
  ModType _modifier; // Other data to process token

public:
   // Default value to mean not a token, used to handle errors
   Token();

  // Constructors to construct a token
  Token(string lexeme, const FilePosition& location, TokenType tokenclass);
  Token(char lexeme, const FilePosition& location, TokenType tokenclass);

  // Constructor used for default keyword tokens
  Token(string lexeme, TokenType tokenclass, ModType modifier);

  /* This uses the default copy constructor, assignment operator, and
     destructor */

  /* Change a token to the default value, ensures value does not carry
    over when processing multiple token values */
  void setToNoToken();

  /* Access to individual fields. The lexeme and file data are fixed when
     the token is created, but what the token means can change */
  const string& getLexeme() const;
  const FilePosition& getFilePosition() const;
  TokenType getType() const;
  ScopeType getScope() const;
  ModType getModifier() const;
  void setType(TokenType tokentype);
  void setScope(ScopeType wantScope);
  void setModifier(ModType modifier);

  // Change the meaning of the token to match the passed token
  void setToTokenMeaning(const Token& model);

  // Equality operator. This is used for searching token lists
  bool operator==(const Token& other) const;

  // Comparison operators. Used for building sets and maps
  bool operator<(const Token& other) const;
  bool operator>(const Token& other) const;
};

inline void Token::setToNoToken()
{
    _location = FilePosition("", 0);
    _lexeme = "";
    _type=notoken;
    _scope = noscope;
    _modifier = nomod;
}

inline const string& Token::getLexeme() const
{ return _lexeme; }

inline const FilePosition& Token::getFilePosition() const
{ return _location; }

inline Token::TokenType Token::getType() const
{ return _type; }

inline Token::ScopeType Token::getScope() const
{ return _scope; }

inline Token::ModType Token::getModifier() const
{ return _modifier; }

inline void Token::setType(Token::TokenType tokentype)
{ _type = tokentype; }

inline void Token::setScope(Token::ScopeType wantScope)
{ _scope = wantScope; }

inline void Token::setModifier(Token::ModType modifier)
{ _modifier = modifier; }

// Change the meaning of the token to match the passed token
inline void Token::setToTokenMeaning(const Token& model)
{
    _type = model._type;
    _scope = model._scope;
    _modifier = model._modifier;
}

// Equality operator. This is used for searching token lists
inline bool Token::operator==(const Token &other) const
{
  /* Under the C standard, non-idenifiers are always tokenized the same.
    If they have the same lexemes, they match. Identifiers must have unique
    lexemes within their namespace. Matching lexemes indicates a symbol clash,
    which should be matched so it can be flagged. In addition, matching
    symbols in different namespaces indicates a shadow, which should also be
    flagged,so ignore namespace in the search. In short, if the lexemes match,
    the tokens match as far as this program is concerned */
  return (_lexeme == other._lexeme);
}

inline bool Token::operator<(const Token& other) const
{
    // Tokens are compared by comparing the lexemes. See equality operator for why
    return (_lexeme < other._lexeme);
}

inline bool Token::operator>(const Token& other) const

{
    // Tokens are compared by comparing the lexemes. See equality operator for why
    return (_lexeme > other._lexeme);
}

// Descrption of a function declaration or call
class FunctionData
{
private:
  string _name;
  FilePosition _location;
  bool _declaration; // True, statement was a function declaration
  string _caller; // Function this function call occured in
  bool _refrence; // True, refrence of function taken instead of calling it
  bool _filescope; // True, scope is restricted to a file

  friend ostream& operator<<(ostream& stream, const FunctionData& data);

public:
  FunctionData(const Token &tokendata, const string& caller);

  bool operator<(const FunctionData& other) const;

  /* This object uses the default copy constructor, assignment operator,
     comparison operator, and destructor */
};
