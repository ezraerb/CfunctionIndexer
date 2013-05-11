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
// Tokenizer.cpp The object that tokenizes file input

#include<string>
#include<fstream>
#include<iostream>
#include<vector>
#include<cctype>
#include<list>
#include"basetypes.h"
#include"errors.h"
#include"filebuffer.h"
#include"tokenizer.h"

using std::string;
using std::isalpha;
using std::isdigit;

/* This oject tokenizes the file input. It does so as follows:
   File input           Resulting token
   &                    ampersand
   ->                   fieldaccess
   .                    fieldaccess
   ;                    semicolon
   {                    openbrace
   }                    closebrace
   (                    openparenthese
   )                    closeparanthese
   quoted string        literal
   one or more digits   literal
   alpha followed by    identifier
     any no of alpha or
     digits
     * [ ] or ,         declsymbol
   anything else        othersymbol
*/

const string Tokenizer::_digits = "1234567890";
const string Tokenizer::_alpha = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
const string Tokenizer::_declChars = "*[], \t";
const string Tokenizer::_otherChars = "`!@#$%^+=|\\<>?/";

// Constructor
Tokenizer::Tokenizer()
    : _location("", 0) // No file yet
{
    init();
}

// Initializes state
void Tokenizer::init()
{
    _buffer = "";
    _location = FilePosition("", 0);
    _charPtr = 0;
    _loadLineData = false;
    _newLinePos = 0;
}

// Returns true if the current char is an escaped newline
bool Tokenizer::isLineWrap(unsigned int pos, bool multiLineQuote)
{
  // If on the last line of input, by definition can't wrap
  if (_file.haveEOF())
    return false;
  else if (pos >= _buffer.length())
    return false; // Not in buffer!
  else if (_buffer[pos] != '\\')
    return false; // Not an escape character
  else
    return (FileBuffer::getEscNewline(_buffer, multiLineQuote) == pos);
}

// Reloads the buffer from the file
void Tokenizer::reloadBuffer(bool multiLineQuote)
{
  string tempBuffer;
  unsigned int numKeepChars; // Num chars from old buffer to retain in new buffer

  if (_charPtr >= _buffer.length())
    numKeepChars = 0; // Going to drop the entire buffer
  else {
    // Find chars to copy into new buffer
    // If the buffer has an escaped newline, don't include it
    unsigned int firstIgnoreChar = FileBuffer::getEscNewline(_buffer, multiLineQuote);
    if (firstIgnoreChar == string::npos) // None present
        firstIgnoreChar = _buffer.length();
    if (firstIgnoreChar <= _charPtr)
        numKeepChars = 0;
    else
        numKeepChars = firstIgnoreChar - _charPtr;
  }

  if (numKeepChars > 0)
    _buffer.assign(_buffer, _charPtr, numKeepChars);
  else
    _buffer.clear();
  _newLinePos = numKeepChars; // New text starts after retained text
  if (!_file.haveEOF()) {
    _file>>tempBuffer;
    _buffer += tempBuffer;
    _loadLineData = true;
  } // Not already at end of file
  _charPtr = 0;
}

// Starts tokenizer on named file
void Tokenizer::start(string fileName)
{
  init();
  _file.open(fileName);
  reloadBuffer(false);
  _location = _file.getFilePosition();
}

// Handles chars without special tokens
Token Tokenizer::handleOtherChars()
{
  /* These chars split into two groups, those that are allowed in declaration
     statements, and those that aren't. Other than that, the parser ignores
     them. */
  /* Optimization: The parser ignores consecutive chars of this type, so
     consolidate them into one token. Can merge declaration chars into non
     declaration chars if the latter are found first */
  string lexeme;
  Token::TokenType wantType;
  unsigned int end;
  if (_declChars.find_first_of(_buffer[_charPtr]) != string::npos) {
    wantType = Token::declsymbol;
    end = _buffer.find_first_not_of(_declChars, (_charPtr+1));
  }
  else {
    wantType = Token::othersymbol;
    end = _buffer.find_first_not_of(_declChars + _otherChars, (_charPtr+1));
  }
  if (end == string::npos)
    end = _buffer.length() - 1;
  else
    end--; // Ends one beyond wanted char
  lexeme.assign(_buffer, _charPtr, (end - _charPtr + 1));
  _charPtr = end;
  return Token(lexeme, _location, wantType);
}

