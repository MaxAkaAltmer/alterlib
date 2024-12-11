#include "aobject_xml.h"
#include "afile.h"
#include "external/tinyxml/tinyxml.h"

using namespace alt;

static bool _ameta_rxml_parcer_recu(object *obj, TiXmlElement *el)
{
    if(!el)return true;
    string node_name=el->Value();
    if(node_name.isEmpty())
        return true;

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
        obj->setAttr(attribs_names[keys[i]],attribs_values[keys[i]]);
    }

    //грузим потомков
    TiXmlNode *child=el->FirstChild();
    while(child)
    {
        if(child->ToText())
        {
            TiXmlText *txt = child->ToText();
            string text = txt->Value();
            obj->addContent(text);
        }
        else if(child->ToElement())
        {
            TiXmlElement *el = child->ToElement();
            string child_name=el->Value();
            if(!_ameta_rxml_parcer_recu(obj->addItem(child_name),el))
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
    TiXmlDocument doc;

    doc.Parse(data());
    if(doc.ErrorId())
    {
        return false;
    }

    return _ameta_rxml_parcer_recu(obj,doc.FirstChildElement());
}

static bool _ameta_rxml_streamer_recu(object *obj, TiXmlElement *el)
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
            TiXmlText *text=new TiXmlText(items[i]->content().toString()());
            el->LinkEndChild(text);
        }
        else
        {
            TiXmlElement *element = new TiXmlElement( items[i]->name()() );
            _ameta_rxml_streamer_recu(items[i],element);
            el->LinkEndChild(element);
        }
    }

    return true;
}

static bool _ameta_rxml_streamer(object *obj, string &fname, bool standalone)
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

bool object_xml::save(string fname, object *root, bool standalone)
{
    return _ameta_rxml_streamer(root,fname,standalone);
}

bool object_xml::save(string fname, bool standalone)
{
    return _ameta_rxml_streamer(root_ptr,fname,standalone);
}
