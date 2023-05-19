/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2023 Maxim L. Grishin  (altmer@arts-union.ru)

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

#include "aeditory.h"

using namespace alt;

editory::editory()
{

}

editory::~editory()
{

}

string editory::getLineCommentKey(intx line)
{
    editory::edLineData *el = ed_line(line);
    if(!el) return string();

    int width_count=0,last_width=0,currx=0;

    string key;

    for(int i=0;i<el->words.size();i++)
    {
        if(el->words[i].left().isInt() || el->words[i].left().isReal()) //пробелы
        {
            intx count=el->words[i].left().toInt();
            if(count<0)
            {
                if(i)
                {
                    if(el->words[i].left().isReal())
                        count=-count-width_count;
                    else count=-count-last_width;
                }
                if(count<=0)count=1;
            }

            currx+=count;
            width_count+=count;
            last_width=count;

            for(int j=0; j<count;j++) key+=" ";
        }
        else if(el->words[i].left().isString()) //строки
        {
            string str=el->words[i].left().toString();
            int len = str.unicodeSize();

            currx+=len;
            width_count+=len;
            last_width=len;

            key += str;
        }
        else
        {
            return string();
        }
    }
    return key.simplified();
}
