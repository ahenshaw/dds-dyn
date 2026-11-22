// fastdds_dyn._core: a small Python binding of Fast DDS 3 built on its dynamic
// types. Types come from IDL at runtime (Fast DDS's own IDL parser); samples
// are plain Python dicts. No code generation, no per-type compile step.

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/wstring.h>

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <fastdds/config.hpp>
#include <fastdds/dds/core/ReturnCode.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/log/Log.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeBuilder.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeBuilderFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeMember.hpp>
#include <fastdds/dds/xtypes/dynamic_types/MemberDescriptor.hpp>
#include <fastdds/dds/xtypes/dynamic_types/TypeDescriptor.hpp>
#include <fastdds/dds/xtypes/dynamic_types/Types.hpp>
#include <fastdds/rtps/attributes/BuiltinTransports.hpp>

namespace nb = nanobind;
using namespace eprosima::fastdds::dds;
using eprosima::fastdds::rtps::BuiltinTransports;

namespace {

using TypeRef = DynamicType::_ref_type;
using DataRef = DynamicData::_ref_type;

void check(ReturnCode_t rc, const std::string& what) {
    if (rc != RETCODE_OK) throw std::runtime_error(what + " failed (Fast DDS ReturnCode " + std::to_string(rc) + ")");
}

TypeDescriptor::_ref_type descriptor(const TypeRef& t) {
    TypeDescriptor::_ref_type d{traits<TypeDescriptor>::make_shared()};
    check(t->get_descriptor(d), "get_descriptor");
    return d;
}

TypeRef resolve(TypeRef t) {  // strip typedefs
    while (t->get_kind() == TK_ALIAS) t = descriptor(t)->base_type();
    return t;
}

struct Member {
    std::string name;
    MemberId id;
    TypeRef type;
    std::string default_value;  // for enum literals: their value
};

std::vector<Member> members(const TypeRef& t) {
    std::vector<Member> out;
    for (uint32_t i = 0; i < t->get_member_count(); ++i) {
        DynamicTypeMember::_ref_type m;
        check(t->get_member_by_index(m, i), "get_member_by_index");
        MemberDescriptor::_ref_type md{traits<MemberDescriptor>::make_shared()};
        check(m->get_descriptor(md), "member get_descriptor");
        out.push_back({m->get_name().to_string(), m->get_id(), md->type(), md->default_value()});
    }
    return out;
}

std::string kind_name(TypeRef t) {
    switch (t->get_kind()) {
        case TK_BOOLEAN: return "boolean";
        case TK_BYTE: return "octet";
        case TK_INT8: return "int8";
        case TK_UINT8: return "uint8";
        case TK_INT16: return "short";
        case TK_UINT16: return "unsigned short";
        case TK_INT32: return "long";
        case TK_UINT32: return "unsigned long";
        case TK_INT64: return "long long";
        case TK_UINT64: return "unsigned long long";
        case TK_FLOAT32: return "float";
        case TK_FLOAT64: return "double";
        case TK_FLOAT128: return "long double";
        case TK_CHAR8: return "char";
        case TK_CHAR16: return "wchar";
        case TK_STRING8: return "string";
        case TK_STRING16: return "wstring";
        case TK_UNION: return "union " + t->get_name().to_string();
        case TK_MAP: return "map";
        case TK_BITSET: return "bitset " + t->get_name().to_string();
        case TK_BITMASK: return "bitmask " + t->get_name().to_string();
        default: return t->get_name().to_string() + " (TypeKind " + std::to_string(t->get_kind()) + ")";
    }
}

[[noreturn]] void unsupported(const TypeRef& t) {
    throw nb::type_error(("fastdds_dyn: unsupported IDL type " + kind_name(t) +
                          "; supported: primitives, strings, enums, structs, sequences, arrays").c_str());
}

// ----------------------------------------------------------------- DDS -> Python

nb::object to_py(const DataRef& d, MemberId id, TypeRef t);

nb::dict struct_to_py(const DataRef& d, const TypeRef& t) {
    nb::dict out;
    for (auto& m : members(t)) out[m.name.c_str()] = to_py(d, m.id, m.type);
    return out;
}

template <typename T, typename Getter>
nb::object get_prim(const DataRef& d, MemberId id, Getter g) {
    T v{};
    check(((*d).*g)(v, id), "get value");
    return nb::cast(v);
}

nb::object to_py(const DataRef& d, MemberId id, TypeRef t) {
    t = resolve(t);
    switch (t->get_kind()) {
        case TK_BOOLEAN: return get_prim<bool>(d, id, &DynamicData::get_boolean_value);
        case TK_BYTE: return get_prim<eprosima::fastdds::rtps::octet>(d, id, &DynamicData::get_byte_value);
        case TK_INT8: return get_prim<int8_t>(d, id, &DynamicData::get_int8_value);
        case TK_UINT8: return get_prim<uint8_t>(d, id, &DynamicData::get_uint8_value);
        case TK_INT16: return get_prim<int16_t>(d, id, &DynamicData::get_int16_value);
        case TK_UINT16: return get_prim<uint16_t>(d, id, &DynamicData::get_uint16_value);
        case TK_INT32: return get_prim<int32_t>(d, id, &DynamicData::get_int32_value);
        case TK_UINT32: return get_prim<uint32_t>(d, id, &DynamicData::get_uint32_value);
        case TK_INT64: return get_prim<int64_t>(d, id, &DynamicData::get_int64_value);
        case TK_UINT64: return get_prim<uint64_t>(d, id, &DynamicData::get_uint64_value);
        case TK_FLOAT32: return get_prim<float>(d, id, &DynamicData::get_float32_value);
        case TK_FLOAT64: return get_prim<double>(d, id, &DynamicData::get_float64_value);
        case TK_FLOAT128: {
            long double v{};
            check(d->get_float128_value(v, id), "get float128");
            return nb::cast(static_cast<double>(v));
        }
        case TK_CHAR8: {
            char v{};
            check(d->get_char8_value(v, id), "get char8");
            return nb::str(std::string(1, v).c_str());
        }
        case TK_CHAR16: {
            wchar_t v{};
            check(d->get_char16_value(v, id), "get char16");
            return nb::cast(std::wstring(1, v));
        }
        case TK_STRING8: return get_prim<std::string>(d, id, &DynamicData::get_string_value);
        case TK_STRING16: return get_prim<std::wstring>(d, id, &DynamicData::get_wstring_value);
        case TK_ENUM: return get_prim<int32_t>(d, id, &DynamicData::get_int32_value);
        case TK_STRUCTURE: {
            DataRef sub = d->loan_value(id);
            if (!sub) throw std::runtime_error("loan_value failed for struct member");
            nb::dict out = struct_to_py(sub, t);
            d->return_loaned_value(sub);
            return out;
        }
        case TK_SEQUENCE:
        case TK_ARRAY: {
            TypeRef elem = resolve(descriptor(t)->element_type());
            DataRef coll = d->loan_value(id);
            if (!coll) throw std::runtime_error("loan_value failed for collection member");
            const uint32_t n = coll->get_item_count();
            nb::object out;
            if (elem->get_kind() == TK_BYTE || elem->get_kind() == TK_UINT8) {  // sequence<octet> -> bytes
                std::vector<uint8_t> raw(n);
                for (uint32_t i = 0; i < n; ++i) raw[i] = nb::cast<uint8_t>(to_py(coll, i, elem));
                out = nb::bytes(reinterpret_cast<const char*>(raw.data()), raw.size());
            } else {
                nb::list l;
                for (uint32_t i = 0; i < n; ++i) l.append(to_py(coll, i, elem));
                out = l;
            }
            d->return_loaned_value(coll);
            return out;
        }
        default: unsupported(t);
    }
}

// ----------------------------------------------------------------- Python -> DDS

void from_py(const DataRef& d, MemberId id, TypeRef t, nb::handle v);

void struct_from_py(const DataRef& d, const TypeRef& t, nb::handle v) {
    if (!nb::isinstance<nb::dict>(v))
        throw nb::type_error(("expected a dict for struct " + t->get_name().to_string()).c_str());
    nb::dict src = nb::borrow<nb::dict>(v);
    auto ms = members(t);
    for (auto [k, val] : src) {
        std::string key = nb::cast<std::string>(k);
        bool found = false;
        for (auto& m : ms) {
            if (m.name != key) continue;
            found = true;
            try {
                from_py(d, m.id, m.type, val);
            } catch (nb::python_error&) {
                throw;
            } catch (nb::cast_error&) {
                throw nb::type_error((t->get_name().to_string() + "." + key + ": " +
                                      nb::cast<std::string>(nb::handle(val).type().attr("__name__")) +
                                      " value is the wrong type or out of range for " +
                                      kind_name(resolve(m.type))).c_str());
            } catch (nb::builtin_exception& e) {  // keep the Python exception type, add context
                throw nb::builtin_exception(e.type(), (t->get_name().to_string() + "." + key + ": " + e.what()).c_str());
            } catch (std::exception& e) {
                throw nb::value_error((t->get_name().to_string() + "." + key + ": " + e.what()).c_str());
            }
        }
        if (!found)
            throw nb::key_error((t->get_name().to_string() + " has no member '" + key + "'").c_str());
    }
}

int32_t enum_value(const TypeRef& t, nb::handle v) {
    if (!nb::isinstance<nb::str>(v)) return nb::cast<int32_t>(v);
    std::string name = nb::cast<std::string>(v);
    auto ms = members(t);
    for (size_t i = 0; i < ms.size(); ++i)
        // literals come back scoped ("demo::GREEN"); accept that or the bare name
        if (ms[i].name == name || (ms[i].name.size() > name.size() + 2 &&
                                   ms[i].name.compare(ms[i].name.size() - name.size() - 2, std::string::npos,
                                                      "::" + name) == 0))
            return ms[i].default_value.empty() ? static_cast<int32_t>(i) : std::stoi(ms[i].default_value);
    std::string list;
    for (auto& m : ms) list += (list.empty() ? "" : ", ") + m.name;
    throw nb::value_error(("'" + name + "' is not a literal of enum " + t->get_name().to_string() + " (" + list + ")").c_str());
}

void from_py(const DataRef& d, MemberId id, TypeRef t, nb::handle v) {
    t = resolve(t);
    switch (t->get_kind()) {
        case TK_BOOLEAN: check(d->set_boolean_value(id, nb::cast<bool>(v)), "set boolean"); break;
        case TK_BYTE: check(d->set_byte_value(id, nb::cast<uint8_t>(v)), "set octet"); break;
        case TK_INT8: check(d->set_int8_value(id, nb::cast<int8_t>(v)), "set int8"); break;
        case TK_UINT8: check(d->set_uint8_value(id, nb::cast<uint8_t>(v)), "set uint8"); break;
        case TK_INT16: check(d->set_int16_value(id, nb::cast<int16_t>(v)), "set int16"); break;
        case TK_UINT16: check(d->set_uint16_value(id, nb::cast<uint16_t>(v)), "set uint16"); break;
        case TK_INT32: check(d->set_int32_value(id, nb::cast<int32_t>(v)), "set int32"); break;
        case TK_UINT32: check(d->set_uint32_value(id, nb::cast<uint32_t>(v)), "set uint32"); break;
        case TK_INT64: check(d->set_int64_value(id, nb::cast<int64_t>(v)), "set int64"); break;
        case TK_UINT64: check(d->set_uint64_value(id, nb::cast<uint64_t>(v)), "set uint64"); break;
        case TK_FLOAT32: check(d->set_float32_value(id, nb::cast<float>(v)), "set float32"); break;
        case TK_FLOAT64: check(d->set_float64_value(id, nb::cast<double>(v)), "set float64"); break;
        case TK_FLOAT128: check(d->set_float128_value(id, nb::cast<double>(v)), "set float128"); break;
        case TK_CHAR8: {
            std::string s = nb::cast<std::string>(v);
            if (s.size() != 1) throw nb::value_error("char needs a 1-character str");
            check(d->set_char8_value(id, s[0]), "set char8");
            break;
        }
        case TK_CHAR16: {
            std::wstring s = nb::cast<std::wstring>(v);
            if (s.size() != 1) throw nb::value_error("wchar needs a 1-character str");
            check(d->set_char16_value(id, s[0]), "set char16");
            break;
        }
        case TK_STRING8: {
            std::string s = nb::cast<std::string>(v);
            uint32_t bound = descriptor(t)->bound().empty() ? 0 : descriptor(t)->bound().at(0);
            if (bound && bound != static_cast<uint32_t>(LENGTH_UNLIMITED) && s.size() > bound)
                throw nb::value_error(("string longer than its bound " + std::to_string(bound)).c_str());
            check(d->set_string_value(id, s), "set string");
            break;
        }
        case TK_STRING16: check(d->set_wstring_value(id, nb::cast<std::wstring>(v)), "set wstring"); break;
        case TK_ENUM: check(d->set_int32_value(id, enum_value(t, v)), "set enum"); break;
        case TK_STRUCTURE: {
            DataRef sub = d->loan_value(id);
            if (!sub) throw std::runtime_error("loan_value failed for struct member");
            try {
                struct_from_py(sub, t, v);
            } catch (...) {
                d->return_loaned_value(sub);
                throw;
            }
            d->return_loaned_value(sub);
            break;
        }
        case TK_SEQUENCE:
        case TK_ARRAY: {
            auto td = descriptor(t);
            TypeRef elem = resolve(td->element_type());
            nb::sequence items = nb::borrow<nb::sequence>(v);
            const size_t n = nb::len(items);
            if (t->get_kind() == TK_ARRAY) {
                size_t total = 1;
                for (auto b : td->bound()) total *= b;
                if (n != total)
                    throw nb::value_error(("array needs exactly " + std::to_string(total) + " elements (flattened)").c_str());
            } else if (!td->bound().empty() && td->bound()[0] != static_cast<uint32_t>(LENGTH_UNLIMITED) &&
                       n > td->bound()[0]) {
                throw nb::value_error(("sequence longer than its bound " + std::to_string(td->bound()[0])).c_str());
            }
            DataRef coll = d->loan_value(id);
            if (!coll) throw std::runtime_error("loan_value failed for collection member");
            try {
                if (t->get_kind() == TK_SEQUENCE) check(coll->clear_all_values(), "clear sequence");
                const bool complex = elem->get_kind() == TK_STRUCTURE || elem->get_kind() == TK_SEQUENCE ||
                                     elem->get_kind() == TK_ARRAY;
                for (size_t i = 0; i < n; ++i) {
                    nb::object item = items[i];
                    if (complex) {
                        // complex elements: build them standalone, then copy in
                        DataRef e = DynamicDataFactory::get_instance()->create_data(elem);
                        if (elem->get_kind() == TK_STRUCTURE) struct_from_py(e, elem, item);
                        else throw nb::type_error("nested sequences/arrays of collections are not supported");
                        check(coll->set_complex_value(static_cast<MemberId>(i), e), "set element");
                        DynamicDataFactory::get_instance()->delete_data(e);
                    } else {
                        from_py(coll, static_cast<MemberId>(i), elem, item);
                    }
                }
            } catch (...) {
                d->return_loaned_value(coll);
                throw;
            }
            d->return_loaned_value(coll);
            break;
        }
        default: unsupported(t);
    }
}

// ----------------------------------------------------------------- entities

struct QoS {
    bool reliable = true;
    bool transient_local = false;
    int32_t depth = 1;               // KEEP_LAST depth; <= 0 means KEEP_ALL
    int32_t heartbeat_ms = 0;        // writer only; 0 = Fast DDS default (3 s)
    std::string representation;      // "", "xcdr1", "xcdr2". Reader default: accept both
};

template <typename Q>
void apply_common(Q& q, const QoS& o) {
    q.reliability().kind = o.reliable ? RELIABLE_RELIABILITY_QOS : BEST_EFFORT_RELIABILITY_QOS;
    q.durability().kind = o.transient_local ? TRANSIENT_LOCAL_DURABILITY_QOS : VOLATILE_DURABILITY_QOS;
    if (o.depth > 0) {
        q.history().kind = KEEP_LAST_HISTORY_QOS;
        q.history().depth = o.depth;
    } else {
        q.history().kind = KEEP_ALL_HISTORY_QOS;
    }
}

std::vector<DataRepresentationId_t> representation(const std::string& r, bool reader) {
    if (r == "xcdr1") return {XCDR_DATA_REPRESENTATION};
    if (r == "xcdr2") return {XCDR2_DATA_REPRESENTATION};
    if (!r.empty()) throw nb::value_error("representation must be '', 'xcdr1' or 'xcdr2'");
    if (reader) return {XCDR2_DATA_REPRESENTATION, XCDR_DATA_REPRESENTATION};
    return {};
}

struct ParticipantState {
    DomainParticipant* participant = nullptr;
    Publisher* publisher = nullptr;
    Subscriber* subscriber = nullptr;
    std::map<std::string, TypeRef> types;
    std::map<std::string, Topic*> topics;

