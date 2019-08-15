#ifndef P2PCLIENT_HPP
#define P2PCLIENT_HPP
#include "common.h"
#include <sys/types.h>
#include <ifaddrs.h>
#include <map>
#include <mutex>
#include <ctime>

#include "ThreadPool.hpp"

#define SHAR_DIR "../share"
#define DOWN_DIR "../download"

class P2PClient{
public:
    P2PClient(int port = 9090) : _port(port), _threadPool(8){}
    ~P2PClient(){}
    bool Start() {
        while(1) {
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

    bool SendPairGetReq(const string& ip);

private:
    ThreadPool _threadPool;
    vector<string> _hostList;
    vector<string> _inlineHostList;
    int _port;
    mutex _mutex;
    string _curPairHostIp;
};

void P2PClient::SelectMenu(){
    int choose;
    while(1){
        cout << "1. 搜索附近主机\n";
        cout << "2. 主机配对\n";
        cout << "3. 显示文件列表\n";
        cout << "4. 下载选定文件\n";
        cout << "输入序号执行功能:";
        fflush(stdout);
        cin >> choose;
        switch(choose){
            case 1:
                _hostList.clear();
                _inlineHostList.clear();
                GetHostList();      // 获取主机列表
                FindNearHost();     // 搜索附近可配对主机
                cout << "在线主机:" << endl;
                for(const auto& e : _inlineHostList){
                    cout << e << endl;
                }
                break;
            case 2:
                PairNearHost();     // 选择某个主机配对
                break;
            case 3:
                GetFileList();      // 获取主机文件列表 GET ../list
                break;
            case 4:
                GetFileRange();     // 获取主机文件列表中某个文件大小 HEAD ../list/xxx.xx
                DownLoadFile();     // 分块下载选定文件(多线程)
                break;
            default:
                std::cout << "输入序号有错误(1 ~ 4)\n";
                break;
        }
    }
}

void P2PClient::GetHostList(){
    struct ifaddrs *addrs = NULL;
    uint32_t net_seg, end_host;
    getifaddrs(&addrs);  //获取本机网卡信息
    while(addrs) {
        if (addrs->ifa_addr->sa_family == AF_INET && strncasecmp(addrs->ifa_name, "lo", 2)) {
            sockaddr_in *addr = (sockaddr_in*)addrs->ifa_addr;
            sockaddr_in *mask = (sockaddr_in*)addrs->ifa_netmask;
            net_seg = ntohl(addr->sin_addr.s_addr & mask->sin_addr.s_addr);
            end_host = ntohl(~mask->sin_addr.s_addr);
            for (int i = 2; i < end_host - 1; i++) {
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
bool P2PClient::SendPairGetReq(const string& ip){
    Client cli(ip.c_str(), _port);
    auto res = cli.Get("/pair");
    if (res && res->status == 200) {        // 判断有哪些主机在线
        std::unique_lock<std::mutex> ulc(_mutex);
        _inlineHostList.push_back(ip);
        //cout << "host "<< ip << " pair success\n";
        return true;
    }
    else{
        //cout << "host "<< ip << " pair failed\n";
        return false;
    }
}

void P2PClient::FindNearHost(){ 
    vector<thread> threads;
    for (const auto& ip : _hostList) {
        threads.emplace_back(&P2PClient::SendPairGetReq, this, std::ref(ip));
    }

    cout << "请等待, 正在搜索附近主机, 整个过程大概需要30秒左右..." << endl;
    for(auto& thread : threads){
        thread.join();
    }
    cout << "搜索结束" << endl;
}

void P2PClient::PairNearHost(){

}

void P2PClient::GetFileList(){

}

void P2PClient::GetFileRange(){

}

void P2PClient::DownLoadFile(){

}
#endif