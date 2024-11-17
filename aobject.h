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

#ifndef AOBJECT_H
#define AOBJECT_H

#include "avariant.h"
#include <functional>

namespace alt {

    class object
    {
    public:

        //General stuff

        object();
        object(const object &val);
        object(const string &name);
        ~object();

        object& operator=(const object &val);
        void clear();

        object* parent(){return m_parent;}
        void moveBefore(object *node, object *before);

        //Attributes stuff

        variant attr(const string &name, const variant &defval=variant())
        { //will create attribute if not exists
            if(name.isEmpty() || m_name.isEmpty())
                return defval;
            if(!m_attributes.contains(name))
                return defval;
            return m_attributes[name];
        }
        bool hasAttr(const string &name) const
        {
            if(m_name.isEmpty())
                return false;
            return m_attributes.contains(name);
        }
        bool checkAttr(const string &name, const variant &val)
        {
            if(name.isEmpty() || m_name.isEmpty())
                return false;
            if(!m_attributes.contains(name))
                return false;
            if(m_attributes[name] == val)
                return true;
            return false;
        }
        void setAttr(const string &name, const variant &val);
        void remAttr(const string &name);

        hash<string, variant> attributes() { if(m_name.isEmpty()) return hash<string, variant>(); return m_attributes; }
        void setAttributes(const hash<string, variant> &list);
        bool checkAttributes(const hash<string, variant> &list);
        array<string> listAttributes() { if(m_name.isEmpty()) return array<string>(); return m_attr_order; }

        //Items stuff

        //call item(...) will create object[s] if not exists
        object* item(const string &path, const hash<string, variant> &with_attribs);
        object* item(const string &path, uint index = 0);

        object* existedItem(const string &path, std::function<bool(object*)> check);
        object* existedItem(const string &path, uint index = 0);

        array<object*> listItems(const string &path);
        object* addItem(const object &val);
        object* addItem(const string &name);
        void remItems(const string &path); //for single item - just delete object
        void remItems(const string &path, std::function<bool(object*)> check);

        array<string> itemNames()
        {
            array<string> rv = m_items.keys();
            rv.removeValue(string());
            return rv;
        }

        //Node stuff

        const string& name(){return m_name;}
        void setName(const string &val)
        {
            if(val.isEmpty())
                return;
            m_name=val;
        }

        variant content(const variant &defval=variant(), uint index = 0);
        void setContent(const variant &val, uint index = 0);
        void addContent(const variant &val);
        void remContent();

    private:

        object *m_parent;
        string m_name;
        hash<string, variant> m_attributes;
        hash<string, array<object*> > m_items;

        //we need to keep order in documents in some cases important and useful for version control systems
        array<string> m_attr_order;
        array<object*> m_item_order;

    };

} // namespace alt

#endif // AOBJECT_H
