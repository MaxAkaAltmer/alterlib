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

#include "aobject.h"

using namespace alt;

object::object()
{
    m_parent = nullptr;
    m_name="root";
}

object::~object()
{
    if(m_parent) //есть родитель?
    { //чистим ссылки
        for(int i=0;i<m_parent->m_item_order.size();i++)
        {
            if(m_parent->m_item_order[i]==this)
            {
                m_parent->m_item_order.cut(i);
                break;
            }
        }
        if(m_parent->m_items.contains(m_name))
        {
            array<object*> *list= &m_parent->m_items[m_name];
            for(int i=0;i<list->size();i++)
            {
                if((*list)[i]==this)
                {
                    list->fastCut(i);
                    if(!list->size())
                        m_parent->m_items.remove(m_name);
                    break;
                }
            }
        }
    }
    clear();
}

object::object(const object &val)
{
    m_parent=nullptr;
    *this=val;
}

object::object(const string &name)
{
    m_parent=nullptr;
    m_name=name;
}

object& object::operator=(const object &val)
{
    clear();

    if(!m_parent)m_name=val.m_name;

    array<array<object*> > list=val.m_items.values();
    array<string> listk=val.m_items.keys();
    for(int i=0;i<list.size();i++)
    {
        m_items.insert(listk[i],array<object*>());
        for(int j=0;j<list[i].size();j++)
        {
            object *obj=new object(*list[i][j]);
            obj->m_parent=this;
            m_items[listk[i]].append(obj);
            m_item_order.append(obj);
        }
    }

    array<variant> atvlist = val.m_attributes.values();
    array<string> atklist = val.m_attributes.keys();
    for(int i=0;i<atvlist.size();i++)
    {
        //!!!deep copy needed???
        m_attributes.insert(atklist[i],atvlist[i]);
    }

    m_attr_order.append(val.m_attr_order);

    return *this;
}

void object::moveBefore(object *node, object *before)
{
    int inod=-1,ibef=-1;
    for(int i=0;i<m_item_order.size();i++)
    {
        if(m_item_order[i]==node)inod=i;
        if(m_item_order[i]==before)ibef=i;
    }

    if(inod<0 || ibef<0)return;

    if(inod<=ibef)
    {
        for(int i=inod+1;i<ibef;i++)
        {
            m_item_order[i-1]=m_item_order[i];
        }
        m_item_order[ibef-1]=node;
    }
    else
    {
        for(int i=inod;i>ibef;i--)
        {
            m_item_order[i]=m_item_order[i-1];
        }
        m_item_order[ibef]=node;
    }
}

void object::clear()
{
    m_attr_order.clear();
    m_item_order.clear();

    m_attributes.clear();
    array<array<object*> > list=m_items.values();
    for(int i=0;i<list.size();i++)
    {
        for(int j=0;j<list[i].size();j++)
        {
            delete list[i][j];
        }
    }
    m_items.clear();
}

void object::remAttr(const string &name)
{
    if(m_name.isEmpty() || name.isEmpty())
        return;
    for(int i=0;i<m_attr_order.size();i++)
    {
        if(m_attr_order[i]==name)
        {
            m_attr_order.cut(i);
            break;
        }
    }
    m_attributes.remove(name);
}

void object::setAttr(const string &name, const variant &val)
{
    if(m_name.isEmpty() || name.isEmpty())
        return;

    if(!m_attributes.contains(name))
    {
        m_attr_order.append(name);
    }
    m_attributes[name]=val;
}

void object::setAttributes(const hash<string, variant> &list)
{
    if(m_name.isEmpty())
        return;
    for(int i=0;i<list.size();i++)
        setAttr(list.keys()[i],list.values()[i]);
}

bool object::checkAttributes(const hash<string, variant> &list)
{
    if(m_name.isEmpty())
        return false;

    for(int i=0;i<list.size();i++)
    {
        if(list.keys()[i].isEmpty())
            return false;
        if(!m_attributes.contains(list.keys()[i]))
            return false;
        const variant& a=attr(list.keys()[i]);
        const variant& que=list.values()[i];

        if(a!=que)return false;
    }
    return true;
}

object* object::existedItem(const string &path, std::function<bool(object*)> check)
{
    if(m_name.isEmpty())
        return nullptr;

    array<string> nodes = path.split('/',true);

    object *curr = this;

    if(!nodes.size() && !check(curr))
        return nullptr;

    for(int i=0;i<nodes.size();i++)
    {
        if(!curr->m_items.contains(nodes[i]))
        {
            return nullptr;
        }
        else
        {
            const array<object*>& arr = curr->m_items[nodes[i]];
            int j=0;
            for(;j<arr.size();j++)
            {
                if(i != nodes.size()-1)
                {
                    curr = arr[j];
                    break;
                }
                if(check(arr[j]))
                {
                    curr = arr[j];
                    break;
                }
            }
            if(j == arr.size())
            {
                return nullptr;
            }
        }
    }

    return curr;
}

object* object::existedItem(const string &path, uint index)
{
    if(m_name.isEmpty())
        return nullptr;

    array<string> nodes = path.split('/',true);

    object *curr = this;

    if(!nodes.size() && index)
        return nullptr;

