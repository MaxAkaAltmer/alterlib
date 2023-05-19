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

#ifndef VMATH_HEADER_DEFINITION
#define VMATH_HEADER_DEFINITION

#include "atypes.h"
#include "amath_int.h"

namespace alt {
namespace fmath {

    //операции с плавающей запятой
    #ifndef ANDROID_NDK
    real80 cos(real80 val);
    real80 acos(real80 val);
    real80 asin(real80 val);
    real80 sin(real80 val);
    real80 sqrt(real80 val);
    real80  atan2(real80 y, real80 x);
    real80  fmod(real80 x, real80 y);
    #endif

    real64 cos(real64 val);
    real32 cos(real32 val);

    real64 acos(real64 val);
    real32 acos(real32 val);

    real64 asin(real64 val);
    real32 asin(real32 val);

    real64 sin(real64 val);
    real32 sin(real32 val);

    real64  sqrt(real64 val);
    real32  sqrt(real32 val);

    real64  atan2(real64 y, real64 x);
    real32  atan2(real32 y, real32 x);

    template <class T>
    __inline T abs(T val)
    {
            if(val<0.0)return -val;
            return val;
    }

    real64  fmod(real64 x, real64 y);
    real32  fmod(real32 x, real32 y);

} //namespace fmath

template <class T> class vec2d;
template <class T>
class line2d
{
public:
    T a,b,c;

    line2d(){a=b=c=0.0;}
    ~line2d(){}
    line2d(T x0, T y0, T x1, T y1)
    {
        a=y0-y1;
        b=x1-x0;
        c=x0*y1-x1*y0;
    }
    line2d(const line2d<T> &val){*this=val;}

    line2d<T>& operator=(const line2d<T> &val)
    {
        a=val.a;
        b=val.b;
        c=val.c;
        return *this;
    }

    T test(T x, T y)
    {
        return x*a+y*b+c;
    }

    T dist(T x, T y) //>0 если слева
    {
        return (x*a+y*b+c)/_vm_sqrt(a*a+b*b);
    }

};

//класс декартовых координат
template <class T>
class vec2d
{
public:
        vec2d(){x=y=0;}
        ~vec2d(){}
        vec2d(T xv, T yv){x=xv;y=yv;}
        vec2d(const vec2d<T> &val){*this=val;}

        vec2d<T>& operator=(const vec2d<T> &val)
        {
            x=val.x;
            y=val.y;
            return *this;
        }

        void setValue(T vx, T vy)
        {
            x=vx;
            y=vy;
        }

        bool operator==(const vec2d<T> &val) const
        {
            return !(val!=(*this));
        }

        bool operator!=(const vec2d<T> &val) const
        {
            if(val.x!=x)return true;
            if(val.y!=y)return true;
            return false;
        }

        T x,y;

        template<class I>
        vec2d<I> conv() const
        {
            return vec2d<I>((I)x,(I)y);
        }

        void Clear()
        {
            x=y=0;
        }

        //сложение векторов
        vec2d<T> operator+(const vec2d<T> &val) const
        {
         vec2d<T> retval;
                retval.x=x+val.x;
                retval.y=y+val.y;
                return retval;
        }

        vec2d<T>& operator+=(const vec2d<T> &val)
        {
                x=x+val.x;
                y=y+val.y;
                return *this;
        }

        //комплиментарный вектор
        vec2d<T> operator-() const
        {
         vec2d<T> retval;
                retval.x=-x;
                retval.y=-y;
                return retval;
        }

        vec2d<T> operator-(const vec2d<T> &val) const
        {
         vec2d<T> retval;
                retval.x=x-val.x;
                retval.y=y-val.y;
                return retval;
        }

        //умножение вектора на число
        vec2d<T> operator*(T val) const
        {
         vec2d<T> retval;
                retval.x=x*val;
                retval.y=y*val;
                return retval;
        }

        //деление вектора на число
        vec2d<T> operator/(T val) const
        {
         vec2d<T> retval;
                retval.x=x/val;
                retval.y=y/val;
                return retval;
        }

