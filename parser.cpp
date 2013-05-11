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
// Parser.cpp Convert tokenized data into program elements

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <algorithm>
#include "basetypes.h"
#include "errors.h"
#include "filebuffer.h"
#include "tokenizer.h"
#include "namespace.h"
#include "parser.h"

using std::string;
using std::vector;
using std::fstream;
using std::list;
using std::cout;

/*   This object implements a very simplified C language parser descigned to
   find function declarations and calls. This parser processes a C program as
   a series of statements. These statements come in four varietes:
   declarations, typedefs, control, and expressions. Control statements are
   detected by the use of control keywords. Declarations and typedefs are
   detected by their use of a type declarer token as the first token in the
   statement. Typedefs are detected within these by the presence of a typedef
   token. Everything else is an expresssion.
     Within these statement types, the program needs to detect functions. An
   unknown identifier with an open paranthese after it is considered a
   function. If it is the first identifier in a declaration statement, it is
   a function declaration or prototype, otherwise it is a function call. Note
   that, although extremely rare, a typedef of a function is legal. These are
   not function declarations, but any other statement that uses the new symbol
   is.
     This parser makes some assumptions about how to handle errors in the
   input. Such assumptions are notoriously hard to do well in bottom-up
   parsers (which this is), so warning messages are produced if the assumption
   will affect the results. In any case, the assumptions are biased toward
   parsing something as a function call, under the theory that reporting
   extra function calls is better than suppressing genuine ones. The
   assumptions this parser makes are:
     1. Openbraces (except inside compound type declarations), semicolons
        (except inside control statements and compound type declarations), and
        control tokens always indicate the start of a new statement.
     2. In a declaration, a second identifier or a literal indicates the
        start of the inital value. The remainder of the statement will be
        treated as an expression.
     3. In expression statements, type symbols are assumed to be casts.
     4. In compound types (struct, etc.) types, operators, parantheses,
        identifiers, and semicolons are assumed to be part of the declaration,
        until the matching close brace is found. If another symbol is found
        before then, all tokens between the compound type and the semicolon
        preceeding the symbol are assumed to be part of the type.
     5. Function argument lists are parsed by counting parantheses. A token
        other than a type, identifier, or operator terminates the list, and
        issues a warning. If the token immediately following an argument list
        is not an openbrace, the function declaration is assumed to be a
        prototype.
     6. Function call arguments are parsed by counting values. If the statement
        ends before the argument list does, a warning message is produced.
     7. Control statements are parsed like expression statements, except that
        the statement is not complete until the correct number of semi-colons
        have been found.

    All new names (functions and variables) are entered into the symbol table.
    Collisions are handled by the symbol table object. */

// Returns true if a token with the given type is in the stack
bool Parser::TokenStack::hasType(Token::TokenType wantType)
{
  return (std::find_if(begin(), end(), SameType(wantType)) != end());
}

Parser::SameType::SameType(const Token::TokenType& wantType)
{
    _compValue = wantType;
};

  // Constructor
Parser::Parser()
{
    init();
}

// Destructor
Parser::~Parser()
{
    newStatement();
}

