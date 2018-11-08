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

#define PORT                    8080
#define NOT_SENT                0
#define SENT                    1
#define ACKED                   2
#define MAX_BUFFER              3000
#define MY_PORT_NUM             60000
#define MAX_BACKLOG_REQUEST     100
#define SLEEP_TIME              20

const int MAX_PACKET_NUM = 10000;
struct sockaddr_in receiverAddr; 
struct sockaddr_in servAddr;
int L, R, W;
int sockFd;
std::chrono::milliseconds timeOut(2000);
vector<int> status(MAX_PACKET_NUM + 1);
vector<chrono::milliseconds> sentTime(MAX_PACKET_NUM + 1);
vector<bool> reTransmit(MAX_PACKET_NUM + 1);

mutex windowLock;
mutex reTransLock;
unique_lock<mutex> windowLocker(windowLock,defer_lock);
unique_lock<mutex> reTransLocker(reTransLock,defer_lock);


int connectToReceiver(string);
int sendPacket(int packetNum);
void timeOutCheck();
void receiveAck();
void displayStats();

auto startTime = chrono::high_resolution_clock::now();
bool simulationActive = false;

std::random_device rdevice;
std::mt19937 mt(rdevice());
std::uniform_real_distribution<double> dist(1.0, 100.0);

int main(int argc, char const *argv[])  {  
    sockFd = connectToReceiver("127.0.0.1");
    windowLocker.lock();
    L = 1;
    R = 1;
    W = 1;
    windowLocker.unlock();
    int cnt = 1;

    std::thread T1(timeOutCheck);   T1.detach();
    std::thread T2(receiveAck);     T2.detach();
    
    simulationActive = true;
    
    while(true) {
        int packetNum = cnt;
        windowLocker.lock();
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
        windowLocker.unlock();
        if(cnt > MAX_PACKET_NUM) {
            simulationActive = false;
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
    LOG_ENTRY;
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
    LOG_EXIT;
    return sockFd;     
}
/*
this function
*/
int sendPacket(int packetNum) {
    LOG_ENTRY;
    double prob = dist(mt);
    if(prob > 2) {
        MP::TcpMessage packet;
        packet.set_packetnum(packetNum);
        packet.set_msg(std::string(1000,'a'));
        string protocolBuffer = packet.SerializeAsString();
        int datalen = protocolBuffer.length();
        int ret = sendto(sockFd,protocolBuffer.c_str(),datalen,0,
                    (struct sockaddr *)(&servAddr),sizeof(servAddr));
                    /* servAddr is global */
        if(ret <= 0) {
            higLog("%s","sendto() failed");
            return FAILURE;
        }
        auto endTime = chrono::high_resolution_clock::now();
        auto elapsedtime = chrono::
                duration_cast<chrono::milliseconds>(endTime - startTime).count();
        sentTime[packetNum] = chrono::milliseconds(elapsedtime);
    }else {
        // dont send this packet
    }
    status[packetNum] = SENT;
    LOG_EXIT;
    return SUCCESS;
}

void timeOutCheck() {
    LOG_ENTRY;
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
    while(true) {
        if(simulationActive == false) {
            break;
        }
        auto currTime = chrono::high_resolution_clock::now();
        unique_lock<mutex> locker(windowLock,defer_lock);
        windowLocker.lock();
        for(int packetNum = L; packetNum<= R; packetNum++) {
            auto elapsedtime = chrono::            
            duration_cast<chrono::milliseconds>(currTime - startTime).count();
            if(status[packetNum] == SENT and 
                chrono::milliseconds(elapsedtime).count() - sentTime[packetNum]
                .count() > timeOut.count() ) {
                
                L = R = packetNum;
                W = 1;
                reTransmit[packetNum] = true;
                break;
            }
        }
        windowLocker.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
    }
    LOG_EXIT;
}

void receiveAck() {
    LOG_ENTRY;
    string recv;
    int cnt = 0;
    char buffer[MAX_BUFFER];
    int addrLen = sizeof(receiverAddr);
    int currReceivedAck;
    int lastReceivedAck = 0;
    int sampleRTT;
    int estimatedRTT;
    int deviation = 0;
    bool flag = false;
    while(true) {
        memset(buffer,0,sizeof(buffer));
        int in = recvfrom(sockFd, buffer, sizeof(buffer), 0, 
                        ( struct sockaddr *) &receiverAddr, (socklen_t *)&addrLen); 
        if(in <= 0) {
            higLog("%s"," recvfrom() failed");
        }

        buffer[in] = '\0';
        string protocolBuffer = buffer;
        MP::TcpMessage packet;
        packet.ParseFromString(protocolBuffer);
        currReceivedAck = (int)(packet.packetnum());

        auto currTime = chrono::high_resolution_clock::now();
        auto elapsedtime = chrono::
            duration_cast<chrono::milliseconds>(currTime - startTime).count();

        sampleRTT = chrono::milliseconds(elapsedtime).count() 
                    - sentTime[currReceivedAck].count();
        if(flag == false) {
            estimatedRTT = sampleRTT << 3;
            flag = true;
        }
        sampleRTT -= (estimatedRTT >> 3);
        estimatedRTT += sampleRTT;
        if (sampleRTT < 0)
            sampleRTT = -sampleRTT;
        sampleRTT -= (deviation >> 3);
        deviation += sampleRTT;
        timeOut = chrono::milliseconds((estimatedRTT >> 3) + (deviation >> 1));

        int acketPackets = currReceivedAck - lastReceivedAck - 1;
        for(int packetNum = lastReceivedAck + 1; packetNum<=currReceivedAck; 
            packetNum++ ) {
            status[packetNum] = ACKED;
        }

        lastReceivedAck = currReceivedAck;
        windowLocker.lock();
        L = lastReceivedAck + 1;
        W = W + (1.0/W) * (acketPackets);
        R = L + floor(W); 
        windowLocker.unlock();
        cnt = currReceivedAck - 1;  // this will go max upto 10000
        if(cnt == MAX_PACKET_NUM) {
            break;
        }    
    }
    LOG_EXIT;
} 

void displayStats() {
    // TODO
}