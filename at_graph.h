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

#ifndef AT_GRAPH_H
#define AT_GRAPH_H

#include "at_hash.h"

namespace alt {

    template<class K, class NodeData, class LinkData>
    class graph
    {
    public:
        graph(){}
        graph(const graph &val)
        {
            nodes=val.nodes;
            links=val.links;
        }
        virtual ~graph(){}

        graph& operator=(const graph &val)
        {
            nodes=val.nodes;
            links=val.links;
            return *this;
        }

        void clear()
        {
            links.clear();
            nodes.clear();
        }

        const array<K>& keys() const
        {
            return nodes.keys();
        }
        K key(int ind) const
        {
            return nodes.key(ind);
        }

        int size() const
        {
            return nodes.size();
        }

        bool contains(const K &id) const
        {
            return nodes.contains(id);
        }

        void insert(const K &id, const NodeData &data)
        {
            nodes.insert(id,data);
        }

        int indeOf(const K &id) const
        {
            return nodes.indexOf(id);
        }
        const NodeData& node(int ind) const
        {
            return nodes.value(ind);
        }

        set<K> connectedWith(const K &val) const
        {
            set<K> rv = links.keysWith(val);
            rv.remove(val);
            return rv;
        }
        graph& removeNode(K id)
        {
            nodes.removeMulty(id);
            links.removeWith(id);
            return *this;
        }
        NodeData& operator[](const K &id)
        {
            return nodes[id];
        }
        const NodeData& operator[](const K &id) const
        {
            return nodes[id];
        }

        ////////////////////////////////////////////////////////////
        //работа со связями
        int addLinkUnique(const K &n1, const K &n2, const LinkData &data)
        {
            return links.insert(n1,n2,data);
        }
        int addLink(const K &n1, const K &n2, const LinkData &data)
        {
            return links.insertMulty(n1,n2,data);
        }
        bool isLinked(const K &n1, const K &n2) const
        {
            return links.contains(n1,n2);
        }
        bool isConnected(const K &n1, const K &n2) const
        {
            return links.contains_unordered(n1,n2);
        }

        int linksTotal() const
        {
            return links.size();
        }
        const array<K>& linkKeys(int ind) const
        {
            return links.key(ind);
        }
        const LinkData& link(int ind) const
        {
            return links.value(ind);
        }

        array<LinkData> linksDataOf(const K &id) const
        {
            return links.valuesWith(id);
        }
        array<int> linksOf(const K &id) const
        {
            return links.indexesWith(id);
        }

        array<int> inputLinksOf(const K &id) const
        {
            return links.indexesWith(id,1);
        }

        array<int> outputLinksOf(const K &id) const
        {
            return links.indexesWith(id,0);
        }

        graph& removeLink(int ind)
        {
            links.removeByIndex(ind);
            return *this;
        }

        const LinkData& operator()(int ind) const
        {
            return links.value(ind);
        }
        const LinkData& operator()(const K &n1, const K &n2) const
        {
            return links[{n1,n2}];
        }
        LinkData& operator()(int ind)
        {
            return links.value_ref(ind);
        }
        LinkData& operator()(const K &n1, const K &n2)
        {
            return links[{n1,n2}];
        }

    protected:

        alt::hash<K,NodeData> nodes;
        alt::multikey<K,LinkData> links; //todo: создать оптимальный контейнер

    };

} // namespace alt

#endif // AT_GRAPH_H
