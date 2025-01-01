#include "aobject_proto.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/util/json_util.h>

#include "../afile.h"
#include <iostream>
#include <fstream>

using namespace alt;

object_proto::object_proto()
{
    root_ptr = new object;
}

object_proto::~object_proto()
{
    delete root_ptr;
}

static bool parse_node(const google::protobuf::Message* msg, object *obj)
{
    if(!msg || !msg->IsInitialized())
    {
        std::cout << "object_proto: error message is not intialized." << std::endl;
        return false;
    }

    const google::protobuf::Reflection* reflection;
    std::unordered_map<std::string, const google::protobuf::FieldDescriptor*> fields;

    reflection = msg->GetReflection();
    std::vector<const google::protobuf::FieldDescriptor*> list;
    reflection->ListFields(*msg, &list);

    obj->setAttr("type",msg->GetTypeName().c_str());

    for (auto it : list)
    {
        switch(it->cpp_type())
        {
        case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
            if(it->is_repeated())
            {
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    auto o = obj->addItem(it->name().c_str());
                    if(!parse_node(&reflection->GetRepeatedMessage(*msg, it, i),o))
                        return false;
                }
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                if(!parse_node(&reflection->GetMessage(*msg, it),o))
                    return false;
            }
            break;
        case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
            if(it->is_repeated())
            {
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    auto o = obj->addItem(it->name().c_str());
                    auto s = reflection->GetRepeatedString(*msg, it, i);
                    if(it->type() == google::protobuf::FieldDescriptor::TYPE_STRING)
                    {
                        o->setAttr("type",it->cpp_type_name().data());
                        o->setContent(s.c_str());
                    }
                    else
                    {
                        o->setAttr("type","raw");
                        byteArray tmp(s.data(),s.size());
                        o->setContent(tmp.toHex(true,16));
                    }
                }
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                auto s = reflection->GetString(*msg, it);
                if(it->type() == google::protobuf::FieldDescriptor::TYPE_STRING)
                {
                    o->setAttr("type",it->cpp_type_name().data());
                    o->setContent(s.c_str());
                }
                else
                {
                    o->setAttr("type","raw");
                    byteArray tmp(s.data(),s.size());
                    o->setContent(tmp.toHex(true,16));
                }
            }
            break;
        case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
            if(it->is_repeated())
            {
                string list;
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    if(i) list += ",";
                    list += reflection->GetRepeatedEnum(*msg, it, i)->name().c_str();
                }
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(list);
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(reflection->GetEnum(*msg, it)->name().c_str());
            }
            break;

        case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
            if(it->is_repeated())
            {
                string list;
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    if(i) list += ",";
                    real64 tmp = reflection->GetRepeatedDouble(*msg, it, i);
                    list += "0x"+string::fromIntFormat(*(uint32*)&tmp,16,16);
                }
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(list);
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                real64 tmp = reflection->GetDouble(*msg, it);
                o->setContent("0x"+string::fromIntFormat(*(uint32*)&tmp,16,16));
            }
            break;
        case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
            if(it->is_repeated())
            {
                string list;
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    if(i) list += ",";
                    real32 tmp = reflection->GetRepeatedFloat(*msg, it, i);
                    list += "0x"+string::fromIntFormat(*(uint32*)&tmp,8,16);
                }
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(list);
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                real32 tmp = reflection->GetFloat(*msg, it);
                o->setContent("0x"+string::fromIntFormat(*(uint32*)&tmp,8,16));
            }
            break;
        case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
            if(it->is_repeated())
            {
                string list;
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    if(i) list += ",";
                    list += string::fromInt(reflection->GetRepeatedInt32(*msg, it, i));
                }
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(list);
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(reflection->GetInt32(*msg, it));
            }
            break;
        case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
            if(it->is_repeated())
            {
                string list;
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    if(i) list += ",";
                    list += string::fromInt(reflection->GetRepeatedInt64(*msg, it, i));
                }
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(list);
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(reflection->GetInt64(*msg, it));
            }
            break;
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
            if(it->is_repeated())
            {
                string list;
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    if(i) list += ",";
                    list += string::fromInt(reflection->GetRepeatedUInt32(*msg, it, i));
                }
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(list);
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(reflection->GetUInt32(*msg, it));
            }
            break;
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
            if(it->is_repeated())
            {
                string list;
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    if(i) list += ",";
                    list += string::fromInt(reflection->GetRepeatedUInt64(*msg, it, i));
                }
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(list);
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(reflection->GetUInt64(*msg, it));
            }
            break;
        case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
            if(it->is_repeated())
            {
                string list;
                for(int i=0;i<reflection->FieldSize(*msg,it);i++)
                {
                    if(i) list += ",";
                    list += reflection->GetRepeatedBool(*msg, it, i) ? "1":"0";
                }
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(list);
            }
            else
            {
                auto o = obj->addItem(it->name().c_str());
                o->setAttr("type",it->cpp_type_name().data());
                o->setContent(reflection->GetBool(*msg, it));
            }
            break;
        default:
            std::cout << "object_proto: undefined field type " << it->cpp_type_name() << std::endl;
            return false;
        };
    }

    return true;
}

