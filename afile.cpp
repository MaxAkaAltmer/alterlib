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

#include "afile.h"

#if defined(_MSC_VER)
    #include "external/dirent.h"
#else
    #include <unistd.h>
    #include <dirent.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#if !defined(linux) && !defined(__APPLE__)
    #include <io.h>
#endif
#include <fcntl.h>

#if defined(_MSC_VER)
    /* Values for the second argument to access.
       These may be OR'd together.  */
    #define R_OK    4       /* Test for read permission.  */
    #define W_OK    2       /* Test for write permission.  */
    #define X_OK    R_OK    /* execute permission - unsupported in Windows,
                               use R_OK instead. */
    #define F_OK    0       /* Test for existence.  */

    #define wstat _wstat
    #define lseek64 _lseeki64
    #define stat _stat
    #define ftruncate64 _chsize_s
#endif

using namespace alt;

void pathParcer::setPath(const string &path)
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

    array<string> list=split(path);
    array<string> rez;
    if(list.size())
    {
        rez.append(list[0]);
    }
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
    _path=string::join(rez,sep);
}

array<string> pathParcer::split(const string &path, bool scip_empty)
{
    array<string> rv;
    string tmp;
    for(int i=0;i<path.size();i++)
    {
        if(path[i]=='/' || path[i]=='\\')
        {
            if(!scip_empty || !tmp.isEmpty())
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

string pathParcer::getExtension()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return string();
        if(_path[i]=='.')return _path.right(i+1);
    }
    return string();
}

string pathParcer::getName()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return _path.right(i+1);
    }
    return _path;
}

string pathParcer::getBaseName()
{
    string tmp=getName();
    for(int i=0;i<tmp.size();i++)
    {
        if(tmp[i]=='.')return tmp.left(i);
    }
    return tmp;
}

string pathParcer::getNameNoExt()
{
    string tmp=getName();
    for(int i=tmp.size();i>=0;i--)
    {
        if(tmp[i]=='.')return tmp.left(i);
    }
    return tmp;
}

string pathParcer::getDirectory()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return _path.left(i);
    }
    return string();
}

string pathParcer::createAbsolutePath(string to, bool this_is_dir)
{
    array<string> curr = split(_path,true);
    array<string> dest = split(to,true);
    int ind=0;

    if(to[0]!='.')
        return to;

    if(!this_is_dir)
        curr.pop();

    curr.append(dest);

    for(int i=0; i<curr.size(); i++)
    {
        if(curr[i]==".")
        {
            curr.cut(i);
            i++;
        }
        else if(curr[i]=="..")
        {
            if(!i) return "";
            curr.cut(i-1,2);
            i-=2;
        }
    }
    return string::join(curr,_defSep);
}

string pathParcer::createRelativePath(string to, bool this_is_dir)
{
    array<string> curr = split(_path,true);
    array<string> dest = split(to,true);
    int ind=0;

    if(!curr.size())
        return to;

    if(!this_is_dir)
        curr.pop();

    for(;ind<curr.size() && ind<dest.size();ind++)
    {
        if(curr[ind]!=dest[ind])
            break;
    }

    if(ind==0)
        return to;

    string rv = ".";
    rv.append(_defSep);
    if(ind < curr.size())
    {
        rv.clear();
        for(int i = ind; i<curr.size(); i++)
        {
            rv += "..";
            rv.append(_defSep);
        }
    }
    for(int i = ind; i<dest.size(); i++)
    {
        rv+=dest[i];
    }
    return rv;
}

bool dirIsWriteble(string path)
{
    if(path.last()!='/')path+="/";
    int testInd=0;
    string fname=path+"test.txt";
    while(file::exists(fname))
    {
        fname=path+"test"+string::fromInt(testInd++)+".txt";
    }
    file hand(fname);
    if(hand.open(file::OWriteOnly))
    {
        if(hand.write(fname(),fname.size())==fname.size())
        {
            hand.close();
            if(hand.open(file::OReadOnly))
            {
                if(hand.readText()==fname)
                {
                    hand.close();
                    file::remove(fname);
                    return true;
                }
            }
        }
        hand.close();
        file::remove(fname);
    }
    return false;
}

array<string> dirEntryList(const string &path, alt::set<string> extFilter, int type)
{
    array<string> rv;
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> upath=path.toUnicode();
    _WDIR *dir;
    struct _wdirent *drnt;
    dir = _wopendir(upath());
    while (dir && (drnt = _wreaddir(dir)) != NULL)
    {
        string node=string::fromUnicode(drnt->d_name);
        if(node=="." || node=="..")continue;
        if(type)
        {
            struct stat _Stat;
            array<wchar_t> sub_name=(path+"/"+node).toUnicode();
            wstat(sub_name(),&_Stat);
            if(type<0 && S_ISREG(_Stat.st_mode))continue;
            if(type>0 && !S_ISREG(_Stat.st_mode))continue;
        }
        if(extFilter.size())
        {
            int ind = node.findBackChar('.');
            if(ind<0)continue;
            string ext=node.right(ind+1).toLower();
            if(!extFilter.contains(ext))continue;
        }
        rv.append(node);
    }
#else
    DIR *dir;
    struct dirent *drnt;
    dir = opendir(path());
    while (dir && (drnt = readdir(dir)) != NULL)
    {
        string node(drnt->d_name);
        if(node=="." || node=="..")continue;
        if(type)
        {
            if(type>0 && drnt->d_type!=DT_REG)continue;
            else if(type<0 && drnt->d_type==DT_REG)continue;
        }
        if(extFilter.size())
        {
            int ind = node.findBackChar('.');
            if(ind<0)continue;
            string ext=node.right(ind+1).toLower();
            if(!extFilter.contains(ext))continue;
        }
        rv.append(node);
    }
#endif
    return rv;
}

