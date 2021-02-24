#ifndef QJSON
#define QJSON

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
// See https://docs.microsoft.com/en-us/cpp/c-runtime-library/security-features-in-the-crt?view=msvc-160
#define _CRT_SECURE_NO_WARNINGS
#define timegm _mkgmtime
#endif

// qjson_decode accept a qjson text string as input and returns a 
// heap allocated string. If the string start with the character '{',
// the string is the json encoding of the input text, otherwise it
// is an error message. qjson_decode will never return NULL or an
// empty string.
char* qjson_decode(const char* qjsonText);


// qjson_version returns the version of the code and the supported
// syntax (e.g. "qjson-c: v0.1.1 syntax: v0.0.0")
const char* qjson_version();

#ifdef __cplusplus
}
#endif

#endif
