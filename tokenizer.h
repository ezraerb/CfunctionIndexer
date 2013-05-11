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
using std::list;
using std::pair;

class Tokenizer {
private:
  static const string _digits;
  static const string _alpha;
  static const string _declChars; // Symbols allowed in declaration statements
  static const string _otherChars; // Chars that tokenize to othersymbol

  FileBuffer _file;
  string _buffer; // Actual line from the file being processed
  unsigned int _charPtr; // Location of data to tokenize
  /* The text for a token may wrap between lines, requiring a file read before the
     token finishes. The location should reflect where it starts in this case. Turns out
     the cheapest way of tracking this is to cache the location in this object, and only
     update it between processing each token */
  FilePosition _location; // Location in file of data for current token
  bool _loadLineData; // True need to reload line data after processing token
  unsigned int _newLinePos; // Location in buffer of start of next file line

  // Initializes state
  void init();

  // Reloads the buffer from the file
  void reloadBuffer(bool multiLineQuote);

  // Returns true if the indexed char indicates a wrapped line
  bool isLineWrap(unsigned int pos, bool multiLineQuote);

  // Handles chars without special tokens
  Token handleOtherChars();

  // Processes a numeric literal
  Token getNumeric();

  // Processes a quoted string literal
  Token getQuotedString();

  // Processes an identifier
  Token getIdentifier();

  // Processes a minus sign
  Token handleMinus();

  // Processes an ampersand
  Token handleAmpersand();

  // Processes a single quote
  Token handleSinQuote();

  // The tokenizer is based on a file, so it can't be copied
  Tokenizer(const Tokenizer& other);
  Tokenizer & operator=(const Tokenizer& other);

public:
  // Constructor
  Tokenizer();

  // This object uses the default destructor

  // Starts tokenizer on named file
  void start(string fileName);

  // Lexes and returns next token
  Token nextToken();

  // Returns true if entire file has been processed
  bool haveEOF();
};

// Returns true if entire file has been processed
inline bool Tokenizer::haveEOF()
{
  return (_file.haveEOF() && (_charPtr >= _buffer.length()));
}

// The tokenizer needs to support lookahead. This is implented as a list
class TokenList {
private:
  Tokenizer _file; // Source of tokens
  list<Token> _holdList; // List of tokenized tokens
  list<Token>::const_iterator _lookPtr; // Last accessed lookahead element

  void initVars();

  // This object is based on a tokenizer, so it can't be copied
  TokenList(const TokenList& other);
  const TokenList& operator=(const TokenList& other);

public:
  // Constructor
  TokenList();

  // Opens the list on the given file
  void start(string fileName);

  // Returns the next token to process
  Token nextToken();

  // Looks ahead one token in the token stream
  Token nextLookahead();

  // Returns the most recently found lookahead token
  Token lastLookahead();

  // Resets the lookahead pointer, so a token can be reprocessed
  void resetLookahead();

  // Returns true if all tokens from the source have been read
  bool haveEOF();
};

inline void TokenList::initVars()
{
    _holdList.clear();
    _lookPtr = _holdList.end();
}

// Opens the list on the given file
inline void TokenList::start(string fileName)
{
    initVars();
    _file.start(fileName);
}

// Resets the lookahead pointer, so a token can be reprocessed
inline void TokenList::resetLookahead()
{
    _lookPtr = _holdList.end();
}

// Returns the most recently found lookahead token
inline Token TokenList::lastLookahead()
{
  if (_lookPtr == _holdList.end()) { // First lookahead after a get
    Token temp;
    return temp;
  }
  else
    return (*_lookPtr);
}

// Returns true when all tokens from the source file have been found
inline bool TokenList::haveEOF()
{
    /* Have reached the end when the source file is fully
        processed, and either the hold list is empty or the
        next token indicates end of file */
    return (_file.haveEOF() &&
            (_holdList.empty() ||
             (_holdList.front().getType() == Token::tokenEOF)));
}

