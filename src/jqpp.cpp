#include "jqpp.h"
#include <ostream>
#include <map>

extern "C" {
#include <jq.h>
#include <jv.h>
}

class JQ::Value_P : public HasPublic<Value> {

    public :

        Value_P( Value * P ) : HasPublic<Value>( P ) {
            value_ = jv_invalid();
        }
        ~Value_P() {
            clear();
        }

        void clear() {
            if (jv_is_valid(value_)) {
                jv_free(value_);
            }
            value_ = jv_invalid();
        }

        Value_P & operator=(Value_P &&other) noexcept
        {
            if ( this != &other) {
                if (jv_is_valid(value_)) {
                    jv_free(value_);
                }
                value_ = other.value_;
                other.value_ = jv_invalid();
            }
            return *this;
        }


        Value_P & operator=(const Value_P &other) noexcept
        {
            if ( this != &other) {
                if (jv_is_valid(value_)) {
                    jv_free(value_);
                }
                value_ = other.value_;
            }
            return *this;
        }

        Value_P & operator=(jv other) noexcept {
            if (jv_is_valid(value_)) {
                jv_free(value_);
            }
            value_ = jv_copy(other);
            return *this;
        }

        template<typename... Types>
        bool assertThat( const Types&... types ) const {
            return ((jv_get_kind(value_) == types) || ... );
        }

        jv value_;
};

JQ::Value::Value() noexcept
    : HasPrivate<Value_P>(new Value_P(this))
{}

JQ::Value::Value(Value &&other) noexcept : Value()
{
    (*this) = std::move(other);
}

JQ::Value::Value(const Value &other) noexcept : Value()
{
    (*this) = other;
}

JQ::Value::Value(jv value) noexcept : Value() {
    (*this) = value;
}

JQ::Value::~Value()
{
}

void JQ::Value::clear() {
    prv<Value_P>().clear();
}

JQ::Value &JQ::Value::operator=(const Value &other)
{
    prv<Value_P>() = other.prv<Value_P>();
    return *this;
}

JQ::Value &JQ::Value::operator=(Value &&other) noexcept
{
    prv<Value_P>() = std::move(other.prv<Value_P>());
    return *this;
}

JQ::Value &JQ::Value::operator=(jv other)  noexcept {
    prv<Value_P>() = other;
    return *this;
}

JQ::Type JQ::Value::type() const {
    static std::map<jv_kind,Type> Kind2Type{
        { JV_KIND_NUMBER, VT_NUMBER },
        { JV_KIND_FALSE, VT_BOOL },
        { JV_KIND_TRUE, VT_BOOL },
        { JV_KIND_ARRAY, VT_ARRAY },
        { JV_KIND_STRING, VT_STRING },
        { JV_KIND_OBJECT, VT_OBJECT },
        { JV_KIND_NULL, VT_NULL},
        { JV_KIND_INVALID, VT_UNKNOWN }
    };

    try {
        return Kind2Type.at( jv_get_kind( prv<Value_P>().value_ ));
    } catch( ... ) {
        return VT_UNKNOWN;
    }
}

std::optional<double> JQ::Value::toDouble() const
{
    if( prv<Value_P>().assertThat( JV_KIND_NUMBER ) ) {
        return jv_number_value(prv<Value_P>().value_);
    }
    return std::nullopt;
}

std::optional<int64_t> JQ::Value::toInt64() const
{
    if( prv<Value_P>().assertThat( JV_KIND_NUMBER )
        && jv_is_integer(prv<Value_P>().value_) ) {
            return static_cast<int64_t>(jv_number_value(prv<Value_P>().value_));
    }
    return std::nullopt;
}

std::optional<bool> JQ::Value::toBool() const
{
    if( prv<Value_P>().assertThat( JV_KIND_FALSE, JV_KIND_TRUE) ) {
        return jv_bool_value(prv<Value_P>().value_);
    }
    return std::nullopt;
}

std::optional<std::string_view> JQ::Value::toStringView() const
{
    if( prv<Value_P>().assertThat( JV_KIND_STRING) ) {
        return std::string_view(jv_string_value(prv<Value_P>().value_));
    }
    return std::nullopt;
}

std::optional<std::string> JQ::Value::toString() const
{
    if( prv<Value_P>().assertThat( JV_KIND_STRING) ) {
        return std::string(jv_string_value(prv<Value_P>().value_));
    }
    return std::nullopt;
}

