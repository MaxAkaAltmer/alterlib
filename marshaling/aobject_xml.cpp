/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2024-2025 Maxim L. Grishin  (altmer@arts-union.ru)

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


#include "aobject_xml.h"
#include "../afile.h"
#include "../external/tinyxml2/tinyxml2.h"

using namespace alt;

static bool _ameta_rxml_parcer_recu(object *obj, tinyxml2::XMLElement *el)
{
    if(!el)return true;
    string node_name=el->Value();
    if(node_name.isEmpty())
        return true;

    obj->setName(node_name);

    const tinyxml2::XMLAttribute *attr = el->FirstAttribute();

    //позаботимся о корректном порядке аттрибутов
    while(attr)
    {
        obj->setAttr(attr->Name(),attr->Value());
        attr=attr->Next();
    }

    //грузим потомков
    tinyxml2::XMLNode *child=el->FirstChild();
    while(child)
    {
        if(child->ToText())
        {
            tinyxml2::XMLText *txt = child->ToText();
            string text = txt->Value();
            obj->addContent(text);
        }
        else if(child->ToElement())
        {
            tinyxml2::XMLElement *el_next_level = child->ToElement();
            string child_name=el_next_level->Value();
            if(!_ameta_rxml_parcer_recu(obj->addItem(child_name),el_next_level))
            {
                return false;
            }
        }
        child = child->NextSibling();
    }

    return true;
}

static bool _ameta_rxml_parcer(object *obj, string &data)
{
    tinyxml2::XMLDocument doc;

    doc.Parse(data());
    if(doc.Error())
    {
        return false;
    }

    return _ameta_rxml_parcer_recu(obj,doc.FirstChildElement());
}

static bool _ameta_rxml_streamer_recu(tinyxml2::XMLDocument *doc, object *obj, tinyxml2::XMLElement *el)
{
    array<string> list=obj->listAttributes();
    for(int i=0;i<list.size();i++)
    {
        el->SetAttribute(list[i](),obj->attr(list[i]).toString()());
    }

    array<object*> items=obj->listItems(string());
    for(int i=0;i<items.size();i++)
    {
        if(items[i]->name().isEmpty())
        {
            tinyxml2::XMLText *text= doc->NewText(items[i]->content().toString()());
            el->LinkEndChild(text);
        }
        else
        {
            tinyxml2::XMLElement *element = doc->NewElement( items[i]->name()() );
            _ameta_rxml_streamer_recu(doc,items[i],element);
            el->LinkEndChild(element);
        }
    }

    return true;
}

static bool _ameta_rxml_streamer(object *obj, string &fname)
{
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLDeclaration * decl = doc.NewDeclaration();
    tinyxml2::XMLElement * element = doc.NewElement( obj->name()() );

    if(!_ameta_rxml_streamer_recu(&doc,obj,element))
        return false;

    doc.LinkEndChild( decl );
    doc.LinkEndChild( element );

    tinyxml2::XMLPrinter printer;

    doc.Accept(&printer);

    // Create a std::string and copy your document data in to the string
    string str = printer.CStr();

    if(!str.size())
        return false;

    file hand(fname);

    if(hand.open(file::OTruncate|file::OWriteOnly))
    {
        hand.write(str(),str.size());
    }

    return false;
}

object_xml::object_xml()
{
    root_ptr = new object;
}

object_xml::~object_xml()
{
    delete root_ptr;
}

void object_xml::clear()
{
    delete root_ptr;
    root_ptr = new object;
}

bool object_xml::load(string fname)
{
    file file(fname);
    if(!file.open(file::OReadOnly))return false;

    string data = file.readText();
    file.close();

    clear();

    return _ameta_rxml_parcer(root_ptr,data);
}

bool object_xml::save(string fname, object *root)
{
    return _ameta_rxml_streamer(root,fname);
}

bool object_xml::save(string fname)
{
    return _ameta_rxml_streamer(root_ptr,fname);
}