// Processes a numeric literal
Token Tokenizer::getNumeric()
{
  unsigned int end = _charPtr;
  bool haveLexeme = false;
  bool seenE = false;
  string lexeme;

  while (!haveLexeme) {
    if (end > (_buffer.length() - 1))
      end = string::npos;
    else
      end = _buffer.find_first_not_of((_digits+'.'), end);
    if (end == string::npos) {
      end = _buffer.length();
      haveLexeme = true;
    }
    else {
      // If line wraps, load the rest
      if (isLineWrap(end, false)) {
        reloadBuffer(false);
        // Begin search from start of chars that were added
        end = _newLinePos;
      }
      else if ((_buffer[end] == 'E') && (!seenE)) {
        // Exponential notation
        end++; // Skip over the E, and keep going
        seenE = true;
      }
      else
        haveLexeme = true;
    }
  }
  if (end == string::npos)
    end = _buffer.length();
  else
    end--; // Ends one beyond what is wanted
  lexeme.assign(_buffer, _charPtr, (end - _charPtr + 1));
  _charPtr = end; // Set position to last extracted char
  return Token(lexeme, _location, Token::literal);
}

// Processes a quoted string literal
Token Tokenizer::getQuotedString()
{
  bool haveValue = false;
  unsigned int end = _charPtr + 1;
  string lexeme;

  while (!haveValue) {
    end = FileBuffer::nextCloseQuote(_buffer, end);
    if ((!_file.haveEOF()) && (end == string::npos)) {
      // Literal wraps to next line
      reloadBuffer(true);
      // Begin search from start of chars that were added
      end = _newLinePos;
    }
    else
      haveValue = true;
  }
  if (end == string::npos)
    end = _buffer.length();
  lexeme.assign(_buffer, _charPtr, (end - _charPtr + 1));
  _charPtr = end;
  return Token(lexeme, _location, Token::literal);
}

// Processes an identifier
Token Tokenizer::getIdentifier()
{
  unsigned int end;
  bool haveLexeme = false;
  string lexeme;
  string temp;

  // The first char has different rules from the rest
  lexeme = _buffer[_charPtr];
  _charPtr++;
  end = _charPtr;

  while (!haveLexeme) {
    if (end > (_buffer.length() - 1))
      end = string::npos;
    else
      end = _buffer.find_first_not_of((_alpha + _digits), end);
    if (end == string::npos)
      haveLexeme = true;
    // If line is wrapped, then load the rest
    else if (isLineWrap(end, false)) {
      reloadBuffer(false);
      // Begin search from start of chars that were added
      end = _newLinePos;
    }
    else
      haveLexeme = true;
  }
  if (end == string::npos)
    end = _buffer.length();
  else
    end--; // Ends one beyond what is wanted
  if (end >= _charPtr) {
    temp.assign(_buffer, _charPtr, (end - _charPtr + 1));
    lexeme+=temp;
  }
  _charPtr = end;
  return Token(lexeme, _location, Token::identifier);
}

// Processes a minus sign
Token Tokenizer::handleMinus()
{
  string lexeme;

  // Need to check next char to see if have ->, which is a field operator
  if (_charPtr == (_buffer.length() - 1))
    return Token(_buffer[_charPtr], _location, Token::othersymbol);
  else {
    // If line is wrapped then load the rest
    if (isLineWrap((_charPtr + 1), false))
      reloadBuffer(false);
    if (_buffer[_charPtr + 1] == '>') {
      lexeme.assign(_buffer, _charPtr, 2);
      _charPtr++;
      return Token(lexeme, _location, Token::fieldaccess);
    }
    else
      return handleOtherChars();
  }
}

