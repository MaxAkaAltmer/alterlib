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

#ifndef AT_GRAPH_H
#define AT_GRAPH_H

#include "at_hash.h"

template<class NodeID, class NodeData, class LinkMeta>
class ATGraph
{
public:
    ATGraph(){}
    ATGraph(const ATGraph &val)
    {
        nodes=val.nodes;
        connects=val.connects;
    }
    ~ATGraph(){}

    ATGraph& operator=(const ATGraph &val)
    {
        nodes=val.nodes;
        connects=val.connects;
        return *this;
    }

    void clear()
    {
        connects.clear();
        nodes.clear();
    }

    ATArray<NodeID> idArray()
    {
        return nodes.keys();
    }

    ////////////////////////////////////////////////////////////
    //работа с узлами
    void insert(const NodeID &id, const NodeData &data)
    {
        nodes.insert(id,data);
    }
    int nodeCount() const
    {
        return nodes.size();
    }
    int nodeIndex(const NodeID &id) const
    {
        return nodes.indexOf(id);
    }
    NodeData nodeData(int ind) const
    {
        return nodes.value(ind);
    }
    NodeData& operator[](const NodeID &id)
    {
        return nodes[id];
    }
    bool contains(const NodeID &id)
    {
        return nodes.contains(id);
    }
    NodeID nodeID(int ind) const
    {
        return nodes.key(ind);
    }    
    ATSet<NodeID> nodeNodes(const NodeID &val) const
    {
        return connects.keysWith(val);
    }
    ATGraph& remove(NodeID id)
    {
        nodes.removeMulty(id);
        connects.removeMulty(id);
        return *this;
    }

    ////////////////////////////////////////////////////////////
    //работа со связями
    int link(const NodeID &n1, const NodeID &n2, const LinkMeta &data)
    {
        return connects.insert(n1,n2,data);
    }
    bool isLinked(const NodeID &n1, const NodeID &n2)
    {
        return connects.contains(n1,n2);
    }
    int linkCount() const
    {
        return connects.size();
    }
    ATArray<NodeID> linkNodes(int ind) const
    {
        return connects.key(ind);
    }
    LinkMeta linkMeta(int ind) const
    {
        return connects.value(ind);
    }
    ATArray<LinkMeta> nodeLinks(const NodeID &id) const
    {
        return connects.valuesWith(id);
    }
    ATArray<int> nodeLinkIndexes(const NodeID &id) const
    {
        return connects.indexesWith(id);
    }
    LinkMeta& operator()(int ind)
    {
        return connects[ind];
    }

    LinkMeta& operator()(const NodeID &n1, const NodeID &n2)
    {
        return connects.value(n1,n2);
    }

private:

    ATHash<NodeID,NodeData> nodes;
    ATChaos<NodeID,LinkMeta> connects;

};

#endif // AT_GRAPH_H
