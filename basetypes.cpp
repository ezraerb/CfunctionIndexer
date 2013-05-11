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
// BaseTypes.cpp Basic types used by the program
#include <string>
#include <iostream>
#include <cctype>
#include <vector>
#include <algorithm>
#include <iomanip>
#include "basetypes.h"

using std::string;
using std::endl;
using std::isalpha;
using std::isdigit;
using std::ios_base;
using std::left;
using std::setw;

// Constructor to set at a given position in a file
FilePosition::FilePosition(const string& fileName, int lineNo)
{
    _fileName = fileName;
    _lineNo = lineNo;
}

// Debug output operator.
ostream& operator<<(ostream& stream, const FilePosition& value)
{
    stream << "line " << value.getLineNo() << " of file " << value.getFileName();
    return stream;
}


Token::Token()
    : _location("", 0)
{
    setToNoToken();
}

// Constructors to construct a token
Token::Token(string lexeme, const FilePosition& location,
             Token::TokenType tokenclass)
        : _location(location)
{
    _lexeme = lexeme;
    _type=tokenclass;
    _scope = noscope;
    _modifier = nomod;
}

Token::Token(char lexeme, const FilePosition& location,
             Token::TokenType tokenclass)
        : _location(location)
{
    _lexeme = lexeme;
    _location=location;
    _type=tokenclass;
    _scope = noscope;
    _modifier = nomod;
}

// Constructor used for default keyword tokens
Token::Token(string lexeme, Token::TokenType tokenclass, Token::ModType modifier)
        : _location("", 0) // Not from file data
{
    _lexeme = lexeme;
    _type=tokenclass;
    _scope=keyword;
    _modifier=modifier;
}

ostream& operator<<(ostream& stream, const Token& data)
{
    stream <<"le:" << data._lexeme << "  lo:" << data._location.getFileName()
           << "-" << data._location.getLineNo() << "  cl:" << data._type
           << "  sc:" << data._scope << "  mo:" << data._modifier;
    return stream;
}

// Output operator for function data. It outputs the data in tabular form
ostream& operator<<(ostream& stream, const FunctionData& data)
{
  stream << std::left << setw(20) << data._name << "  ";
  if (data._filescope)
    stream << "file   ";
  else
    stream << "global ";
  if (data._declaration)
    stream << "declared                         ";
  else {
    if (data._refrence)
      stream << "refrenced in ";
    else
      stream << "called from  ";
    stream << setw(20) << data._caller;
  }
  stream << "  ";
  stream << setw(14) << data._location.getFileName() << "  ";
  stream << data._location.getLineNo() << endl;
  return stream;
}

// Constructor for functiondata
FunctionData::FunctionData(const Token& tokendata, const string& caller)
    : _location(tokendata.getFilePosition())
{
  _name = tokendata.getLexeme();
  _declaration = (tokendata.getType() == Token::functdecl);
  _filescope = (tokendata.getScope() == Token::filescope);
  if (_declaration) {
    _caller = _name;
    _refrence = false;
  }
  else {
    _caller = caller;
    _refrence = (tokendata.getModifier() == Token::funcref);
  }
}

// Comparsion operator. Used to sort final function list
bool FunctionData::operator<(const FunctionData& other) const
{
  // Functions are first sorted by name
  if (_name != other._name)
    return (_name < other._name);
  // Then file scope functions sort before global scope functions
  else if (_filescope != other._filescope)
    return _filescope;
  // File scope functions are sorted by the file they have scope to
  else if (_filescope && (_location.getFileName() != other._location.getFileName()))
    return (_location.getFileName() < other._location.getFileName());
  // Declarations sort before calls
  else if (_declaration != other._declaration)
    return _declaration;
  else
    // Functions finally sort by location
    return (_location < other._location);
}