        //установка длины вектора
        void SetLength(T len)
        {
                Normalize();
                (*this)=(*this)*len;
        }

        //нормализация - вектор станет единичным
        vec2d<T>& Normalize()
        {
         T det=Length();
                x/=det;
                y/=det;
                return *this;
        }

        vec2d<T>& operator*=(T val)
        {
                x*=val;
                y*=val;
                return *this;
        }

        //длина вектора
        T Length() const
        {
                return _vm_sqrt(x*x+y*y);
        }


        //расстояние между точками
        T Distance(const vec2d<T> &pnt) const
        {
                return ((*this)-pnt).Length();
        }
        //угол между векторами
        T AngelTo(const vec2d<T> &pnt) const
        {
         T retval;

                retval=(x*pnt.x+y*pnt.y)/(pnt.Length()*Length());

                if(retval>1.0)retval=1.0;
                else if(retval<-1.0)retval=-1.0;
                return _vm_acos(retval);
        }

        T CosinusTo(const vec2d<T> &pnt) const
        {
         T retval;

                retval=(x*pnt.x+y*pnt.y)/(pnt.Length()*Length());

                if(retval>1.0)retval=1.0;
                else if(retval<-1.0)retval=-1.0;
                return retval;
        }

};

template <class T>
class voxel
{
public:
    voxel(){};
    voxel(T _x, T _y, T _z, T _w, T _h, T _d){ x=_x; y=_y; z=_z; width=_w; height=_h; depth=_d; }
    voxel(const voxel &q){ x=q.x; y=q.y; z=q.z; width=q.width; height=q.height; depth=q.depth; }

    voxel& operator = (const voxel &q) { x=q.x; y=q.y; z=q.z; width=q.width; height=q.height; depth=q.depth; return *this;}

    bool test(voxel qd) const
    {
        if(x+width<qd.x)return false;
        if(x>qd.x+qd.width)return false;
        if(y+height<qd.y)return false;
        if(y>qd.y+qd.height)return false;
        if(z+depth<qd.z)return false;
        if(z>qd.z+qd.depth)return false;
        return true;
    }

    T x = 0, y = 0, z = 0, width = 0, height = 0, depth = 0; //< the top-left-far corner, as well as width, height and depth of the volume
};

template <class T>
class rect
{
public:

    //! various constructors
    rect(){x=y=width=height=0;}
    rect(T _x, T _y, T _width, T _height){x=_x;y=_y;width=_width;height=_height;}
    rect(const rect& r){x=r.x;y=r.y;width=r.width;height=r.height;}

    rect& operator = ( const rect& r ){x=r.x;y=r.y;width=r.width;height=r.height;return *this;}
    //! the top-left corner
    vec2d<T> tl() const {return vec2d<T>(x,y);}
    vec2d<T> tr() const {return vec2d<T>(x+width,y);}
    vec2d<T> dl() const {return vec2d<T>(x,y+height);}
    vec2d<T> dr() const {return vec2d<T>(x+width,y+height);}

    vec2d<T> center(){return vec2d<T>(x+width/2,y+height/2);}

    rect operator+(const vec2d<T> &val)
    {
        rect rv=*this;
        rv.x+=val.x;
        rv.y+=val.y;
        return rv;
    }

    rect operator-(const vec2d<T> &val)
    {
        rect rv=*this;
        rv.x-=val.x;
        rv.y-=val.y;
        return rv;
    }

    //! area (width*height) of the rectangle
    T area() const {return width*height;}

    //! conversion to another data type
    template<class _Tp2> operator rect<_Tp2>() const
    {
        rect<_Tp2> rv;
        rv.x=x;
        rv.y=y;
        rv.width=width;
        rv.height=height;
        return rv;
    }

    bool test(vec2d<T> p) const
    {
        if(p.x>=x && p.x<x+width &&
                p.y>y && p.y<y+height)
            return true;
        return false;
    }