    void close() {
        if (!participant) return;
        participant->delete_contained_entities();
        DomainParticipantFactory::get_instance()->delete_participant(participant);
        participant = nullptr;
    }
    void alive() const {
        if (!participant) throw std::runtime_error("participant is closed");
    }
    ~ParticipantState() { close(); }
};

class Writer {
public:
    Writer(std::shared_ptr<ParticipantState> s, DataWriter* w, TypeRef t) : s_(std::move(s)), w_(w), type_(std::move(t)) {}

    void write(nb::handle sample) {
        s_->alive();
        DataRef d = DynamicDataFactory::get_instance()->create_data(type_);
        try {
            struct_from_py(d, type_, sample);
        } catch (...) {
            DynamicDataFactory::get_instance()->delete_data(d);
            throw;
        }
        ReturnCode_t rc;
        {
            nb::gil_scoped_release unlock;
            rc = w_->write(&d);
        }
        DynamicDataFactory::get_instance()->delete_data(d);
        check(rc, "DataWriter::write");
    }

    int32_t matched_count() {
        s_->alive();
        PublicationMatchedStatus st;
        check(w_->get_publication_matched_status(st), "get_publication_matched_status");
        return st.current_count;
    }

private:
    std::shared_ptr<ParticipantState> s_;
    DataWriter* w_;
    TypeRef type_;
};

class Reader {
public:
    Reader(std::shared_ptr<ParticipantState> s, DataReader* r, TypeRef t) : s_(std::move(s)), r_(r), type_(std::move(t)) {}

