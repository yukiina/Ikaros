#include "httplib.h"
#include <iostream>
#include "../src/P2PServer.hpp"

using namespace httplib;

static void HelloWorld(const Request &req, Response &rsp)
{
    rsp.status = 200;
    rsp.set_content("<html></body><h1>hello world</h1></body></html>", "text/html");
    return;
}

void TestServer(){
    P2PServer srv;
    srv.Start();
}

int main()
{
    // Server srv;
    // srv.Get("/", HelloWorld);
    // srv.listen("0.0.0.0", 9000);
    TestServer();
    return 0;
}