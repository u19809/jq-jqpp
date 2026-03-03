#include "jqpp.h"
#include <iostream>

int main() {
    try {
        JQ::JQ jq;

        jq.compile(".name");

        JQ::Value data = jq.parse( R"({"name":"Alice","age":30})" );

        jq.trace( true, false );

        auto results = jq.run( data );

        for (const auto & r : results) {
            std::cout << r << std::endl;
        }
    }
    catch (const JQ::Exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Other Error" << std::endl;
    }
    return 0;
}