// Process a combination type
void Parser::procCombType()
{
  Token next; // Next token in the sequence
  Token next2; // The token after that one
  int braceCount; // Count of no openbraces seen
  int parenCount; // Count of consecutive open parantheses seen
  bool readNext; // True, need to reload token lookahead

  next = _buffer.nextLookahead();
  next2 = _buffer.nextLookahead();

  /* Find if the combined type is a declaration, or used as a type. If
     processing a control or expression statement, it MUST be a type */
  if (((next.getType() != Token::identifier) && (next.getType() != Token::openbrace)) ||
      ((next.getType() == Token::identifier) && (next2.getType() != Token::openbrace)) ||
      // Already processing a control or expression statement
      (_statementType == expression) || (_statementType == constmt)) {
        // Used as a type. If no tag, assume the programmer forgot it.
        if (next.getType() == Token::identifier)
            _buffer.nextToken(); // Burn the tag
        // Treat the combined type as a type symbol
        _currToken.setType(Token::typetoken);
  }
  else {
    /* Have a declaration. Want to burn it as long as the tokens inside it
       are legal. If encounter an illegal token, assume the declaration
       statement started at the most recent semicolon (This is most important
       for processing functions, which need the preceeding type tokens). To
       do this properly, look ahead until reach a semicolon or brace, then
       burn. Note that combined types can contain other combined types, so
       need to count the brace pairs */
    /* Optimization note: If terminate early, set currToken to the statement
       seperator that comes after the combo declaration. This will force the
       parser state to be processed properly for the next statement */

    if (next.getType() == Token::identifier) {
      next = next2; // Skip over it
      readNext = true;
    }
    else
      readNext = false;

    braceCount = 1;
    parenCount = 0;
    while (_currToken.getType() == Token::compound) { // Still processing it
      while ((next.getType() != Token::closebrace) && (next.getType() != Token::semicolon) &&
             (next.getType() != Token::functcall) && (next.getType() != Token::control) &&
             (next.getType() != Token::reserved) && (next.getType() != Token::tokenEOF)) {
        // Get next token in line
        if (readNext)
            next = _buffer.nextLookahead();
        else
            next = _buffer.lastLookahead();
        readNext = true;

        // Check for symbolic names
        if (next.getType() == Token::identifier)
            _symbolTable.checkForSymbol(next);

        // Check for inner compound statements
        if (next.getType() == Token::compound) {
            next2 = _buffer.nextLookahead();
            if (next2.getType() == Token::identifier)
                next2 = _buffer.nextLookahead();
            if (next2.getType() == Token::openbrace) {
                // Have an inner compound declaration
                next = next2;
                braceCount++;
            }
            else {
                next.setType(Token::typetoken);
                readNext = false; // Token not part of declaration, so proces it
            }
        }
        else if (next.getType() == Token::identifier) {
            /* Check for function call. If next token after burning parantheses
               around the identifier is an open paren, have a function call */
            // Burn matching parens around the identifier
            while ((_buffer.nextLookahead().getType() == Token::closeparen) &&
                   (parenCount > 0))
                parenCount--;
            if (_buffer.lastLookahead().getType() == Token::openparen)
                next.setType(Token::functcall);
            readNext = false; // Just did it above
        }

        /* Count stacks of consecutive parantheses (Must be done after
           processing identifiers because that process needs the count */
        if (next.getType() == Token::openparen)
            parenCount++;
        else
            parenCount = 0;
      } // Reading the next part of the declaration
      if ((next.getType() == Token::closebrace) || (next.getType() == Token::semicolon)) {
        // Have a valid declaration so far. Burn the actual tokens
        _buffer.nextToken(); // Burn previous seperator
        // Burn to the next seperator
        while ((_buffer.nextLookahead().getType() != Token::semicolon) &&
               (_buffer.lastLookahead().getType() != Token::closebrace))
            _buffer.nextToken();
        if (next.getType() == Token::closebrace) {
            braceCount--;
            if (braceCount <= 0) {
                // Found the end of the declaration
                _buffer.nextToken(); // Burn off the closing brace
                _currToken.setType(Token::typetoken);
            }
        }
        // The burn invalidates the lookahead
        next.setToNoToken();
      }
      else { // Have an early termination
        // Read the statement seperator from the input so statement is dropped
        _currToken = _buffer.nextToken();
        /* If the seperator is a close brace, change it to a semicolon, so the
           brace count (which determines scope) is not affected (In reality,
           have a complete inner struct declaration, which should be reprocessed.
           This is hard to do and the case is never seen in practice, so just
           ignore it) */
        if (_currToken.getType() == Token::closebrace)
            _currToken.setType(Token::semicolon);
      }
    } // Still reading the declaration
  } // Declaration
}

