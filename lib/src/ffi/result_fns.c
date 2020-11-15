#include "result_fns.h"

Result new_result(Handle object, Apriori2Error error) {
    Result result = { 0 };
    result.error = error;
    result.object = object;

    return result;
}

Result apriori2_success() {
    return new_result(NULL, SUCCESS);
}

Result apriori2_error(Apriori2Error error) {
    return new_result(NULL, error);
}