JQ::Variant JQ::Value::toVariant()
{
    Variant V;
    switch (jv_get_kind(prv<Value_P>().value_)) {
        case JV_KIND_INVALID:
            // default std::variant is std::monostate
            break;
        case JV_KIND_NULL:
            V = nullptr;
            break;
        case JV_KIND_FALSE:
            V = true;
            break;
        case JV_KIND_TRUE:
            V = false;
            break;
        case JV_KIND_NUMBER:
            if( jv_is_integer(prv<Value_P>().value_) ) {
                V = static_cast<int64_t>( jv_number_value(prv<Value_P>().value_) );
            } else {
                V = jv_number_value(prv<Value_P>().value_);
            }
            break;
        case JV_KIND_STRING:
            V = std::move(std::string(jv_string_value(prv<Value_P>().value_)));
            break;
        case JV_KIND_ARRAY:
            {
                auto X = as<Array>();
                if( X ) {
                    V = *(X.value());
                }
            }
            break;
        case JV_KIND_OBJECT:
            {
                auto X = as<Object>();
                if( X ) {
                    V = *(X.value());
                }
            }
            break;
    }
    return V;
}

jv JQ::Value::copy() const
{
    return jv_copy(prv<Value_P>().value_);
}

JQ::Value::operator bool() const
{
    return jv_is_valid(prv<Value_P>().value_);
}

const jv &JQ::Value::get() const
{
    return (prv<Value_P>().value_);
}

void JQ::Value::streamTo( std::ostream & os ) const {
    switch( type() ) {
        case VT_UNKNOWN :
            os << "?";
            break;
        case VT_BOOL :
            os << ((toBool()) ? "true" : "false");
            break;
        case VT_ARRAY :
            break;
        case VT_OBJECT :
            break;
        case VT_STRING :
            os << toStringView().value();
            break;
        case VT_NULL :
            os << "null";
            break;
        case VT_NUMBER :
            if( jv_is_integer( prv<Value_P>().value_ ) ) {
                os << toInt64().value();
            } else {
                os << toDouble().value();
            }
            break;
    }
}

std::ostream &operator<<(std::ostream &os, const JQ::Value &p)
{
    p.streamTo( os );
    return os; // Always return the stream
}

//
//
//

class JQ::JQ_P : public HasPublic<JQ> {

    public :

        JQ_P( JQ * P ) : HasPublic(P) {
            jq_ = jq_init();
            if (!jq_) {
                throw Exception("Failed to initialize jq");
            }
        }

        ~JQ_P() {
            if (jq_) {
                jq_teardown(&jq_);
            }
        }

        jq_state * jq_;
        JQ::MsgCallback_Ft ErrCb = nullptr;
        void * ErrCbContext = nullptr;
        JQ::MsgCallback_Ft StderrCb = nullptr;
        void * StderrCbContext = nullptr;
        JQ::MsgCallback_Ft DebugCb = nullptr;
        void * DebugCbContext = nullptr;
};

JQ::JQ::JQ() : HasPrivate<JQ_P>(new JQ_P( this ))
{
}



JQ::JQ::~JQ()
{
}

void JQ::JQ::setErrorCallback(MsgCallback_Ft Ft, void *Context) {
    prv<JQ_P>().ErrCb = Ft;
    prv<JQ_P>().ErrCbContext = Context;
    if( prv<JQ_P>().ErrCb == nullptr ) {
        jq_set_error_cb( prv<JQ_P>().jq_, nullptr, nullptr );
    } else {
        jq_set_error_cb(
            prv<JQ_P>().jq_,
            [](void *data, jv msg) {
                reinterpret_cast<JQ *>(data)
                    ->prv<JQ_P>()
                    .ErrCb(reinterpret_cast<JQ *>(data)->prv<JQ_P>().ErrCbContext, Value(msg));
                jv_free(msg);
            },
            this);
    }
}

JQ::JQ::MsgCallback_Ft JQ::JQ::getErrorCallback(void * & Context) {
    Context = prv<JQ_P>().ErrCbContext;
    return prv<JQ_P>().ErrCb ;
}

