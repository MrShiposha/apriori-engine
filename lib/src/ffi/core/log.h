#ifndef ___APRIORI2_CORE_LOG_H___
#define ___APRIORI2_CORE_LOG_H___

#include "def.h"

#define LOG_FFI_TARGET "FFI"
#define LOG_SUB_TARGET(parent_target, child_target) parent_target "/" child_target
#define LOG_STRUCT_TARGET(struct_name) LOG_SUB_TARGET(LOG_FFI_TARGET, #struct_name)

void log(const char *level, const char *target, const char *format, ...);

void trace(const char *target, const char *format, ...);
void debug(const char *target, const char *format, ...);
void info(const char *target, const char *format, ...);
void warn(const char *target, const char *format, ...);
void error(const char *target, const char *format, ...);

#define LOG_GROUP_TYPE(group_name) ___apriori_impl_LogGroup_##group_name
#define LOG_GROUP_CASE(group_name) LOG_GROUP_TYPE(group_name)*
#define DEFINE_LOG_GROUP(group_name) typedef struct { Byte _; } LOG_GROUP_TYPE(group_name)

DEFINE_LOG_GROUP(struct);
DEFINE_LOG_GROUP(struct_op);
DEFINE_LOG_GROUP(struct_op_block);

#define LOG_GROUP(group_name, ...) _Generic(((LOG_GROUP_CASE(group_name))(NULL)), \
    LOG_GROUP_CASE(struct): "\t- " __VA_ARGS__, \
    LOG_GROUP_CASE(struct_op): "\t+---- " __VA_ARGS__, \
    LOG_GROUP_CASE(struct_op_block): "\t+----x.... " __VA_ARGS__ \
)

#undef DEFINE_LOG_GROUP

#endif // ___APRIORI2_CORE_LOG_H___