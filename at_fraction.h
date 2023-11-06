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


#ifndef FRACTION_H
#define FRACTION_H

#include "atypes.h"
#include "astring.h"

namespace alt {

    template<class T>
    class fraction
    {
    private:
        T num = 0;
        T den = 0;

        T evclid_gcd(T a, T b)
        {
            return b ? evclid_gcd(b, a % b) : a;
        }

    public:
        fraction() {}
        fraction(T n) { num = n; den = 1; }
        fraction(T n, T d) { num = n; den = d; }
        template<class I>
        fraction(const fraction<I> &val) { num = val.numerator();  den = val.denominator(); }

        void fix(bool sign_only = false)
        {
            if(!num)
            {
                den = 0;
                return;
            }
            if(!sign_only)
            {
                T d = evclid_gcd(den,num);
                if(d)
                {
                    num/=d;
                    den/=d;
                }
            }
            if(den<0 && num)
            {
                den = -den;
                num = -num;
            }
        }

        bool inf()
        {
            return !num && den;
        }

        T numerator() const { return num; }
        void setNumerator(T n) { num = n; }
        T denominator() const { return den; }
        void setDenominator(T d) { den = d; }

        template<class F>
        static fraction from(F val)
        {
            fraction rv;
            rv.den = 1;
            uint32 max_count = imath::bsrT(T(-1)) - (utils::signed_type<T>()?1:0);
            while(val != ((F)((T)val)) && max_count>0)
            {
                rv.den *= 2;
                val *= 2;
                max_count--;
            }
            rv.num = val;
            rv.fix();
            return rv;
        }

        template<class F>
        F to() const
        {
            return F(num)/F(den);
        }

        fraction& operator=(const T &val)
        {
            num = val.num;
            den = 1;
            return *this;
        }
        fraction& operator=(const fraction &val)
        {
            num = val.num;
            den = val.den;
            return *this;
        }

        fraction& operator+=(const T &val)
        {
            if(!num)
            {
                *this = val;
                return *this;
            }
            num += den*val;
            return *this;
        }
        fraction& operator+=(const fraction &val)
        {
            if(!num)
            {
                *this = val;
                return *this;
            }
            if(!val.num)
                return *this;

            if(den == val.den)
            {
                num += val.num;
            }
            else
            {
                num = val.den*num + val.num*den;
                den *= val.den;
            }
            return *this;
        }
        fraction& operator-=(const T &val)
        {
            if(!num)
            {
                *this = -val;
                return *this;
            }
            num += den*val;
            return *this;
        }
        fraction& operator-=(const fraction &val)
        {
            if(!num)
            {
                *this = -val;
                return *this;
            }
            if(!val.num)
                return *this;

            if(den == val.den)
            {
                num -= val.num;
            }
            else
            {
                num = val.den*num - val.num*den;
                den *= val.den;
            }
            return *this;
        }
        fraction& operator*=(const T &val)
        {
            num *= val;
            return *this;
        }
        fraction& operator*=(const fraction &val)
        {
            num *= val.num;
            den *= val.den;
            return *this;
        }
        fraction& operator/=(const T &val)
        {
            den *= val;
            return *this;
        }
        fraction& operator/=(const fraction &val)
        {
            num *= val.den;
            den *= val.num;
            return *this;
        }
        fraction operator+(const fraction &val) const
        {
            fraction rv;
            if(!num)
                return val;
            if(!val.num)
                return *this;
            if(den == val.den)
            {
                rv.setNumerator(num + val.num);
                rv.setDenominator(den);
            }
            else
            {
                rv.setNumerator(val.den*num + val.num*den);
                rv.setDenominator(den * val.den);
            }
            return rv;
        }
        fraction operator-(const fraction &val) const
        {
            fraction rv;
            if(!num)
                return -val;
            if(!val.num)
                return *this;

            if(den == val.den)
            {
                rv.setNumerator(num - val.num);
                rv.setDenominator(den);
            }
            else
            {
                rv.setNumerator(val.den*num - val.num*den);
                rv.setDenominator(den * val.den);
            }
            return rv;
        }
        fraction operator*(const fraction &val) const
        {
            return fraction(num*val.num,den*val.den);
        }
        fraction operator/(const fraction &val) const
        {
            return fraction(num*val.den,den*val.num);
        }
        fraction operator-() const
        {
            return fraction(-num,den);
        }
        const fraction& operator++()
        {
            num += den;
            return *this;
        }
        const fraction& operator--()
        {
            num -= den;
            return *this;
        }
        fraction operator++(int)
        {
            fraction rv = *this;;
            num += den;
            return rv;
        }
        fraction& operator--(int)
        {
            fraction rv = *this;;
            num -= den;
            return rv;
        }

        fraction& preIncNumerator()
        {
            num ++;
            return *this;
        }
        fraction& preDecNumerator()
        {
            num --;
            return *this;
        }

        bool operator==(const fraction &val) const
        {
            return (num*val.den) == (den*val.num);
        }
        bool operator !=(const fraction &val) const
        {
            return !(*this == val);
        }
        bool operator<(const fraction &val) const
        {
            return (num*val.den) < (den*val.num);
        }
        bool operator<=(const fraction &val) const
        {
            return (num*val.den) <= (den*val.num);
        }
        bool operator>(const fraction &val) const
        {
            return !(*this <= val);
        }
        bool operator>=(const fraction &val) const
        {
            return !(*this < val);
        }

        string toString(bool get_integer = false)
        {
            fix(true);
            if(den == 1 || (den==0 && num==0))
                return string::fromInt(num);
            if(get_integer && num>den)
            {
                if(num<0)
                    return string::fromInt(num/den)+string::fromInt(num%den)+"/"+string::fromInt(den);
                return string::fromInt(num/den)+"+"+string::fromInt(num%den)+"/"+string::fromInt(den);
            }
            return string::fromInt(num)+"/"+string::fromInt(den);
        }

    };

}

#endif // FRACTION_H
