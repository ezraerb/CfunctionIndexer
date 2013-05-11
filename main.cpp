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
/* This program will list all function declarations and calls in a group of C
   program files. It will also report on missing prototypes, name collisions
   between functions and other C named elements, functions declared in the
   wrong scope, and other problems related to functions.
   The input files for the program must be run through a preprocessor first.
   If this not done, an error message is produced, and the results will be
   incorrect. To preprocess a file use: gcc -E filename.c > newfilename.i
   Files are assumed to be in the directory where the program is invoked. For
   any file which is not, the full path name must be specified */

#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<list>
#include<algorithm>
#include<set>
#include<map>
#include"basetypes.h"
#include"errors.h"
#include"filebuffer.h"
#include"tokenizer.h"
#include"namespace.h"
#include"parser.h"
#include"functfinder.h"

using std::cout;
using std::endl;
using std::vector;
using std::stringstream;

typedef vector<FunctionData> FuncDataVect;

int main(int argc, char* argv[])
{
  string fileName;
  int fileIndex;
  FuncDataVect functData;
  FuncDataVect::const_iterator functIndex;
  FunctFinder inputData;

  cout << endl;
  fileIndex = 1;
  if (argc <= 1)
    cout << "Must specify at least one file to process" << endl;
  else {
    while (fileIndex < argc) {
      try {
        fileName = argv[fileIndex];
        inputData.start(fileName);
        while (!inputData.haveEOF())
            functData.push_back(inputData.nextFunction());
      }
      catch (exception& error) {
        cout << "Processing file " << fileName << " stopped early due to error: "
             << error.what() << endl;
      }
      fileIndex++;
    }
    // Output the results
    if (functData.empty())
      cout << "No functions were found!" << endl;
    else {
      std::sort(functData.begin(), functData.end());
      cout << "Function name         scope               caller                source          line"
           << endl;
      for (functIndex = functData.begin(); functIndex != functData.end();
        functIndex++)
      cout << *functIndex;
    }
  }
}