file::file()
{
    handler = -1;
}

file::file(const string &name)
{
    fname = name;
    handler = -1;
}

file::~file()
{
    close();
}

bool file::setFileName(const string &name)
{
    if(open_flags)return false;
    fname=name;
    return true;
}

void file::close()
{
    if(handler>=0)
    {
        ::close(handler);
        handler = -1;
    }
    fileProto::close();
}

bool file::open(int flags)
{
    if(fname.isEmpty())return false;
    if(!flags)return false;

    int oflags=0;
#ifdef O_LARGEFILE
    oflags|=O_LARGEFILE;
#endif
#ifdef O_BINARY
    oflags|=O_BINARY;
#endif
    if((flags&OReadOnly) && (flags&OWriteOnly))oflags|=O_RDWR;
    else if(flags&OReadOnly)oflags|=O_RDONLY;
    else if(flags&OWriteOnly)oflags|=O_WRONLY;
    if(flags&OAppend)oflags|=O_APPEND;
    if(flags&OTruncate)oflags|=O_TRUNC;

#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> wname=fname.toUnicode();
    int hand=::_wopen(wname(),oflags);

    if(hand<0 && (flags&OWriteOnly))
    {
#ifdef _MSC_VER
        int pmode = 0;
        if((flags&OReadOnly) && (flags&OWriteOnly))
            pmode|=_S_IREAD|_S_IWRITE;
        else if(flags&OReadOnly)
            return false;
        else if(flags&OWriteOnly)
            pmode|=_S_IWRITE;
        hand=::_wcreat(wname(),pmode);
#else
        hand=::_wcreat(wname(),00666);
#endif
        if(hand>=0)
        {
            ::close(hand);
            hand=::_wopen(wname(),oflags);
        }
    }
#else
    int hand=::open(fname(),oflags);

    if(hand<0 && (flags&OWriteOnly))
    {
        hand=::creat(fname(),00666);
        if(hand>=0)
        {
            ::close(hand);
            hand=::open(fname(),oflags);
        }
    }
#endif

    if(hand<0)return false;

    open_flags = flags;
    handler=hand;

    return true;
}

int64 file::size() const
{
    if(handler<0)return -1;

    int64 oldpos=pos();
#if !defined(__APPLE__)
    ::lseek64(handler,0,SEEK_END);
#else
    ::lseek(handler,0,SEEK_END);
#endif
    int64 rv=pos();
#if !defined(__APPLE__)
    ::lseek64(handler,oldpos,SEEK_SET);
#else
    ::lseek(handler,oldpos,SEEK_SET);
#endif
    return rv;
}

bool file::seek(int64 pos)
{
    if(handler<0)return false;
#if !defined(__APPLE__)
    return ::lseek64(handler,pos,SEEK_SET)==pos;
#else
    return ::lseek(handler,pos,SEEK_SET)==pos;
#endif
}

int64 file::pos() const
{
    if(handler<0)return -1;
#if !defined(__APPLE__)
    return ::lseek64(handler,0,SEEK_CUR);
#else
    return ::lseek(handler,0,SEEK_CUR);
#endif
}

int file::read_hand(void *buff, int size)
{
    if(handler<0)return -1;
    if(size<=0)return 0;
    int rv = ::read(handler,buff,size);

    return rv;
}

int file::write_hand(const void *buff, int size)
{
    if(handler<0)return -1;
    if(size<=0)return 0;
    return ::write(handler,buff,size);
}

bool file::resize(int64 size)
{
    if(handler<0)return false;
#if !defined(__APPLE__)
    if (::ftruncate64(handler, size) != 0) return false;
#else
    if (::ftruncate(handler, size) != 0) return false;
#endif
    return true;
}

bool file::isSequential() const
{
    return false;
}

bool file::exists(string fname)
{
    if(fname.isEmpty())return false;
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> wname=fname.toUnicode();
    if(_waccess(wname(),F_OK)==-1)return false;
#else
    if(access(fname(),F_OK)==-1)return false;
#endif
    return true;
}


bool file::remove(string fname)
{
    if(fname.isEmpty())return false;
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> wname=fname.toUnicode();
    if(_wunlink(wname())==-1)return false;
#else
    if(unlink(fname())==-1)return false;
#endif
    return true;
}

bool file::rename(string before, string after)
{
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> bname=before.toUnicode();
    array<wchar_t> aname=after.toUnicode();
    if(!(_wrename(bname(),aname())))return true;
#else
    if(!(::rename(before(),after())))return true;
#endif
    return false;
}

alt::time file::changeTime(string fname)
{
    struct stat t_stat;
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> wname=fname.toUnicode();
    wstat(wname(), &t_stat);
#else
    stat(fname(), &t_stat);
#endif
    if(sizeof(t_stat.st_mtime)==8)
        return alt::time(((uint64)t_stat.st_mtime)*(uint64)1000000);
    return alt::time(((uint32)t_stat.st_mtime)*(uint64)1000000);
}

bool file::replicate(string src, string dst)
{
    file hsrc(src);
    file hdst(dst);
    if(!hsrc.open())return false;
    if(!hdst.create())return false;

    int64 total=hsrc.size();
    if(hdst.copy(&hsrc,total)!=total)return false;
    return true;
}
