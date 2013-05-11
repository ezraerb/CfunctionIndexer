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
// parser.h
class Parser {
 private:
    // Stack of token objects
    class TokenStack: public vector<Token>
    {

    public:
        void push(const Token& value);

        Token pop();

        // Pop tokens until the wanted type is found
        Token popTillType(Token::TokenType wantType);

        // Returns true if a token with the given type is in the stack
        bool hasType(Token::TokenType wantType);
    };

    // This is a functor to allow searching token lists by tokentype
    class SameType
    {
    private:
        Token::TokenType _compValue;

    public:
        SameType(const Token::TokenType& wantType);

        bool operator()(const Token& other) const;
    };

  TokenList _buffer;
  NameSpace _symbolTable;
  TokenStack _parseStack;
  bool _readNextToken; // True, need to reload input before parsing
  Token _currToken; // Current token to process
  Token _functToken; // Last found function token
  // Type of statement being processed
  enum { undet, declaration, expression, constmt } _statementType;
  int _braceCount; // Count of unmatched open braces

  // Completes processing of a statement
  void newStatement();

  // Process a combination type
  void procCombType();

  // Process declaration statements
  void procDeclaration();

  // Process a function declaration statement
  void procFunctDeclaration(Token& declToken, const Token& nextToken,
                            bool insideParams);

  // Finds the next function in the source file
  void findNextFunction();

  // Resets parser to initial state
  void init();

  // This object is based on a file, so it can't be copied
  Parser(const Parser& other);
  const Parser& operator=(const Parser& other);

 public:
  // Constructor
  Parser();

  // Destructor
  ~Parser();

  // Starts the parser on the named file
  void start(const string& fileName);

  // Finds and returns the next function token in the file
  Token nextFunction();

  // Returns whether more functions exist to process
  bool haveEOF();
};

// Starts the parser on the named file
inline void Parser::start(const string& fileName)
{
    init();
    _buffer.start(fileName);
    findNextFunction();
}

// Resets parser to initial state
inline void Parser::init()
{
  _readNextToken = true;
  _currToken.setToNoToken();
  _functToken.setToNoToken();
  _statementType = undet;
  _braceCount = 0;
  _symbolTable.clearGlobalNames();
  newStatement();
}

// Completes processing of a statement
inline void Parser::newStatement()
{
  Token temp;
  while (!_parseStack.empty()) {
    temp = _parseStack.popTillType(Token::functcall);
    if (temp.getType() != Token::notoken)
      logTokenError(std::cout, temp, "Call of function ", " is incomplete");
  }
  _statementType = undet;
}

// Returns the next function token in the file
inline Token Parser::nextFunction()
{
    /* To implement end of file properly, this method is implemented as a
        look-ahead. It finds the next function (which may not exist in the
        remaining file input), caches it in the object, and then returns the
        last one */
    Token result(_functToken);
    findNextFunction();
    return result;

}// Returns whether more functions exist to process
inline bool Parser::haveEOF()
{
    /* At end of file when no more tokens exist to process
        and the ones cached in the object have been returned */
    return (_buffer.haveEOF() && (_functToken.getType() == Token::notoken));
}

inline void Parser::TokenStack::push(const Token& value)
{
    push_back(value);
}

inline Token Parser::TokenStack::pop()
{
  Token temp;
  if (!empty()) {
    temp = back();
    pop_back();
  }
  return temp;
}

inline Token Parser::TokenStack::popTillType(Token::TokenType wantType)
{
  while ((!empty()) && (back().getType() != wantType))
    pop_back();
  return pop();
}

inline bool Parser::SameType::operator()(const Token& other) const
{
    return (other.getType() == _compValue);
}