// Processes an ampersand
Token Tokenizer::handleAmpersand()
{
  string lexeme;

  /* Need to check next char to see if have &&, which is the boolean AND
     operator, not a potential refrence operator (The parser handles splitting
     the bitwise AND operator from a refrence operator) */
  if (_charPtr == (_buffer.length() - 1))
    return Token(_buffer[_charPtr], _location, Token::ampersand);
  else {
    // If line is wrapped then load the rest
    if (isLineWrap((_charPtr + 1), false))
      reloadBuffer(false);
    if (_buffer[_charPtr + 1] == '&') { // boolean AND operator
      lexeme.assign(_buffer, _charPtr, 2);
      _charPtr++;
      return Token(lexeme, _location, Token::othersymbol);
    }
    else
      return Token(_buffer[_charPtr], _location, Token::ampersand);
  }
}

// Processes a single quote
Token Tokenizer::handleSinQuote()
{
  /* Expect to find one of the following:
     1. quote char quote
     2. quote escape char quote
     3. quote escape digit digit digit quote
     4. quote escape x digit digit quote.
    This function is implemented as a state machine based on lexeme length */
  bool haveError = false;
  bool haveValue = false;
  bool haveEscape = false;
  bool haveHex = false;
  bool haveOct = false;
  bool haveZero = false;
  unsigned int length = 1;
  char testChar;
  string lexeme;

  while ((!haveValue) && (!haveError)) {
    length++;
    if ((_charPtr + length - 1) >= _buffer.length())
      haveError = true;
    else if (isLineWrap((_charPtr + length - 1), true)) {
      reloadBuffer(true);
      length--; // Burned the escape newline; shrink length to compensate
    }
    else {
      testChar = _buffer[_charPtr + length - 1];
      switch((int)length) {
      case 2:
        if (testChar == '\'')
            haveError = true;
        else if (testChar == '\\')
            haveEscape = true;
        break;

      case 3:
        if (!haveEscape) {
            if (testChar == '\'')
                haveValue = true;
            else
                haveError = true;
        }
        /* If this char is a zero, it can be either start of an octal number,
           or the NUL escape sequence. Need next char to tell which */
        else if (testChar == '0')
            haveZero = true;
        else if (isdigit(testChar))
            haveOct = true;
        else if (testChar == 'x')
            haveHex = true;
        else if ((testChar != 'a') && (testChar != 'b') &&
                 (testChar != 'f') && (testChar != 'n') &&
                 (testChar != 'r') && (testChar != 't') &&
                 (testChar != 'v') && (testChar != '\\') &&
                 (testChar != '?') && (testChar != '"') &&
                 (testChar != '\''))
            haveError = true;
        break;

      case 4:
        // Check if a zero is part of oct number, or NUL escape sequence
        if (haveZero && isdigit(testChar))
            haveOct = true; // Zero is first digit of octal number

        if (haveOct)
            haveError = (!isdigit(testChar));
        else if (haveHex)
            haveError = ((!isdigit(testChar)) && (testChar != 'A') &&
                         (testChar != 'B') &&(testChar != 'C') &&
                         (testChar != 'D') &&(testChar != 'E') &&
                         (testChar != 'F'));
        else if ((haveEscape) && (testChar == '\''))
            haveValue = true;
        else
            haveError = true;
        break;

      case 5:
        if (haveOct)
            haveError = (!isdigit(testChar));
        else if (haveHex)
            haveError = ((!isdigit(testChar)) && (testChar != 'A') &&
                         (testChar != 'B') &&(testChar != 'C') &&
                         (testChar != 'D') &&(testChar != 'E') &&
                         (testChar != 'F'));
        else
            haveError = true;
        break;

      case 6:
        if ((haveHex || haveOct) && (testChar == '\''))
            haveValue = true;
        else
            haveError = true;
        break;

      default:
        haveError = true;
      }
    }
  }
  if (haveValue) {
    lexeme.assign(_buffer, _charPtr, length);
    _charPtr += (length - 1);
    return Token(lexeme, _location, Token::literal);
  }
  else
    return handleOtherChars();
}