static bool descriptor_to_xml(object *obj, const google::protobuf::Descriptor& descriptor)
{
    object *root = obj->addItem(descriptor.name().c_str());

    for (int i = 0; i < descriptor.field_count(); ++i)
    {
        const google::protobuf::FieldDescriptor* field = descriptor.field(i);

        if(field->containing_oneof())
            continue;

        object *child = root->addItem(field->name().c_str());
        child->setAttr("type",field->type_name().data());
        child->setAttr("id",field->number());

        if (field->label() == google::protobuf::FieldDescriptor::LABEL_REPEATED)
        {
            child->setAttr("repeated", "true");
        }
        if (field->label() == google::protobuf::FieldDescriptor::LABEL_OPTIONAL)
        {
            child->setAttr("optional", "true");
        }
        if (field->label() == google::protobuf::FieldDescriptor::LABEL_REQUIRED)
        {
            child->setAttr("required", "true");
        }

    }

    for (int i = 0; i < descriptor.nested_type_count(); ++i)
    {
        descriptor_to_xml(root, *descriptor.nested_type(i));
    }

    for (int i = 0; i < descriptor.enum_type_count(); ++i)
    {
        const google::protobuf::EnumDescriptor* enum_descriptor = descriptor.enum_type(i);
        object *enum_node = root->addItem(enum_descriptor->name().c_str());
        enum_node->setAttr("type", "enum");
        for (int j = 0; j < enum_descriptor->value_count(); ++j)
        {
            const google::protobuf::EnumValueDescriptor* value = enum_descriptor->value(j);
            object *value_node = enum_node->addItem(value->name().c_str());
            value_node->setAttr("value", value->number());
        }
    }

    for (int i = 0; i < descriptor.oneof_decl_count(); ++i)
    {
        const google::protobuf::OneofDescriptor* oneof = descriptor.oneof_decl(i);
        object *oneof_node = root->addItem(oneof->name().c_str());
        oneof_node->setAttr("type", "oneof");
        for (int j = 0; j < oneof->field_count(); ++j)
        {
            const google::protobuf::FieldDescriptor* field = oneof->field(j);
            object *field_node = oneof_node->addItem(field->name().c_str());
            field_node->setAttr("type", field->type_name().data());
            field_node->setAttr("id", field->number());
        }
    }

    for (int i = 0; i < descriptor.reserved_range_count(); ++i)
    {
        const google::protobuf::Descriptor::ReservedRange* range = descriptor.reserved_range(i);
        object *reserved_node = root->addItem("reserved");
        reserved_node->setAttr("start", range->start);
        reserved_node->setAttr("end", range->end);
    }

    for (int i = 0; i < descriptor.reserved_name_count(); ++i)
    {
        const std::string& name = descriptor.reserved_name(i);
        object *reserved_node = root->addItem("reserved");
        reserved_node->setAttr("name", name.c_str());
    }

    return true;
}

