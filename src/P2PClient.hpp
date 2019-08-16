#ifndef P2PCLIENT_HPP
#define P2PCLIENT_HPP
#include "common.h"
#include <sys/types.h>
#include <ifaddrs.h>
#include <map>
#include <mutex>
#include <ctime>

#include "ThreadPool.hpp"

#define RANGE_COUNT 4
#define SHAR_DIR "../share"
#define DOWN_DIR "../download"

class P2PClient
{
public:
    P2PClient(int port = 9090) : _port(port) {}
    ~P2PClient() {}
    bool Start()
    {
        while (1)
        {
            SelectMenu();
        }
        return true;
    }

private:
    void SelectMenu();
    void GetHostList();
    void FindNearHost();
    void PairNearHost();
    void GetFileList();
    void GetFileRange();
    void DownLoadFile();
    void SelectFile();
    bool WriteFile(const string &filename, string &body, int64_t start, int64_t end);
    bool SendPairGetReq(const string &ip);

private:
    ThreadPool _threadPool;
    vector<string> _hostList;
    vector<string> _inlineHostList;
    vector<string> _fileList;
    int _port;
    mutex _mutex;
    string _curPairHostIp;
    string _curFile;
    int64_t _curSize;
};

void P2PClient::SelectMenu()
{
    int choose;
    while (1)
    {
        cout << "1. 搜索附近主机\n";
        cout << "2. 主机配对\n";
        cout << "3. 显示文件列表\n";
        cout << "4. 下载选定文件\n";
        cout << "输入序号执行功能:";
        fflush(stdout);
        cin >> choose;
        switch (choose)
        {
        case 1:
            _hostList.clear();
            _inlineHostList.clear();
            _curPairHostIp.clear();
            GetHostList();  // 获取主机列表
            FindNearHost(); // 搜索附近可配对主机
            cout << "在线主机:" << endl;
            for (const auto &e : _inlineHostList)
            {
                cout << e << endl;
            }
            break;
        case 2:
            _curPairHostIp.clear();
            PairNearHost(); // 选择某个主机配对
            _curPairHostIp = "192.168.79.140";
            break;
        case 3:
            GetFileList(); // 获取主机文件列表 GET ../list
            break;
        case 4:
            _curSize = 0;
            SelectFile();
            GetFileRange(); // 获取主机文件列表中某个文件大小 HEAD ../list/xxx.xx
            if (_curSize == 0)
            {
                cout << "错误: 无法完成下载" << endl;
                break;
            }
            cout << _curFile << " : " << _curSize << endl;
            DownLoadFile(); // 分块下载选定文件(多线程)
            break;
        default:
            std::cout << "输入序号有错误(1 ~ 4)\n";
            break;
        }
    }
}

void P2PClient::GetHostList()
{
    struct ifaddrs *addrs = NULL;
    uint32_t net_seg, end_host;
    getifaddrs(&addrs); //获取本机网卡信息
    while (addrs)
    {
        if (addrs->ifa_addr->sa_family == AF_INET && strncasecmp(addrs->ifa_name, "lo", 2))
        {
            sockaddr_in *addr = (sockaddr_in *)addrs->ifa_addr;
            sockaddr_in *mask = (sockaddr_in *)addrs->ifa_netmask;
            net_seg = ntohl(addr->sin_addr.s_addr & mask->sin_addr.s_addr);
            end_host = ntohl(~mask->sin_addr.s_addr);
            for (int i = 2; i < end_host - 1; i++)
            {
                struct in_addr addr;
                addr.s_addr = htonl(net_seg + i);
                std::string str = inet_ntoa(addr);
                _hostList.push_back(str);
            }
        }
        addrs = addrs->ifa_next;
    }
    freeifaddrs(addrs);
}
bool P2PClient::SendPairGetReq(const string &ip)
{
    Client cli(ip.c_str(), _port);
    auto res = cli.Get("/pair");
    if (res && res->status == 200)
    { // 判断有哪些主机在线
        std::unique_lock<std::mutex> ulc(_mutex);
        _inlineHostList.push_back(ip);
        //cout << "host "<< ip << " pair success\n";
        return true;
    }
    else
    {
        //cout << "host "<< ip << " pair failed\n";
        return false;
    }
}

void P2PClient::FindNearHost()
{
    vector<thread> threads;
    for (const auto &ip : _hostList)
    {
        threads.emplace_back(&P2PClient::SendPairGetReq, this, std::ref(ip));
    }

    cout << "请等待, 正在搜索附近主机, 整个过程大概需要30秒左右..." << endl;
    for (auto &thread : threads)
    {
        thread.join();
    }
    cout << "搜索结束" << endl;
}

void P2PClient::PairNearHost()
{
    if (_inlineHostList.empty())
    {
        cout << "没有发现在线主机, 请先执行 1. 搜索附近主机" << endl;
        return;
    }

    int choose;
    int index = 1;
    cout << "选择在线主机进行配对, 输入配对的主机序号" << endl;
    for (const auto &ip : _inlineHostList)
    {
        cout << index++ << ". " << ip << endl;
    }

    while (1)
    {
        cout << "选择配对主机: ";
        fflush(stdout);
        cin >> choose;
        if (choose <= 0 || choose > _inlineHostList.size())
        {
            cout << "输入有误, 重新输入" << endl;
            continue;
        }
        else
        {
            cout << "配对成功, 可以获取对方的文件列表了" << endl;
            _curPairHostIp = _inlineHostList[choose - 1];
            break;
        }
    }
}

