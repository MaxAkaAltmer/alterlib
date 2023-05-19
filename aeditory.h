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

#ifndef AEDITORY_H
#define AEDITORY_H

#include "atypes.h"
#include "at_array.h"
#include "avariant.h"
#include "adelegate.h"

namespace alt {

    typedef dualVal<variant,variant> dualVar;
    typedef trioVal<variant,variant,variant> tribleVar;

    class hexEditObject
    {
    public:
        hexEditObject(){object=NULL;}
        hexEditObject(const hexEditObject &val){object=val.object;method=val.method;user_val=val.user_val;}
        hexEditObject& operator=(const hexEditObject &val)
        {
            object=val.object;
            method=val.method;
            user_val=val.user_val;
            return *this;
        }

        template<typename OT>
        hexEditObject(OT *obj,intx (OT::*proc)(void*,int,uintx,intx), void *user)
        {
            object=static_cast<alt::delegateBase*>(obj);
            method=static_cast<intx (alt::delegateBase::*)(void*,int,uintx,intx)>(proc);
            user_val=user;
        }
        ~hexEditObject(){}

        enum Commands
        {
            SIZE, GET, SET,
            COLOR, BASE,
            EXIT
        };

        intx operator()(int code, uintx addr, intx val)
        {
            if(!object)return 0;
            return (object->*method)(user_val,code,addr,val);
        }

    private:
        alt::delegateBase *object;
        intx (alt::delegateBase::*method)(void*,int,uintx,intx);
        void *user_val;
    };

    class editory
    {
    public:

        editory();
        virtual ~editory();

        //размер документа в символах
        virtual intx ed_width(){return -1;}
        virtual intx ed_height(){return 0;}
        virtual uint32 ed_defaultFill(){return 0;}
        virtual uint32 ed_defaultColor(){return 0;}

        //структура строки
        struct edLineData
        {
            uintx index; //реальный индекс строки, поскольку строки могут быть разной высоты!
            uintx count; //высота строки в символах
            uint32 background; //подсветка строки

            //левый:    указатель или данные   => изображение,
            //          строка                 => строка,
            //          целое или с запятой    => пробелы
            //               (для отридцательных позиция: с запятой - абсолютная, целое - относительно последнего слова)
            //средний:  [Если пусто - переход по Ctrl+Click запрещен]
            //          целое - номер строки для реализации перехода
            //          прочее - пользовательский идентификатор
            //правый:   [Если пусто - DblClick-выделение запрещено]
            //          цвет в случае строки (color, fill - подложка),
            //          цвет в случае пробелов, (целое - цвет подложки)
            //          параметры в случае изображения (w,h,count - символов строки)
            array<tribleVar> words;

            //todo: рассчет позиции здесь! И вообще все отрефакторить!
        };

        //запрос содержимого строки
        virtual edLineData* ed_line(uintx index){return NULL;}

        enum Markers
        {
            BRANCH,
            BOOKMARK,
            BREAKPOINT,
            READTRAP,
            WRITETRAP
        };

        //графические маркеры
        struct edMarkers
        {
            uintx type;
            uintx up,down,left,right;
            variant inf;
        };
        virtual array<edMarkers>* ed_markers(uintx start, uintx size){return NULL;}

        virtual void setCommentary(intx line, string comment)
        {
            string key = getLineCommentKey(line);
            if(key.isEmpty()) return;
            if(comment.isEmpty())
                commentary_map.remove(key);
            else
                commentary_map[key] = comment;
        }

        virtual string defaultCommentary(intx line)
        {
            return string();
        }

        string commentary(intx line)
        {
            string key = getLineCommentKey(line);
            if(key.isEmpty()) return string();
            if(!commentary_map.contains(key))
                return defaultCommentary(line);
            return commentary_map[key];
        }
        void setCommentaryMap(alt::hash<string,string> map) {commentary_map = map;}
        alt::hash<string,string> commentaryMap() {return commentary_map;}

        virtual bool onDoubleClick(variant middle) { return false; }
        virtual bool onControlClick(variant middle) { return false; }

    protected:

        //комментарии
        alt::hash<string,string> commentary_map;
        virtual string getLineCommentKey(intx line);

    };

} // namespace alt

#endif // AEDITORY_H
