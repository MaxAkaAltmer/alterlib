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

#ifndef AOBJECT_PROTO_H
#define AOBJECT_PROTO_H

#include "../aobject.h"

namespace alt {

    /**
     * @class object_proto
     * @brief A class for loading and managing protocol buffer-based objects.
     */
    class object_proto
    {
    public:
        /**
         * @brief Default constructor.
         *
         * Initializes the object.
         */
        object_proto();

        /**
         * @brief Virtual destructor.
         *
         * Cleans up resources associated with the object.
         */
        virtual ~object_proto();

        /**
         * @brief Loads protobuf serialized data.
         *
         * @param data_fname Path to the data file.
         * @param proto_fname Path to the protobuf descriptor file.
         * @param entry The name of the root entry to load.
         * @return true if loading was successful, false otherwise.
         */
        bool load(string data_fname, string proto_fname, string entry);

        /**
         * @brief Checks if a file is in binary protobuf format.
         *
         * @param data_fname Path to the file to check.
         * @return true if the file is in binary protobuf format, false otherwise.
         */
        static bool is_binary_format(string data_fname);

        /**
         * @brief Inverts the representation of a protobuf object from BIN to/from TXT.
         *
         * @param input_fname Path to the input file.
         * @param proto_fname Path to the protobuf descriptor file.
         * @param output_fname Path to the output file.
         * @param entry The name of the root entry.
         * @return true if inversion was successful, false otherwise.
         */
        static bool invert_representation(string input_fname, string proto_fname, string output_fname, string entry);

        /**
         * @brief Convert of a protobuf object from PB/PROTOTXT into JSON.
         *
         * @param input_fname Path to the input file.
         * @param proto_fname Path to the protobuf descriptor file.
         * @param output_fname Path to the output file.
         * @param entry The name of the root entry.
         * @return true if conversion was successful, false otherwise.
         */
        static bool convert_to_json(string input_fname, string proto_fname, string output_fname, string entry);

        /**
         * @brief Loads protobuf descriptor.
         *
         * @param proto_fname Path to the protobuf descriptor file.
         * @return true if loading was successful, false otherwise.
         */
        bool load_descriptor(string proto_fname);

        /**
         * @brief Returns a pointer to the root object.
         *
         * @return A pointer to the root object of type `object`.
         */
        object* root() { return root_ptr; }

        /**
         * @brief Clears the object's data.
         *
         * Frees memory and resets the object's state.
         */
        void clear();

    private:
        /**
         * @brief Pointer to the root object.
         *
         * Stores a reference to the loaded root object.
         * Defaults to `nullptr`.
         */
        object *root_ptr = nullptr;

    };

} // namespace alt

#endif // AOBJECT_PROTO_H
