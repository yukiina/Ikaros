#include "httplib.h"
#include <iostream>
#include "../src/P2PClient.hpp"

using namespace httplib;

void TestClient(){
    P2PClient clnt;
    clnt.Start();
}

int main()
{
   
    TestClient();
    return 0;
}