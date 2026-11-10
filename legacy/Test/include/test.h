#include <iostream>
#include <unistd.h>       
#include <sys/time.h> 
class Rate
{
public:
    Rate();
    Rate(unsigned int);
    void sleep();

private:
    int m_LoopRate;
    double m_start_time_with_usec;
};
int Test();
