#include <bits/stdc++.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <unistd.h>

#include "logging.h"
#include "Message.pb.h"
using namespace std;
#define PORT 8080

#define NOT_SENT    0
#define SENT        1
#define ACKED       2

const int MAX_PACKET_NUM = 10000;
struct sockaddr_in address; 
struct sockaddr_in servAddr;
// global varibales
int L, R, W;        // int is ok
std::chrono::milliseconds timeOut(2000);
vector<int> status(MAX_PACKET_NUM + 1);
vector<chrono::milliseconds> sentTime(MAX_PACKET_NUM + 1);
vector<bool> reTransmit(MAX_PACKET_NUM + 1);

mutex windowLock;
mutex reTransLock;
unique_lock<mutex> windowLocker(windowLock,defer_lock);
unique_lock<mutex> reTransLocker(reTransLock,defer_lock);

// function prototypes
int connectToReceiver(string);
int sendPacket(int packetNum);
void timeOoutCheck();
void receiveAck();
void displayStats();

auto startTime = chrono::high_resolution_clock::now();

int main(int argc, char const *argv[])  {  
    srand(time(NULL));   
    int sockFd = connectToReceiver("127.0.0.1");
    windowLocker.lock();
    L = 1;
    R = 1;
    W = 1;
    windowLocker.unlock();
    int cnt = 1;
    while(true) {
        int packetNum = cnt;
        locker.lock();
        if(L == R) {            // this is a case where we need to retransmit
                                // or may be this is the very first packet
            reTransLocker.lock();
            packetNum = L;
            if(reTransmit[L]) {
                int ret = sendPacket(packetNum);
                if(ret == FAILURE) {
                    higLog("%s","sendPacket() failed");
                }else {
                    cnt = L + 1;
                }
                reTransmit[L] = false;
            }
            reTransLocker.unlock();
        }else if(packetNum >=L and packetNum <= R) {
            int ret = sendPacket(packetNum);
            if(ret == FAILURE) {
                higLog("%s","sendPacket() failed");
            }else {
                cnt++;
            }
        }
        locker.unlock();
        if(cnt > MAX_PACKET_NUM) {
            break;
        }
    }
    displayStats();
    return 0; 
} 

/*
This function connects to the receiver and creates a socketfd
returns > 0 (sockFd) is success // -1 if failure
*/
int connectToReceiver(string Ip) {
    int sockFd;
    if ((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
        higLog("%s","Socket creation error");
        return FAILURE; 
    } 
    memset(&servAddr, 0, sizeof(servAddr)); 
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(Ip.c_str());
    servAddr.sin_port = htons(PORT); 
    if(connect(sockFd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) { 
        higLog("%s","\nConnection Failed\n");
        return FAILURE; 
    }
    return sockFd;     
}
int sendPacket(int packetNum) {
    int prob = rand(100) + 1;
    if(prob > 10) {
        MP::TcpMessage packet;
        packet.set_packetnum(packetNum);
        packet.set_msg(std::string(1000,'a'));
        string protocolBuffer = packet.SerializeAsString();
        int datalen = protocolBuffer.length();
        int ret = sendto(sockFd,protocolBuffer.c_str(),datalen,0,
                    (struct sockaddr_in*)(&servAddr),sizeof(servAddr));
                    /* servAddr is global */
        if(ret <= 0) {
            higLog("%s","sendto() failed");
            return FAILURE;
        }
        auto endTime = chrono::high_resolution_clock::now();
        auto elapsedtime = chrono::
                duration_cast<chrono::milliseconds>(endTime - startTime).count();
        sentTime[i] = chrono::milliseconds(elapsedtime);
    }else {
        // dont send this packet
    }
    status[packetNum] = SENT;   // set this irrespective of sent of not sent
    return SUCCESS;
}

void timeOoutCheck() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    while(true) {
        auto startTime = chrono::high_resolution_clock::now();
        unique_lock<mutex> locker(windowLock,defer_lock);
        windowLocker.lock();
        for(int packetNum = L; packetNum<= R; packetNum++) {
            if(status[packetNum] == SENT and 
                currTime - sentTime[packetNum] > timeOut.count()) {
                
                R = L = packetNum;
                W = 1;
                reTransmit[packetNum] = true;
                break;
            
            }
        }









        windowLocker.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}


/* void TimeOutChecker(){
    while(1){
        int currTine = Time();
        for(int i=li;i<ri;i++){
            if(currTine-sendTime[i]>threshold){
                Ri = Li = i;
                windoSize = 1;
                //send packet i
        }
    }
    int x = 20000;
    Sleep(x);

    }

}

void ReceiveACK(){
    string recv;

    while(1){
        char buffer[1024] = {0};
        recv = read( sock , buffer, 1024);
         
        printf("%s\n",buffer ); 

    }
    // temp = curent time
    // decode the buffer message and get the ACK num = n
    // change the left window variable Li
    // update the window size variable
    //update the Ri 
    //Update the time-out value
    //now go for sending next packet

} */