    bool test(rect rc) const
    {
        if(x+width<rc.x)return false;
        if(x>rc.x+rc.width)return false;
        if(y+height<rc.y)return false;
        if(y>rc.y+rc.height)return false;
        return true;
    }

    rect crossResult(rect rc)
    {
        rect rv;
        if(!test(rc))return rv;
        rv.x=__max_val(x,rc.x);
        rv.y=__max_val(y,rc.y);
        rv.width=__min_val(x+width,rc.x+rc.width)-rv.x;
        rv.height=__min_val(y+height,rc.y+rc.height)-rv.y;
        return rv;
    }

    vec2d<T> affinePoint(rect rc, vec2d<T> p)
    {
        return vec2d<T>(
                    x+(width*(p.x-rc.x))/rc.width,
                    y+(height*(p.y-rc.y))/rc.height
                    );
    }

    void setHeightProp(T h)
    {
        width=(width*h)/height;
        height=h;
    }

    rect zoom(real32 factor)
    {
        T w=width*factor;
        T h=height*factor;
        T xoff=(width-w)/2.0;
        T yoff=(height-h)/2.0;
        return rect(x+xoff,y+yoff,w,h);
    }

    //вписание по центру с сохранением пропорций
    rect fitInCenter(rect rc)
    {
        T asp;
        T ax=width/rc.width;
        T ay=height/rc.height;
        if(ax<ay)asp=ax;
        else asp=ay;
        T xoff=(width-rc.width*asp)/2.0;
        T yoff=(height-rc.height*asp)/2.0;
        return rect(x+xoff,y+yoff,rc.width*asp,rc.height*asp);
    }

    //произведет аналогичную часть от текущего сектора, part of main
    rect affine(rect main, rect part)
    {
        T tx=(width*(part.x-main.x))/main.width;
        T ty=(height*(part.y-main.y))/main.height;
        T tw=(width*(part.width))/main.width;
        T th=(height*(part.height))/main.height;
        return rect(x+tx,y+ty,tw,th);
    }

    T x, y, width, height; //< the top-left corner, as well as width and height of the rectangle
};

template <class T>
T poly_square(vec2d<T> *poly, int n)
{
   T s = 0;
   int index;
   for(int i = 0; i < n; i++)
   {
       if ( i == (n - 1) )
           index = 0;
       else
           index = i + 1;
       s += (poly[i].x + poly[index].x) * (poly[i].y - poly[index].y);
   }
   return s / 2;
}


template <class T> class sh1Point;
template <class T> class spheric;
template <class T> class plane;
template <class T> class line3d;

//класс декартовых координат
template <class T>
class vec3d
{
public:
        vec3d(){x=y=z=0.0;}
        ~vec3d(){/*x=y=z=0.0;*/}
        vec3d(T xv, T yv, T zv=0.0){x=xv;y=yv;z=zv;}
        vec3d(const vec3d<T> &val){*this=val;}

        vec3d<T>& operator=(const vec3d<T> &val)
        {
            x=val.x;
            y=val.y;
            z=val.z;
            return *this;
        }

        bool isBetween(const vec3d<T> v1, const vec3d<T> v2, T eps=0.0)
        {
            if(x+eps>v1.x && x+eps>v2.x && x-eps>v1.x && x-eps>v2.x)return false;
            if(x+eps<v1.x && x+eps<v2.x && x-eps<v1.x && x-eps<v2.x)return false;
            if(y+eps>v1.y && y+eps>v2.y && y-eps>v1.y && y-eps>v2.y)return false;
            if(y+eps<v1.y && y+eps<v2.y && y-eps<v1.y && y-eps<v2.y)return false;
            if(z+eps>v1.z && z+eps>v2.z && z-eps>v1.z && z-eps>v2.z)return false;
            if(z+eps<v1.z && z+eps<v2.z && z-eps<v1.z && z-eps<v2.z)return false;
            return true;
        }

        void setValue(T vx, T vy, T vz)
        {
            x=vx;
            y=vy;
            z=vz;
        }

