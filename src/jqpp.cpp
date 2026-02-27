#include "jqpp.h"
#include <ostream>
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
            if (jv_is_valid(value_)) {
                jv_free(value_);
            }
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

        Value_P & operator=(const jv & other) noexcept {
            if (jv_is_valid(value_)) {
                jv_free(value_);
            }
            value_ = jv_copy(other);
            return *this;
        }

        Value_P & operator=(jv && other) noexcept {
            if (jv_is_valid(value_)) {
                jv_free(value_);
            }
            value_ = other;
            other = jv_invalid();
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

JQ::Value::Value(const jv &value) noexcept : Value() {
    (*this) = value;
}

JQ::Value::Value(jv &&value) noexcept : Value() {
    (*this) = std::move(value);
}

JQ::Value::~Value()
{
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

JQ::Value &JQ::Value::operator=(const jv & other) noexcept {
    prv<Value_P>() = other;
    return *this;
}

JQ::Value &JQ::Value::operator=(jv && other)  noexcept {
    prv<Value_P>() = std::move(other);
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

std::optional<JQ::Object> JQ::Value::toObject() const
{
    if( prv<Value_P>().assertThat( JV_KIND_OBJECT) ) {
        Object L;
        jv_object_foreach(prv<Value_P>().value_, key, val)
        {
            L.try_emplace( jv_string_value(key), val );
        }
        return L;
    }
    return std::nullopt;
}

std::optional<JQ::ObjectView> JQ::Value::toObjectView() const
{
    if( prv<Value_P>().assertThat( JV_KIND_OBJECT) ) {
        ObjectView L;
        jv_object_foreach(prv<Value_P>().value_, key, val)
        {
            L.try_emplace(jv_string_value(key), val );
        }
        return L;
    }
    return std::nullopt;
}

std::optional<JQ::Array> JQ::Value::toArray() const
{
    if( prv<Value_P>().assertThat( JV_KIND_ARRAY) ) {
        Array L;
        jv_array_foreach(prv<Value_P>().value_, iter, val)
        {
            L.emplace_back(val);
        }
        return L;
    }
    return std::nullopt;
}

JQ::VariantView JQ::Value::asVariantView() const
{
    VariantView V;
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
            V = std::move(std::string_view(jv_string_value(prv<Value_P>().value_)));
            break;
        case JV_KIND_ARRAY:
            V = toArray().value();
            break;
        case JV_KIND_OBJECT:
            V = toObjectView().value();
            break;
    }
    return V;
}


JQ::Variant JQ::Value::toVariant() const
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
            V = toArray().value();
            break;
        case JV_KIND_OBJECT:
            V = toObject().value();
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

};

JQ::JQ::JQ() : HasPrivate<JQ_P>(new JQ_P( this ))
{
}

JQ::JQ::~JQ()
{
}

void JQ::JQ::compile(const std::string &filter)
{
    if (!jq_compile(prv<JQ_P>().jq_, filter.c_str())) {
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
        L.emplace_back(output);
    }

    return L;
}