bool object_proto::load_descriptor(string proto_fname)
{
    pathParcer proto(proto_fname);

    google::protobuf::compiler::DiskSourceTree tree;
    tree.MapPath("", proto.getDirectory()());

    google::protobuf::compiler::Importer importer(&tree, nullptr);

    const google::protobuf::FileDescriptor* file_desc = importer.Import(proto.getName()());
    if(!file_desc)
        return false;

    for (int j = 0; j < file_desc->enum_type_count(); ++j)
    {
        const google::protobuf::EnumDescriptor* enum_descriptor = file_desc->enum_type(j);
        object *enum_node = root_ptr->addItem(enum_descriptor->name().c_str());
        enum_node->setAttr("type", "enum");
        for (int j = 0; j < enum_descriptor->value_count(); ++j)
        {
            const google::protobuf::EnumValueDescriptor* value = enum_descriptor->value(j);
            object *value_node = enum_node->addItem(value->name().c_str());
            value_node->setAttr("value", value->number());
        }
    }

    for (int j = 0; j < file_desc->message_type_count(); ++j)
    {
        descriptor_to_xml(root_ptr, *file_desc->message_type(j));
    }

    for (int j = 0; j < file_desc->extension_count(); ++j)
    {
        std::cout << "extension: " << file_desc->extension(j)->name() << std::endl;
    }

    for (int j = 0; j < file_desc->service_count(); ++j)
    {
        std::cout << "service: " << file_desc->service(j)->name() << std::endl;
    }

    for (int j = 0; j < file_desc->dependency_count(); ++j)
    {
        std::cout << "dependency: " << file_desc->dependency(j)->name() << std::endl;
    }

    for (int j = 0; j < file_desc->public_dependency_count(); ++j)
    {
        std::cout << "public_dependency: " << file_desc->public_dependency(j)->name() << std::endl;
    }

    for (int j = 0; j < file_desc->weak_dependency_count(); ++j)
    {
        std::cout << "weak_dependency: " << file_desc->weak_dependency(j)->name() << std::endl;
    }

    return true;
}