        bool operator==(const vec3d<T> &val) const
        {
            return !(val!=(*this));
        }

        bool operator!=(const vec3d<T> &val) const
        {
            if(val.x!=x)return true;
            if(val.y!=y)return true;
            if(val.z!=z)return true;
            return false;
        }

        T x,y,z;

        void Clear()
        {
            x=y=z=0.0;
        }

        //ближайшая к данной точка на отрезке
        vec3d<T> NearToLine(const vec3d<T> &p1, const vec3d<T> &p2) const
        {
         line3d<T> line;
         plane<T> pln;
         vec3d<T> val;
         T tmp1, tmp2;

                line.Create(p1,p2);
                pln.CreateNormalViaPoint(line,*this);
                val=pln.CrossByLine(p1,p2);
                tmp1=val.Distance(p1);
                tmp2=val.Distance(p2);
                if((tmp1+tmp2)>p1.Distance(p2))
                {
                        if(tmp1<tmp2)return p1;
                        return p2;
                }
                return val;
        };

        //сложение векторов
        vec3d<T> operator+(const vec3d<T> &val) const
        {
         vec3d<T> retval;
                retval.x=x+val.x;
                retval.y=y+val.y;
                retval.z=z+val.z;
                return retval;
        };

        vec3d<T>& operator+=(const vec3d<T> &val)
        {
                x=x+val.x;
                y=y+val.y;
                z=z+val.z;
                return *this;
        };

        //комплиментарный вектор
        vec3d<T> operator-() const
        {
         vec3d<T> retval;
                retval.x=-x;
                retval.y=-y;
                retval.z=-z;
                return retval;
        };

        vec3d<T> operator-(const vec3d<T> &val) const
        {
         vec3d<T> retval;
                retval.x=x-val.x;
                retval.y=y-val.y;
                retval.z=z-val.z;
                return retval;
        };

        //векторное произведение
        vec3d<T> operator*(const vec3d<T> &val) const
        {
         vec3d<T> retval;
                retval.x=y*val.z-z*val.y;
                retval.y=z*val.x-x*val.z;
                retval.z=x*val.y-y*val.x;
                return retval;
        }

        //умножение вектора на число
        vec3d<T> operator*(T val) const
        {
         vec3d<T> retval;
                retval.x=x*val;
                retval.y=y*val;
                retval.z=z*val;
                return retval;
        }

        friend vec3d<T> operator*(T val, const vec3d<T> &v)
        {
            vec3d<T> retval;
                   retval.x=v.x*val;
                   retval.y=v.y*val;
                   retval.z=v.z*val;
                   return retval;
        }

        //деление вектора на число
        vec3d<T> operator/(T val) const
        {
         vec3d<T> retval;
                retval.x=x/val;
                retval.y=y/val;
                retval.z=z/val;
                return retval;
        };

        //установка длины вектора
        void SetLength(T len)
        {
                Normalize();
                (*this)=(*this)*len;
        };

        //нормализация - вектор станет единичным
        vec3d<T>& Normalize()
        {
         T det=Length();
                x/=det;
                y/=det;
                z/=det;
                return *this;
        };

        vec3d<T>& operator*=(T val)
        {
                x*=val;
                y*=val;
                z*=val;
                return *this;
        };

        //длина вектора
        T Length() const
        {
                return fmath::sqrt(x*x+y*y+z*z);
        };

        //вращение вокруг данного вектора на угол в радианах
        //вектор через который происходит вызов должен быть нормализован
        vec3d<T> RotateAroundThis(const vec3d<T> &pnt, T ang)
        {
         T cfi,sfi;
         vec3d<T> retval;

                cfi=_vm_cos(ang);
                sfi=fmath::sin(ang);

                retval.x=(cfi+(1.0-cfi)*x*x)*pnt.x+((1.0-cfi)*x*y-sfi*z)*pnt.y+((1.0-cfi)*x*z+sfi*y)*pnt.z;
                retval.y=((1.0-cfi)*y*x+sfi*z)*pnt.x+(cfi+(1.0-cfi)*y*y)*pnt.y+((1.0-cfi)*y*z-sfi*x)*pnt.z;
                retval.z=((1.0-cfi)*z*x-sfi*y)*pnt.x+((1.0-cfi)*y*z+sfi*x)*pnt.y+(cfi+(1.0-cfi)*z*z)*pnt.z;

                return retval;
        };