void P2PClient::GetFileList()
{
    if (_curPairHostIp.empty())
    {
        cout << "没有配对主机, 无法获取文件列表, 请先搜索并配对主机" << endl;
        return;
    }
    else
    {
        cout << "当前配对的主机是: " << _curPairHostIp << ", 开始获取对方共享文件列表" << endl;
    }

    Client cli(_curPairHostIp.c_str(), _port);
    auto res = cli.Get("/list");
    if (res && res->status != 200)
    {
        cout << "获取文件列表失败, 检查网络" << endl;
        return;
    }

    _fileList.clear();
    stringstream ssr;
    ssr << res->body;
    string fileName;
    while (std::getline(ssr, fileName))
    {
        _fileList.push_back(fileName);
        cout << fileName << endl;
    }
}

void P2PClient::SelectFile()
{
    if (_fileList.empty())
    {
        cout << "主机" << _curPairHostIp << "没有共享文件, 请选择其他主机" << endl;
        return;
    }

    int choose;
    int index = 1;
    cout << "选择主机" << _curPairHostIp << "共享文件夹中的文件: " << endl;

    for (const auto &file : _fileList)
    {
        cout << index++ << ". " << file << endl;
    }

    while (1)
    {
        cout << "选择要下载的文件: ";
        fflush(stdout);
        cin >> choose;
        if (choose <= 0 || choose > _fileList.size())
        {
            cout << "输入有误, 重新输入" << endl;
            continue;
        }
        else
        {
            _curFile = _fileList[choose - 1];
            break;
        }
    }
}

void P2PClient::GetFileRange()
{

    Client cli(_curPairHostIp.c_str(), _port);
    cout << "选择下载的文件是: " << _curFile << endl;

    const string url("/list/" + _curFile);

    auto res = cli.Head(url.c_str());
    if (res && res->status != 200)
    {
        cout << "下载失败, 检查网络" << endl;
        return;
    }
    if (res->has_header("Content-Length"))
    {
        stringstream ss;
        ss << res->get_header_value("Content-Length", 0);
        ss >> _curSize;
    }
    else
    {
        std::cout << "无法分块下载, 文件会单独下载" << endl;
        _curSize = -1;
    }
    return;
}

void P2PClient::DownLoadFile()
{
    int64_t rangeStart, rangeEnd, rangeSize;
    rangeSize = _curSize / RANGE_COUNT;
    vector<std::future<bool>> results;

    string pathFile = DOWN_DIR; 
    pathFile += "/";
    pathFile += _curFile;
    path pathStr(pathFile);
    
    if (exists(pathStr)){
        cout << "存在同名文件, 停止下载" << endl; //TODO: 可以使用MD5值, 判断是否为相同文件
        return;
    }

    for (int i = 0; i < RANGE_COUNT; i++)
    {
        rangeStart = i * rangeSize;
        if (i == (RANGE_COUNT - 1))
        {
            rangeEnd = _curSize - 1;
        }
        else
        {
            rangeEnd = (i + 1) * rangeSize - 1;
        }

        results.emplace_back(_threadPool.AddTask([=]() {
            Client range_cli(_curPairHostIp.c_str(), 9090);
            Headers hdr = {make_range_header(rangeStart, rangeEnd)};
            const string url("/list/" + _curFile);
            auto res = range_cli.Get(url.c_str(), hdr);
            if (res && res->status == 206)
            {

                if(WriteFile(_curFile, res->body, rangeStart, rangeEnd))
                    return true;
                else{
                    cout << "下载失败(客户端错误):" << rangeStart << "--->" << rangeEnd << endl;
                    return false;
                } 
                    
            }
            else
            {
                cout << "下载失败(服务器错误):" << rangeStart << "--->" << rangeEnd << endl;
                cout << res->status << endl;
                return false;
            }
        }));
    }
    cout << "下载中..." << endl;
    for (auto &res : results)
    {
       res.get();
    }
    cout << "下载完成!!" << endl;
}

bool P2PClient::WriteFile(const string &fileName, string &body, int64_t start, int64_t end)
{
    int64_t write_len = end - start + 1;

    std::unique_lock<std::mutex> ulc(_mutex);
    path pathStr(DOWN_DIR);
    if (!exists(pathStr))
    {
        create_directory(pathStr); // 文件夹不存在, 创建文件夹
    }
    string pathFile = DOWN_DIR; 
    pathFile += "/";
    pathFile += fileName;

   
    
    std::ofstream ofs(pathFile, std::ios::app|std::ios::out|std::ios::binary);
    if (! ofs){
        cout << "无法打开文件: " << pathFile << endl;
        return false; 
    }

    cout << start << "---->" << end << endl;
    ofs.seekp(start);
    ofs.write(&body[0], write_len);

    ofs.close();
    return true;
}
#endif