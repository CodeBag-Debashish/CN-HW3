// Server side C/C++ program to demonstrate Socket programming 
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

auto startTime = chrono::high_resolution_clock::now();
std::chrono::milliseconds lastAckSentTime(0);
std::chrono::microseconds ackSendTimeOut(20);

int addrLen,sockFd,currRcvPacket,lastAckSent = 1; 
int nextAck;
const int MAX_PACKET_NUM = 10000;
struct sockaddr_in servAddr,senderAddr;
int opt = 1; 
int addrlen = sizeof(servAddr); 
char buffer[MAX_BUFFER];
 
int recvPcket[MAX_PACKET_NUM + 1];
int sentAck[MAX_PACKET_NUM + 1];

int sendPacket(int);
void timeOoutCheck(int);
int createServer(string);

int main(int argc, char const *argv[])  { 
    LOG_ENTRY;
    signal(SIGPIPE, SIG_IGN);
    sockFd = createServer("10.129.135.192");
    if(sockFd <=0 ) {
        higLog("%s","server sockFd is not correctly created");
        exit(1);
    }
    memset(&senderAddr, 0, sizeof(senderAddr)); 
    unsigned int len = sizeof(senderAddr);
    while(true) {
        memset(buffer,0,sizeof(buffer));
        int in = recvfrom(sockFd, buffer, sizeof(buffer), MSG_WAITALL, 
                    (struct sockaddr *)&senderAddr, (socklen_t *)&len);
        if(in < 0) {
            higLog("%s"," recvfrom() failed");
        }else {
            char* ipString = inet_ntoa(senderAddr.sin_addr);
            higLog("Request came from sender : %s",ipString);
        }
        


        buffer[in] = '\0';
        string protocolBuffer = buffer;
        MP::TcpMessage packet;
        packet.ParseFromString(protocolBuffer);
        currRcvPacket = (int)(packet.packetnum());
        midLog("Packet received with packet num : %d",currRcvPacket);
        recvPcket[currRcvPacket] = true;
        nextAck = currRcvPacket + 1;
        int curr, threshold;

        auto currTime = chrono::high_resolution_clock::now();
        auto elapsedtime = chrono::
        duration_cast<chrono::milliseconds>(currTime - startTime).count();

        if(chrono::milliseconds(elapsedtime).count() - lastAckSentTime.count() 
                > ackSendTimeOut.count() ) {
            int packetNum;
            for(packetNum=lastAckSent;packetNum<=currRcvPacket;packetNum++){
                if(recvPcket[packetNum] == false) {
                    nextAck = packetNum;
                    break;
                }
            }
            for(packetNum++;packetNum<=currRcvPacket;packetNum++) {
                recvPcket[packetNum] = false;
            }
            higLog("Next ack value in the server is : %d",nextAck);
            int ret = sendPacket(nextAck);
            if(ret == FAILURE) {
                higLog("%s","sendPacket() failed");
            }

            lastAckSent = nextAck;
            currTime = chrono::high_resolution_clock::now();
            elapsedtime = chrono::
            duration_cast<chrono::milliseconds>(currTime - startTime).count();
            lastAckSentTime = std::chrono::milliseconds(elapsedtime);
        }   
    }
    return 0; 
} 

int createServer(string Ip) {
    LOG_ENTRY;
    int _sockFd; 
    char buffer[MAX_BUFFER];
    if ((_sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    }
    memset(&servAddr, 0, sizeof(servAddr)); 
    servAddr.sin_family         = AF_INET;
    servAddr.sin_addr.s_addr    = inet_addr(Ip.c_str());
    servAddr.sin_port           = htons(MY_PORT_NUM);

    if(bind(_sockFd,(const struct sockaddr *)&servAddr,sizeof(servAddr)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE);
    } 
    LOG_EXIT;
    return _sockFd;
}

int sendPacket(int packetNum) {
    LOG_ENTRY;
    higLog("sendpacket entry with packetnum : %d",packetNum);
    MP::TcpMessage packet;
    packet.set_packetnum(packetNum);
    packet.set_msg(std::string("ACK"));
    string protocolBuffer = packet.SerializeAsString();
    int datalen = protocolBuffer.length();
    int ret = sendto(sockFd,protocolBuffer.c_str(),datalen,MSG_CONFIRM,
                    (struct sockaddr *)(&senderAddr),sizeof(senderAddr));
                /* servAddr is global */
    if(ret < 0) {
        higLog("%s","sendto() failed");
        return FAILURE;
    }else {
        higLog("Successfully sent ack %d",packetNum);
    }
    sentAck[packetNum] = true;
    lastAckSent = packetNum;
    LOG_EXIT;
    return SUCCESS;
}

int connectToSender(string Ip) {
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

