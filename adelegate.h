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

#ifndef ADELEGATE_H
#define ADELEGATE_H

#include "types.h"

class ADelegateAllow{};

template<typename RT, typename AT>
class ADelegate
{
public:
    ADelegate(){object=NULL;}
    ADelegate(const ADelegate &val){object=val.object;method=val.method;defret=val.defret;}
    ADelegate& operator=(const ADelegate &val){object=val.object;method=val.method;defret=val.defret;return *this;}

    template<typename OT>
    ADelegate(OT *obj,RT (OT::*proc)(AT))
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<RT (ADelegateAllow::*)(AT)>(proc);
    }
    ~ADelegate(){}

    void setDefRet(RT val){defret=val;}
    RT operator()(AT val)
    {
        if(!object)return defret;
        return (object->*method)(val);
    }

private:
    ADelegateAllow *object;
    RT (ADelegateAllow::*method)(AT);
    RT defret;
};

template<typename RT>
class ADelegateNoArg
{
public:
    ADelegateNoArg(){object=NULL;}
    ADelegateNoArg(const ADelegateNoArg &val){object=val.object;method=val.method;defret=val.defret;}
    ADelegateNoArg& operator=(const ADelegateNoArg &val){object=val.object;method=val.method;defret=val.defret;return *this;}

    template<typename OT>
    ADelegateNoArg(OT *obj,RT (OT::*proc)())
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<RT (ADelegateAllow::*)()>(proc);
    }
    ~ADelegateNoArg(){}

    void setDefRet(RT val){defret=val;}
    RT operator()()
    {
        if(!object)return defret;
        return (object->*method)();
    }

private:
    ADelegateAllow *object;
    RT (ADelegateAllow::*method)();
    RT defret;
};

template<typename RT, typename AT1, typename AT2>
class ADelegateDualArg
{
public:
    ADelegateDualArg(){object=NULL;}
    ADelegateDualArg(const ADelegateDualArg &val){object=val.object;method=val.method;defret=val.defret;}
    ADelegateDualArg& operator=(const ADelegateDualArg &val){object=val.object;method=val.method;defret=val.defret;return *this;}

    template<typename OT>
    ADelegateDualArg(OT *obj,RT (OT::*proc)(AT1,AT2))
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<RT (ADelegateAllow::*)(AT1,AT2)>(proc);
    }
    ~ADelegateDualArg(){}

    void setDefRet(RT val){defret=val;}
    RT operator()(AT1 v1, AT2 v2)
    {
        if(!object)return defret;
        return (object->*method)(v1,v2);
    }

private:
    ADelegateAllow *object;
    RT (ADelegateAllow::*method)(AT1,AT2);
    RT defret;
};

template<typename RT, typename AT1, typename AT2, typename AT3>
class ADelegateTribleArg
{
public:
    ADelegateTribleArg(){object=NULL;}
    ADelegateTribleArg(const ADelegateTribleArg &val){object=val.object;method=val.method;defret=val.defret;}
    ADelegateTribleArg& operator=(const ADelegateTribleArg &val){object=val.object;method=val.method;defret=val.defret;return *this;}

    template<typename OT>
    ADelegateTribleArg(OT *obj,RT (OT::*proc)(AT1,AT2,AT3))
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<RT (ADelegateAllow::*)(AT1,AT2,AT3)>(proc);
    }
    ~ADelegateTribleArg(){}

    void setDefRet(RT val){defret=val;}
    RT operator()(AT1 v1, AT2 v2, AT3 v3)
    {
        if(!object)return defret;
        return (object->*method)(v1,v2,v3);
    }

private:
    ADelegateAllow *object;
    RT (ADelegateAllow::*method)(AT1,AT2,AT3);
    RT defret;
};

template<typename RT, typename AT1, typename AT2, typename AT3, typename AT4>
class ADelegateQuadArg
{
public:
    ADelegateQuadArg(){object=NULL;}
    ADelegateQuadArg(const ADelegateQuadArg &val){object=val.object;method=val.method;defret=val.defret;}
    ADelegateQuadArg& operator=(const ADelegateQuadArg &val){object=val.object;method=val.method;defret=val.defret;return *this;}

    template<typename OT>
    ADelegateQuadArg(OT *obj,RT (OT::*proc)(AT1,AT2,AT3,AT4))
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<RT (ADelegateAllow::*)(AT1,AT2,AT3,AT4)>(proc);
    }
    ~ADelegateQuadArg(){}

    void setDefRet(RT val){defret=val;}
    RT operator()(AT1 v1, AT2 v2, AT3 v3, AT4 v4)
    {
        if(!object)return defret;
        return (object->*method)(v1,v2,v3,v4);
    }

