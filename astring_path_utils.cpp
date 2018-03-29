/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2018 Maxim L. Grishin  (altmer@arts-union.ru)

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

#include "astring_path_utils.h"

void AStringPathParcer::setPath(const AString &path)
{
    char sep=_defSep;
    for(int i=0;i<path.size();i++)
    {
        if(path[i]=='/' || path[i]=='\\')
        {
            sep=path[i];
            break;
        }
    }

    ATArray<AString> list=split(path);
    ATArray<AString> rez;
    if(list.size())rez.append(list[0]);
    for(int i=1;i<list.size();i++)
    {
        if(list[i]=="..")
        {
            if(rez.size() && rez.last()!="..")
            {
                rez.pop();
                continue;
            }
            else
            {
                rez.append(list[i]);
            }
        }
        else if(list[i]==".")
        {
            if(rez.size())
            {
                continue;
            }
            else
            {
                rez.append(list[i]);
            }
        }
        else if(list[i].isEmpty())
        {
            continue;
        }
        else
        {
            rez.append(list[i]);
        }
    }
    _path=AString::join(rez,sep);
}

ATArray<AString> AStringPathParcer::split(const AString &path)
{
    ATArray<AString> rv;
    AString tmp;
    for(int i=0;i<path.size();i++)
    {
        if(path[i]=='/' || path[i]=='\\')
        {
            rv.append(tmp);
            tmp.clear();
        }
        else
        {
            tmp.append(path[i]);
        }
    }
    if(!tmp.isEmpty())rv.append(tmp);
    return rv;
}

AString AStringPathParcer::getExtension()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return AString();
        if(_path[i]=='.')return _path.right(i+1);
    }
    return AString();
}

AString AStringPathParcer::getName()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return _path.right(i+1);
    }
    return _path;
}

AString AStringPathParcer::getBaseName()
{
    AString tmp=getName();
    for(int i=0;i<tmp.size();i++)
    {
        if(tmp[i]=='.')return tmp.left(i);
    }
    return tmp;
}

AString AStringPathParcer::getNameNoExt()
{
    AString tmp=getName();
    for(int i=tmp.size();i>=0;i--)
    {
        if(tmp[i]=='.')return tmp.left(i);
    }
    return tmp;
}

AString AStringPathParcer::getDirectory()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return _path.left(i);
    }
    return AString();
}

