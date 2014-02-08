/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/config.h>

#include <string>
#include <libpackaged-fs/logging.h>

namespace AppLib
{
    bool Logging::debug = false;
    bool Logging::verbose = true;
     std::string Logging::appname = "apptools";
     std::string Logging::appblnk = "        ";

    void Logging::showErrorW(std::string msg, ...)
    {
        va_list arglist;
         va_start(arglist, msg);
         Logging::showMsgW("error", msg, arglist);
         va_end(arglist);
    }

    void Logging::showErrorO(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        Logging::showMsgO(msg, arglist);
        va_end(arglist);
    }

    void Logging::showWarningW(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        Logging::showMsgW("warning", msg, arglist);
        va_end(arglist);
    }

    void Logging::showWarningO(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        Logging::showMsgO(msg, arglist);
        va_end(arglist);
    }

    void Logging::showInfoW(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        if (Logging::verbose)
            Logging::showMsgW("info", msg, arglist);
        va_end(arglist);
    }

    void Logging::showInfoO(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        if (Logging::verbose)
            Logging::showMsgO(msg, arglist);
        va_end(arglist);
    }

    void Logging::showSuccessW(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        Logging::showMsgW("success", msg, arglist);
        va_end(arglist);
    }

    void Logging::showSuccessO(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        Logging::showMsgO(msg, arglist);
        va_end(arglist);
    }

    void Logging::showDebugW(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        if (Logging::debug)
            Logging::showMsgW("debug", msg, arglist);
        va_end(arglist);
    }

    void Logging::showDebugO(std::string msg, ...)
    {
        va_list arglist;
        va_start(arglist, msg);
        if (Logging::debug)
            Logging::showMsgO(msg, arglist);
        va_end(arglist);
    }

    void Logging::setApplicationName(std::string name)
    {
        Logging::appname = name;
        Logging::appblnk = "";
        while (Logging::appblnk.length() < Logging::appname.length())
            Logging::appblnk += " ";
    }

    // Internal functions.

    void Logging::showMsgW(std::string type, std::string msg, va_list arglist)
    {
        std::string resultMessage = "";
        resultMessage = buildPrefix(type);
        resultMessage += msg;
        resultMessage += "\n";
        vprintf(resultMessage.c_str(), arglist);
    }

    void Logging::showMsgO(std::string msg, va_list arglist)
    {
        std::string resultMessage = appblnk;
        resultMessage += "    ";	// ": [ "
        resultMessage += "       ";	// "_______"
        resultMessage += "   ";	// " ] "
        resultMessage += msg;
        resultMessage += "\n";
        vprintf(resultMessage.c_str(), arglist);
    }

    std::string Logging::alignText(std::string text, unsigned int len)
    {
        std::string resultMessage = text;
        while (resultMessage.length() < len)
            resultMessage += " ";
        return resultMessage;
    }

    std::string Logging::buildPrefix(std::string type)
    {
        std::string resultMessage = "";
        resultMessage = appname;
        resultMessage += ": [ ";
        resultMessage += alignText(type, 7);
        resultMessage += " ] ";
        return resultMessage;
    }
}