    nb::list take(int32_t max_samples) {
        s_->alive();
        nb::list out;
        auto f = DynamicDataFactory::get_instance();
        for (int32_t n = 0; max_samples <= 0 || n < max_samples;) {
            DataRef d = f->create_data(type_);
            SampleInfo info;
            ReturnCode_t rc;
            {
                nb::gil_scoped_release unlock;
                rc = r_->take_next_sample(&d, &info);
            }
            if (rc != RETCODE_OK) {
                f->delete_data(d);
                break;
            }
            if (info.valid_data) {
                out.append(struct_to_py(d, type_));
                ++n;
            }
            f->delete_data(d);
        }
        return out;
    }

    bool wait(double timeout_s) {
        s_->alive();
        nb::gil_scoped_release unlock;
        return r_->wait_for_unread_message(Duration_t(timeout_s));
    }

    int32_t matched_count() {
        s_->alive();
        SubscriptionMatchedStatus st;
        check(r_->get_subscription_matched_status(st), "get_subscription_matched_status");
        return st.current_count;
    }

private:
    std::shared_ptr<ParticipantState> s_;
    DataReader* r_;
    TypeRef type_;
};

BuiltinTransports parse_transports(const std::string& t) {
    if (t == "default") return BuiltinTransports::DEFAULT;
    if (t == "udpv4") return BuiltinTransports::UDPv4;
    if (t == "udpv6") return BuiltinTransports::UDPv6;
    if (t == "shm") return BuiltinTransports::SHM;
    if (t == "large_data") return BuiltinTransports::LARGE_DATA;
    throw nb::value_error("transports must be one of: default, udpv4, udpv6, shm, large_data");
}

class Participant {
public:
    Participant(uint32_t domain_id, const std::string& name, const std::string& transports)
        : s_(std::make_shared<ParticipantState>()) {
        auto* factory = DomainParticipantFactory::get_instance();
        DomainParticipantQos q = PARTICIPANT_QOS_DEFAULT;
        if (!name.empty()) q.name(name);
        // "env" leaves Fast DDS's defaults alone, including FASTDDS_BUILTIN_TRANSPORTS
        if (transports != "env") q.setup_transports(parse_transports(transports));
        s_->participant = factory->create_participant(domain_id, q);
        if (!s_->participant) throw std::runtime_error("create_participant failed");
        s_->publisher = s_->participant->create_publisher(PUBLISHER_QOS_DEFAULT);
        s_->subscriber = s_->participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        if (!s_->publisher || !s_->subscriber) throw std::runtime_error("create publisher/subscriber failed");
    }

