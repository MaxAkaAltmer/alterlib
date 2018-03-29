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

#include "math_vec.h"

#include <math.h>
#include <string.h>

/*
int _matherrl (struct _exceptionl *a)  //обработка исключений математики
{
  if (a->type == DOMAIN)
  {
        if(!strcmp(a->name,"atan2l"))
        {
                a->retval = 0.0;
                return 1;
        }
  }
  return 0;
}
*/

/*int _matherr (struct _exception *a)  //обработка исключений математики
{
  if (a->type == DOMAIN)
  {
        if(!strcmp(a->name,"atan2"))
        {
                a->retval = 0.0;
                return 1;
        }
  }
  return 0;
}*/

#ifndef ANDROID_NDK
real80  _vm_cos(real80 val){return cosl(val);}
real80  _vm_acos(real80 val){return acosl(val);}
real80  _vm_asin(real80 val){return asinl(val);}
real80  _vm_sin(real80 val){return sinl(val);}
real80  _vm_sqrt(real80 val){return sqrtl(val);}
real80  _vm_atan2(real80 y, real80 x){return atan2l(y,x);}
real80  _vm_fmod(real80 x, real80 y){return fmodl(x,y);}
#endif

real64  _vm_cos(real64 val){return cos(val);}
real32  _vm_cos(real32 val){return cos(val);}

real64  _vm_acos(real64 val){return acos(val);}
real32  _vm_acos(real32 val){return acos(val);}

real64  _vm_asin(real64 val){return asin(val);}
real32  _vm_asin(real32 val){return asin(val);}

real64  _vm_sin(real64 val){return sin(val);}
real32  _vm_sin(real32 val){return sin(val);}

real64  _vm_sqrt(real64 val){return sqrt(val);}
real32  _vm_sqrt(real32 val){return sqrt(val);}

real64  _vm_atan2(real64 y, real64 x){return atan2(y,x);}
real32  _vm_atan2(real32 y, real32 x){return atan2(y,x);}

real64  _vm_fmod(real64 x, real64 y){return fmod(x,y);}
real32  _vm_fmod(real32 x, real32 y){return fmod(x,y);}