bool object_proto::is_binary_format(string data_fname)
{
    std::ifstream file(data_fname(), std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;

    char magic;
    file.read(&magic, sizeof(magic));
    file.close();

    return magic == 0x08;
}

bool object_proto::convert_to_json(string input_fname, string proto_fname, string output_fname, string entry)
{ //todo: убрать повторения кода
    pathParcer proto(proto_fname);

    google::protobuf::compiler::DiskSourceTree tree;
    tree.MapPath("", proto.getDirectory()());

    google::protobuf::compiler::Importer importer(&tree, nullptr);

    const google::protobuf::FileDescriptor* file_desc = importer.Import(proto.getName()());
    if(!file_desc)
        return false;

    const google::protobuf::Descriptor* msg_desc = file_desc->FindMessageTypeByName(entry());
    if(!msg_desc)
        return false;

    google::protobuf::DynamicMessageFactory *msg_factory = new google::protobuf::DynamicMessageFactory;
    const google::protobuf::Message* root_msg = msg_factory->GetPrototype(msg_desc);
    if(!root_msg)
    {
        delete msg_factory;
        return false;
    }

    google::protobuf::Message* mutable_root_msg = root_msg->New();
    if (!mutable_root_msg)
    {
        delete msg_factory;
        return false;
    }

    bool binary = is_binary_format(input_fname);
    std::ifstream stream(input_fname(), std::ios::in | std::ios::binary);
    google::protobuf::io::IstreamInputStream input(&stream);

    if(binary)
    {
        google::protobuf::io::CodedInputStream coded_input(&input);
        if(!mutable_root_msg->ParseFromCodedStream(&coded_input))
        {
            stream.close();
            delete mutable_root_msg;
            delete msg_factory;
            return false;
        }
    }
    else
    {
        std::string file_content((std::istreambuf_iterator<char>(stream)),
                                      std::istreambuf_iterator<char>());
        if (!google::protobuf::TextFormat::ParseFromString(file_content, mutable_root_msg))
        {
            stream.close();
            delete mutable_root_msg;
            delete msg_factory;
            return false;
        }
    }

    stream.close();

    std::string jsonString;
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    if(!google::protobuf::util::MessageToJsonString(*mutable_root_msg, &jsonString, options).ok())
    {
        delete mutable_root_msg;
        delete msg_factory;
        return false;
    }

    std::ofstream outfile(output_fname(), std::ios::out);
    outfile.write(jsonString.data(), jsonString.size());
    outfile.close();

    delete mutable_root_msg;
    delete msg_factory;
    return true;
}

bool object_proto::invert_representation(string input_fname, string proto_fname, string output_fname, string entry)
{
    pathParcer proto(proto_fname);

    google::protobuf::compiler::DiskSourceTree tree;
    tree.MapPath("", proto.getDirectory()());

    google::protobuf::compiler::Importer importer(&tree, nullptr);

    const google::protobuf::FileDescriptor* file_desc = importer.Import(proto.getName()());
    if(!file_desc)
        return false;

    const google::protobuf::Descriptor* msg_desc = file_desc->FindMessageTypeByName(entry());
    if(!msg_desc)
        return false;

    google::protobuf::DynamicMessageFactory *msg_factory = new google::protobuf::DynamicMessageFactory;
    const google::protobuf::Message* root_msg = msg_factory->GetPrototype(msg_desc);
    if(!root_msg)
    {
        delete msg_factory;
        return false;
    }

    google::protobuf::Message* mutable_root_msg = root_msg->New();
    if (!mutable_root_msg)
    {
        delete msg_factory;
        return false;
    }

    bool binary = is_binary_format(input_fname);
    std::ifstream stream(input_fname(), std::ios::in | std::ios::binary);
    google::protobuf::io::IstreamInputStream input(&stream);

    if(binary)
    {
        google::protobuf::io::CodedInputStream coded_input(&input);
        if(!mutable_root_msg->ParseFromCodedStream(&coded_input))
        {
            stream.close();
            delete mutable_root_msg;
            delete msg_factory;
            return false;
        }
    }
    else
    {
        std::string file_content((std::istreambuf_iterator<char>(stream)),
                                      std::istreambuf_iterator<char>());
        if (!google::protobuf::TextFormat::ParseFromString(file_content, mutable_root_msg))
        {
            stream.close();
            delete mutable_root_msg;
            delete msg_factory;
            return false;
        }
    }

    stream.close();

    bool success = true;

    if(binary)
    {
        std::string text_format;
        if (google::protobuf::TextFormat::PrintToString(*mutable_root_msg, &text_format))
        {
            std::ofstream output(output_fname(), std::ios::out);
            if (!output.is_open())
            {
                success = false;
            }
            else
            {
                output << text_format;
                output.close();
            }
        }
        else
        {
            success = false;
        }
    }
    else
    {
        std::vector<char> buffer(mutable_root_msg->ByteSize());
        google::protobuf::io::ArrayOutputStream  array(buffer.data(), buffer.size());
        google::protobuf::io::CodedOutputStream output(&array);
        if (!mutable_root_msg->SerializeToCodedStream(&output))
        {
            success = false;
        }
        else
        {
            std::ofstream outfile(output_fname(), std::ifstream::binary);
            outfile.write(buffer.data(), buffer.size());
            outfile.close();
        }
    }

    delete mutable_root_msg;
    delete msg_factory;
    return success;

}

bool object_proto::load(string data_fname, string proto_fname, string entry)
{
    pathParcer proto(proto_fname);

    google::protobuf::compiler::DiskSourceTree tree;
    tree.MapPath("", proto.getDirectory()());

    google::protobuf::compiler::Importer importer(&tree, nullptr);

    const google::protobuf::FileDescriptor* file_desc = importer.Import(proto.getName()());
    if(!file_desc)
        return false;

    const google::protobuf::Descriptor* msg_desc = file_desc->FindMessageTypeByName(entry());
    if(!msg_desc)
        return false;

    google::protobuf::DynamicMessageFactory *msg_factory = new google::protobuf::DynamicMessageFactory;
    const google::protobuf::Message* root_msg = msg_factory->GetPrototype(msg_desc);
    if(!root_msg)
    {
        delete msg_factory;
        return false;
    }

    google::protobuf::Message* mutable_root_msg = root_msg->New();
    if (!mutable_root_msg)
    {
        delete msg_factory;
        return false;
    }

    bool binary = is_binary_format(data_fname);
    std::ifstream stream(data_fname(), std::ios::in | std::ios::binary);
    google::protobuf::io::IstreamInputStream input(&stream);

    if(binary)
    {
        google::protobuf::io::CodedInputStream coded_input(&input);
        if(!mutable_root_msg->ParseFromCodedStream(&coded_input))
        {
            stream.close();
            delete mutable_root_msg;
            delete msg_factory;
            return false;
        }
    }
    else
    {
        std::string file_content((std::istreambuf_iterator<char>(stream)),
                                      std::istreambuf_iterator<char>());
        if (!google::protobuf::TextFormat::ParseFromString(file_content, mutable_root_msg))
        {
            stream.close();
            delete mutable_root_msg;
            delete msg_factory;
            return false;
        }
    }

    stream.close();

    if(!parse_node(mutable_root_msg,root_ptr))
    {
        root_ptr->clear();
        delete mutable_root_msg;
        delete msg_factory;
        return false;
    }

    delete mutable_root_msg;
    delete msg_factory;
    return true;
}

void object_proto::clear()
{
    delete root_ptr;
    root_ptr = new object;
}
