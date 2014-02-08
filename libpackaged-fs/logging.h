/* vim: set ts=4 sw=4 tw=0 et ai :*/

// FIXME: Unlike in the Python libraries, messages are not automatically
//        intended on newline characters.

#ifndef CLASS_LOGGING
#define CLASS_LOGGING

#include <libpackaged-fs/config.h>

#include <string>
#include <stdarg.h>

namespace AppLib
{
    class Logging
    {
          public:
        static bool debug;
        static bool verbose;
        static void showErrorW(std::string msg, ...);
        static void showErrorO(std::string msg, ...);
        static void showWarningW(std::string msg, ...);
        static void showWarningO(std::string msg, ...);
        static void showInfoW(std::string msg, ...);
        static void showInfoO(std::string msg, ...);
        static void showSuccessW(std::string msg, ...);
        static void showSuccessO(std::string msg, ...);
        static void showDebugW(std::string msg, ...);
        static void showDebugO(std::string msg, ...);
        static void setApplicationName(std::string name);

          private:
        static std::string appname;
        static std::string appblnk;
        static void showMsgW(std::string type, std::string msg, va_list arglist);
        static void showMsgO(std::string msg, va_list arglist);
        static std::string alignText(std::string text, unsigned int len);
        static std::string buildPrefix(std::string type);
    };
}

#endif
