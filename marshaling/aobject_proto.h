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

#ifndef AOBJECT_PROTO_H
#define AOBJECT_PROTO_H

#include "../aobject.h"

//For direct compile (DEFINES += NOMINMAX):

//../../abseil-cpp/absl/hash/internal/city.cc \
//../../abseil-cpp/absl/hash/internal/hash.cc \
//../../abseil-cpp/absl/hash/internal/low_level_hash.cc \
//../../abseil-cpp/absl/log/internal/log_message.cc \
//../../abseil-cpp/absl/log/internal/log_format.cc \
//../../abseil-cpp/absl/log/internal/check_op.cc \
//../../abseil-cpp/absl/log/internal/conditions.cc \
//../../abseil-cpp/absl/log/internal/nullguard.cc \
//../../abseil-cpp/absl/log/internal/proto.cc \
//../../abseil-cpp/absl/log/internal/log_sink_set.cc \
//../../abseil-cpp/absl/log/internal/globals.cc \
//../../abseil-cpp/absl/log/globals_absl.cc \
//../../abseil-cpp/absl/log/log_sink.cc \
//../../abseil-cpp/absl/strings/cord.cc \
//../../abseil-cpp/absl/strings/str_cat.cc \
//../../abseil-cpp/absl/strings/str_split.cc \
//../../abseil-cpp/absl/strings/str_replace.cc \
//../../abseil-cpp/absl/strings/ascii.cc \
//../../abseil-cpp/absl/strings/match.cc \
//../../abseil-cpp/absl/strings/numbers.cc \
//../../abseil-cpp/absl/strings/escaping.cc \
//../../abseil-cpp/absl/strings/internal/escaping_int.cc \
//../../abseil-cpp/absl/strings/charconv.cc \
//../../abseil-cpp/absl/strings/substitute.cc \
//../../abseil-cpp/absl/strings/cord_analysis.cc \
//../../abseil-cpp/absl/strings/internal/str_format/arg.cc \
//../../abseil-cpp/absl/strings/internal/str_format/bind.cc \
//../../abseil-cpp/absl/strings/internal/str_format/parser_absl.cc \
//../../abseil-cpp/absl/strings/internal/str_format/extension.cc \
//../../abseil-cpp/absl/strings/internal/str_format/output.cc \
//../../abseil-cpp/absl/strings/internal/str_format/float_conversion.cc \
//../../abseil-cpp/absl/strings/internal/stringify_sink.cc \
//../../abseil-cpp/absl/strings/internal/cordz_info.cc \
//../../abseil-cpp/absl/strings/internal/cordz_handle.cc \
//../../abseil-cpp/absl/strings/internal/charconv_bigint.cc \
//../../abseil-cpp/absl/strings/internal/charconv_parse.cc \
//../../abseil-cpp/absl/strings/internal/memutil.cc \
//../../abseil-cpp/absl/strings/internal/utf8.cc \
//../../abseil-cpp/absl/strings/internal/cord_rep_btree_reader.cc \
//../../abseil-cpp/absl/strings/internal/cord_rep_btree_navigator.cc \
//../../abseil-cpp/absl/strings/internal/cord_rep_crc.cc \
//../../abseil-cpp/absl/strings/internal/cord_rep_btree.cc \
//../../abseil-cpp/absl/strings/internal/cord_rep_consume.cc \
//../../abseil-cpp/absl/strings/internal/cord_internal.cc \
//../../abseil-cpp/absl/status/statusor.cc \
//../../abseil-cpp/absl/status/status.cc \
//../../abseil-cpp/absl/status/status_payload_printer.cc \
//../../abseil-cpp/absl/status/internal/status_internal.cc \
//../../abseil-cpp/absl/container/internal/raw_hash_set.cc \
//../../abseil-cpp/absl/synchronization/mutex.cc \
//../../abseil-cpp/absl/synchronization/internal/graphcycles.cc \
//../../abseil-cpp/absl/synchronization/internal/per_thread_sem.cc \
//../../abseil-cpp/absl/synchronization/internal/create_thread_identity.cc \
//../../abseil-cpp/absl/synchronization/internal/win32_waiter.cc \
//../../abseil-cpp/absl/synchronization/internal/waiter_base.cc \
//../../abseil-cpp/absl/synchronization/internal/kernel_timeout.cc \
//../../abseil-cpp/absl/base/internal/thread_identity.cc \
//../../abseil-cpp/absl/base/internal/low_level_alloc.cc \
//../../abseil-cpp/absl/base/internal/spinlock.cc \
//../../abseil-cpp/absl/base/internal/spinlock_wait.cc \
//../../abseil-cpp/absl/base/internal/sysinfo.cc \
//../../abseil-cpp/absl/base/internal/raw_logging.cc \
//../../abseil-cpp/absl/base/internal/cycleclock.cc \
//../../abseil-cpp/absl/base/internal/unscaledcycleclock.cc \
//../../abseil-cpp/absl/base/internal/strerror.cc \
//../../abseil-cpp/absl/base/internal/throw_delegate.cc \
//../../abseil-cpp/absl/debugging/symbolize.cc \
//../../abseil-cpp/absl/debugging/stacktrace.cc \
//../../abseil-cpp/absl/debugging/leak_check.cc \
//../../abseil-cpp/absl/debugging/internal/examine_stack.cc \
//../../abseil-cpp/absl/time/clock.cc \
//../../abseil-cpp/absl/time/time.cc \
//../../abseil-cpp/absl/time/duration.cc \
//../../abseil-cpp/absl/time/internal/cctz/src/time_zone_lookup.cc \
//../../abseil-cpp/absl/time/internal/cctz/src/time_zone_impl.cc \
//../../abseil-cpp/absl/time/internal/cctz/src/time_zone_fixed.cc \
//../../abseil-cpp/absl/time/internal/cctz/src/time_zone_if.cc \
//../../abseil-cpp/absl/time/internal/cctz/src/time_zone_libc.cc \
//../../abseil-cpp/absl/time/internal/cctz/src/time_zone_info.cc \
//../../abseil-cpp/absl/time/internal/cctz/src/time_zone_posix.cc \
//../../abseil-cpp/absl/time/internal/cctz/src/zone_info_source.cc \
//../../abseil-cpp/absl/numeric/int128.cc \
//../../abseil-cpp/absl/crc/internal/crc_cord_state.cc \
//../../abseil-cpp/absl/crc/crc32c.cc \
//../../abseil-cpp/absl/crc/internal/crc.cc \
//../../abseil-cpp/absl/crc/internal/crc_memcpy_fallback.cc \
//../../abseil-cpp/absl/crc/internal/crc_x86_arm_combined.cc \
//../../protobuf/third_party/utf8_range/utf8_validity.cc \
//../../protobuf/third_party/utf8_range/utf8_range.c \
//../../protobuf/src/google/protobuf/compiler/parser.cc \
//../../protobuf/src/google/protobuf/compiler/importer.cc \
//../../protobuf/src/google/protobuf/text_format.cc \
//../../protobuf/src/google/protobuf/dynamic_message.cc \
//../../protobuf/src/google/protobuf/descriptor.cc \
//../../protobuf/src/google/protobuf/io/coded_stream.cc \
//../../protobuf/src/google/protobuf/json/json.cc \
//../../protobuf/src/google/protobuf/json/internal/iparser.cc \
//../../protobuf/src/google/protobuf/json/internal/unparser.cc \
//../../protobuf/src/google/protobuf/json/internal/untyped_message.cc \
//../../protobuf/src/google/protobuf/json/internal/writer.cc \
//../../protobuf/src/google/protobuf/json/internal/lexer.cc \
//../../protobuf/src/google/protobuf/json/internal/message_path.cc \
//../../protobuf/src/google/protobuf/json/internal/zero_copy_buffered_stream.cc \
//../../protobuf/src/google/protobuf/io/zero_copy_stream.cc \
//../../protobuf/src/google/protobuf/io/zero_copy_stream_impl.cc \
//../../protobuf/src/google/protobuf/io/zero_copy_sink.cc \
//../../protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.cc \
//../../protobuf/src/google/protobuf/io/tokenizer.cc \
//../../protobuf/src/google/protobuf/io/io_win32.cc \
//../../protobuf/src/google/protobuf/io/strtod.cc \
//../../protobuf/src/google/protobuf/stubs/common.cc \
//../../protobuf/src/google/protobuf/message.cc \
//../../protobuf/src/google/protobuf/message_lite.cc \
//../../protobuf/src/google/protobuf/repeated_field.cc \
//../../protobuf/src/google/protobuf/repeated_ptr_field.cc \
//../../protobuf/src/google/protobuf/reflection_ops.cc \
//../../protobuf/src/google/protobuf/reflection_mode.cc \
//../../protobuf/src/google/protobuf/generated_message_reflection.cc \
//../../protobuf/src/google/protobuf/generated_message_util.cc \
//../../protobuf/src/google/protobuf/map.cc \
//../../protobuf/src/google/protobuf/map_field.cc \
//../../protobuf/src/google/protobuf/raw_ptr.cc \
//../../protobuf/src/google/protobuf/generated_message_tctable_lite.cc \
//../../protobuf/src/google/protobuf/generated_message_tctable_full.cc \
//../../protobuf/src/google/protobuf/generated_message_tctable_gen.cc \
//../../protobuf/src/google/protobuf/wire_format.cc \
//../../protobuf/src/google/protobuf/wire_format_lite.cc \
//../../protobuf/src/google/protobuf/parse_context.cc \
//../../protobuf/src/google/protobuf/extension_set.cc \
//../../protobuf/src/google/protobuf/extension_set_heavy.cc \
//../../protobuf/src/google/protobuf/arena.cc \
//../../protobuf/src/google/protobuf/arenastring.cc \
//../../protobuf/src/google/protobuf/generated_enum_util.cc \
//../../protobuf/src/google/protobuf/unknown_field_set.cc \
//../../protobuf/src/google/protobuf/descriptor.pb.cc \
//../../protobuf/src/google/protobuf/descriptor.cc \
//../../protobuf/src/google/protobuf/any.cc \
//../../protobuf/src/google/protobuf/type.pb.cc \
//../../protobuf/src/google/protobuf/any.pb.cc \
//../../protobuf/src/google/protobuf/source_context.pb.cc \
//../../protobuf/src/google/protobuf/any_lite.cc \
//../../protobuf/src/google/protobuf/descriptor_database.cc \
//../../protobuf/src/google/protobuf/cpp_features.pb.cc \
//../../protobuf/src/google/protobuf/feature_resolver.cc \
//../../protobuf/src/google/protobuf/inlined_string_field.cc \
//../../protobuf/src/google/protobuf/port.cc

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