    // Parse `idl_path` and register `type_name` (fully qualified, e.g. "sensors::Reading").
    // include_paths empty = no C preprocessor needed (no #include support).
    std::string register_idl(const std::string& idl_path, const std::string& type_name,
                             const std::vector<std::string>& include_paths) {
        s_->alive();
        // Parse the whole file (create_type_w_uri stops the parser at the match, and
        // Fast DDS then logs a spurious "IDL parsing failed").
        DynamicTypeBuilder::_ref_type builder;
        std::vector<std::string> seen;
        ReturnCode_t rc = DynamicTypeBuilderFactory::get_instance()->for_each_type_w_uri(
            idl_path, include_paths, [&](DynamicTypeBuilder::_ref_type b) {
                const std::string n = b->get_name().to_string();
                seen.push_back(n);
                if (n == type_name) builder = b;
                return true;
            });
        if (rc != RETCODE_OK)
            throw nb::value_error(("could not parse IDL " + idl_path + " (see the Fast DDS log above)").c_str());
        if (!builder) {
            std::string list;
            for (auto& n : seen) list += (list.empty() ? "" : ", ") + n;
            throw nb::value_error(("no type '" + type_name + "' in " + idl_path + "; it defines: " +
                                   (list.empty() ? "nothing (a parse error? see the Fast DDS log above)" : list)).c_str());
        }
        TypeRef type = builder->build();
        if (!type) throw std::runtime_error("DynamicTypeBuilder::build failed for " + type_name);
        if (resolve(type)->get_kind() != TK_STRUCTURE)
            throw nb::type_error(("topic type must be a struct: " + kind_name(type)).c_str());
        TypeSupport ts(new DynamicPubSubType(type));
        check(ts.register_type(s_->participant), "register_type");
        const std::string name = ts.get_type_name();
        s_->types[name] = type;
        return name;
    }

