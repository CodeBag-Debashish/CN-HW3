// Server side C/C++ program to demonstrate Socket programming 
#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include "logging.h"
#include "Message.pb.h"
#define PORT 8080 

int server_fd, sockFd, valread,currRcvPacket,lastAckSent; 
struct sockaddr_in address; 
int opt = 1; 
int addrlen = sizeof(address); 
char buffer[1024] = {0};
 
int recvPcket[1001];
int sentAck[1001];  
int SENT = 1;

int sendPacket(int packetNum) {
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
    sentAck[packetNum] = true;
    lastAckSent = packetNum;
    return SUCCESS;
}

void timeOoutCheck() {
    int packetNum;
    for(packetNum = lastAckSent; packetNum<= LastRecvAck; packetNum++) {
        if(recvPcket[packetNum] != true) {
            LastRecvAck = packetNum-1;
            break;   
        }
    }
}
int connectToSender(string Ip) {
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

int main(int argc, char const *argv[]) 
{ 
    
    srand(time(NULL));   
    int sockFd = connectToSender("127.0.0.1");  
    
    while(true){
        memset(buffer,0,sizeof(buffer));
        int in = recvfrom(sockFd, buffer, sizeof(buffer), 0, 
             ( struct sockaddr *) &address, &addrlen); 
        if(in <= 0) {
          higLog("%s"," recvfrom() failed");
        }
        buffer[in] = '\0';
        string protocolBuffer = buffer;
        Mp::TcpMessage packet;
        packet.ParseFromString(protocolBuffer);
        currRcvPacket = (int)(packet.packetnum());
        
        recvPcket[currRcvPacket] = true;
        LastRecvAck = currRcvPacket;

        if(curr-lastAckSent>threshold){
            timeOoutCheck();
            int ret = sendPacket(LastRecvAck);
            if(ret == FAILURE) {
                higLog("%s","sendPacket() failed");
            }
            lastAckSent = LastRecvAck; 
        }
          
       
    }
    
    return 0; 
} 