// Process declaration statements.
void Parser::procDeclaration()
{
  Token declToken; // Actual token of the declared entity
  TokenStack varNames; // Names of declared variables
  bool haveFunction; // Have a function declaration
  bool insideParams; // Inside  parameter list of a function
  int parenCount; // Count of unmatched parentheses
  int consParenCount; // Count of consecutive open parantheses

  declToken = _currToken;
  if (_buffer.lastLookahead().getType() == Token::openparen) {
    haveFunction = true;
    insideParams = true;
    parenCount = 1;
    // Burn the paranthese, so not confused with argument declarations
    _buffer.nextToken();
  }
  else {
    haveFunction = false;
    insideParams = false;
    parenCount = 0;
  }

  // Process the rest of the declaration.
  consParenCount = 0;
  while (_statementType == declaration) {
    _currToken = _buffer.nextToken();
    // Check for defined symbols
    if (_currToken.getType() == Token::identifier)
      _symbolTable.checkForSymbol(_currToken);
    // Process combined types
    if (_currToken.getType() == Token::compound)
      procCombType();
    switch (_currToken.getType()) {
    case Token::identifier:
      // Burn parenthases around the identifier
      while ((_buffer.nextLookahead().getType() == Token::closeparen) &&
             (consParenCount > 0)) {
        _buffer.nextToken();
        consParenCount--;
      }
      if (_buffer.lastLookahead().getType() == Token::openparen)
        /* Function call. This terminates a function declaration, and marks
           the start of the initial value for a variable declaration */
        _statementType = expression;
      else {
        /* Variable name. For functions, this is a parameter name; for
           variable declarations, assume multiple vars are being declared in
           one declaration statement. Note: due to K+R style parameter
           declarations, function parameter declarations do not need to be
           whithin parentheses */
        _currToken.setType(Token::varname);
        if (haveFunction || (_braceCount > 0))
            _currToken.setScope(Token::localscope);
        else
            _currToken.setScope(Token::filescope);
        varNames.push(_currToken); // Add to variable list
        /* K+R variable declarations have a trailing semicolon, which must
           be burned also */
        if (haveFunction && (!insideParams) && // K+R style declaration
            (_buffer.lastLookahead().getType() == Token::semicolon))
            _buffer.nextToken();
      }
      break;

    case Token::openparen:
      parenCount++;
      break;

    case Token::closeparen:
      parenCount--;
      if (insideParams && (parenCount <= 0))
        insideParams = false;
      break;

    case Token::typedeftoken:
    case Token::statictoken:
      if (!insideParams) // Modifier on the entire declaration
        _parseStack.push(_currToken);
      break;

    case Token::ampersand:
    case Token::othersymbol:
      /* Have reached the initializer list for var declarations; this is an
         error for function declarations */
      if (haveFunction)
        _statementType = undet;
      else
        _statementType = expression;
      break;

    case Token::typetoken:
    case Token::declsymbol:
      // Ignore it
      break;

    case Token::fieldaccess:
      /* If inside params and have a dot, assume this is part of varags symbol
         (It is not common enough to have a seperate token for it) */
      if ((!insideParams) || (_currToken.getLexeme() != "."))
        _statementType = undet;
      // else part of varargs indicator, so ignore it
      break;

    default:
      // token is not allowed in declarations
      _statementType = undet;
    }
    // Update or clear consecutive parenthese count, as needed
    if (_currToken.getType() == Token::openparen)
      consParenCount++;
    else
      consParenCount = 0;
  }

  // Process the just read declaration
  if (haveFunction)
    procFunctDeclaration(declToken, _currToken, insideParams);
  else { // variable or type declaration
    // Set token type
    if (_parseStack.hasType(Token::typedeftoken))
      declToken.setType(Token::typetoken);
    else
      declToken.setType(Token::varname);

    // Set its scope
    if (_braceCount > 0)
      declToken.setScope(Token::localscope);
    else
      declToken.setScope(Token::filescope);
    _symbolTable.updateNameSpace(declToken);
  }

  /* add new variables to the namespace. Ignore the vars in a function
     declaration unless it is followed by an openbrace, as it is actually
     a prototype, not a declaration */
  if ((declToken.getType() == Token::varname) || (declToken.getType() == Token::functdecl))
    while (!varNames.empty())
      _symbolTable.updateNameSpace(varNames.pop());
  _readNextToken = false; // Need to process token that ended declaration
}

// Process a function declaration statement
void Parser::procFunctDeclaration(Token& declToken, const Token& nextToken,
                                  bool insideParams)
{
  // Find out if it is a typedef, a prototype, or an actual declaration
  /* Typedefs for function declarations are legal, but never used in practice.
     If declaration has a typedef token, treat it as a typedef only if it is
     completely legal to do so */
  if (_parseStack.hasType(Token::typedeftoken) &&
      (!_symbolTable.isKeyword(declToken)) && (_braceCount == 0))
    declToken.setType(Token::functtypedef);
  else if (nextToken.getType() == Token::openbrace)
    declToken.setType(Token::functdecl);
  else
    declToken.setType(Token::functproto);

  // Issue warning if declaration is not complete
  if (insideParams ||
      ((declToken.getType() != Token::functdecl) &&
       (nextToken.getType() != Token::semicolon))) {
    if (declToken.getType() == Token::functtypedef)
      logTokenError(cout, declToken, "Function type definition ",
                    " is incomplete");
    else if (declToken.getType() == Token::functdecl)
      logTokenError(cout, declToken, "Declaration of function ",
                    " is incomplete");
    else
      logTokenError(cout, declToken, "Prototype of function ",
                    " is incomplete");
  }

  // Set its scope
  if (_parseStack.hasType(Token::statictoken))
    declToken.setScope(Token::filescope);
  else
    declToken.setScope(Token::globalscope);

  // Issue warning if the declaration occurs inside another function
  if (_braceCount > 0) {
    // Typedefs are ignored if this problem exists
    if (declToken.getType() == Token::functdecl)
      logTokenError(cout, declToken, "Declaration of function ",
                    " occurs within another function");
    else
      logTokenError(cout, declToken, "Prototype of function ",
                    " occurs within another function");
  }
  // update the symbol table
  _symbolTable.updateNameSpace(declToken);

  // Return it if it is a genuine function declaration
  if (declToken.getType() == Token::functdecl)
    _functToken = declToken;
  _parseStack.clear();
}

