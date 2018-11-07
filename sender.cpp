#include <bits/stdc++.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include "logging.h"
#include "Message.pb.h"
using namespace std;

#define PORT 8080 
struct sockaddr_in address; 
struct sockaddr_in servAddr;
// global varibales
int L, R, W, timeOut;
int status[10009];
int sentTime[10009];
mutex windowLock;
// function prototypes
int connectToReceiver(string);
void timeOoutCheck();
void receiveAck();

int main(int argc, char const *argv[])  {     
    int sockFd = connectToReceiver("127.0.0.1");
    int nextPckt = 1;
    
    unique_lock<mutex> locker(windowLock,defer_lock);
    locker.lock();
    L = 1;
    R = 1;
    W = 1;
    locker.unlock();
    int l,r,w;
    int cnt = 1;
    while(true) {
        locker.lock();
        l = L; r = R; w = W;
        locker.unlock();
        
        MP::TcpMessage packet;
        packet.set_packetnum(cnt);
        packet.set_msg(std::string(1000,'a'));
        string protocolBuffer = packet.SerializeAsString();
        int datalen = protocolBuffer.length();
        

    }
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
    memset(&servAddr, '0', sizeof(servAddr)); 
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(Ip.c_str());
    servAddr.sin_port = htons(PORT); 
    if(connect(sockFd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) { 
        higLog("%s","\nConnection Failed \n");
        return FAILURE; 
    }
    return sockFd;     
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