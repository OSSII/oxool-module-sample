#include "LOOLWSD.hpp"
#include "JsonUtil.hpp"
#include "sample.h"
#include <memory>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <LibreOfficeKit/LibreOfficeKit.hxx>

#include <Poco/FileStream.h>
#include <Poco/TemporaryFile.h>
#include <Poco/StringTokenizer.h>
#include <Poco/StreamCopier.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/Net/MessageHeader.h>
#include <Poco/Net/NameValueCollection.h>
#include <Poco/Util/Application.h>
#include <Poco/FileChannel.h>
#include <Poco/FormattingChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/Path.h>
#include <Poco/Timestamp.h>
#include <Poco/JSON/Object.h>
#include <Poco/Process.h>

std::string module_name = "sample";

using Poco::File;
using Poco::FileChannel;
using Poco::Path;
using Poco::PatternFormatter;
using Poco::StreamCopier;
using Poco::StringTokenizer;
using Poco::TemporaryFile;
using Poco::Net::HTMLForm;
using Poco::Net::HTTPResponse;
using Poco::Net::MessageHeader;
using Poco::Net::NameValueCollection;
using Poco::Net::PartHandler;
using Poco::Util::Application;
using Poco::Process;

std::string addSlashes(const std::string &source)
{
    std::string out;
    for (const char c : source)
    {
        switch (c)
        {
        case '\\':
            out += "\\\\";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

static Poco::Logger &logger()
{
    return Application::instance().logger();
}

/// Handles the filename part of the sample POST request payload.
class samplePartHandler : public PartHandler
{
public:
    samplePartHandler()
        : _filename("")
    {
        tempPath = Path::forDirectory(
            Poco::TemporaryFile::tempName() + "/");
        File(tempPath).createDirectories();
    }

    std::string getFilename()
    {
        return _filename;
    }

    virtual void handlePart(const MessageHeader &header, std::istream &stream) override
    {
        // Extract filename and put it to a temporary directory.
        std::string disp;
        NameValueCollection params;
        if (header.has("Content-Disposition"))
        {
            std::string cd = header.get("Content-Disposition");
            MessageHeader::splitParameters(cd, disp, params);
        }

        tempPath.setFileName(params.get("filename"));
        std::string filePath = tempPath.toString();
        if (params.get("name") == "filename")
            _filename = filePath;

        // Copy the stream to _filename.
        std::ofstream fileStream;
        fileStream.open(filePath);
        StreamCopier::copyStream(stream, fileStream);
        fileStream.close();
    }

private:
    std::string _filename;
    Path tempPath;
};

sample::sample()
{
#if ENABLE_DEBUG
    ConfigFile = std::string(DEV_DIR) + "/sample.xml";
#else
    ConfigFile = "/etc/oxool/conf.d/sample/sample.xml";
#endif
    startStamp = std::chrono::steady_clock::now();
    xml_config = new Poco::Util::XMLConfiguration(ConfigFile);

    //Initialize
    setLogPath();
    Application::instance().logger().setChannel(channel);
    setProgPath(LOOLWSD::LoTemplate);
}

/// init. logger
/// 設定 log 檔路徑後直接 init.
void sample::setLogPath()
{
    Poco::AutoPtr<FileChannel> fileChannel(new FileChannel);
    std::string logFile = xml_config->getString("logging.log_file");
    fileChannel->setProperty("path", logFile);
    fileChannel->setProperty("archive", "timestamp");
    fileChannel->setProperty("rotation", "weekly");
    fileChannel->setProperty("compress", "true");
    Poco::AutoPtr<PatternFormatter> patternFormatter(new PatternFormatter());
    patternFormatter->setProperty("pattern", "%Y/%m/%d %L%H:%M:%S: %t");
    channel = new Poco::FormattingChannel(patternFormatter, fileChannel);
}

/// callback for lok
void sample::ViewCallback(const int type,
                          const char *p,
                          void *data)
{
    std::cout << "CallBack Type: " << type << std::endl;
    std::cout << "payload: " << p << std::endl;
    sample *self = reinterpret_cast<sample *>(data);
    if (type == LOK_CALLBACK_UNO_COMMAND_RESULT)
    {
        self->isUnoCompleted = true;
    }
}

/// 轉檔：轉成圖片
std::string sample::doSample(const std::string filename, std::string msg)
{
    Poco::Path pdfdir(filename);
    std::string outfile = pdfdir.parent().toString() + "result.pdf";

    /*
	 * 如果是用新的 userprofile, ,要先啟動一次讓環境初始化
	 * 然後在啟動一次才會有完整的環境可以使用
	 * 第一次啟動：註冊 unoservice, 但不能呼叫
	 * 第二次啟動：正常呼叫 unoservice
	 */
    lok::Office *llo = NULL;
    Path tempPath = Path::forDirectory(
        Poco::TemporaryFile::tempName() + "/");
    auto userprofile = File(tempPath);
    userprofile.createDirectories();
    std::string userprofile_uri = "file://" + tempPath.toString();
    std::cout << "loPath: " << loPath << "\n";
    std::cout << "loUserProfilePath: : " << tempPath.toString() << "\n";
    try
    {
        llo = lok::lok_cpp_init(loPath.c_str(), userprofile_uri.c_str());
        //llo = lok::lok_cpp_init(loPath.c_str());
        if (!llo)
        {
            std::cout << ": Failed to initialise LibreOfficeKit" << std::endl;
            return "Failed to initialise LibreOfficeKit";
        }
    }
    catch (const std::exception &e)
    {
        delete llo;
        std::cout << ": LibreOfficeKit threw exception (" << e.what() << ")" << std::endl;
        return e.what();
    }
    delete llo;
    try
    {
        llo = lok::lok_cpp_init(loPath.c_str(), userprofile_uri.c_str());
        if (!llo)
        {
            std::cout << ": Failed to initialise LibreOfficeKit" << std::endl;
            return "Failed to initialise LibreOfficeKit";
        }
    }
    catch (const std::exception &e)
    {
        delete llo;
        std::cout << ": LibreOfficeKit threw exception (" << e.what() << ")" << std::endl;
        return e.what();
    }
    std::cout << "Init LO Kit success\n";
    lok::Document *lodoc = llo->documentLoad(filename.c_str(), nullptr);
    if (!lodoc)
    {
        const char *errmsg = llo->getError();
        std::cerr << ": LibreOfficeKit failed to load document (" << errmsg << ")" << std::endl;
        return errmsg;
    }
    std::cout << "load document OK\n";
    lodoc->registerCallback(ViewCallback, this);

    std::string command;
    command = "macro:///Sample.TextEdit.insertText(\"" + msg + "\")";
    lodoc->postUnoCommand(command.c_str(), nullptr, true);
    /* Using UnoCommand also can do the insertText
    std::string json = R"MULTILINE(
        {
            "Text":
            {
                "type":"string",
                "value":"%s"
            }
        }
    )MULTILINE";
    std::string args_str = Poco::format(json, msg);

    lodoc->postUnoCommand(".uno:InsertText", args_str.c_str(), true);
    */
    std::string format = "odt";
    if (!lodoc->saveAs(outfile.c_str(), format.c_str(), nullptr))
    {
        const char * errmsg = llo->getError();
        std::cerr << ": LibreOfficeKit failed to export (" << errmsg << ")" << std::endl;
        delete lodoc;
        return errmsg;
    }
    delete llo;

    return outfile;
}

/// http://server/lool/sample
void sample::handlesample(std::weak_ptr<StreamSocket> _socket,
                          Poco::Net::HTTPRequest &request,
                          Poco::MemoryInputStream &message,
                          SocketDisposition &disposition)
{
    auto socket = _socket.lock();

    HTTPResponse response;
    response.set("Access-Control-Allow-Origin", "*");
    response.set("Access-Control-Allow-Methods", "POST, OPTIONS");
    response.set("Access-Control-Allow-Headers",
                 "Origin, X-Requested-With, Content-Type, Accept");
    samplePartHandler handler;
    HTMLForm form(request, message, handler);
    std::string logmsg = "來源 IP: " + socket->clientAddress();
    std::string filename;

    filename = handler.getFilename();
    if (!filename.empty())
    {
        std::cout << "odt file: " << filename << "\n";
        logmsg += ", 檔案名稱: " + filename;
        auto ftype = filename.find_last_of(".");
        if (ftype == std::string::npos)
        {
            quickHttpRes(_socket,
                         HTTPResponse::HTTP_BAD_REQUEST,
                         "轉檔失敗, 檔案格式錯誤");
            logmsg += " 轉檔失敗, 檔案格式錯誤";
            logger().notice(logmsg);
            return;
        }
        std::string extension = filename.substr(ftype, filename.size());
        std::cout << "extension: " << extension << "\n";
        if (extension != ".odt")
        {
            quickHttpRes(_socket,
                         HTTPResponse::HTTP_BAD_REQUEST,
                         "轉檔失敗, 檔案格式錯誤");
            logmsg += " 轉檔失敗, 檔案格式錯誤";
            logger().notice(logmsg);
            return;
        }
    }
    else
    {
        quickHttpRes(_socket,
                     HTTPResponse::HTTP_BAD_REQUEST,
                     "轉檔失敗, 無提供檔案");
        logmsg += " 轉檔失敗, 無提供檔案";
        logger().notice(logmsg);
        return;
    }
    std::string outfile = "";
    std::string msg = "default message";
    if(form.has("message"))
    {
        msg = form.get("message");
    }
    
    outfile = doSample(filename, msg);
    std::cout << "outfile: " << outfile << "\n";

    Poco::File check_result(outfile);
    if (!outfile.empty() && check_result.exists())
    {
        std::cout << outfile << "\n";
        auto mimeType = getMimeType(outfile);
        std::cout << "mimetype: " << mimeType << std::endl;
        HttpHelper::sendFile(socket, outfile,
                             mimeType, response);
        logmsg += " 成功";
    }
    else
    {
        quickHttpRes(_socket,
                     HTTPResponse::HTTP_INTERNAL_SERVER_ERROR,
                     "API Fail");
        logmsg += " 失敗";
    }
    // Write log into loggin file
    logger().notice(logmsg);
}

std::string sample::getHTMLFile(std::string fileName)
{
    std::string filePath = "";
#ifdef DEV_DIR
    std::string dev_path(DEV_DIR);
    filePath = dev_path + "/html/";
#else
    filePath = "/var/lib/oxool/sample/html/";
#endif
    filePath += fileName;
    return filePath;
}

void sample::handleRequest(std::weak_ptr<StreamSocket> _socket,
                           MemoryInputStream &message,
                           HTTPRequest &request,
                           SocketDisposition &disposition)
{
    Poco::URI requestUri(request.getURI());
    std::vector<std::string> reqPathSegs;
    requestUri.getPathSegments(reqPathSegs);
    std::string method = request.getMethod();
    Process::PID pid = fork();
    if (pid < 0)
    {
        quickHttpRes(_socket,
                     HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
    }
    else if (pid == 0)
    {
        // This would trigger this fork exit after the socket finish write
        // note: exit point is in wsd/LOOLWSD.cpp where ClientRequestDispatcher (SocketHandler)'s performWrites()
        try
        {
            Poco::URI requestUri(request.getURI());
            std::vector<std::string> reqPathSegs;
            requestUri.getPathSegments(reqPathSegs);
            std::string method = request.getMethod();
            if (request.getMethod() == HTTPRequest::HTTP_GET && reqPathSegs.size() == 2)
            {
                quickHttpRes(_socket, HTTPResponse::HTTP_OK);
            }
            else if (request.getMethod() == HTTPRequest::HTTP_POST &&
                     reqPathSegs[1] == "sample")
            { // /lool/sample
                handlesample(_socket, request, message, disposition);
            }
            else
            {
                quickHttpRes(_socket, HTTPResponse::HTTP_NOT_FOUND);
            }
        }
        catch (std::exception &e)
        {
            std::cout << e.what() << "\n";
            logger().notice("[Exception]" + std::string(e.what()));
            quickHttpRes(_socket, HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        }

        exit_application = true;
    }
    else
    {
        std::cout << "call from parent" << std::endl;
    }
}

std::string sample::handleAdmin(std::string command)
{
    /*
     *command format: module <modulename> <action> <data in this format: x,y,z ....>
     */
    auto tokenOpts = StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM;
    StringTokenizer tokens(command, " ", tokenOpts);
    std::string result = "Module Loss return";
    std::string action = tokens[2];

    // 以 json 字串傳回 sample.xml 
    if (action == "getConfig" && tokens.count() >= 4)
    {
        std::ostringstream oss;
        oss << "settings {\n";
        for (size_t i = 3; i < tokens.count(); i++)
        {
            std::string key = tokens[i];

            if (i > 3)
                oss << ",\n";

            oss << "\"" << key << "\": ";
            // 下列三種 key 是陣列形式

            if (xml_config->has(key))
            {
                std::string p_value = addSlashes(xml_config->getString(key, "")); // 讀取 value, 沒有的話預設為空字串
                std::string p_type = xml_config->getString(key + "[@type]", "");  // 讀取 type, 沒有的話預設為空字串
                if (p_type == "int" || p_type == "uint" || p_type == "bool" ||
                    p_value == "true" || p_value == "false")
                    oss << p_value;
                else
                    oss << "\"" << p_value << "\"";
            }
            else
            {
                oss << "null";
            }
        }
        oss << "\n}\n";
        result = oss.str();
    }
    else if (action == "setConfig" && tokens.count() > 1)
    {
        Poco::JSON::Object::Ptr object;
        if (JsonUtil::parseJSON(command, object))
        {
            for (Poco::JSON::Object::ConstIterator it = object->begin(); it != object->end(); ++it)
            {
                // it->first : key, it->second.toString() : value
                xml_config->setString(it->first, it->second.toString());
            }
            xml_config->save(ConfigFile);
            result = "setConfigOk";
        }
        else
        {
            result = "setConfigNothing";
        }
    }
    else
        result = "No such command for module " + tokens[1];

    return result;
}

// Self Register
extern "C"
{
    oxoolmodule *maker()
    {
        return new sample;
    }
    class proxy
    {
    public:
        proxy()
        {
            // register the maker with the factory
            apilist[module_name] = maker;
        }
    };
    // our one instance of the proxy
    proxy p;
}
