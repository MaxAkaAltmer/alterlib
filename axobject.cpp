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

#include "axobject.h"
#include "afile.h"
#include "external/tinyxml/tinyxml.h"

using namespace alt;

bool _ameta_rxml_parcer_recu(xobject *obj, TiXmlElement *el)
{
    if(!el)return true;
    string node_name=el->Value();
    if(node_name.isEmpty())return true;

    obj->setName(node_name);

    TiXmlAttribute *attr = el->FirstAttribute();

    //позаботимся о корректном порядке аттрибутов
    hash<uint64,string> attribs_names;
    hash<uint64,string> attribs_values;
    while(attr)
    {

        string attr_name=attr->Name();
        if(attr_name.isEmpty())break;

        attribs_names.insert((((uint64)attr->Row())<<32)|attr->Column(),attr_name);
        string attr_val=attr->Value();
        attribs_values.insert((((uint64)attr->Row())<<32)|attr->Column(),attr_val);

        attr=attr->Next();
    }
    array<uint64> keys=attribs_names.keys();
    for(int i=0;i<keys.size();i++)
    {
        obj->setAttribute(attribs_names[keys[i]],attribs_values[keys[i]]);
    }

    //грузим потомков
    bool no_child=true;
    TiXmlElement *child=el->FirstChildElement();
    while(child)
    {
        string child_name=child->Value();
        if(!_ameta_rxml_parcer_recu(obj->addItem(child_name),child))
        {
            return false;
        }
        child = child->NextSiblingElement();
        no_child=false;
    }

    if(no_child)
    {
        //грузим текстовое содержимое
        string text = el->GetText();
        if(!el->isStrongClosed())
        {
            obj->setContent(text);
        }

    }

    return true;
}

bool _ameta_rxml_parcer(xobject *obj, string &data)
{
    TiXmlDocument doc;

    doc.Parse(data());
    if(doc.ErrorId())
    {
        return false;
    }

    return _ameta_rxml_parcer_recu(obj,doc.FirstChildElement());
}

bool _ameta_rxml_streamer_recu(xobject *obj, TiXmlElement *el)
{
    array<string> list=obj->listAttributes();
    for(int i=0;i<list.size();i++)
    {
        el->SetAttribute(list[i](),obj->attribute(list[i]).toString()());
    }

    if(obj->type()==xobject::SIMPLE)
    {
        TiXmlText *text=new TiXmlText(obj->content().toString()());
        el->LinkEndChild(text);
    }
    else
    {
        array<xobject*> items=obj->listAllItems();
        for(int i=0;i<items.size();i++)
        {
            TiXmlElement *element = new TiXmlElement( items[i]->name()() );
            _ameta_rxml_streamer_recu(items[i],element);
            el->LinkEndChild(element);
        }
    }

    return true;
}