// Finds the next function token in the file
void Parser::findNextFunction()
{
  int conParenCount = 0; // Number of consecutive open paranthese found
  Token tempToken;

  _functToken.setToNoToken(); // Clear last function
  while ((_functToken.getType() == Token::notoken) && (!_buffer.haveEOF())) {
    // Get next token to parse
    if (_readNextToken)
        _currToken = _buffer.nextToken();
    else {
        _readNextToken = true;
        _buffer.resetLookahead();
    }

    // Check for symbolic name
    if (_currToken.getType() == Token::identifier)
        _symbolTable.checkForSymbol(_currToken);

    // Process compound types
    if (_currToken.getType() == Token::compound)
        procCombType();

    // Now update state based on the token
    switch(_currToken.getType()) {
    case Token::ampersand:
        if (_parseStack.empty() ||
            (_parseStack.back().getType() == Token::openparen))
            // Refrence operator
            _parseStack.push(_currToken);
        // else bitwise AND operator, or an error. Ignore it
        break;

    case Token::fieldaccess:
        if (_statementType == expression) {
            if ((!_parseStack.empty()) &&
                (_parseStack.back().getType() == Token::ampersand))
                // Assume the struct name was left out
                _parseStack.pop();
            _parseStack.push(_currToken);
        }
        // Else symbol is in error; ignore it
        break;

    case Token::semicolon:
        /* Have either a new statement, or just finished part of a control
           statement with multiple parts */
        if (_statementType == constmt)
            // Stack is cleared before control is added, so it will be first
            tempToken = _parseStack.front();
        else
            tempToken.setToNoToken();
        newStatement();
        // If had a control statement, see if it still has args
        if ((tempToken.getType() == Token::control) &&
            (tempToken.getModifier() != Token::onearg)) {
            _statementType = constmt;
            // Drop the argument count by one, and push it back on the stack
            if (tempToken.getModifier() == Token::twoarg)
                tempToken.setModifier(Token::onearg);
            else
                tempToken.setModifier(Token::twoarg);
            _parseStack.push(tempToken);
            // Replace the following parenthese which was poped above
            /* NOTE: Assumes the open paren and semi-colon are on the same line, should be
                good enough in practice */
            _parseStack.push(Token('(', tempToken.getFilePosition(), Token::openparen));
        }
        break;

    case Token::openbrace:
        _braceCount++;
        newStatement();
        break;

    case Token::closebrace:
        if (_braceCount == 1) // About to pass from local scope to global
            _symbolTable.clearLocalNames();
        if (_braceCount > 0)
            _braceCount--;
        newStatement();
        break;

    case Token::openparen:
        /* Only expression statements can have a paranthese as the
           first token */
        if ((_statementType == undet) && (_parseStack.empty()))
            _statementType = expression;
        // Declarations don't care about where parens are, only their count
        if (_statementType != declaration)
            _parseStack.push(_currToken);
        conParenCount++;
        break;

    case Token::closeparen:
        if (_statementType != declaration) {
            // Find the matching open paranthese
            _parseStack.popTillType(Token::openparen);
            // If just finished a function call arglist, pop the functcall token
            if ((!_parseStack.empty()) &&
                (_parseStack.back().getType() == Token::functcall))
                _parseStack.pop();
            /* If last token is a control token, have just finshed a control
               statement */
            if ((!_parseStack.empty()) &&
                (_parseStack.back().getType() == Token::control)) {
                _statementType = undet;
                _parseStack.pop();
            }

            // If there is an operator on the stack, pop it
            if ((!_parseStack.empty()) &&
                ((_parseStack.back().getType() == Token::ampersand) ||
                 (_parseStack.back().getType() == Token::functcall)))
                _parseStack.pop();
        }
        break;

    case Token::declsymbol:
    case Token::othersymbol:
        /* Ignore it. If statement is a declaration, assume an othersymbol was
           inserted accidentally */
        break;

    case Token::literal:
        if (_statementType == undet)
            _statementType = expression;
        /* Else ignore it. If statement is a declaration, assume the literal
           was inserted accidentally */
        break;

    case Token::identifier:
        // Burn parentheses around the identifier
        while ((_buffer.nextLookahead().getType() == Token::closeparen) &&
               (conParenCount > 0)) {
            _buffer.nextToken();
            conParenCount--;
            if (_statementType != declaration)
                _parseStack.pop();
        }
        if (_statementType == declaration)
            procDeclaration();
        else { // Use of a variable or function
            if (_buffer.lastLookahead().getType() == Token::openparen) {
                // function call
                _currToken.setType(Token::functcall);
                // Scope was set when it was looked up in the symbol table

                // Find if a refrence was taken rather than actually calling it
                if ((!_parseStack.empty()) &&
                    (_parseStack.back().getType() == Token::ampersand))
                    _currToken.setModifier(Token::funcref);

                // Issue a warning if the call is refrenced from a struct
                if ((!_parseStack.empty()) &&
                    (_parseStack.back().getType() == Token::fieldaccess))
                    logTokenError(cout, _currToken, "Function call ",
                                  " is an element of a structured type");
            }
            else {
                _currToken.setType(Token::varname);
                // Set its scope
                if (_braceCount > 0)
                    _currToken.setScope(Token::localscope);
                else
                    _currToken.setScope(Token::filescope);
            }

            // Add it to the symbol table
            _symbolTable.updateNameSpace(_currToken);

            // Update the stack
            if ((!_parseStack.empty()) &&
                ((_parseStack.back().getType() == Token::fieldaccess) ||
                 (_parseStack.back().getType() == Token::ampersand)))
                _parseStack.pop();
            if (_statementType == undet)
                _statementType = expression;

            if (_currToken.getType() == Token::functcall) {
                // Push it on the stack so know to parse its arguments
                _parseStack.push(_currToken);
                /* Push the following parenthese so it is not included in the
                   consecutive parenthese count */
                _parseStack.push(_buffer.nextToken());
                // Set it to be returned by the object
                _functToken = _currToken;
            }
        }
        break;

    case Token::typedeftoken:
    case Token::statictoken:
        if (_statementType == undet)
            _statementType = declaration;
        if (_statementType == declaration)
            _parseStack.push(_currToken);
        break;

    case Token::typetoken:
        if (_statementType == undet)
            _statementType = declaration;
        break;

    case Token::functtypedef:
        /* A function declared through a previously defined type. Never seen in
           practice but allowed by the language */
        // Next token, ignoring parens, must be an identifier
        conParenCount = 0; // Going to clear it anyway, so use it here
        while (_buffer.nextLookahead().getType() == Token::openparen)
            conParenCount++;
        tempToken = _buffer.lastLookahead();
        if (tempToken.getType() == Token::identifier)
            _symbolTable.checkForSymbol(tempToken);
        if (tempToken.getType() == Token::identifier) {
            // It must have closing parens for each opening paren
            while ((_buffer.nextLookahead().getType() == Token::closeparen) &&
                   (conParenCount > 0))
                conParenCount--;
            if (conParenCount <= 0) {
                // Have an actual declaration. Burn the paren tokens and process it
                conParenCount = 0;
                _currToken = _buffer.nextToken();
                while (_currToken.getType() == Token::openparen) {
                    conParenCount++;
                    _currToken = _buffer.nextToken();
                }
                while (conParenCount > 0)
                    _buffer.nextToken();
                procFunctDeclaration(_currToken, _buffer.nextLookahead(),
                                     false);
            }
        }
        break;

    case Token::control:
        newStatement();
        _statementType = constmt;
        _parseStack.push(_currToken);
        // If the next token is not an open paren, assume it was left out
        if (_buffer.nextLookahead().getType() != Token::openparen)
            _parseStack.push(Token('(', _currToken.getFilePosition(), Token::openparen));
        break;

    case Token::reserved:
        newStatement();
        break;

    default:
        // Ignore anything else
        break;
    } // Switch statement on token type

    if (_buffer.haveEOF()) // Read last token as part of finding current function
        newStatement();
    // If not an open paranthese, clear the parenthese count
    if (_currToken.getType() != Token::openparen)
        conParenCount = 0;
  } // While don't have wanted token but still have tokens to process
}
