/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2024 Maxim L. Grishin  (altmer@arts-union.ru)

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

#ifndef AOBJECT_XML_H
#define AOBJECT_XML_H

#include "../aobject.h"

namespace alt {

    /**
     * @brief The `object_xml` class provides functionality for loading and saving objects to/from XML files.
     *
     * This class allows you to serialize and deserialize a tree-like object structure, represented by the `object` class
     * (defined in "aobject.h"), into an XML format. It supports loading from an XML file, saving to an XML file,
     * and managing the root object of the structure.
     */
    class object_xml
    {
    public:
        /**
         * @brief Constructor for the `object_xml` class.
         *
         * Initializes a new `object_xml` instance with an empty object tree.
         */
        object_xml();

        /**
         * @brief Destructor for the `object_xml` class.
         *
         * Cleans up any resources held by the `object_xml` instance. This typically involves deleting the object tree
         * rooted at `root_ptr` if it exists.
         */
        virtual ~object_xml();

        /**
         * @brief Loads an object tree from an XML file.
         *
         * This function parses the XML file specified by `fname`, creates the corresponding object tree, and stores the
         * root of the tree in `root_ptr`.
         *
         * @param fname The path to the XML file to load.
         * @return `true` if the file was loaded successfully and the object tree was created, `false` otherwise.
         */
        bool load(string fname);

        /**
         * @brief Saves the current object tree to an XML file.
         *
         * This function serializes the object tree rooted at `root_ptr` into an XML format and saves it to the file
         * specified by `fname`.
         *
         * @param fname The path to the XML file to save to.
         * @param standalone  A boolean flag indicating whether to include a standalone XML declaration in the output file
         *                      (e.g., `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>`).
         *                      Defaults to `false`.
         * @return `true` if the object tree was saved successfully, `false` otherwise.
         */
        bool save(string fname, bool standalone=false);

        /**
         * @brief Saves an object tree to an XML file.
         *
         * This static function serializes the object tree rooted at the provided `root` object into an XML format and
         * saves it to the file specified by `fname`.
         *
         * @param fname The path to the XML file to save to.
         * @param root  A pointer to the root object of the tree to be saved.
         * @param standalone A boolean flag indicating whether to include a standalone XML declaration in the output file.
         *                      Defaults to `false`.
         * @return `true` if the object tree was saved successfully, `false` otherwise.
         */
        static bool save(string fname, object *root, bool standalone=false);

        /**
         * @brief Returns a pointer to the root object of the object tree.
         *
         * @return A pointer to the root object (`object*`). Returns `nullptr` if no object tree has been loaded or created.
         */
        object* root() { return root_ptr; }

        /**
         * @brief Clears the current object tree.
         *
         * This function deletes the existing object tree rooted at `root_ptr`, if any, and resets `root_ptr` to `nullptr`.
         */
        void clear();

    private:

        /**
         * @brief A pointer to the root object of the object tree.
         *
         * This pointer stores the root of the object structure loaded from or to be saved to an XML file. It is
         * initialized to `nullptr` and is updated when an object tree is loaded or created.
         */
        object *root_ptr = nullptr;

    };

} // namespace alt

#endif // AOBJECT_XML_H
