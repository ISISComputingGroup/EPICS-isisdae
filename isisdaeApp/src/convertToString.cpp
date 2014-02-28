#include <string>
#include "convertToString.h"

#ifdef _WIN32
#define snprintf _snprintf
#endif

template<typename T>
std::string convertToString(T t)
{
    should_never_get_called;
}

template<>
std::string convertToString(double t)
{
    char buffer[30];
	snprintf(buffer, sizeof(buffer), "%f", t);
    return buffer;
}

template<>
std::string convertToString(int t)
{
    char buffer[30];
	snprintf(buffer, sizeof(buffer), "%d", t);
    return buffer;
}