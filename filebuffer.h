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
// filebuffer.h The object to access a C file
using std::ifstream;

/* This object does the lowest level of text processing. It reads lines from the
   file, eliminates comments, and handles preprocesor output commands. Most of this
   program cares where something appears in the source file, which is not the same
   as its position in the preprocessor output, so need to track both.

   To set EOF properly, this class actually reads the file in advance with a buffer.
   When read, it returns the current buffer contents and then refills it */
class FileBuffer
{
 private:
  typedef enum { comment, quote, preproc, other } TextState;

  ifstream _file;
  FilePosition _sourcePosition; // Position of last returned contents in original source files
  FilePosition _bufferPosition; // Position represented by current buffer contents
  FilePosition _inputPosition; // Position in preprocessor output file
  string _buffer;

  TextState _currState; // Type of text being processed
  bool _haveWrap; // Text state continued from previous line

  // Copy constructor and equality operator. This object can't be copied
  FileBuffer(const FileBuffer& other);
  const FileBuffer& operator=(const FileBuffer& other);

  // Sets FileBuffer vars to their initial values
  void resetVars();

  // Reads the next line to tokenize from the file.
  void fetchNextLine();

  // Handle preprocessor comamnds in the input
  void handlePreproc(string buffer);

   // Returns the start of the next quoted string
  unsigned int nextOpenQuote(const string& buffer, unsigned int startPos);

public:
  FileBuffer();

  // Destructor.
  ~FileBuffer();

  // Opens the given file, which must be in the current directory.
  void open(string fileName);

  // Closes the filebuffer
  void close();

  // Reads a processed line from the file
  FileBuffer& operator>>(string &result);

  // Returns true if at EOF
  bool haveEOF() const;

  // Return the position data for the most recently read line of the source
  const FilePosition& getFilePosition() const;

  // Returns the end of the current quoted string
  static unsigned int nextCloseQuote(const string& buffer,
                                     unsigned int startPos);

  // Returns true if the final char in the string is an escaped newline char
  static bool hasEscNewline(const string& buffer,
                            bool multiLineQuote);

  /* If this line ends with an escaped newline, returns the position of the
     escape char, otherwise returns string::npos */
  static unsigned int getEscNewline(const string& buffer,
                                    bool multiLineQuote);

  // Burns all spaces after the given position, and returns the position afterwards
  static unsigned int burnSpaces(const string& buffer,
                                 unsigned int startPos = 0);
};

// Set a filebuffer to its initial state
inline void FileBuffer::resetVars()
{
  _sourcePosition = FilePosition("", 0);
  _inputPosition = _sourcePosition;
  _bufferPosition = _sourcePosition;
  _currState = other;
  _haveWrap = false;
}

// Closes the filebuffer
inline void FileBuffer::close()
{
  if (_file.is_open())
    _file.close();
  resetVars();
}

// Returns true if at EOF
inline bool FileBuffer::haveEOF() const
{
    /* At end of file when last line is read AND the buffer
        has been returned, signaled by it being empty */
    return (_file.eof() && _buffer.empty());
}

// Reads the next line to tokenize from the file.
inline FileBuffer& FileBuffer::operator>> (string& result)
{
    /* To set EOF properly, must do this as a look-ahead.
        Return the last line found, search for the next one,
        anc cache the result for the next call. This will change
        the file position data, so need to cache that as well so
        it will match the location of the returned data */
    result = _buffer;
    _sourcePosition = _bufferPosition;
    fetchNextLine();
    return *this;
}

// Return the position data for the most recently read line of the source
inline const FilePosition& FileBuffer::getFilePosition() const
{
    return _sourcePosition;
}

// Burns all spaces after the given position, and returns the position afterwards
inline unsigned int FileBuffer::burnSpaces(const string& buffer,
                                           unsigned int startPos)
{
    // NOTE: \t is the tab character
    return buffer.find_first_not_of(" \t", startPos);
}

// Returns true if the final char in the string is an escaped newline char
inline bool FileBuffer::hasEscNewline(const string& buffer, bool multiLineQuote)
{
    return (getEscNewline(buffer, multiLineQuote) != string::npos);
}
