#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include "SerialPort.h"
#include "math_tools.hpp"
#include <poll.h>
namespace io
{
bool SerialPort::configure_serial(int fd)
{
    struct termios options;
    
    if (tcgetattr(fd, &options) < 0) 
    {
        std::cerr << "获取串口属性失败" << std::endl;
        return false;
    }

    // 设置波特率
    cfsetispeed(&options, buadrate_);
    cfsetospeed(&options, buadrate_);

    // 8N1配置
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    
    // 硬件流控
    #ifdef CRTSCTS
    options.c_cflag &= ~CRTSCTS;
    #endif
    
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;
    
    if (tcsetattr(fd, TCSANOW, &options) < 0) 
    {
        std::cerr << "设置串口属性失败" << std::endl;
        return false;
    }
    
    tcflush(fd, TCIOFLUSH);
    return true;
}

SerialPort::SerialPort(const char *filename):queue_(5000),file_name_(filename),buadrate_(B115200),DEBUG_SERIAL(false)
{
    data_ahead_.timestamp = std::chrono::steady_clock::now();
    data_behind_.timestamp = std::chrono::steady_clock::now();
    serial_quit = false;
    fd = open(file_name_, O_RDWR | O_NOCTTY | O_SYNC);
    last_fd = fd;
    if (fd != -1) {
        fcntl(fd, F_SETFL, 0);
        configure_serial(fd);  
    }
    read_data();
}

SerialPort::~SerialPort(){
    serial_quit = true;
    if (serial_thread_.joinable())
    serial_thread_.join();
}

// 重启函数
void SerialPort::restart_serial(void)
{
    close(fd);
    fd = open(file_name_, O_RDWR | O_NOCTTY | O_SYNC);
    last_fd = fd;
    if (fd != -1) {
        fcntl(fd, F_SETFL, 0);
        configure_serial(fd);  
    }
}

void SerialPort::read_data()
{ 
    serial_thread_=std::thread{[this] {
        tools::logger()->info("serial thread started.");
        while(!serial_quit){     
            // 使用poll非阻塞检查
            struct pollfd fds[1];
            fds[0].fd = fd;
            fds[0].events = POLLIN;
            
            if (poll(fds, 1, 0) <= 0) {
                continue;
            }
            
            // 大缓冲区
            unsigned char read_buffer[17];
            long bytes_read = ::read(fd, read_buffer, sizeof(read_buffer));
            
            if (bytes_read <= 0) {
                static int error_count = 0;
                if (++error_count > 5) {
                    restart_serial();
                    error_count = 0;
                }
                continue;
            }

            if (read_buffer[0] == 0xAA && read_buffer[1] == 0xBB) 
            {
                Data_Save ds;
                ds.timestamp = std::chrono::steady_clock::now();
                int32_t yaw_ = (read_buffer[3] << 24) | (read_buffer[4] << 16) | 
                            (read_buffer[5] << 8) | read_buffer[6];
                int32_t pit_ = (read_buffer[7] << 24) | (read_buffer[8] << 16) | 
                            (read_buffer[9] << 8) | read_buffer[10];
                int32_t roll_ = (read_buffer[11] << 24) | (read_buffer[12] << 16) | 
                            (read_buffer[13] << 8) | read_buffer[14];
                
                // 使用预计算的缩放因子
                static constexpr double SCALE = PI / 180.0 / 10000.0;
                ds.camera_yaw_angle = _std_radian(yaw_ * SCALE);
                ds.camera_pitch_angle = pit_ * SCALE;
                ds.camera_roll_angle = _std_radian(roll_ * SCALE - PI);
                ds.color = read_buffer[15];
                ds.armor_or_buff = read_buffer[16];
                queue_.push(ds);
            }
            
        }
    }};
}

void SerialPort::Query(Data_Save &ds,const time_point &t){
    if (data_behind_.timestamp < t) data_ahead_ = data_behind_;
    while (true) {
        // if (queue_.empty())tools::logger()->warn("[SerialPort] WARNING: Queue is empty!");
        queue_.pop(data_behind_);
        if (data_behind_.timestamp > t) break;
        data_ahead_ = data_behind_;
    }
    ds.timestamp = t;
    ds.color = data_behind_.color;
    ds.camera_yaw_angle = (data_behind_.camera_yaw_angle+data_ahead_.camera_yaw_angle)/2;
    ds.camera_pitch_angle = (data_behind_.camera_pitch_angle+data_ahead_.camera_pitch_angle)/2;
    ds.camera_roll_angle = (data_behind_.camera_roll_angle+data_ahead_.camera_roll_angle)/2;  

}

void SerialPort::SendData(const struct Data_Get &data)
{
    if (data.size != write(fd, data.raw_data, data.size))
    {

        if (DEBUG_SERIAL == true)
        {
            tools::logger()->info("!!! send data failure !!!{},restart fd{}",fd,fd) ;
        }
        restart_serial();
    }
    else
    {
        if (DEBUG_SERIAL == true)
        {
            tools::logger()->info("!!! send data success !!!{}",fd);
        }
    }
}
}
void Data_Get::Vision2Control(int32_t x, int32_t y, int32_t ui_x, int32_t ui_y, uint8_t found, uint8_t fire)
{
    raw_data[2] = (x >> 24) & 0xff;
    raw_data[3] = (x >> 16) & 0xff;
    raw_data[4] = (x >> 8) & 0xff;
    raw_data[5] = (x >> 0) & 0xff;
    raw_data[6] = (y >> 24) & 0xff;
    raw_data[7] = (y >> 16) & 0xff;
    raw_data[8] = (y >> 8) & 0xff;
    raw_data[9] = (y >> 0) & 0xff;
    raw_data[10] = (ui_x >> 24) & 0xff;
    raw_data[11] = (ui_x >> 16) & 0xff;
    raw_data[12] = (ui_x >> 8) & 0xff;
    raw_data[13] = (ui_x >> 0) & 0xff;
    raw_data[14] = (ui_y >> 24) & 0xff;
    raw_data[15] = (ui_y >> 16) & 0xff;
    raw_data[16] = (ui_y >> 8) & 0xff;
    raw_data[17] = (ui_y >> 0) & 0xff;
    raw_data[18] = found;
    raw_data[19] = fire;
}