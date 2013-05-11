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
// filebuffer.cpp Low level file access

#include<fstream>
#include<iostream>
#include<string>
#include<vector>
#include<cstdlib>
#include"basetypes.h"
#include"filebuffer.h"
#include"errors.h"

using std::ifstream;
using std::string;
using std::atoi;
using std::cout;
using std::endl;

FileBuffer::FileBuffer()
    : _sourcePosition("", 0), _bufferPosition("", 0),
      _inputPosition("", 0) // No file yet
{
    resetVars();
}

// Destructor.
FileBuffer::~FileBuffer()
{
    close();
}

/* Opens the given file. Second parameter tells whether to search system dirs
   Throws a file not found exception if can't find the file */
void FileBuffer::open(string fileName)
{
  // Close old file (if any) before attempting to process a new one
  close();
  _file.open(fileName.c_str());
  if (!_file.is_open())
    throw NoSuchFileException(fileName);
  else {
    _sourcePosition = FilePosition(fileName, 0);
    _bufferPosition = _sourcePosition;
    _inputPosition = _sourcePosition;
    _buffer.clear();
    // Load first buffer, fetch is a lookahead
    fetchNextLine();
  }
}

// Reads the next line to tokenize from the file.
void FileBuffer::fetchNextLine()
{
  /* For this routine, the file consists of four things:
     comments, quoted strings, preprocessor commands, and
     other text. A given character in the file falls in only
     one category, so the obvious design is a state machine.

     Each category except 'other text' is signaled by a pair
     of strings. One of the pair starts the category and
     another ends it. Searching for these gives the state of
     a given part of the file, with 'other text' occurring by
     default between them. Keep in mind that a given category
     may cover multiple lines, a condition called wrap. This
     hsa special handling depending on the category */
  TextState nextState = other;
  string fileDataLine; // Actual data from the file
  unsigned int start; // Start of next group of chars to process
  unsigned int end; // End of next group of chars to process

  _buffer.clear();
  while (_buffer.empty() && (!_file.eof())) {
    // load another line from the file and process it
    getline(_file, fileDataLine);
    _bufferPosition.incrLine();
    _inputPosition.incrLine();
    start = 0;
    end = 0;
    /* If the current status is 'other', this line
        may be a preprocessor line. It's signaled by the first
        non-space being a hash */
    if (_currState == other) {
        unsigned int firstChar = burnSpaces(fileDataLine);
        if (firstChar != string::npos)
            if (fileDataLine.at(firstChar) == '#') {
                _currState = preproc;
                _haveWrap = false; // Other state may have carried from previous line
            }
    } // New line not part of wrapped comment or string litteral
    while (end != string::npos) {
        start = end; // New entity begins where last one ends
        switch (_currState) {
        case comment:
            /* Find the location of the close comment. If the
                comment did not wrap, need to skip over the
                '/*' that indicates the start of the comment */
            if (!_haveWrap)
                end += 2;
            end = fileDataLine.find("*/", end);
            _haveWrap = (end == string::npos); // Comment wraps
            if (!_haveWrap) {
                end+=2; /* Search routine returns first char of close comment,
                            need first NON-comment char */
                nextState = other; // Default
            }
            /* Comments are burned. C compilers convert the entire thing
                into a single space, so this code does as well */
            _buffer += " ";
            break;
        case quote:
            /* Find the location of the close quote. If the quoted string
                did not wrap, need to skip over the opening quote */
            if (!_haveWrap)
                end += 1;
            end = nextCloseQuote(fileDataLine, end);
            _haveWrap = (end == string::npos);
            if (_haveWrap) {
                _buffer.append(fileDataLine, start, string::npos);
                /* If don't have an escaped return at the end of the string,
                    either the quote or the escape was left out. GCC assumes the
                    latter, so this code does too. */
                if (!hasEscNewline(_buffer, true)) {
                    cout << "WARNING: Unterminated string literal found at "
                         << _bufferPosition << endl;
                    // Insert a backslash into the output. Note that it must be escaped
                    _buffer.append("\\");
                } // Multi-line quote without escaped newline at end
            } // Quoted string does not end with a quote
            else {
                end++; // Search routine returns last char of comment, need one beyond it
                _buffer.append(fileDataLine, start, end - start);
                nextState = other; // Default
            }
            break;

        case preproc:
            /* Preprocs are complicated enough to need their own routine. The important
                result here is that they never end up in the output, and if they don't
                wrap the next state is the default */
            handlePreproc(fileDataLine);
            if (!_haveWrap) // Preproc did not wrap
                nextState = other;
            end = string::npos; // The entire preproc line is processed at once
            break;

        case other:
            /* Find the location of the next commment and the next quoted string.
                The section ends at the earlier of the two. If it contains at least
                one char, copy them over */
            _haveWrap = false;
            unsigned int nextQuote = nextOpenQuote(fileDataLine, start);
            unsigned int nextComment = fileDataLine.find("/*", start);
            if ((nextQuote == string::npos) &&
                (nextComment == string::npos)) {
                end = string::npos;
                _haveWrap = true;
            }
            else if ((nextQuote == string::npos) ||
                     (nextComment < nextQuote)) {
                end = nextComment;
                nextState = comment;
            }
            else {
                end = nextQuote;
                nextState = quote;
            }
            if (_haveWrap)
                // Result is from start to the end of the fileDataLine
                _buffer.append(fileDataLine, start, string::npos);
            else if (start < end)
                _buffer.append(fileDataLine, start, end - start);
            break;
        } // Switch on current state

        // Find next state
        if (!_haveWrap)
            _currState = nextState;

        // If processed entire fileDataLine, force termination
        if (end >= fileDataLine.size())
            end = string::npos;
    } // While text in fileDataLine to process

    /* If the result contains nothing but whitespace, ignore it. If it is
         not part of a quoted string and contains only spaces and an escaped
         newline, also ignore it
         TRICKY NOTE: A quoted string of all whitespace must contain
         at least one character, either a quote or a backslash in front
         of the line end */
    unsigned int testChar = burnSpaces(_buffer);
    if ((testChar == string::npos) ||
        ((testChar == getEscNewline(_buffer, false)) &&
         ((!_haveWrap) || (_currState != quote))))
        _buffer.clear();
    } // While not found data to process and data still in the file
}