    Writer create_writer(const std::string& topic, const std::string& type_name, const QoS& o) {
        Topic* t = topic_for(topic, type_name);
        DataWriterQos q = DATAWRITER_QOS_DEFAULT;
        apply_common(q, o);
        if (o.heartbeat_ms > 0)
            q.reliable_writer_qos().times.heartbeat_period =
                Duration_t(o.heartbeat_ms / 1000, static_cast<uint32_t>(o.heartbeat_ms % 1000) * 1000000u);
        auto rep = representation(o.representation, false);
        if (!rep.empty()) q.representation().m_value = rep;
        DataWriter* w = s_->publisher->create_datawriter(t, q);
        if (!w) throw std::runtime_error("create_datawriter failed (see the Fast DDS log above)");
        return Writer(s_, w, s_->types.at(type_name));
    }

    Reader create_reader(const std::string& topic, const std::string& type_name, const QoS& o) {
        Topic* t = topic_for(topic, type_name);
        DataReaderQos q = DATAREADER_QOS_DEFAULT;
        apply_common(q, o);
        q.representation().m_value = representation(o.representation, true);
        DataReader* r = s_->subscriber->create_datareader(t, q);
        if (!r) throw std::runtime_error("create_datareader failed (see the Fast DDS log above)");
        return Reader(s_, r, s_->types.at(type_name));
    }