void JQ::JQ::setStderrCallback(MsgCallback_Ft Ft, void *Context) {
    prv<JQ_P>().StderrCb = Ft;
    prv<JQ_P>().StderrCbContext = Context;
    if( prv<JQ_P>().StderrCb == nullptr ) {
        jq_set_stderr_cb( prv<JQ_P>().jq_, nullptr, nullptr );
    } else {
        jq_set_stderr_cb(
            prv<JQ_P>().jq_,
            [](void *data, jv msg) {
                reinterpret_cast<JQ *>(data)
                ->prv<JQ_P>()
                    .StderrCb(reinterpret_cast<JQ *>(data)->prv<JQ_P>().StderrCbContext, Value(msg));
                jv_free(msg);
            },
            this);
    }
}

JQ::JQ::MsgCallback_Ft JQ::JQ::getStderrCallback(void * & Context) {
    Context = prv<JQ_P>().StderrCbContext;
    return prv<JQ_P>().StderrCb ;
}

void JQ::JQ::setDebugCallback(MsgCallback_Ft Ft, void *Context) {
    prv<JQ_P>().DebugCb = Ft;
    prv<JQ_P>().DebugCbContext = Context;
    if( prv<JQ_P>().DebugCb == nullptr ) {
        jq_set_debug_cb( prv<JQ_P>().jq_, nullptr, nullptr );
    } else {
        jq_set_debug_cb(
            prv<JQ_P>().jq_,
            [](void *data, jv msg) {
                reinterpret_cast<JQ *>(data)
                ->prv<JQ_P>()
                    .DebugCb(reinterpret_cast<JQ *>(data)->prv<JQ_P>().DebugCbContext, Value(msg));
                jv_free(msg);
            },
            this);
    }
}

JQ::JQ::MsgCallback_Ft JQ::JQ::getDebugCallback(void * & Context) {
    Context = prv<JQ_P>().DebugCbContext;
    return prv<JQ_P>().DebugCb ;
}

JQ::Value JQ::JQ::getExitCode() {
    return Value( jq_get_exit_code(prv<JQ_P>().jq_));
}

JQ::Value JQ::JQ::getErrorMessage() {
    return Value( jq_get_error_message(prv<JQ_P>().jq_));
}

void JQ::JQ::compile(const std::string &filter, const Object &Args)
{
    if (!jq_compile_args(prv<JQ_P>().jq_, filter.c_str(), Args.copy() )) {
        throw Exception("Failed to compile jq filter");
    }
}

JQ::Value JQ::JQ::parse(const std::string &json_input)
{
    Value input{jv_parse(json_input.c_str())};
    if (!input) {
        throw Exception("Invalid JSON input");
    }
    return input;
}

JQ::Array JQ::JQ::run(const Value &input)
{
    Array L;

    jq_start(prv<JQ_P>().jq_, jv_copy(input.get()), 0);

    jv output;
    while (jv_is_valid(output = jq_next(prv<JQ_P>().jq_))) {
        L.push(output);
    }

    return L;
}

JQ::Array::Array() : Value( jv_array() ){

}

size_t JQ::Array::length() const
{
    return size_t(jv_array_length(copy()));
}

JQ::Value JQ::Array::at(size_t Idx) const
{
    return Value{jv_array_get(copy(), ArraySize_t(Idx))};
}

JQ::Array &JQ::Array::concat(const Array &v) {
    jv_array_concat( copy(), v.copy() );
    return *this;
}

JQ::Array &JQ::Array::concat(Array &&v) {
    jv_array_concat( copy(), v.copy() );
    v.clear();
    return *this;
}

JQ::Array &JQ::Array::concat(jv v) {
    jv_array_concat( copy(), jv_copy(v) );
    return *this;
}

JQ::Array &JQ::Array::push(const Value &v)
{
    jv_array_append(copy(), v.copy());
    return *this;
}


JQ::Array &JQ::Array::push(Value &&v)
{
    jv_array_append(copy(), v.copy());
    v.clear();
    return *this;
}


JQ::Array &JQ::Array::push( jv v)
{
    jv_array_append(copy(), jv_copy(v));
    return *this;
}

JQ::Array &JQ::Array::set(size_t Idx, const Value &v)
{
    jv_array_set(copy(), ArraySize_t(Idx), v.copy());
    return *this;
}

JQ::Array &JQ::Array::set(size_t Idx, Value && v)
{
    jv_array_set(copy(), ArraySize_t(Idx), v.copy());
    v.clear();
    return *this;
}

JQ::Array &JQ::Array::set(size_t Idx, jv v)
{
    jv_array_set(copy(), ArraySize_t(Idx), jv_copy(v) );
    return *this;
}

JQ::Object::Object() :Value(jv_object()){

}