// Handle preprocessor comamnds in the input
void FileBuffer::handlePreproc(string fileDataLine)
{
    /*Thanks to the preprocessor step, the location of text in
        the input file rarely matches that in the source file, but
        locations should refer to source. The preprocessor handles
        this by inserting source file locations into its output.
        These consist of a hash, a number, and the file name in
        quotes. Hunt for them here and update the source file
        location accordingly

        Anything else starting with a hash is an actual
        preprocessor command. The source code should be run
        through the preprocessor before calling this routine,
        so finding one is an error. Issue a warning and ignore
        it */

    // Search for the location by trying to extract it.
    bool haveLocation = false;
    unsigned int lineNo = 0;
    string fileName;
    bool wasWrapped = _haveWrap; // Wrapped from previous line
    _haveWrap = hasEscNewline(fileDataLine, false); // Wraps to next line
    // Locations never wrap
    if ((!wasWrapped) && (!_haveWrap)) {
        // Find the line number
        unsigned short start = 0;
        unsigned short end = 0;
        start = fileDataLine.find_first_of('#');
        start = burnSpaces(fileDataLine, start + 1); // Actual text of command
        if (start != string::npos) { // Something on line other than hash
            if (isdigit(fileDataLine[start])) {
                // Find first non digit and extract line number
                end = fileDataLine.find_first_not_of("0123456789", start);
                if (end != string::npos) { // Something after the digits
                    /* NOTE: This is not the "official" way of extracting
                        an integer from a string, but it is the fastest */
                    lineNo = atoi(fileDataLine.substr(start, end-start).c_str());
                    /* The line gives the location of the next source line. Since reading
                        that line will increment the line counter, need to decrement it here
                        to compensate */
                    lineNo--;

                    // Find the first non-space after the digits. Must be a quote
                    start = burnSpaces(fileDataLine, end);
                    if (start != string::npos) { // Found more data
                        if (fileDataLine[start] == '"') {
                            start++;
                            // Find next quote and extract file name
                            end = fileDataLine.find_first_of('"', start);
                            if (end != string::npos) {
                                // Filename with no chars is illegal
                                if (end > start) {
                                    fileName = fileDataLine.substr(start, end - start);
                                    /* Find the first non-space after the just extracted
                                        file name. If any exist, it's not a location */
                                    end++;
                                    if (end != fileDataLine.length()) {
                                        end = burnSpaces(fileDataLine, end);
                                        haveLocation = (end == string::npos);
                                    } // Quote not last char on the line
                                    else
                                        haveLocation = true;
                                    if (haveLocation)
                                        _bufferPosition = FilePosition(fileName, lineNo);
                                } // Have at least one char between quotes
                            } // Quote at end of filename exists
                        } // Quote at start of filename exists
                    } // Have non-space after the line number
                } // More chars exist after the line number
            } // First thing after hash is a digit
        } // Have something after the hash
    } // Preprocessor command did not wrap

    /* If a file location was no found, this is an actual preprocessor command
        which indicates a processing error */
    if ((!haveLocation) && (!wasWrapped))
        cout << "WARNING: Preprocessor directive " << fileDataLine << " ignored on "
             << _inputPosition << ". Must g++ -E source files before calling" << endl;
}