        //расстояние между точками
        T Distance(const vec3d<T> &pnt) const
        {
                return ((*this)-pnt).Length();
        };
        //угол между векторами
        T AngelTo(const vec3d<T> &pnt) const
        {
         T retval;

                retval=(x*pnt.x+y*pnt.y+z*pnt.z)/(pnt.Length()*Length());

                if(retval>1.0)retval=1.0;
                else if(retval<-1.0)retval=-1.0;
                return _vm_acos(retval);
        };

        T CosinusTo(const vec3d<T> &pnt) const
        {
         T retval;

                retval=(x*pnt.x+y*pnt.y+z*pnt.z)/(pnt.Length()*Length());

                if(retval>1.0)retval=1.0;
                else if(retval<-1.0)retval=-1.0;
                return retval;
        };

        //уравнители :)  (для автоматического кастинга)
        operator vec3d<real64>() const
        {
         vec3d<real64> val;
                val.x=x;
                val.y=y;
                val.z=z;
                return val;
        };
        operator vec3d<real32>() const
        {
         vec3d<real32> val;
                val.x=x;
                val.y=y;
                val.z=z;
                return val;
        };

        operator sh1Point<T>() const
        {
         sh1Point<T> retval;
         T L;

                L=Length();
                retval.lat=fmath::asin(z/L);
                retval.lon=_vm_atan2(y/L, x/L);
                return retval;
        };

        operator spheric<T>() const
        {
         spheric<T> retval;

                retval.alt=Length();
                retval.lat=fmath::asin(z/retval.alt);
                retval.lon=_vm_atan2(y/retval.alt, x/retval.alt);
                return retval;
        };

};

//класс сферических координат
template <class T>
class spheric
{
public:
        //в радианах
        T lat, lon;
        T alt;

        spheric(){lat=lon=alt=0.0;}
        ~spheric(){/*lat=lon=alt=0.0;*/}
        spheric(T latv, T lonv, T altv){lat=latv;lon=lonv;alt=altv;}
        spheric(const spheric<T> &val){*this=val;}

        spheric<T>& operator=(const spheric<T> &val)
        {
            lat=val.lat;
            lon=val.lon;
            alt=val.alt;
            return *this;
        }

        //уравнители - автокастинг
        operator vec3d<T>() const
        {
         vec3d<T> retval;
                retval.y=_vm_cos(lat);
                retval.x=retval.y*_vm_cos(lon)*alt;
                retval.y*=fmath::sin(lon)*alt;
                retval.z=fmath::sin(lat)*alt;
                return retval;
        };

        operator sh1Point<T>() const
        {
         sh1Point<T> retval;
                retval.lat=lat;
                retval.lon=lon;
                return retval;
        };
};

//класс линии в трехмерном декартовом пространстве, параметрическая запись
template <class T>
class line3d
{
public:
        T x,y,z;
        T l,m,n;

        line3d(){x=y=z=l=m=n=0.0;}
        line3d(const vec3d<T> &p1, const vec3d<T> &p2)
        {
            Create(p1,p2);
        }
        ~line3d(){/*x=y=z=l=m=n=0.0;*/}

        line3d(const line3d<T> &val){*this=val;}

        line3d<T>& operator=(const line3d<T> &v)
        {
            x=v.x;
            y=v.y;
            z=v.z;
            l=v.l;
            m=v.m;
            n=v.n;
            return *this;
        }

