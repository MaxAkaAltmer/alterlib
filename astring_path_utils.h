/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2020 Maxim L. Grishin  (altmer@arts-union.ru)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*****************************************************************************/

#ifndef ASTRING_FS_UTILS_H
#define ASTRING_FS_UTILS_H

#include "astring.h"
#include "at_array.h"

class AStringPathParcer
{
public:
    AStringPathParcer(){_defSep='/';}
    AStringPathParcer(const AString &path){_defSep='/'; setPath(path);}
    ~AStringPathParcer(){}

    void setDefSep(char sep){_defSep=sep;}
    void setPath(const AString &path);

    static ATArray<AString> split(const AString &path);

    AString getExtension();
    AString getName();
    AString getBaseName();
    AString getNameNoExt();
    AString getDirectory();
    AString getPath(){return _path;}

    AString createRelativePath(AString to, bool this_is_dir = true);

private:

    AString _path;
    char _defSep;
};

#endif // ASTRING_FS_UTILS_H
