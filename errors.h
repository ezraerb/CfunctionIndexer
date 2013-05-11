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
// Define throwable exceptions, and warning methods
#include<exception>

using std::exception;

// Logs a token for error reporting purposes
void logTokenError(ostream& stream, const Token& token, const string& leadText,
                   const string& trailText);

// Exception for missing file
class NoSuchFileException: public exception
{
  private:
    /* Compilers need to know the size of the array at compile time. This is
        the standard technique for creating a compile-time constant */
    enum { _MessageSize = 41 };
    enum { _FileNameSize = 20 };

  /* Exceptions can not throw exceptions themselves, so all data needs to be
    on the stack */
  char _message[_MessageSize];

 public:
  NoSuchFileException(const string &fileName);
  virtual const char* what() const throw();
};

inline const char* NoSuchFileException::what() const throw()
{
    return _message;
}

class DouFuncRelException: public exception
{
 private:
    /* Compilers need to know the size of the array at compile time. This is
        the standard technique for creating a compile-time constant */
    enum { _MessageSize = 55 };

  /* Exceptions can not throw exceptions themselves, so all data needs to be
    on the stack */
  char _message[_MessageSize];
 public:
  DouFuncRelException();
  virtual const char* what() const throw();
};

inline const char* DouFuncRelException::what() const throw()
{
    return _message;
}
