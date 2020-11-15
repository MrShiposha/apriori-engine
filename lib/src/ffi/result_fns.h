#ifndef ___APRIORI2_RESULT_FNS_H___
#define ___APRIORI2_RESULT_FNS_H___

#include <vulkan/vulkan.h>

#include "def.h"
#include "result.h"

Result new_result(Handle object, Apriori2Error error);

Result apriori2_success();
Result apriori2_error(Apriori2Error error);

#endif // ___APRIORI2_RESULT_FNS_H___