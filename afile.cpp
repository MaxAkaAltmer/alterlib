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

#include "afile.h"

#if defined(_MSC_VER)
    #include "external/dirent.h"
#else
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


bool aDirIsWriteble(AString path)
{
    if(path.last()!='/')path+="/";
    int testInd=0;
    AString fname=path+"test.txt";
    while(AFile::exists(fname))
    {
        fname=path+"test"+AString::fromInt(testInd++)+".txt";
    }
    AFile hand(fname);
    if(hand.open(AFile::OWriteOnly))
    {
        if(hand.write(fname(),fname.size())==fname.size())
        {
            hand.close();
            if(hand.open(AFile::OReadOnly))
            {
                if(hand.readText()==fname)
                {
                    hand.close();
                    AFile::remove(fname);
                    return true;
                }
            }
        }
        hand.close();
        AFile::remove(fname);
    }
    return false;
}

ATArray<AString> aDirEntryList(const AString &path, ATSet<AString> extFilter, int type)
{
    ATArray<AString> rv;
#if !defined(linux) && !defined(__APPLE__)
    ATArray<wchar_t> upath=path.toUnicode();
    _WDIR *dir;
    struct _wdirent *drnt;
    dir = _wopendir(upath());
    while (dir && (drnt = _wreaddir(dir)) != NULL)
    {
        AString node=AString::fromUnicode(drnt->d_name);
        if(node=="." || node=="..")continue;
        if(type)
        {
            struct stat _Stat;
            ATArray<wchar_t> sub_name=(path+"/"+node).toUnicode();
            wstat(sub_name(),&_Stat);
            if(type<0 && S_ISREG(_Stat.st_mode))continue;
            if(type>0 && !S_ISREG(_Stat.st_mode))continue;
        }
        if(extFilter.size())
        {
            int ind = node.findBackChar('.');
            if(ind<0)continue;
            AString ext=node.right(ind+1).toLower();
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
        AString node(drnt->d_name);
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
            AString ext=node.right(ind+1).toLower();
            if(!extFilter.contains(ext))continue;
        }
        rv.append(node);
    }
#endif
    return rv;
}

AFile::AFile()
{
    handler = -1;
}

AFile::AFile(const AString &name)
{
    fname = name;
    handler = -1;
}

AFile::~AFile()
{
    close();
}

bool AFile::setFileName(const AString &name)
{
    if(open_flags)return false;
    fname=name;
    return true;
}

void AFile::close()
{
    if(handler>=0)
    {
        ::close(handler);
        handler = -1;
    }
    AFileProto::close();
}

bool AFile::open(int flags)
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
    ATArray<wchar_t> wname=fname.toUnicode();
    int hand=::_wopen(wname(),oflags);

    if(hand<0 && (flags&OWriteOnly))
    {
        hand=::_wcreat(wname(),00666);
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

int64 AFile::size() const
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

bool AFile::seek(int64 pos)
{
    if(handler<0)return false;
#if !defined(__APPLE__)
    return ::lseek64(handler,pos,SEEK_SET)==pos;
#else
    return ::lseek(handler,pos,SEEK_SET)==pos;
#endif
}

int64 AFile::pos() const
{
    if(handler<0)return -1;
#if !defined(__APPLE__)
    return ::lseek64(handler,0,SEEK_CUR);
#else
    return ::lseek(handler,0,SEEK_CUR);
#endif
}

int AFile::read_hand(void *buff, int size)
{
    if(handler<0)return -1;
    if(size<=0)return 0;
    int rv = ::read(handler,buff,size);

    return rv;
}

int AFile::write_hand(const void *buff, int size)
{
    if(handler<0)return -1;
    if(size<=0)return 0;
    return ::write(handler,buff,size);
}

bool AFile::resize(int64 size)
{
    if(handler<0)return false;
#if !defined(__APPLE__)
    if (::ftruncate64(handler, size) != 0) return false;
#else
    if (::ftruncate(handler, size) != 0) return false;
#endif
    return true;
}

bool AFile::isSequential() const
{
    return false;
}

bool AFile::exists(AString fname)
{
    if(fname.isEmpty())return false;
#if !defined(linux) && !defined(__APPLE__)
    ATArray<wchar_t> wname=fname.toUnicode();
    if(_waccess(wname(),F_OK)==-1)return false;
#else
    if(access(fname(),F_OK)==-1)return false;
#endif
    return true;
}


bool AFile::remove(AString fname)
{
    if(fname.isEmpty())return false;
#if !defined(linux) && !defined(__APPLE__)
    ATArray<wchar_t> wname=fname.toUnicode();
    if(_wunlink(wname())==-1)return false;
#else
    if(unlink(fname())==-1)return false;
#endif
    return true;
}

bool AFile::rename(AString before, AString after)
{
#if !defined(linux) && !defined(__APPLE__)
    ATArray<wchar_t> bname=before.toUnicode();
    ATArray<wchar_t> aname=after.toUnicode();
    if(!(_wrename(bname(),aname())))return true;
#else
    if(!(::rename(before(),after())))return true;
#endif
    return false;
}

ATime AFile::changeTime(AString fname)
{
    struct stat t_stat;
#if !defined(linux) && !defined(__APPLE__)
    ATArray<wchar_t> wname=fname.toUnicode();
    wstat(wname(), &t_stat);
#else
    stat(fname(), &t_stat);
#endif
    if(sizeof(t_stat.st_mtime)==8)
        return ATime(((uint64)t_stat.st_mtime)*(uint64)1000000);
    return ATime(((uint32)t_stat.st_mtime)*(uint64)1000000);
}

bool AFile::replicate(AString src, AString dst)
{
    AFile hsrc(src);
    AFile hdst(dst);
    if(!hsrc.open())return false;
    if(!hdst.create())return false;

    int64 total=hsrc.size();
    if(hdst.copy(&hsrc,total)!=total)return false;
    return true;
}
