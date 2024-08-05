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

#ifndef AXOBJECT_H
#define AXOBJECT_H

#include "avariant.h"

namespace alt {

    //в некоторой мере повторяет структуру XML, данные упорядоченны и быстро доступны
    class xobject
    {
    public:

        //создание и удаление объекта
        xobject();
        xobject(const xobject &val);
        xobject(const string &name);
        ~xobject();
        xobject& operator=(const xobject &val);
        void clear(bool with_name=false);
        bool load(string fname);
        bool save(string fname, bool standalone=false);

        //работа с иерархией
        string getTransformPathToMe(const string &tattrname);
        string getPathToMe(bool with_order=false);
        string getPathToMe(xobject *stop_node, bool with_order=false);
        xobject* Parent(){return m_parent;}
        void moveBefor(xobject *node, xobject *befor);

        //работа с атрибутами
        variant& attribute(string name, const variant &defval=variant()) //создаст объект
        {
            if(!m_attributes.contains(name))
            {
                m_attributes[name]=defval;
                m_attr_order.append(name);
            }
            return m_attributes[name];
        }
        bool haveAttribute(string name){return m_attributes.contains(name);}
        xobject& setAttribute(string name, variant val);
        void setAttributes(hash<string, variant> &list);
        bool haveAttributes(hash<string, variant> &list);
        void delAttribute(string name);

        /****************************************************************************************************
          Обращение к элеметам в виде простейшего запроса
          TODO: имя[I]{C}/имя[I]{C}/.../имя[I]{C}
            [I] - индекс среди равных
                el[15] - 15-й элемент
            {C} - указывает условие эквивалентности атрибутов
                пример: el{a==12, b=='test'} - атрибуты должны иметь соответствующее значение
        ****************************************************************************************************/
        xobject* item(string path); //дает указатель для быстрого доступа (или создаст объект[ы])
        xobject* existedItem(string path);
        array<xobject*> itemList(string path); //даст список объектов
        xobject* addItem(const xobject &val, string path="");
        xobject* addItem(const string &name, string path="");
        void delItems(string path);

        static string makeCode(charx val);
        static string makeCodedString(string val, bool with_apostrofs=false);

        //основные атрибуты объекта
        string name(){return m_name;} //имя узла
        void setName(string val){m_name=val;}

        variant content(const variant &defval=variant()) //текстовое поле
        {
            if(m_items.size())return variant();
            if(!m_content.isValid())return defval;
            return m_content;
        }
        void setContent(variant val);

        enum NodeType
        {
            EMPTY,
            SIMPLE,
            COMPLEX
        };

        int type()
        {
            if(m_items.size())return COMPLEX;
            if(m_content.isValid())return SIMPLE;
            return EMPTY;
        }

        //списки атрибутов и элементов
        array<string> listAttributes(){return m_attr_order;}
        array<string> listItemNames(){return m_items.keys();}
        array<xobject*> listItemsByName(string name)
            {if(m_items.contains(name))return m_items[name];return array<xobject*>();}
        array<xobject*> listAllItems(){return m_item_order;}
        int countItems(string name){if(!m_items.contains(name))return 0; return m_items[name].size();}

    private:

        xobject *m_parent;
        string m_name;
        variant m_content; //если установить - дочерние элементы изчезают и наоборот
        hash<string, variant> m_attributes;
        hash<string, array<xobject*> > m_items;

        array<string> m_attr_order;
        array<xobject*> m_item_order;

        //парсинг запросов
        static string parcePath(string path, string &next);
        //парсинг элемента пути, вернет: 0..N - индекс, -1 - условие
        static int parceCondition(string &path_el, hash<string,variant> &val_list);
        static variant parceConst(string val);

    };

} // namespace alt

#endif // AXOBJECT_H
