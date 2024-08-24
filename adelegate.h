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

#ifndef ADELEGATE_H
#define ADELEGATE_H

namespace alt {

    //For MSVC you need to use CXXFLAGS /vmg /vmm

    class delegateBase{};

    template<typename RT, typename... AT>
    class delegate
    {
    public:
        delegate(){object=nullptr;method=nullptr;defret=RT();}
        delegate(const delegate &val){object=val.object;method=val.method;defret=val.defret;}
        delegate& operator=(const delegate &val){object=val.object;method=val.method;defret=val.defret;return *this;}

        template<typename OT>
        delegate(OT *obj,RT (OT::*proc)(AT...))
        {
            object=static_cast<delegateBase*>(obj);
            method=static_cast<RT (delegateBase::*)(AT...)>(proc);
        }
        ~delegate(){}

        void setDefRet(RT val){defret=val;}
        RT operator()(AT... args)
        {
            if(!object)return defret;
            return (object->*method)(args...);
        }

        bool empty() { return !object; }

    private:
        delegateBase *object;
        RT (delegateBase::*method)(AT...);
        RT defret;
    };


    template<typename... AT>
    class delegateProc
    {
    public:
        delegateProc(){object=nullptr;method=nullptr;}
        delegateProc(const delegateProc &val){object=val.object;method=val.method;}
        delegateProc& operator=(const delegateProc &val){object=val.object;method=val.method;return *this;}

        template<typename OT>
        delegateProc(OT *obj,void (OT::*proc)(AT...))
        {
            object=static_cast<delegateBase*>(obj);
            method=static_cast<void (delegateBase::*)(AT...)>(proc);
        }
        ~delegateProc(){}

        void operator()(AT... args)
        {
            if(!object)return;
            (object->*method)(args...);
        }

        bool empty() { return !object; }

    private:
        delegateBase *object;
        void (delegateBase::*method)(AT...);
    };

} // namespace alt

#endif // ADELEGATE_H