    for(int i=0;i<nodes.size();i++)
    {
        if(i == nodes.size()-1)
        {
            int append = index + 1 - curr->m_items[nodes[i]].size();
            if(append > 0)
            {
                return nullptr;
            }
            curr = curr->m_items[nodes[i]][index];
        }
        else if(!curr->m_items.contains(nodes[i]))
        {
            return nullptr;
        }
        else
        {
            curr = curr->m_items[nodes[i]][0];
        }
    }

    return curr;
}

object* object::item(const string &path, uint index)
{
    if(m_name.isEmpty())
        return nullptr;

    array<string> nodes = path.split('/',true);

    object *curr = this;

    for(int i=0;i<nodes.size();i++)
    {
        if(i == nodes.size()-1)
        {
            int append = index + 1 - curr->m_items[nodes[i]].size();
            while(append > 0)
            {
                object *tmp = new object(nodes[i]);
                tmp->m_parent = curr;
                curr->m_items[nodes[i]].append(tmp);
                curr->m_item_order.append(tmp);
                append--;
            }
            curr = curr->m_items[nodes[i]][index];
        }
        else if(!curr->m_items.contains(nodes[i]))
        {
            object *tmp = new object(nodes[i]);
            tmp->m_parent = curr;
            curr->m_items[nodes[i]].append(tmp);
            curr->m_item_order.append(tmp);
            curr = tmp;
        }
        else
        {
            curr = curr->m_items[nodes[i]][0];
        }
    }

    return curr;
}

object* object::item(const string &path, const hash<string, variant> &with_attribs)
{
    if(m_name.isEmpty())
        return nullptr;

    array<string> nodes = path.split('/',true);

    object *curr = this;

    for(int i=0;i<nodes.size();i++)
    {
        if(!curr->m_items.contains(nodes[i]))
        {
            object *tmp = new object(nodes[i]);
            tmp->m_parent = curr;
            curr->m_items[nodes[i]].append(tmp);
            curr->m_item_order.append(tmp);
            curr = tmp;
        }
        else
        {
            const array<object*>& arr = curr->m_items[nodes[i]];
            int j=0;
            for(;j<arr.size();j++)
            {
                if(i != nodes.size()-1)
                {
                    curr = arr[j];
                    break;
                }
                if(arr[j]->checkAttributes(with_attribs))
                {
                    curr = arr[j];
                    break;
                }
            }
            if(j == arr.size())
            {
                object *tmp = new object(nodes[i]);
                tmp->m_parent = curr;
                curr->m_items[nodes[i]].append(tmp);
                curr->m_item_order.append(tmp);
                curr = tmp;
            }
        }
    }

    curr->setAttributes(with_attribs);

    return curr;
}

array<object*> object::listItems(const string &path)
{
    if(m_name.isEmpty())
        return array<object*>();

    array<string> nodes = path.split('/',true);

    object *curr = this;

    if(!nodes.size())
        return m_item_order;

    for(int i=0;i<nodes.size();i++)
    {
        if(!curr->m_items.contains(nodes[i]))
        {
            return array<object*>();
        }
        else
        {
            if(i == nodes.size()-1)
                return curr->m_items[nodes[i]];

            curr = curr->m_items[nodes[i]][0];
        }
    }

    return array<object*>();
}

object* object::addItem(const object &val)
{
    if(m_name.isEmpty())
        return nullptr;

    object *obj=new object(val);
    obj->m_parent=this;
    m_items[obj->name()].append(obj);
    m_item_order.append(obj);
    return obj;
}

object* object::addItem(const string &name)
{
    if(m_name.isEmpty())
        return nullptr;

    object *obj=new object(name);
    obj->m_parent=this;
    m_items[obj->name()].append(obj);
    m_item_order.append(obj);
    return obj;
}

void object::remItems(const string &path, std::function<bool(object*)> check)
{
    if(m_name.isEmpty())
        return;

    array<object*> list=listItems(path);
    for(int i=0;i<list.size();i++)
    {
        if(check(list[i]))
            delete list[i];
    }
}

void object::remItems(const string &path)
{
    if(m_name.isEmpty())
        return;

    array<object*> list=listItems(path);
    for(int i=0;i<list.size();i++)
    {
        delete list[i];
    }
}

variant object::content(const variant &defval, uint index)
{
    if(m_name.isEmpty())
    {
        if(!m_attributes.contains(string()))
            return defval;
        return m_attributes[string()];
    }

    uint size = 0;
    if(m_items.contains(string()))
        size = m_items[string()].size();

    if(size<=index)
        return defval;

    return m_items[string()][index]->m_attributes[string()];
}

void object::setContent(const variant &val, uint index)
{
    if(m_name.isEmpty())
    {
        m_attributes[string()] = val;
        return;
    }

    uint size = 0;
    if(m_items.contains(string()))
        size = m_items[string()].size();

    while(size<=index)
    {
        addContent(variant());
        size++;
    }

    m_items[string()][index]->m_attributes[string()] = val;
}

void object::addContent(const variant &val)
{
    if(m_name.isEmpty())
    {
        m_attributes[string()] += val;
        return;
    }

    object *obj=new object(string());
    obj->m_parent=this;
    m_items[string()].append(obj);
    m_item_order.append(obj);
    obj->m_attributes[string()] = val;
}

void object::remContent()
{
    if(m_name.isEmpty())
    {
        m_attributes[string()] = variant();
        return;
    }

    if(!m_items.contains(string()))
        return;
    array<object*> list = m_items[string()];
    for(int i=0;i<list.size();i++)
    {
        delete list[i];
    }
}
