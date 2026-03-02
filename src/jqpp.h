#pragma once

#include <assert.h>
#include <stdexcept>
#include <string>
#include <variant>

// to hide all libjq API
#include <optional>
#include <pimpl.h>

#include <type_traits>

extern "C" {
struct jv;
typedef struct jv jv;
struct jq_state;
}

namespace JQ
{
    class Value;

    class Object;
    class Array;

    using Variant = std::variant<
        std::monostate,
        std::nullptr_t,
        double,
        int64_t,
        std::string,
        Object,
        Array,
        bool
    >;

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
            explicit Value(jv value) noexcept;
            Value(const Value & other) noexcept;
            Value(Value &&other) noexcept;
            ~Value();

            void clear();

            Value &operator=(const Value & other);
            Value &operator=(Value &&other) noexcept;
            Value &operator=(jv other) noexcept;

            operator bool( ) const;

            Type type() const;

            static bool canConvert() { return true; }

            template <typename T> auto as();

            void streamTo( std::ostream & os ) const;

            const jv & get() const;
            jv copy() const;
        private :

            // returns type if type
            std::optional<double> toDouble() const;
            std::optional<int64_t> toInt64() const;
            std::optional<bool> toBool() const;
            std::optional<std::string> toString() const;
            std::optional<std::string_view> toStringView() const;
            Variant toVariant();

    };

    class Array : public Value
    {
        public :

            struct Iterator {
                    Array & ptr;
                    size_t Idx;
                    // 1. Dereference
                    Value operator*() const { return ptr.at(Idx); }
                    // 2. Increment
                    Iterator& operator++() { Idx++; return *this; }
                    // 3. Comparison
                    bool operator!=(const Iterator& other) const { return Idx != other.Idx; }
            };

            Array();

            static std::string_view validTypes() { return "array"; }
            static bool canConvert( const Value & V ) { return V.type() == VT_ARRAY; }

            Iterator begin() {
                return Iterator{*this,0};
            }

            // array specific operators
            size_t length() const;

            Iterator end() {
                return Iterator{*this,length()-1};
            }

            Value operator[]( size_t Idx ) const {
                return at(Idx);
            }
            Value at( size_t Idx ) const;

            Array & push( const Value & v );
            Array & push( Value && v );
            Array & push( jv v );

            Array & concat( const Array & v );
            Array & concat( Array && v );
            Array & concat( jv v );

            Array & set( size_t Idx, const Value & v );
            Array & set( size_t Idx, Value && v );
            Array & set( size_t Idx, jv v );

    };

    class Object : public Value
    {
        public :

            Object();
            static std::string_view validTypes() { return "object"; }
            static bool canConvert( const Value & V ) { return V.type() == VT_OBJECT; }

            // object specific operators
            void emplace_back( jv && value );

    };

    /*
     * downcast helper
     *
     * usage :
     *
     * JQ::Value F;
     * auto Q = F.as<ValueDerived>(); // raises exception of F cannot be converted to ValueDerived
     */

    template<typename T>
    inline auto Value::as()
    {
        // define the expected return type at the top
        using OptType = std::conditional_t<std::is_base_of_v<JQ::Value, T>,
                                           std::optional<T*>,
                                           std::optional<T>>;


        // 1. Check if T is derived from JQ::Value
        if constexpr (std::is_base_of_v<JQ::Value, T>) {
            if (T::canConvert(*this)) {
                return std::optional<T*>{static_cast<T*>(this)};
            }
            return OptType{std::nullopt};
        }
        if constexpr (std::is_same_v<T, double>) {
            return toDouble(); // returns std::optional<double>
        }
        if constexpr (std::is_same_v<T, int64_t>) {
            return toInt64();
        }
        if constexpr (std::is_same_v<T, int>) {
            return std::optional<int>{(int)(toInt64().value())};
        }
        if constexpr (std::is_same_v<T, std::string>) {
            return toString();
        }
        if constexpr (std::is_same_v<T, std::string_view>) {
            return toStringView();
        }
        if constexpr (std::is_same_v<T, bool>) {
            return toBool();
        }
        if constexpr (std::is_same_v<T, JQ::Variant>) {
            return toVariant();
        }

        return OptType{std::nullopt};
    }

    class JQ_P; // private data of JQ
    class JQ : private HasPrivate<JQ_P>
    {
        public:
            using MsgCallback_Ft = void (*)(void *, const Value & V );
            enum {
                JQ_DEBUG_TRACE = 1,
                JQ_DEBUG_TRACE_DETAIL = 2,
                JQ_DEBUG_TRACE_ALL = JQ_DEBUG_TRACE | JQ_DEBUG_TRACE_DETAIL,
            };

            JQ();
            JQ(const std::string &filter, const Object & Args = Object() ) : JQ()
            {
                compile(filter, Args);
            }
            ~JQ();

            // compile query
            void compile(const std::string &filter, const Object & Args = Object());

            // compile json expression
            static Value parse(const std::string &json_input);

            // execute this string on this expression
            Array run(const Value & input);

            void setErrorCallback( MsgCallback_Ft, void * Context );
            MsgCallback_Ft getErrorCallback( void * & Context );
            void setStderrCallback(MsgCallback_Ft, void *);
            MsgCallback_Ft getStderrCallback(void *&);
            void setDebugCallback(MsgCallback_Ft, void *);
            MsgCallback_Ft getDebugCallback(void *&);

            // void jq_dump_disassembly(jq_state *, int );
            Value getExitCode();
            Value getErrorMessage();

#ifdef GONE

            typedef jv (*jq_input_cb)(jq_state *, void *);
            void jq_set_input_cb(jq_state *,jq_input_cb, void *);
            void jq_get_input_cb(jq_state *, jq_input_cb *, void **);

            void jq_set_attrs(jq_state *, jv);
            jv jq_get_attrs(jq_state *);

            jv jq_get_jq_origin(jq_state *);
            jv jq_get_prog_origin(jq_state *);
            jv jq_get_lib_dirs(jq_state *);
            void jq_set_attr(jq_state *, jv, jv);
            jv jq_get_attr(jq_state *, jv);
#endif
            /*
            // void jq_set_nomem_handler(jq_state *, void (*)(void *), void *);
            // jv jq_format_error(jv msg);
            // void jq_report_error(jq_state *, jv);


            typedef struct jq_util_input_state jq_util_input_state;
            typedef void (*jq_util_msg_cb)(void *, const char *);

            jq_util_input_state *jq_util_input_init(jq_util_msg_cb, void *);
            void jq_util_input_set_parser(jq_util_input_state *, jv_parser *, int);
            void jq_util_input_free(jq_util_input_state **);
            void jq_util_input_add_input(jq_util_input_state *, const char *);
            int jq_util_input_errors(jq_util_input_state *);
            jv jq_util_input_next_input(jq_util_input_state *);
            jv jq_util_input_next_input_cb(jq_state *, void *);
            jv jq_util_input_get_position(jq_state*);
            jv jq_util_input_get_current_filename(jq_state*);
            jv jq_util_input_get_current_line(jq_state*);

            int jq_set_colors(const char *);
*/

    };
} // namespace JQ

// implement iostream operator
std::ostream& operator<<(std::ostream& os, const JQ::Value & p);
