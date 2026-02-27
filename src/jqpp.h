#pragma once

#include <assert.h>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// to hide all libjq API
#include <optional>
#include <pimpl.h>
extern "C" {
struct jv;
typedef struct jv jv;
struct jq_state;
}

namespace JQ
{
    class Value;

    using Array = std::vector<Value>;
    using ArrayView = std::vector<Value>;
    using Object = std::map<std::string,Value>;
    using ObjectView = std::map<std::string_view,Value>;
    using Variant = std::variant<std::monostate,std::nullptr_t,double,int64_t,std::string,Object,Array,bool>;
    using VariantView = std::variant<std::monostate,std::nullptr_t,double,int64_t,std::string_view,ObjectView,Array,bool>;

    enum Type {
        VT_UNKNOWN,
        VT_NULL,
        VT_NUMBER, // int64 or double
        VT_STRING,
        VT_OBJECT,
        VT_ARRAY,
        VT_BOOL
    };

    class Exception : public std::runtime_error
    {
        public:
            using std::runtime_error::runtime_error;
    };

    class Value_P; // private data of Value
    class Value : private HasPrivate<Value_P>
    {
        public:

            Value() noexcept;
            explicit Value(const jv & value) noexcept;
            explicit Value(jv && value) noexcept;
            Value(const Value & other) noexcept;
            Value(Value &&other) noexcept;
            ~Value();

            Value &operator=(const Value & other);
            Value &operator=(Value &&other) noexcept;
            Value &operator=(const jv & other) noexcept;
            Value &operator=(jv && other) noexcept;

            operator bool( ) const;

            Type type() const;

            // returns type if type
            std::optional<double> toDouble() const;
            std::optional<int64_t> toInt64() const;
            std::optional<bool> toBool() const;
            std::optional<std::string> toString() const;
            std::optional<std::string_view> toStringView() const;
            std::optional<Array> toArray() const;
            std::optional<Object> toObject() const;
            std::optional<ObjectView> toObjectView() const;
            Variant toVariant() const;
            VariantView asVariantView() const;

            void streamTo( std::ostream & os ) const;

            const jv & get() const;
            jv copy() const;
    };

    class JQ_P; // private data of JQ
    class JQ : private HasPrivate<JQ_P>
    {
        public:
            JQ();
            ~JQ();

            // compile query
            void compile(const std::string &filter);

            // compile json expression
            Value parse(const std::string &json_input);

            // execute this string on this expression
            Array run(const Value & input);
    };
} // namespace JQ

// implement iostream operator
std::ostream& operator<<(std::ostream& os, const JQ::Value & p);
