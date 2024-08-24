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

#ifndef ATYPES_H
#define ATYPES_H

//////////////////////////////////////////////////////////////////
//определение типов
//////////////////////////////////////////////////////////////////

typedef  unsigned char      uint8;
typedef  unsigned short     uint16;
typedef  unsigned int       uint32;
typedef  unsigned long long uint64;

typedef  signed char        int8;
typedef  short              int16;
typedef  int                int32;
typedef  signed long long   int64;

typedef  float              real32;
typedef  double             real64;
#ifndef ANDROID_NDK
typedef  long double        real80;
typedef  long double        realx;
#else
typedef  double             realx;
#endif
typedef  double             real;

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) \
    || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
typedef  signed long long   intz;
typedef  unsigned long long uintz;
 #else
typedef  int                intz;
typedef  unsigned int       uintz;
#endif

typedef  signed long long   intx;
typedef  unsigned long long uintx;

typedef  unsigned int       uint;

typedef  unsigned short     charx;

#ifndef NULL
 #define NULL 0
#endif

namespace alt {

    template<class T>
    __inline T roundup(T size, T align)
    {
        return ((size+align-1)/align)*align;
    }

    template<class T>
    __inline T roundup_div(T size, T align)
    {
        return (size+align-1)/align;
    }

    namespace utils {

        template<class T>
        __inline bool signed_type()
        {
            T a = 0;
            T b = a-1;
            return b<a;
        }

        __inline uintz upsize(uintz x)
        {
            const uintz MINSIZE = (sizeof(uintz)*8);
            return ( ((x+1)+((x)>>1))<MINSIZE ? MINSIZE : ((x+1)+((x)>>1)) );
        }

        template <class T>
        __inline uint strlen(const T *Str)
        {
            if(!Str)return 0;

            uint i=0;
            while(Str[i]){i++;};
            return i;
        }

        template <class T>
        __inline void memcpy(T *dst, const T *src, intz num)
        {
            intz i;
            if(dst<src)  //в случае перекрытий следует копировать с нужной стороны
            {
                    for(i=0;i<num;i++)dst[i]=src[i];
            }
            else if(dst>src)
            {
                    for(i=num-1;i>=0;i--)dst[i]=src[i];
            }
        }

        template <class T>
        __inline int memcmp(const T *dst, const T *src, intz num)
        {
            intz i;
            for(i=0;i<num;i++)
            {
                    if(dst[i]<src[i])return -1;
                    if(dst[i]>src[i])return 1;
            }
            return 0;
        }

        template <class T>
        __inline int strcmp(const T *dst, const T *src)
        {
            int i=0;
            while(dst[i] && src[i])
            {
                    if(dst[i]<src[i])return -1;
                    if(dst[i]>src[i])return 1;
                    i++;
            }
            if(dst[i]<src[i])return -1;
            if(dst[i]>src[i])return 1;
            return 0;
        }

        template <class T>
        __inline void memset(T *dst, T c, intz num)
        {
            for(intz i=0;i<num;i++)dst[i]=c;
        }

    } // namespace utils

    inline uintz ptr2int(void *val)
    {
        return reinterpret_cast<uintz>(val);
    }

    template<class T>
    inline T* int2ptr(uintz val)
    {
        return reinterpret_cast<T*>(val);
    }

    template<class T>
    inline T unaligned_read(void *buff)
    {
        T rv;
        for(uint i=0;i<sizeof(T) && i<sizeof(void*);i++)
        {
            ((uint8*)&rv)[i]=((uint8*)buff)[i];
        }
        return rv;
    }

    template<class T>
    inline void unaligned_write(void *buff, T val)
    {
        for(uint i=0;i<sizeof(T) && i<sizeof(void*);i++)
        {
            ((uint8*)buff)[i]=((uint8*)&val)[i];
        }
    }

    class retCode
    {
    public:
        retCode(){ state=0; }
        retCode(const retCode &val){ state=val.state; }
        retCode(const int &val){ state=val; }
        virtual ~retCode(){}

        virtual retCode& operator=(const retCode &val)
        {
            state=val.state;
            return *this;
        }
        virtual retCode& operator=(int val)
        {
            state=val;
            return *this;
        }

        virtual retCode& set(int val){state=val;return *this;}
        int get(){return state;}

        uint value() //значение
        {
            if(state<0)return 0;
            return state;
        }
        uint error() //номер ошибки
        {
            if(state>=0)return 0;
            return -state;
        }

        void clear()
        {
            state = 0;
        }

        operator int(){return state;}

    protected:
        int state;
    };


    template <typename L, typename R>
    class pair
    {
    public:

        pair(){vl = L(); vr = R();}
        ~pair(){}
        pair(L left_val, R right_val){vl=left_val;vr=right_val;}
        pair(const pair &val){*this=val;}
        pair& operator=(const pair &val){vl=val.vl;vr=val.vr;return *this;}

        L& left(){return vl;}
        R& right(){return vr;}

        L left() const {return vl;}
        R right() const {return vr;}

    private:
        L vl;
        R vr;
    };

    template <typename L, typename M, typename R>
    class trio
    {
    public:

        trio(){}
        ~trio(){}
        trio(L left_val, M middle_val, R right_val){vl=left_val;vm=middle_val;vr=right_val;}
        trio(const pair<L,R> &val){vl=val.left();vr=val.right();}
        trio(const trio &val){*this=val;}
        trio& operator=(const trio &val){vl=val.vl;vm=val.vm;vr=val.vr;return *this;}

        L& left(){return vl;}
        M& middle(){return vm;}
        R& right(){return vr;}

        const L& left() const {return vl;}
        const M& middle() const {return vm;}
        const R& right() const {return vr;}

    private:
        L vl;
        M vm;
        R vr;
    };

    template <class T>
    class shared
    {
    public:
        shared()
        {
        }
        explicit shared(T *val)
        {
            data = new Internal();
            data->object = val;
            data->refcount ++;
        }
        shared(const shared<T> &val)
        {
            if(val.data)
            {
                data = val.data;
                data->refcount++;
            }
        }
        shared& operator = (const shared<T> &val)
        {
            if(data == val.data)
                return *this;
            removeInternal();
            if(val.data)
            {
                data = val.data;
                data->refcount++;
            }
            return *this;
        }
        ~shared()
        {
            removeInternal();
        }

        void free()
        {
            removeInternal();
        }

        T* get()
        {
            if(!data)
                return nullptr;
            return data->object;
        }

        T* operator->() const
        {
            return data->object;
        }

        T& operator*()  const
        {
            return *(data->object);
        }

    private:
        struct Internal
        {
            T *object = nullptr;
            uint refcount = 0;
        };

        Internal *data = nullptr;

        void removeInternal()
        {
            if(data)
            {
                data->refcount--;
                if(!data->refcount)
                {
                    delete data->object;
                    delete data;
                }
            }
            data = nullptr;
        }
    };


} //namespace alt

#ifdef __GNUC__
 #define __fastcall __attribute__((__fastcall__))
#endif

#endif // ATYPES_H
