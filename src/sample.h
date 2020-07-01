#ifndef __sample_H__
#define __sample_H__
#include "config.h"
#include "Socket.hpp"
#include "oxoolmodule.h"

#include <Poco/Net/HTTPRequest.h>
#include <Poco/MemoryStream.h>
#include <Poco/FormattingChannel.h>
#include <Poco/AutoPtr.h>
#include <Poco/Util/XMLConfiguration.h>

using Poco::Net::HTTPRequest;
using Poco::MemoryInputStream;

class sample :public oxoolmodule
{
public:
    sample();

    void handleRequest(std::weak_ptr<StreamSocket>, MemoryInputStream&, HTTPRequest&, SocketDisposition&);
    std::string handleAdmin(std::string);
    std::string getHTMLFile(std::string);
    virtual void setProgPath(std::string path)
    {
        loPath = path + "/program";
    }


    virtual void setLogPath();
    virtual void handlesample(std::weak_ptr<StreamSocket>,
                                 Poco::Net::HTTPRequest&,
                                 Poco::MemoryInputStream&,
                                 SocketDisposition& );

    static void ViewCallback(const int type, const char* p, void* data);

private:

    std::chrono::steady_clock::time_point startStamp;
    Poco::Util::XMLConfiguration *xml_config;
    std::string ConfigFile;

    bool isUnoCompleted;

    Poco::AutoPtr<Poco::FormattingChannel> channel;
    std::string doSample(const std::string, std::string);

    std::string loPath;  // soffice program path
};
#endif