// Returns the start of the next quoted string
unsigned int FileBuffer::nextOpenQuote(const string& buffer, unsigned int startPos)
{
  unsigned int pos = startPos;
  bool haveQuote = false;
  while ((!haveQuote) && (pos != string::npos)) {
    pos = buffer.find_first_of('"', pos);
    if (pos != string::npos) {
      if (((pos == 0) || (buffer[pos-1] != '\'')) &&
          ((pos == (buffer.length() - 1)) || (buffer[pos+1] != '\'')))
        haveQuote = true;
      else
        pos++; /* move off char for next search */
    }
  }
  return pos;
}

// Returns the end of the current quoted string
unsigned int FileBuffer::nextCloseQuote(const string& buffer, unsigned int startPos)
{
  unsigned int pos = startPos;
  bool haveQuote = false;
  while ((!haveQuote) && (pos != string::npos)) {
    pos = buffer.find_first_of('"', pos);
    if (pos != string::npos) {
      if ((pos == 0) || (buffer[pos-1] != '\\'))
        haveQuote = true;
      else
        pos++; /* move off char for next search */
    }
  }
  return pos;
}

/* If this line ends with an escaped newline, returns the position of the
   escape char, otherwise returns string::npos */
unsigned int FileBuffer::getEscNewline(const string& buffer,
                                       bool multiLineQuote)
{
  /* An escaped newline is a backslash as the last char on the line.
     A common program mistake is to put spaces after the backslash, so
     this code burns trailing spaces before looking for the backslash.
     An escaped space is not a legal symbol. */
  unsigned int index = buffer.find_last_not_of(" \t");
  bool haveEscNewLine = false;
  if (index != string::npos) { // Not all spaces
    if (buffer[index] == '\\') {
        // Last non-space is an escape
        // NOTE: Need to escape the escape to get a litteral '\'
        /* In quoted strings, certain chars are escaped to insert them
            literally. The backslash is one of them. Must make sure a
            backslash is for an escape, not a literal backslash */
        if (!multiLineQuote)
            haveEscNewLine = true;
        else {
            /* Find the number of consecutive backslashes. If ite even,
                all of them are literal backslashes and the newline
                is NOT esacped */
            unsigned int testPos = buffer.find_last_not_of('\\', index);
            if (testPos == string::npos) // Entire string is backslashes
                // Remember that the buffer is indexed from zero
                haveEscNewLine = (((index + 1) % 2) == 1);
            else
                haveEscNewLine = (((index - testPos) % 2) == 1);
        } // Quoted string
    } // Last non-space is a backslash
  } // String has something in it

  if (haveEscNewLine)
    return index;
  else
    return string::npos;
}