        line3d<T>& Create(const vec3d<T> &p1, const vec3d<T> &p2)
        {
                x=p1.x;
                y=p1.y;
                z=p1.z;
                l=p2.x-p1.x;
                m=p2.y-p1.y;
                n=p2.z-p1.z;
                return *this;
        }

        line3d<T> makeParallel(const vec3d<T> &p)
        {
            line3d<T> rv=*this;
            rv.x=p.x;
            rv.y=p.y;
            rv.z=p.z;
            return rv;
        }

};

//класс сферических единичных координат
template <class T>
class sh1Point
{
public:
        sh1Point(){lat=lon=0.0;}
        ~sh1Point(){/*lat=lon=0.0;*/}
        //в радианах
        T lat, lon;

        T Lat(){return lat;}
        T Lon(){return lon;}

        sh1Point(T latv, T lonv){lat=latv;lon=lonv;}
        sh1Point(const sh1Point<T> &val){*this=val;}

        sh1Point<T>& operator=(const sh1Point<T> &val)
        {
            lat=val.lat;
            lon=val.lon;
            return *this;
        }

        operator vec3d<T>() const
        {
         vec3d<T> retval;
                retval.y=_vm_cos(lat);
                retval.x=retval.y*_vm_cos(lon);
                retval.y*=fmath::sin(lon);
                retval.z=fmath::sin(lat);
                return retval;
        };

        //угол меж точками через начало координат
        T AngelTo(const sh1Point<T> &pnt) const
        {
         vec3d<T> dp1, dp2;
         T retval;

                dp1=*this;
                dp2=pnt;

                retval=dp1.x*dp2.x+dp1.y*dp2.y+dp1.z*dp2.z;

                if(retval>1.0)retval=1.0;
                else if(retval<-1.0)retval=-1.0;
                return _vm_acos(retval);
        };

        //средняя точка на дуге
        sh1Point<T>  MiddlePoint(const sh1Point<T> &pnt) const
        {
                return (((vec3d<T>)*this)+(pnt));
        };

};

//класс плоскости
template <class T>
class plane
{
public:
        T       a,b,c,d;

        plane(){a=b=c=d=0.0;}
        ~plane(){/*a=b=c=d=0.0;*/}

        plane(const plane<T> &val){*this=val;}

        plane<T>& operator=(const plane<T> &v)
        {
            a=v.a;
            b=v.b;
            c=v.c;
            d=v.d;
            return *this;
        }

        //положение точки относительно плоскости
        T Test(const vec3d<T> &pnt) const
        {
                return a*pnt.x+b*pnt.y+c*pnt.z+d;
        };
        int Sign(const vec3d<T> &pnt) const
        {
            T r=Test(pnt);
            if(r<0.0)return -1;
            if(r>0.0)return 1;
            return 0;
        }

        //вектор линии пересечения двух плоскостей
        vec3d<T> CrossVector(const plane<T> &pl) const
        {
         vec3d<T> retval;

                retval.x=b*pl.c-c*pl.b;
                retval.y=c*pl.a-a*pl.c;
                retval.z=a*pl.b-b*pl.a;

                return retval;
        };

        //точка пересечения трех плоскостей
        vec3d<T> CrossPoint(const plane<T> &pl2, const plane<T> &pl3) const
        {
         vec3d<T> tmp;
         T det;

                det=-pl3.a*pl2.b*c+pl3.a*b*pl2.c+pl2.a*pl3.b*c-b*pl2.a*pl3.c+pl2.b*a*pl3.c-a*pl3.b*pl2.c;
                tmp.z = -(-b*pl2.a*pl3.d+pl3.a*b*pl2.d-pl3.a*pl2.b*d+pl2.b*a*pl3.d+pl2.a*pl3.b*d-a*pl3.b*pl2.d)/(det);
                tmp.y = -(-pl2.a*pl3.c*d+pl2.a*pl3.d*c+pl3.a*pl2.c*d+pl3.c*a*pl2.d-pl3.a*pl2.d*c-pl3.d*a*pl2.c)/(det);
                tmp.x = (pl2.c*pl3.b*d-pl2.d*pl3.b*c+pl3.c*b*pl2.d-pl3.c*pl2.b*d+pl3.d*pl2.b*c-pl3.d*b*pl2.c)/(det);
                return  tmp;
        };