private:
    ADelegateAllow *object;
    RT (ADelegateAllow::*method)(AT1,AT2,AT3,AT4);
    RT defret;
};

class ADelegateNoRetNoArg
{
public:
    ADelegateNoRetNoArg(){object=NULL;}
    ADelegateNoRetNoArg(const ADelegateNoRetNoArg &val){object=val.object;method=val.method;}
    ADelegateNoRetNoArg& operator=(const ADelegateNoRetNoArg &val){object=val.object;method=val.method;return *this;}

    template<typename OT>
    ADelegateNoRetNoArg(OT *obj,void (OT::*proc)())
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<void (ADelegateAllow::*)()>(proc);
    }
    ~ADelegateNoRetNoArg(){}

    void operator()()
    {
        if(!object)return;
        (object->*method)();
    }

private:
    ADelegateAllow *object;
    void (ADelegateAllow::*method)();
};

template<typename AT>
class ADelegateNoRet
{
public:
    ADelegateNoRet(){object=NULL;}
    ADelegateNoRet(const ADelegateNoRet &val){object=val.object;method=val.method;}
    ADelegateNoRet& operator=(const ADelegateNoRet &val){object=val.object;method=val.method;return *this;}

    template<typename OT>
    ADelegateNoRet(OT *obj,void (OT::*proc)(AT))
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<void (ADelegateAllow::*)(AT)>(proc);
    }
    ~ADelegateNoRet(){}

    void operator()(AT val)
    {
        if(!object)return;
        (object->*method)(val);
    }

private:
    ADelegateAllow *object;
    void (ADelegateAllow::*method)(AT);
};

template<typename AT1, typename AT2>
class ADelegateNoRetDualArg
{
public:
    ADelegateNoRetDualArg(){object=NULL;}
    ADelegateNoRetDualArg(const ADelegateNoRetDualArg &val){object=val.object;method=val.method;}
    ADelegateNoRetDualArg& operator=(const ADelegateNoRetDualArg &val){object=val.object;method=val.method;return *this;}

    template<typename OT>
    ADelegateNoRetDualArg(OT *obj,void (OT::*proc)(AT1,AT2))
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<void (ADelegateAllow::*)(AT1,AT2)>(proc);
    }
    ~ADelegateNoRetDualArg(){}

    void operator()(AT1 v1, AT2 v2)
    {
        if(!object)return;
        (object->*method)(v1,v2);
    }

private:
    ADelegateAllow *object;
    void (ADelegateAllow::*method)(AT1,AT2);
};

template<typename AT1, typename AT2, typename AT3>
class ADelegateNoRetTribleArg
{
public:
    ADelegateNoRetTribleArg(){object=NULL;}
    ADelegateNoRetTribleArg(const ADelegateNoRetTribleArg &val){object=val.object;method=val.method;}
    ADelegateNoRetTribleArg& operator=(const ADelegateNoRetTribleArg &val){object=val.object;method=val.method;return *this;}

    template<typename OT>
    ADelegateNoRetTribleArg(OT *obj,void (OT::*proc)(AT1,AT2,AT3))
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<void (ADelegateAllow::*)(AT1,AT2,AT3)>(proc);
    }
    ~ADelegateNoRetTribleArg(){}

    void operator()(AT1 v1, AT2 v2, AT3 v3)
    {
        if(!object)return;
        (object->*method)(v1,v2,v3);
    }

private:
    ADelegateAllow *object;
    void (ADelegateAllow::*method)(AT1,AT2,AT3);
};

template<typename AT1, typename AT2, typename AT3, typename AT4>
class ADelegateNoRetQuadArg
{
public:
    ADelegateNoRetQuadArg(){object=NULL;}
    ADelegateNoRetQuadArg(const ADelegateNoRetQuadArg &val){object=val.object;method=val.method;}
    ADelegateNoRetQuadArg& operator=(const ADelegateNoRetQuadArg &val){object=val.object;method=val.method;return *this;}

    template<typename OT>
    ADelegateNoRetQuadArg(OT *obj, void (OT::*proc)(AT1,AT2,AT3,AT4) )
    {
        object=static_cast<ADelegateAllow*>(obj);
        method=static_cast<void (ADelegateAllow::*)(AT1,AT2,AT3,AT4)>(proc);
    }
    ~ADelegateNoRetQuadArg(){}

    void operator()(AT1 v1, AT2 v2, AT3 v3, AT4 v4)
    {
        if(!object)return;
        (object->*method)(v1,v2,v3,v4);
    }

private:
    ADelegateAllow *object;
    void (ADelegateAllow::*method)(AT1,AT2,AT3,AT4);
};

#endif // ADELEGATE_H
