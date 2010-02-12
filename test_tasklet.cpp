#include <iostream>
#include "cerl.hpp"

#include <stdio.h>

using namespace std;
using namespace cerl;

class task0;
class task1;
class task2;
class task3;
class task4;
class task5;
class task6;
task0 *t0;
task1 *t1;
task2 *t2;
task3 *t3;
task4 *t4;
task5 *t5;
task6 *t6;

tasklet_service service(4);

long times = 1000000L;

volatile long v = 0;

class task0 : public tasklet
{
public:
    void run()
    {
        printf("start task0: %x\n", this);
        fflush(stdout);
        long i = 0;
        long sum = 0;
        for (; i < times; i++)
        {
            message msg = recv();
             //cout << "task0, i:"<< i << ", msg: " << msg.content.ivalue << ", " << msg.type << endl;
            //sum += msg.content.ivalue;
            if (i % 100000 == 0)
            {
                printf("i: %ld\n", i);
            }
        }
        printf("ready to stop\n");
        service.stop();
    }

    ~task0()
    {
        printf("~task0: %x\n", this);
    }
};

class task1 : public tasklet
{
public:
    void run()
    {
        printf("start task1: %x\n", this);
        fflush(stdout);
        long i = 0;
        long sum = 0;
        for (; i < times; i++)
        {
            //cout << "task1: " << i << endl;
            message msg = {{i}};
            send(t0, msg);
            //sum += i;
        }
    }
    ~task1()
    {
        printf("~task1: %x\n", this);
    }
};

class task2 : public tasklet
{
public:
    void run()
    {
        printf("task2: %x\n", this);
        message msg = recv(2);
        cout << "msg.type: " << msg.type << endl;
        //service.stop();
    }
    ~task2()
    {
        printf("~task2: %x\n", this);
    }
};

class task3 : public tasklet
{
public:
    void run()
    {
        printf("task3: %x\n", this);
        message msg = {{3}, msg_int};
        cout << "prepare to send to task2" << endl;
        send(t2, msg);
    }
    ~task3()
    {
        printf("~task3: %x\n", this);
    }
};

class task4 : public tasklet
{
public:
    void run()
    {
        printf("task4: %x\n", this);
        message msg = recv(1000);
    }
    ~task4()
    {
        printf("~task4: %x\n", this);
    }
};

class task5: public tasklet
{
public:
    void run()
    {
        printf("task5: %x\n", this);
    }
    ~task5()
    {
        printf("~task5: %x\n", this);
    }
};

class task6: public tasklet
{
public:
    void run()
    {
        printf("task6: %x\n", this);
        cout << 1/0 << endl;
    }
    ~task6()
    {
        printf("~task6: %x\n", this);
    }
};


int main()
{
    //tasklet_service service(2);

    t0 = new task0();
    t1 = new task1();
    t2 = new task2();
    t3 = new task3();
    t4 = new task4();
    t5 = new task5();
    t6 = new task6();
    service.add(t0);
    service.add(t1);
    service.add(t2);
    service.add(t3);
    service.add(t4);
    service.add(t5);
    service.add(t6);
    service.start();
//    service.join();
}
