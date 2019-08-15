#ifndef P2PSERVER_HPP
#define P2PSERVER_HPP
#include "common.h"

#define SHAR_DIR "../share"
#define DOWN_DIR "../download"


class P2PServer{
public:
  P2PServer(int port = 9090) : _port(port){}
  void Start();
public:
  static void GetPair(const Request& req, Response& rsp);
  static void GetFileList(const Request& req, Response& rsp);
  static void GetDownLoad(const Request& req, Response& rsp);
  static bool CalcRange(const string &range, int64_t& range_start, int64_t& range_end);
private:
  static void GetFileVector();
private:
  class File{
  public:
    File(string name = "", int64_t size = 0): _fileName(name), _fileSize(size){}
    const string& GetName() const {return _fileName;}
    const int64_t GetSize() const {return _fileSize;}
  private:
    string _fileName;
    int64_t _fileSize;
  };
private:
  httplib::Server _srv;
  static vector<File> _files;
  int _port;
};

vector<P2PServer::File> P2PServer::_files;



void P2PServer::Start(){
  _srv.Get("/pair", GetPair);
  _srv.Get("/list", GetFileList);
  _srv.Get("/list/(.*)", GetDownLoad);
  _srv.listen("0.0.0.0", _port);
}

void P2PServer::GetPair(const Request& req, Response& rsp){
  std::cout << "Get pair success" << std::endl;
  rsp.status = 200; // OK

}

void P2PServer::GetFileList(const Request& req, Response& rsp){
  //std::cout << "Get List success" << std::endl;
  _files.clear();
  GetFileVector();
  rsp.status = 200;
  for(const auto& file : _files){
    rsp.body += file.GetName() + "\n";
    //std::cout << file.GetName() << "----" << file.GetSize() << std::endl;
  }
}

void P2PServer::GetDownLoad(const Request& req, Response& rsp){
  //std::cout << "Get DownLoad success" << std::endl;
  string filePath = SHAR_DIR;
  filePath += "/";
  filePath += req.matches[1];

  path pathStr(filePath);
  if (!exists(pathStr)){      // 共享文件夹没有这个文件
    rsp.status = 404;
    rsp.set_content("no such file or directory", "text/plain");
    return;
  }

  string fileName = req.matches[1];
  int64_t fileSize = file_size(pathStr);
  // 文件存在, GET 直接下载, HEAD 返回文件长度信息
  if (req.method == "HEAD") {
    stringstream header;
    header << fileSize;
    rsp.set_header("Content-Length", header.str().c_str());
  }else if(req.method == "GET") {
    if (!req.has_header("Range"))
    {   
      rsp.status = 400;
      rsp.set_content("no range", "text/plain");
      return;
    }
    int64_t rangeStart, rangeEnd, rangeSize;
    string range = req.get_header_value("Range", 0);
    if (CalcRange(range, rangeStart, rangeEnd) == false) { // 计算出 Range: byte=x-x 分块下载的大小
      return ;
    }
    
    if (rangeStart < 0 || rangeEnd > fileSize || rangeEnd <= rangeStart) { // 不合理的大小
      rsp.status = 400;
      return ;
    }

    rangeSize = rangeEnd - rangeStart + 1;
    // 开始下载
    cout << "下载" << fileName << ": " << rangeStart << "~" << rangeEnd << endl;
    cout << "大小: " << rangeSize << endl;

    ifstream ifs(filePath, std::ios_base::binary);
    if (!ifs.is_open()) {
      rsp.status = 400;
      rsp.set_content("file is not open", "text/plain");
      return;
    }

    ifs.seekg(rangeStart);
    rsp.body.resize(rangeSize);
    ifs.read(&rsp.body[0], rangeSize);
    rsp.set_content(rsp.body, "text/plain");
    rsp.status = 206;

    ifs.close();
    return ;
  }

}
bool P2PServer::CalcRange(const string &range, int64_t& range_start, int64_t& range_end){
  size_t start, end;
  start = range.find("=");
  end = range.find("-");

  if (start == string::npos && end == string::npos){
    return false;
  }

  stringstream ss;
  ss << range.substr(start + 1, end - start + 1);
  ss >> range_start;

  ss.clear();
  ss << range.substr(end + 1);
  ss >> range_end;
  return true;
}
void P2PServer::GetFileVector(){
  path pathStr(SHAR_DIR);
  if (!exists(pathStr)){		
    create_directory(pathStr); // 文件夹不存在, 创建文件夹
    return;
  }
  directory_iterator list(pathStr);
  for (const auto& it:list) {
    _files.emplace_back(it.path().filename(), file_size(it.path()));
  }
  
  return ;
}
#endif
