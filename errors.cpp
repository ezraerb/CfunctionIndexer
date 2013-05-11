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
#include<iostream>
#include<string>
#include<vector>
#include<cstring>
#include"basetypes.h"
#include"errors.h"

using std::string;
using std::cerr;
using std::endl;
using std::strncpy;

// Error for a missing file
NoSuchFileException::NoSuchFileException(const string &fileName)
{
  strncpy(_message, "Could not open file ", _MessageSize - _FileNameSize - 1);
  /* If the file name (which may include the full path) is smaller than
    the space available, just copy it. Otherwise, try to remove the path
    and just use the filename. Note that a standard string substring can't
    be used here, because it alloates memory which in turn could cause an
    exception */
  if (fileName.length() < _FileNameSize)
    strncat(_message, fileName.c_str(), _FileNameSize);
  else {
    unsigned int startPos = fileName.find_last_of('/');
    if (startPos == string::npos)
        startPos = 0; // No directory information
    else if ((fileName.length() - startPos) < _FileNameSize)
        // File name less than space available, copy some of the path also
        startPos = fileName.length() - _FileNameSize - 1; // One char for NUL
    else
        startPos++; // Don't include the slash
    // NOTE: Hacky C style pointer arithmetric, needed by the function signature
    strncat(_message, fileName.c_str() + startPos, _FileNameSize);
  } // File name too long
  _message[_MessageSize - 1] = '\0'; // Ensure string is terminated
}

// Error for trying to process tokens when previous function data should be processed instead
DouFuncRelException::DouFuncRelException()
{
  strncpy(_message, "Internal error, double release of held function tokens", _MessageSize - 1);
  _message[_MessageSize - 1] = '\0'; // Ensure string is terminated
}

// Logs a token for error reporting purposes
void logTokenError(ostream& stream, const Token& token, const string& leadText,
                   const string& trailText)
{
  stream << "WARNING: " << leadText << token.getLexeme() << " found "
         << token.getFilePosition() << trailText << endl;
}