    void close() { s_->close(); }

private:
    Topic* topic_for(const std::string& topic, const std::string& type_name) {
        s_->alive();
        if (!s_->types.count(type_name))
            throw nb::key_error(("type '" + type_name + "' is not registered; call register_idl first").c_str());
        auto it = s_->topics.find(topic);
        if (it != s_->topics.end()) {
            if (it->second->get_type_name() != type_name)
                throw nb::value_error(("topic '" + topic + "' already uses type " + it->second->get_type_name()).c_str());
            return it->second;
        }
        Topic* t = s_->participant->create_topic(topic, type_name, TOPIC_QOS_DEFAULT);
        if (!t) throw std::runtime_error("create_topic failed for " + topic);
        s_->topics[topic] = t;
        return t;
    }

    std::shared_ptr<ParticipantState> s_;
};

void set_log_level(const std::string& level) {
    using eprosima::fastdds::dds::Log;
    if (level == "error") Log::SetVerbosity(Log::Error);
    else if (level == "warning") Log::SetVerbosity(Log::Warning);
    else if (level == "info") Log::SetVerbosity(Log::Info);
    else throw nb::value_error("level must be error, warning or info");
}

}  // namespace

NB_MODULE(_core, m) {
    m.doc() = "Fast DDS dynamic-types binding (see fastdds_dyn)";
    m.attr("FASTDDS_VERSION") = FASTDDS_VERSION_STR;
    m.def("set_log_level", &set_log_level, nb::arg("level"));

    nb::class_<QoS>(m, "QoS")
        .def(nb::init<>())
        .def_rw("reliable", &QoS::reliable)
        .def_rw("transient_local", &QoS::transient_local)
        .def_rw("depth", &QoS::depth)
        .def_rw("heartbeat_ms", &QoS::heartbeat_ms)
        .def_rw("representation", &QoS::representation);

    nb::class_<Writer>(m, "Writer")
        .def("write", &Writer::write, nb::arg("sample"))
        .def("matched_count", &Writer::matched_count);

    nb::class_<Reader>(m, "Reader")
        .def("take", &Reader::take, nb::arg("max_samples") = 0)
        .def("wait", &Reader::wait, nb::arg("timeout") = 1.0)
        .def("matched_count", &Reader::matched_count);

    nb::class_<Participant>(m, "Participant")
        .def(nb::init<uint32_t, const std::string&, const std::string&>(),
             nb::arg("domain_id") = 0, nb::arg("name") = "", nb::arg("transports") = "env")
        .def("register_idl", &Participant::register_idl, nb::arg("idl_path"), nb::arg("type_name"),
             nb::arg("include_paths") = std::vector<std::string>{})
        .def("create_writer", &Participant::create_writer, nb::arg("topic"), nb::arg("type_name"), nb::arg("qos"))
        .def("create_reader", &Participant::create_reader, nb::arg("topic"), nb::arg("type_name"), nb::arg("qos"))
        .def("close", &Participant::close);
}
