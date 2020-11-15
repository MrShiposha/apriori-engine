#ifndef ___APRIORI2_LOG_H___
#define ___APRIORI2_LOG_H___

void log(const char *level, const char *target, const char *format, ...);

void trace(const char *target, const char *format, ...);
void debug(const char *target, const char *format, ...);
void info(const char *target, const char *format, ...);
void warn(const char *target, const char *format, ...);
void error(const char *target, const char *format, ...);

#endif // ___APRIORI2_LOG_H___