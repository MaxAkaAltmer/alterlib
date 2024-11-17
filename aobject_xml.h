#ifndef AOBJECT_XML_H
#define AOBJECT_XML_H

#include "aobject.h"

namespace alt {

    class object_xml
    {
    public:
        object_xml();
        virtual ~object_xml();

        bool load(string fname);
        bool save(string fname, bool standalone=false);

        object* root() { return root_ptr; }

        void clear();

    private:

        object *root_ptr = nullptr;

    };

} // namespace alt

#endif // AOBJECT_XML_H