// Lexes the next token in the file
Token Tokenizer::nextToken()
{
  Token returnToken;
  bool haveChar;

  if (haveEOF()) {
    FilePosition temp(_location); // Should be pointing to last line of file
    temp.incrLine();
    return Token("", temp, Token::tokenEOF);
  }
  else {
    if (isalpha(_buffer[_charPtr]) || (_buffer[_charPtr] == '_') ||
	(_buffer[_charPtr] == '~'))
      returnToken = getIdentifier();
    else if (isdigit(_buffer[_charPtr]))
      returnToken = getNumeric();
    else
      switch(_buffer[_charPtr]) {
      case '"':
        returnToken = getQuotedString();
        break;

      case '-':
        returnToken = handleMinus();
        break;

      case '\'':
        returnToken = handleSinQuote();
        break;

      case '&':
        returnToken = handleAmpersand();
        break;

      case '.':
        // Check for leading decimal point of a numeric
        if ((_charPtr == (_buffer.length() - 1)) ||
            (!isdigit(_buffer[_charPtr + 1])))
            returnToken = Token(_buffer[_charPtr], _location, Token::fieldaccess);
        else
            returnToken = getNumeric();
        break;

      case ';':
        returnToken = Token(_buffer[_charPtr], _location, Token::semicolon);
        break;

      case '{':
        returnToken = Token(_buffer[_charPtr], _location, Token::openbrace);
        break;

      case '}':
        returnToken = Token(_buffer[_charPtr], _location, Token::closebrace);
        break;

      case '(':
        returnToken = Token(_buffer[_charPtr], _location, Token::openparen);
        break;

      case ')':
        returnToken = Token(_buffer[_charPtr], _location, Token::closeparen);
        break;

      default:
        returnToken = handleOtherChars();
      }
    // Find the next char to process
    _charPtr++; // move off previous char
    haveChar = false;
    while ((!haveChar) &&
           ((!_file.haveEOF()) || (_charPtr < _buffer.length()))) {
      // Burn spaces and tabs
      if (_charPtr < _buffer.length())
        _charPtr = FileBuffer::burnSpaces(_buffer, _charPtr);

      if (_charPtr == string::npos)
        _charPtr = _buffer.length();
      // If now on an escaped newline, burn it
      else if (isLineWrap(_charPtr, false))
        _charPtr = _buffer.length();

      // If beyond end of _buffer, reload; else have wanted char
      if (_charPtr >= _buffer.length())
        reloadBuffer(false);
      else
        haveChar = true;
    }
    // Update position information if needed
    if (_loadLineData && (_charPtr >= _newLinePos)) {
      _location = _file.getFilePosition();
      _loadLineData = false;
    }
    return returnToken;
  }
}

// Constructor
TokenList::TokenList()
{
    initVars();
}

Token TokenList::nextToken()
{
  Token temp;
  if (_holdList.empty())
    temp = _file.nextToken();
  else {
    temp = _holdList.front();
    _holdList.pop_front();
  }
  resetLookahead(); // Just read a token, so old lookahead is invalid
  return temp;
}

Token TokenList::nextLookahead()
{
  list<Token>::const_iterator temp;

  if (_lookPtr == _holdList.end()) { // First lookahead after a Get
    if (_holdList.empty())
      _holdList.push_back(_file.nextToken());
    _lookPtr = _holdList.begin();
  }
  else {
    temp = _lookPtr;
    _lookPtr++;
    if (_lookPtr == _holdList.end()) {
      // Next element is not in the list
      _holdList.push_back(_file.nextToken());
      // Restore and reiterate, so iterator points to new element
      _lookPtr = temp;
      _lookPtr++;
    }
  }
  return (*_lookPtr);
}