bool _ameta_rxml_streamer(xobject *obj, string &fname, bool standalone)
{
    TiXmlDocument doc;

    TiXmlDeclaration * decl = new TiXmlDeclaration( "1.0", "utf-8", standalone?"yes":"" );
    TiXmlElement * element = new TiXmlElement( obj->name()() );

    if(!_ameta_rxml_streamer_recu(obj,element))return false;

    doc.LinkEndChild( decl );
    doc.LinkEndChild( element );

    TiXmlPrinter printer;

    doc.Accept(&printer);

    // Create a std::string and copy your document data in to the string
    string str = printer.CStr();

    if(!str.size())return false;

    file hand(fname);

    if(hand.open(file::OTruncate|file::OWriteOnly))
    {
        hand.write(str(),str.size());
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////

xobject::xobject()
{
    m_parent=NULL;
    m_name="root";
}

xobject::~xobject()
{
    if(m_parent) //есть родитель?
    { //чистим ссылки
        for(int i=0;i<m_parent->m_item_order.size();i++)
        {
            if(m_parent->m_item_order[i]==this)
            {
                m_parent->m_item_order.fastCut(i);
                break;
            }
        }
        if(m_parent->m_items.contains(m_name))
        {
            array<xobject*> *list=&m_parent->m_items[m_name];
            for(int i=0;i<list->size();i++)
            {
                if((*list)[i]==this)
                {
                    list->fastCut(i);
                    if(!list->size())m_parent->m_items.remove(m_name);
                    break;
                }
            }
        }
    }
    clear(true);
}

xobject::xobject(const xobject &val)
{
    m_parent=NULL;
    *this=val;
}

xobject::xobject(const string &name)
{
    m_parent=NULL;
    m_name=name;
}

xobject& xobject::operator=(const xobject &val)
{
    clear();

    if(!m_parent)m_name=val.m_name;
    m_content=val.m_content;

    array<array<xobject*> > list=val.m_items.values();
    array<string> listk=val.m_items.keys();
    for(int i=0;i<list.size();i++)
    {
        m_items.insert(listk[i],array<xobject*>());
        for(int j=0;j<list[i].size();j++)
        {
            xobject *obj=new xobject(*list[i][j]);
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

bool xobject::load(string fname)
{
    file file(fname);
    if(!file.open(file::OReadOnly))return false;

    string data = file.readText();
    file.close();

    clear();

    return _ameta_rxml_parcer(this,data);
}

bool xobject::save(string fname, bool standalone)
{
    return _ameta_rxml_streamer(this,fname,standalone);
}

void xobject::moveBefor(xobject *node, xobject *befor)
{
    int inod=-1,ibef=-1;
    for(int i=0;i<m_item_order.size();i++)
    {
        if(m_item_order[i]==node)inod=i;
        if(m_item_order[i]==befor)ibef=i;
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

string xobject::getPathToMe(xobject *stop_node, bool with_order)
{
    if(this==stop_node)return string();

    if(m_parent)
    {
        string rv=m_parent->getPathToMe(stop_node,with_order);
        if(!rv.isEmpty())rv+="/";
        if(!m_parent->m_items.contains(m_name))return m_name;
        array<xobject*> list=m_parent->m_items[m_name];
        for(int i=0;i<list.size();i++)
        {
            if(list[i]==this)
            {
                rv+=m_name;
                if(with_order)rv+="<"+string::fromInt(i)+">";
                return rv;
            }
        }
        return m_name;
    }
    else
    {
        return m_name;
    }
}

string xobject::getTransformPathToMe(const string &tattrname)
{
    if(!haveAttribute(tattrname))return string();
    if(m_parent)
    {
        string rv=m_parent->getTransformPathToMe(tattrname)+"/";
        if(!m_parent->m_items.contains(m_name))return string();
        array<xobject*> list=m_parent->m_items[m_name];
        for(int i=0;i<list.size();i++)
        {
            if(list[i]==this)
            {
                rv+=attribute(tattrname).toString();
                return rv;
            }
        }
        return string();
    }
    else
    {
        return string();
    }
}

string xobject::getPathToMe(bool with_order)
{
    if(m_parent)
    {
        string rv=m_parent->getPathToMe(with_order)+"/";
        if(!m_parent->m_items.contains(m_name))return m_name;
        array<xobject*> list=m_parent->m_items[m_name];
        for(int i=0;i<list.size();i++)
        {
            if(list[i]==this)
            {
                rv+=m_name;
                if(with_order)rv+="<"+string::fromInt(i)+">";
                return rv;
            }
        }
        return m_name;
    }
    else
    {
        return m_name;
    }
}

void xobject::clear(bool with_name)
{
    m_attr_order.clear();
    m_item_order.clear();

    if(with_name)m_name.clear();
    m_content.clear();
    m_attributes.clear();
    array<array<xobject*> > list=m_items.values();
    for(int i=0;i<list.size();i++)
    {
        for(int j=0;j<list[i].size();j++)
        {
            delete list[i][j];
        }
    }
    m_items.clear();
}

string xobject::parcePath(string path, string &next)
{
    int ind=path.indexOf('/');
    if(ind<0){next="";return path;}
    next=path.right(ind+1);
    return path.left(ind);
}

variant xobject::parceConst(string val)
{
    bool ok;
    if(val[0]!='\"') //число
    {        
        int ival=val.toInt<int>(10,&ok);
        if(ok)return ival;
        return variant();
    }
    else //строка
    {
        val=val.mid(1,val.size()-2);
        array<string> list = val.split('#');
        if(!list.size())return variant();
        string rv=list[0];
        for(int i=1;i<list.size();i++)
        {
            if(list[i].size())
            {
                rv.append_unicode(list[i].left(4).toInt<uint32>(16,&ok));
                rv+=list[i].right(4);
            }
            else
            {
                i++;
                if(i<list.size())rv+="#"+list[i];
            }
        }
        return rv;
    }
}

int xobject::parceCondition(string &path_el, hash<string, variant> &val_list)
{
    int start=path_el.indexOf('<');
    if(start<0)return 0;

    int end=path_el.indexOf('>',start+1);
    if(start==0 || end<0 || (start+1)==end)
    {
        path_el=path_el.left(start);
        return 0;
    }

    string tmp=path_el.mid(start+1,end-start-1);
    path_el=path_el.left(start);

    //проверяем на индекс
    bool ok;
    int index=tmp.toInt<int>(10,&ok);
    if(ok)return index+1;

    //парсим на условия
    array<string> list = tmp.split(',',true);
    if(!list.size())return 0;

    for(int i=0;i<list.size();i++)
    {
        array<string> av = list[i].split('=');
        if(av.size()!=2 || av[0].isEmpty())continue;
        val_list.insert(av[0].simplified(),parceConst(av[1].trimmed()));
    }

    if(!val_list.size())return 0;
    return -1; //проверять атрибуты
}

string xobject::makeCode(charx val)
{
    return "#"+string::fromIntFormat(val,4,16);
}

string xobject::makeCodedString(string val, bool with_apostrofs)
{
    string rv, rep(",=>\"/#");

    for(int i=0;i<val.size();i++)
    {
        if(rep.contains(val[i]))rv+=makeCode((uint8)val[i]);
        else rv.append(val[i]);
    }
    if(with_apostrofs)return "\""+rv+"\"";
    return rv;
}

void xobject::delAttribute(string name)
{
    for(int i=0;i<m_attr_order.size();i++)
    {
        if(m_attr_order[i]==name)
        {
            m_attr_order.fastCut(i);
            break;
        }
    }
    m_attributes.remove(name);
}

xobject& xobject::setAttribute(string name, variant val)
{
    if(!m_attributes.contains(name))
    {
        m_attr_order.append(name);
    }
    m_attributes[name]=val;

    return *this;
}

void xobject::setAttributes(hash<string,variant> &list)
{
    for(int i=0;i<list.size();i++)
        setAttribute(list.keys()[i],list.values()[i]);
}

bool xobject::haveAttributes(hash<string,variant> &list)
{
    for(int i=0;i<list.size();i++)
    {
        variant attr=attribute(list.keys()[i]);
        variant que=list.values()[i];
        if(attr!=que)return false;
    }
    return true;
}

xobject* xobject::existedItem(string path)
{
    string next;
    xobject * rv;

    path=parcePath(path,next);

    if(path.isEmpty())return this;

    //проверка условия
    hash<string, variant> val_list;
    int index=parceCondition(path,val_list);
    if(path.isEmpty())return this;
    if(!index) //безусловно
    {
        if(!m_items.contains(path))
        {
            return NULL;
        }
        else
        {
            rv=m_items[path][0];
        }
    }
    else if(index>0) //индексация
    {
        if(index>m_items[path].size())
        {
            return NULL;
        }
        rv=m_items[path][index-1];
    }
    else //условия атрибутов
    {
        if(!m_items.contains(path))
        {
            return NULL;
        }
        else
        {
            array<xobject*>  list=m_items[path];
            for(int i=0;i<list.size();i++)
            {
                if(list[i]->haveAttributes(val_list))
                    return list[i]->existedItem(next);
            }
            return NULL;
        }
    }
    return rv->existedItem(next);
}

xobject* xobject::item(string path)
{
    string next;
    xobject * rv;

    path=parcePath(path,next);

    if(path.isEmpty())return this;

    //проверка условия
    hash<string, variant> val_list;
    int index=parceCondition(path,val_list);
    if(path.isEmpty())return this;
    if(!index) //безусловно
    {
        if(!m_items.contains(path))
        {
            rv=new xobject(path);
            rv->m_parent=this;
            m_items[path].append(rv);
            m_item_order.append(rv);
        }
        else
        {
            rv=m_items[path][0];
        }
    }
    else if(index>0) //индексация
    {
        while(index>m_items[path].size())
        {
            rv=new xobject(path);
            rv->m_parent=this;
            m_items[path].append(rv);
            m_item_order.append(rv);
        }
        rv=m_items[path][index-1];
    }
    else //условия атрибутов
    {
        if(!m_items.contains(path))
        {
            rv=new xobject(path);
            rv->m_parent=this;
            m_items[path].append(rv);
            m_item_order.append(rv);
        }
        else
        {
            array<xobject*>  list=m_items[path];
            for(int i=0;i<list.size();i++)
            {
                if(list[i]->haveAttributes(val_list))return list[i]->item(next);
            }
            rv=new xobject(path);
            rv->m_parent=this;
            m_items[path].append(rv);
            m_item_order.append(rv);
        }
        rv->setAttributes(val_list);
    }
    return rv->item(next);
}

array<xobject *> xobject::itemList(string path)
{
    string next;
    array<xobject*> rv;

    path=parcePath(path,next);
    hash<string, variant> val_list;
    int index=parceCondition(path,val_list);

    if(path.isEmpty())return rv;

    if(!index) //безусловно
    {
        if(m_items.contains(path))
        {
            array<xobject*> list=m_items[path];
            for(int i=0;i<list.size();i++)
            {
                if(next.isEmpty())rv.append(list[i]);
                else rv.append(list[i]->itemList(next));
            }
        }
    }
    else if(index>0) //индексация
    {
        if(m_items.contains(path))
        {
            if(index<=m_items[path].size())
            {
                if(next.isEmpty()) rv.append(m_items[path][index-1]);
                else rv.append(m_items[path][index-1]->itemList(next));
            }
        }
    }
    else //условия атрибутов
    {
        if(m_items.contains(path))
        {
            array<xobject*> list=m_items[path];
            for(int i=0;i<list.size();i++)
            {
                if(list[i]->haveAttributes(val_list))
                {
                    if(next.isEmpty())rv.append(list[i]);
                    else rv.append(list[i]->itemList(next));
                }
            }
        }
    }
    return rv;

}

xobject* xobject::addItem(const xobject &val, string path)
{
    xobject *tmp,*obj=new xobject(val);
    tmp=item(path);
    obj->m_parent=tmp;
    tmp->m_items[obj->name()].append(obj);
    tmp->m_item_order.append(obj);
    return obj;
}

xobject* xobject::addItem(const string &name, string path)
{
    xobject *tmp,*obj=new xobject(name);
    tmp=item(path);
    obj->m_parent=tmp;
    tmp->m_items[obj->name()].append(obj);
    tmp->m_item_order.append(obj);
    return obj;
}

void xobject::delItems(string path)
{
    array<xobject*> list=itemList(path);
    for(int i=0;i<list.size();i++)
    {
        delete list[i];
    }
}

void xobject::setContent(variant val)
{
    array<array<xobject*> > list=m_items.values();
    for(int i=0;i<list.size();i++)
    {
        for(int j=0;j<list[i].size();j++)
        {
            delete list[i][j];
        }
    }
    m_content=val;
}
