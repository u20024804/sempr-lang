#include <iostream>
#include "cerl.hpp"
#include <boost/function.hpp>

//#include "log.hpp"

using namespace std;
using namespace cerl;

void task0()
{
    cout << "task0" << endl;
}

//    tasklet_service tts;

int main()
{
    tasklet_service service;
    //tasklet_service *service = & tts;
    //tasklet* p_tasklet = new tasklet(*service, task0);
    tasklet *ptasklet_ = new tasklet(service, task0);
    //tasklet_service service;
    //tasklet* p_tasklet = new tasklet(service, task0);
    service.start();
    //service->join();
    //delete p_tasklet;
    //delete service;
}