        //точка пересечения плоскости с дугой, начало координат принадлежит плоскости
        //перед использованием нужно убедиться, что точки лежат по разные стороны плоскости
        sh1Point<T> CrossPoint(const sh1Point<T> &p1, const sh1Point<T> &p2) const
        {
         vec3d<T> dp1, dp2, tmp;
         T t;

                dp1=(p1);
                dp2=(p2);

                t=(-a*dp1.x-b*dp1.y-c*dp1.z-d)/((dp2.x-dp1.x)*a+(dp2.y-dp1.y)*b+(dp2.z-dp1.z)*c);

                tmp.x=t*(dp2.x-dp1.x)+dp1.x;
                tmp.y=t*(dp2.y-dp1.y)+dp1.y;
                tmp.z=t*(dp2.z-dp1.z)+dp1.z;

                return tmp;
        };

        //пересечение прямой с плоскостью
        vec3d<T> CrossByLine(const vec3d<T> &p1, const vec3d<T> &p2) const
        {
         vec3d<T> tmp;
         T t;

                t=(-a*p1.x-b*p1.y-c*p1.z-d)/((p2.x-p1.x)*a+(p2.y-p1.y)*b+(p2.z-p1.z)*c);

                tmp.x=t*(p2.x-p1.x)+p1.x;
                tmp.y=t*(p2.y-p1.y)+p1.y;
                tmp.z=t*(p2.z-p1.z)+p1.z;

                return tmp;
        };

        //точка пересечения с вектором
        vec3d<T> CrossByLine(const vec3d<T> &pnt) const
        {
         vec3d<T> tmp;
         T t;

                t=(-a*pnt.x-b*pnt.y-c*pnt.z-d)/((-pnt.x)*a+(-pnt.y)*b+(-pnt.z)*c);

                tmp.x=t*(-pnt.x)+pnt.x;
                tmp.y=t*(-pnt.y)+pnt.y;
                tmp.z=t*(-pnt.z)+pnt.z;

                return tmp;
        };

        //плоскость строится нормально вектору через его вершину
        plane<T>& CreateByVectorVertex(const vec3d<T> &pnt)
        {
                a=pnt.x;
                b=pnt.y;
                c=pnt.z;
                d=-a*a-b*b-c*c;
                return *this;
        }
        //нормальная прямой плоскость через указанную точку
        plane<T>& CreateNormalViaPoint(const line3d<T> &line, const vec3d<T> &pnt)
        {
                a=line.l;
                b=line.m;
                c=line.n;
                d=-line.l*pnt.x-line.m*pnt.y-line.n*pnt.z;
                return *this;
        };

        //по трем точкам
        plane<T>& CreateBy3DP(const vec3d<T> &p1, const vec3d<T> &p2, const vec3d<T> &p3)
        {
                a=(p2.y-p1.y)*(p3.z-p1.z)-(p2.z-p1.z)*(p3.y-p1.y);
                b=(p2.z-p1.z)*(p3.x-p1.x)-(p2.x-p1.x)*(p3.z-p1.z);
                c=(p2.x-p1.x)*(p3.y-p1.y)-(p2.y-p1.y)*(p3.x-p1.x);
                d=-p1.x*a-p1.y*b-p1.z*c;
                return *this;
        };
        //нормальная двум плоскостям через указанную точку
        plane<T>& CreateNormalViaPoint(const plane<T> &pl1, const plane<T> &pl2, const vec3d<T> &pnt)
        {
                a=pl1.b*pl2.c-pl1.c*pl2.b;
                b=pl1.c*pl2.a-pl1.a*pl2.c;
                c=pl1.a*pl2.b-pl1.b*pl2.a;
                d=-pnt.x*a-pnt.y*b-pnt.z*c;
                return *this;
        };
        //плоскость параллельная даннной через указанную точку
        plane<T>& CreateParallelViaPoint(const plane<T> &pln, const vec3d<T> &pnt)
        {
                *this=pln;
                d=-pnt.x*pln.a-pnt.y*pln.b-pnt.z*pln.c;
                return *this;
        };

