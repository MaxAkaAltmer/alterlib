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

#ifndef AOBJECT_JSON_H
#define AOBJECT_JSON_H

#include "../aobject.h"

#include <memory>
#include <string>

namespace alt {

    /**
     * @brief The `object_json` class loads and saves `alt::object` trees from/to
     *        JSON files using `nlohmann::ordered_json_attrs` - the vendored
     *        nlohmann::json flavour that knows about attributes.
     *
     * Mapping between the `alt::object` tree and JSON:
     *  - The top-level JSON object maps to the root node's children and
     *    attributes; the root node's own name (default `root`) is internal and
     *    is not written to the document. A quoted key becomes a child item, a
     *    bare identifier key (`attr: value`) becomes an attribute.
     *  - A JSON array under a key expands into several sibling items sharing the
     *    same name, accessible through `object::item(path, index)` (with the same
     *    index-based semantics as `alt::object` itself). An empty array round-trips
     *    as a single item whose content is an empty `alt::array<variant>`.
     *  - A JSON scalar (bool / integer / float / string / null) is stored as the
     *    item's content via `object::addContent`.
     *  - A nested JSON array (i.e. an array reached *through* an array or through
     *    an item value that is not a key-split) is stored as a single content of
     *    type `array<variant>`.
     *
     * The format is a JSON superset with an attribute concept: an object member
     * whose key is a bare C++ identifier (written without quotes) is an
     * attribute, and its value must be a scalar - string, number, bool or null.
     * Attributes are written before the ordinary members. An attribute whose
     * name is not a valid identifier, or whose value has no scalar form
     * (`fraction`, `array`, `hash`, `dimensions`), cannot be expressed in this
     * format at all and is simply not written. A quoted `"@..."` key has no
     * special meaning: it is an ordinary member whose child item is named
     * `@...`.
     *
     * A leaf item (no attributes and no named children) is written as its raw
     * JSON value: a scalar or null for a single content, an array when there are
     * several contents, and `{}` when the item is completely empty. When an item
     * carries content together with attributes and/or named children, its
     * content(s) appear under the reserved key `#text`.
     *
     * Limitations (documented so callers are not surprised):
     *  - JSON cannot interleave text with children; content + named children
     *    always split into `#text` plus per-name keys, so original interleaving
     *    order is not preserved (a Jackson-XML-style compromise).
     *  - `alt::variant` sub-types without a native JSON counterpart are
     *    one-way on save: `byteArray` becomes a hex string,
     *    `fraction<intx>` becomes `{"numerator": N, "denominator": D}`,
     *    `dimensions<uintz>` becomes an array of unsigned integers. Reading
     *    those shapes back yields a string / hash / array variant rather than
     *    the original alt type.
     *  - A top-level JSON value that is not an object is preserved as the
     *    root's single content, which round-trips but is accessed through
     *    `root()->content()` rather than via children.
     */
    class object_json final
    {
    public:
        /**
         * @brief Construct with a fresh internal root node.
         * @param root_name Name of the owned internal root node. JSON has no
         *                    root element, so this name is not written to the
         *                    document; it only satisfies alt::object's rule that
         *                    named nodes may contain children/attributes.
         */
        explicit object_json(const string &root_name = string());

        /**
         * @brief Destroy the tree rooted at `root()`.
         */
        ~object_json();

        /**
         * @brief Copying is forbidden: the root object tree is owned uniquely.
         */
        object_json(const object_json &) = delete;

        /**
         * @brief Copy-assignment is forbidden (unique ownership).
         */
        object_json &operator=(const object_json &) = delete;

        /**
         * @brief Move the tree rooted at `other.root()` into `*this`.
         */
        object_json(object_json &&other) noexcept;

        /**
         * @brief Move-assign the tree rooted at `other.root()` into `*this`.
         */
        object_json &operator=(object_json &&other) noexcept;

        /**
         * @brief Load a JSON file into the tree.
         * @param fname Path to the JSON file.
         * @return true on success, false if the file cannot be opened or the
         *         content is not valid JSON.
         */
        bool load(string fname);

        /**
         * @brief Save the current tree to a JSON file (pretty-printed, indent 2).
         * @param fname Path to the destination file.
         * @return true on success.
         */
        bool save(string fname);

        /**
         * @brief Static helper: save an arbitrary `object` tree to a JSON file.
         */
        static bool save(string fname, object *root);

        /**
         * @brief Access the tree root. Never returns nullptr (an empty root is
         *        always available; see `clear()`).
         */
        object* root();

        /**
         * @brief Drop the current tree and start with a fresh root.
         */
        void clear();

    private:
        std::string m_root_name{"root"};
        std::unique_ptr<object> m_root_ptr;
    };

} // namespace alt

#endif // AOBJECT_JSON_H
