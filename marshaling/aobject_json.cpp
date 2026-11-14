/***********************************************************************

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

***********************************************************************/


#include "aobject_json.h"
#include "../afile.h"
#include "../external/json/single_include/nlohmann/json.hpp"

#include <string>
#include <utility>

using namespace alt;

//ordered_json_attrs keeps insertion order of both ordinary members and
//attributes, which lets us round-trip them between alt::object and JSON.
using json_t = nlohmann::ordered_json_attrs;

//----------- helpers: alt::string <-> std::string --------------------

static std::string to_std(const string &s)
{
    return std::string(s(), (size_t)s.size());
}

static string from_std(const std::string &s)
{
    return string::fromFix(s.data(), (int)s.size());
}

//----------- attribute name/value predicates -------------------------

//An attribute name has to be a bare C++ identifier. true/false/null are read
//back as literals by the lexer, so they cannot serve as attribute names.
static bool is_attr_name(const string &n)
{
    std::string s = to_std(n);
    if(s.empty()) return false;
    char c = s[0];
    if(!(c=='_' || (c>='a'&&c<='z') || (c>='A'&&c<='Z'))) return false;
    for(size_t i=1;i<s.size();i++)
    {
        c = s[i];
        if(!(c=='_' || (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9')))
            return false;
    }
    return !(s=="true" || s=="false" || s=="null");
}

//Attributes carry scalar values only - no objects, no arrays.
static bool is_attr_value(const json_t &v)
{
    return v.is_null() || v.is_boolean() || v.is_number() || v.is_string();
}

//----------- variant -> json ----------------------------------------

static json_t variant_to_json(const variant &v)
{
    if(!v.isValid())    return json_t();
    if(v.isPointer())   return json_t();
    if(v.isBool())      return json_t(v.toBool());
    if(v.isInt())       return json_t((long long)v.toInt());
    if(v.isReal())      return json_t((double)v.toReal());
    if(v.isString())    return json_t(to_std(v.toString()));
    if(v.isData())
    {
        //byteArray -> plain lowercase hex, no separators. Reading it back yields
        //a string; the caller is expected to re-parse the hex when binary data
        //needs to survive the round trip.
        byteArray d = v.toData();
        return json_t(to_std(d.toHex(false, 0, string())));
    }
    if(v.isFraction())
    {
        fraction<intx> f = v.toFraction();
        json_t o = json_t::object();
        o["numerator"]   = (long long)f.numerator();
        o["denominator"] = (long long)f.denominator();
        return o;
    }
    if(v.isArray())
    {
        array<variant> a = v.toArray();
        json_t arr = json_t::array();
        for(int i=0;i<a.size();i++) arr.push_back(variant_to_json(a[i]));
        return arr;
    }
    if(v.isHash())
    {
        hash<string,variant> h = v.toHash();
        const array<string> &keys = h.keys();
        const array<variant> &vals = h.values();
        json_t o = json_t::object();
        for(int i=0;i<keys.size();i++) o[to_std(keys[i])] = variant_to_json(vals[i]);
        return o;
    }
    if(v.isDim())
    {
        dimensions<uintz> d = v.toDim();
        json_t arr = json_t::array();
        uintz n = d.size();
        for(uintz i=0;i<n;i++) arr.push_back((unsigned long long)d[i]);
        return arr;
    }
    return json_t();
}

//----------- json -> variant ----------------------------------------

static variant json_to_variant(const json_t &j)
{
    if(j.is_null())     return variant();
    if(j.is_boolean())  return variant(j.get<bool>());
    //is_number_unsigned is a strict subset of is_number_integer - check it first
    if(j.is_number_unsigned())
        return variant((intx)j.get<unsigned long long>());
    if(j.is_number_integer())
        return variant((intx)j.get<long long>());
    if(j.is_number_float())
        return variant((realx)j.get<double>());
    if(j.is_string())
        return variant(from_std(j.get<std::string>()));
    if(j.is_array())
    {
        array<variant> a;
        for(auto it = j.begin(); it != j.end(); ++it) a.append(json_to_variant(*it));
        return variant(a);
    }
    if(j.is_object())
    {
        hash<string,variant> h;
        //alt::hash has no attributes, so they are flattened under their plain
        //names; if a name is used by both an attribute and a member, the member
        //wins, because hash::insert overwrites the previous value.
        for(auto it = j.attr_begin(); it != j.attr_end(); ++it)
            h.insert(from_std(it->first), json_to_variant(it->second));
        for(auto it = j.begin(); it != j.end(); ++it)
            h.insert(from_std(it.key()), json_to_variant(it.value()));
        return variant(h);
    }
    return variant();
}

//----------- object -> json -----------------------------------------

static json_t anonymous_content_json(object *anon)
{
    //alt::object stores content on an anonymous child as m_attributes[string()].
    //content() on an empty-named object returns exactly that value.
    return variant_to_json(anon->content());
}

static json_t serialize_object(object *obj)
{
    if(obj->name().isEmpty())
        return variant_to_json(obj->content());

    array<string> attrs = obj->listAttributes();
    array<object*> items = obj->listItems(string()); //full m_item_order, anon+named mixed

    array<object*> contents;                        //anonymous children in order
    array<string>  names;                           //named children, first-occurrence order
    hash<string,array<object*> > by_name;           //name -> list of children (in order)

    for(int i=0;i<items.size();i++)
    {
        object *it = items[i];
        if(it->name().isEmpty())
        {
            contents.append(it);
        }
        else
        {
            if(!by_name.contains(it->name()))
            {
                names.append(it->name());
                by_name.insert(it->name(), array<object*>());
            }
            by_name[it->name()].append(it);
        }
    }

    bool has_attrs    = attrs.size()    > 0;
    bool has_children = names.size()    > 0;
    bool has_contents = contents.size() > 0;

    //Pure leaf: no attributes and no named children -> emit the raw value.
    if(!has_attrs && !has_children)
    {
        if(!has_contents) return json_t::object();       //empty item -> {}
        if(contents.size()==1) return anonymous_content_json(contents[0]);
        json_t arr = json_t::array();
        for(int i=0;i<contents.size();i++) arr.push_back(anonymous_content_json(contents[i]));
        return arr;
    }

    json_t out = json_t::object();

    //Attributes come first and are written as bare `name: value` pairs. The
    //format accepts an identifier name and a scalar value only, so an attribute
    //that does not fit - a non-identifier name, or a value such as fraction,
    //array, hash or dimensions - has no representation and is not written.
    for(int i=0;i<attrs.size();i++)
    {
        json_t value = variant_to_json(obj->attr(attrs[i]));
        if(!is_attr_name(attrs[i]) || !is_attr_value(value)) continue;
        out.attr().set(to_std(attrs[i]), value);
    }

    //Content(s), if any, live under the reserved "#text" key.
    if(has_contents)
    {
        if(contents.size()==1)
        {
            out[std::string("#text")] = anonymous_content_json(contents[0]);
        }
        else
        {
            json_t arr = json_t::array();
            for(int i=0;i<contents.size();i++) arr.push_back(anonymous_content_json(contents[i]));
            out[std::string("#text")] = arr;
        }
    }

    //Named children, preserving first-occurrence order; groups larger than one
    //become JSON arrays so that item(name, index) round-trips.
    for(int i=0;i<names.size();i++)
    {
        const array<object*> &list = by_name[names[i]];
        std::string key = to_std(names[i]);
        if(list.size()==1)
        {
            out[key] = serialize_object(list[0]);
        }
        else
        {
            json_t arr = json_t::array();
            for(int j=0;j<list.size();j++) arr.push_back(serialize_object(list[j]));
            out[key] = arr;
        }
    }

    return out;
}

//----------- json -> object -----------------------------------------

static void fill_from_json_object(object *obj, const json_t &j);

static void load_value_into_item(object *child, const json_t &v)
{
    if(v.is_object())
    {
        fill_from_json_object(child, v);
        return;
    }
    //Scalars, nulls, and nested arrays all become content. Nested arrays land
    //as a single array<variant> so the shape survives a save/load cycle.
    child->addContent(json_to_variant(v));
}

static void fill_from_json_object(object *obj, const json_t &j)
{
    if(!j.is_object()) return;
    //Real attributes (bare `name: value` keys) are restored as attributes.
    for(auto it = j.attr_begin(); it != j.attr_end(); ++it)
    {
        string name = from_std(it->first);
        if(!name.isEmpty())
            obj->setAttr(name, json_to_variant(it->second));
    }
    //Ordinary members only from here on; size()/begin()/end() ignore attributes.
    for(auto it = j.begin(); it != j.end(); ++it)
    {
        const std::string &k = it.key();
        const json_t &v = it.value();

        if(k == "#text")
        {
            if(v.is_array())
            {
                for(auto e = v.begin(); e != v.end(); ++e)
                    obj->addContent(json_to_variant(*e));
            }
            else
            {
                obj->addContent(json_to_variant(v));
            }

            continue;
        }

        string name = from_std(k);
        if(name.isEmpty()) continue;            //empty JSON key - skip

        if(v.is_array())
        {
            if(v.empty())
            {
                //Preserve [] shape: one child with an empty array<variant> content.
                object *child = obj->addItem(name);
                if(child) child->addContent(variant(array<variant>()));
            }
            else
            {
                for(auto e = v.begin(); e != v.end(); ++e)
                {
                    object *child = obj->addItem(name);
                    if(!child) continue;
                    load_value_into_item(child, *e);
                }
            }
            continue;
        }

        object *child = obj->addItem(name);
        if(!child) continue;
        load_value_into_item(child, v);
    }
}

//----------- public API ---------------------------------------------

object_json::object_json(const string &root_name)
    : m_root_name(to_std(root_name))
{
    if(m_root_name.empty())
        m_root_name = "root";

    m_root_ptr.reset(new object(from_std(m_root_name)));
}

object* object_json::root()
{
    if(!m_root_ptr)
    {
        if(m_root_name.empty())
            m_root_name = "root";

        m_root_ptr.reset(new object(from_std(m_root_name)));
    }

    return m_root_ptr.get();
}

object_json::~object_json() = default;

object_json::object_json(object_json &&other) noexcept
    : m_root_name(std::move(other.m_root_name))
    , m_root_ptr(std::move(other.m_root_ptr))
{
    other.m_root_name.clear();
}

object_json &object_json::operator=(object_json &&other) noexcept
{
    if (this != &other)
    {
        m_root_ptr = std::move(other.m_root_ptr);
        std::swap(m_root_name, other.m_root_name);
        other.m_root_name.clear();
    }

    return *this;
}

void object_json::clear()
{
    if(m_root_name.empty())
        m_root_name = "root";

    m_root_ptr.reset(new object(from_std(m_root_name)));
}

bool object_json::load(string fname)
{
    file f(fname);
    if(!f.open(file::OReadOnly)) return false;

    string data = f.readText();
    f.close();

    clear();

    json_t j = json_t::parse(std::string(data(), (size_t)data.size()), nullptr, false);
    if(j.is_discarded())
        return false;

    if(j.is_object())
    {
        fill_from_json_object(m_root_ptr.get(), j);
    }
    else
    {
        //Non-object top-level value: store as a single content so save() can
        //emit the same shape back and no data is lost.
        m_root_ptr->addContent(json_to_variant(j));
    }
    return true;
}

bool object_json::save(string fname, object *root)
{
    if(!root) return false;

    json_t j = serialize_object(root);

    std::string out = j.dump(2, ' ', false, json_t::error_handler_t::replace);

    file f(fname);
    if(!f.open(file::OTruncate|file::OWriteOnly)) return false;

    int written = f.write(out.data(), (int)out.size());
    f.close();

    return written == (int)out.size();
}

bool object_json::save(string fname)
{
    return save(fname, root());
}