        //через две точки и начало координат
        plane<T>& CreateViaCenter(const vec3d<T> &p1, const vec3d<T> &p2)
        {
                a=p1.y*p2.z-p2.y*p1.z;
                b=p1.z*p2.x-p1.x*p2.z;
                c=p1.x*p2.y-p1.y*p2.x;
                d=0.0;
                return *this;
        }

        //угол между плоскостями
        T AngelTo(const plane<T> &pl) const
        {
         T retval;
                retval=(a*pl.a+b*pl.b+c*pl.c)/(_vm_sqrt((a*a+b*b+c*c)*(pl.a*pl.a+pl.b*pl.b+pl.c*pl.c)));
                if(retval>1.0)retval=1.0;
                else if(retval<-1.0)retval=-1.0;
                retval=_vm_acos(retval);
                return retval;
        }

        //угол между вектором и плоскостью
        T AngelTo(const vec3d<T> &pnt) const
        {
         T retval;

                retval=(a*pnt.x+b*pnt.y+c*pnt.z)/_vm_sqrt((a*a+b*b+c*c)*(pnt.x*pnt.x+pnt.y*pnt.y+pnt.z*pnt.z));
                if(retval>1.0)retval=1.0;
                else if(retval<-1.0)retval=-1.0;
                return fmath::asin(retval);
        }

        //нормализация плоскости
        void Normalize()
        {
         T det;
                det=_vm_sqrt(a*a+b*b+c*c);
                if(d>0.0)det=-det;
                a/=det;
                b/=det;
                c/=det;
                d=-fmath::abs(d/det);
        }

        //расстояние до указанной точки
        T DistTo(const vec3d<T> &pnt) const
        {
                return (a*pnt.x+b*pnt.y+c*pnt.z+d)/_vm_sqrt(a*a+b*b+c*c);
        }

        //нормальный вектор плоскости
        vec3d<T> NormVector() const
        {
         vec3d<T> retval;
                retval.x=a;
                retval.y=b;
                retval.z=c;
                return retval;
        }

        //сместить плоскость на указанное расстояние от начала координат
        void SetDistFromSO(T dist)
        {
         vec3d<T> nor;
                Normalize();
                nor=NormVector();
                nor.Normalize();
                nor*=dist;
                d=-nor.x*a-nor.y*b-nor.z*c;
        }

};


template <typename T>
class quad3d
{
public:

    quad3d(){}
    ~quad3d(){}
    quad3d(vec3d<T> left_up_val, vec3d<T> right_up_val, vec3d<T> right_down_val, vec3d<T> left_down_val)
    {
        lu=left_up_val;
        ru=right_up_val;
        rd=right_down_val;
        ld=left_down_val;
    }
    quad3d(const quad3d &val){*this=val;}
    quad3d& operator=(const quad3d &val){lu=val.lu;ru=val.ru;rd=val.rd;ld=val.ld;return *this;}

    vec3d<T> operator[](int i) const
    {
        switch(i&3)
        {
        case 0: return lu;
        case 1: return ru;
        case 2: return rd;
        default: break;
        }
        return ld;
    }

    vec3d<T>& leftUp() {return lu;}
    vec3d<T>& leftDown() {return ld;}
    vec3d<T>& rightUp() {return ru;}
    vec3d<T>& rightDown() {return rd;}

    const vec3d<T>& leftUp() const {return lu;}
    const vec3d<T>& leftDown() const {return ld;}
    const vec3d<T>& rightUp() const {return ru;}
    const vec3d<T>& rightDown() const {return rd;}

private:
    vec3d<T> lu,ru,rd,ld;
};

} //namespace alt

#endif
 
