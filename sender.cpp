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
struct sockaddr_in servAddr;
int L, R;
double W;
int sockFd;

auto startTime = chrono::high_resolution_clock::now();
std::chrono::microseconds timeOut(2000);
vector<int> status(MAX_PACKET_NUM + 1);
vector<chrono::microseconds> sentTime(MAX_PACKET_NUM + 1);
vector<bool> reTransmit(MAX_PACKET_NUM + 1);

mutex windowLock;
mutex reTransLock;


int connectToReceiver(string);
int sendPacket(int packetNum);
void timeOutCheck();
void receiveAck();
void displayStats();

bool simulationActive = false;
std::random_device rdevice;
std::mt19937 mt(rdevice());
std::uniform_real_distribution<double> dist(1.0, 1000.0);

ofstream fout("w_vs_time.txt");
ofstream ffout("sent_time_vs_packet.txt");
ofstream fffout("acked_time_vs_packet.txt");
ofstream ffffout("timeoutpacket_vs_time.txt");

int main(int argc, char const *argv[])  {  
    LOG_ENTRY;
    simulationActive = true;
    signal(SIGPIPE, SIG_IGN);
    sockFd = connectToReceiver("10.129.135.192");

    unique_lock<mutex> lockerA(windowLock,defer_lock);
    lockerA.lock();
    L = 1;
    R = 1;
    W = 1;
    lockerA.unlock();
    int cnt = 1;
    
    std::thread T1(timeOutCheck);   T1.detach();
    std::thread T2(receiveAck);     T2.detach();
    
    reTransmit[1] = true;
    while(true) {
        int packetNum = cnt;
        lockerA.lock();
        if(L == R) {
            unique_lock<mutex> lockerB(reTransLock,defer_lock);
            lockerB.lock();
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
            lockerB.unlock();
        }else if(packetNum >=L and packetNum <= R) {
            int ret = sendPacket(packetNum);
            if(ret == FAILURE) {
                higLog("%s","sendPacket() failed");
            }else {
                cnt++;
            }
        }
        lockerA.unlock();

        if(cnt > MAX_PACKET_NUM) {
            simulationActive = false;
            break;
        }
    }
    displayStats();
    return 0; 
} 

int connectToReceiver(string Ip) {
    LOG_ENTRY;
    int _sockFd;
    if ((_sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
        higLog("%s","Socket creation error");
        return FAILURE; 
    } 

    memset(&servAddr, 0, sizeof(servAddr)); 
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(Ip.c_str());
    servAddr.sin_port = htons(MY_PORT_NUM); 
    
    
    LOG_EXIT;
    return _sockFd;     
}

int sendPacket(int packetNum) {
    LOG_ENTRY;
    double prob = dist(mt);
    if(prob > 5) {
        MP::TcpMessage packet;
        packet.set_packetnum(packetNum);
        packet.set_msg(std::string(1000,'a'));
        string protocolBuffer = packet.SerializeAsString();
        int datalen = protocolBuffer.length();
        int ret = sendto(sockFd,protocolBuffer.c_str(),datalen,MSG_CONFIRM,
                    (struct sockaddr *)(&servAddr),sizeof(servAddr));

        if(ret <= 0) {
            higLog("%s","sendto() failed");
            return FAILURE;
        }
    	//fout << chrono::microseconds(elapsedtime).count() <<" "<<packetNum<<endl;
    }else {
        higLog("drop %d",packetNum);
    }

    auto endTime = chrono::high_resolution_clock::now();
    auto elapsedtime = chrono::
		duration_cast<chrono::microseconds>(endTime - startTime).count();
    sentTime[packetNum] = chrono::microseconds(elapsedtime);
    status[packetNum] = SENT;
    ffout << chrono::microseconds(elapsedtime).count() <<" "<<packetNum<<endl;
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

	auto elapsedtime = chrono::
            duration_cast<chrono::microseconds>(currTime - startTime).count();

        unique_lock<mutex> windowLocker(windowLock,defer_lock);
        windowLocker.lock();
	cout <<"going to check from L = "<<L<<" to R = "<<R<<endl;
	bool f = false;
	int x;
	for(int packetNum = L; packetNum<= R; packetNum++) {
            if(status[packetNum] == SENT and status[packetNum] != ACKED and 
                chrono::microseconds(elapsedtime).count() - sentTime[packetNum]
                .count() > timeOut.count() ) {
                L = R = packetNum;
                W = 1;
                reTransmit[packetNum] = true;
                f = true;
		x = packetNum;
		higLog("@@@@@@@@@@@@ TimeOut packetNum : %d",packetNum);
                break;
            }
        }
        windowLocker.unlock();
	if(f)
	    ffffout << chrono::microseconds(elapsedtime).count() <<" "<<x <<endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
    }
    LOG_EXIT;
}

void receiveAck() {
    LOG_ENTRY;
    string recv;
    int cnt = 0;
    char buffer[MAX_BUFFER];
    int addrLen;
    int currReceivedAck;
    int lastReceivedAck = 1;
    int sampleRTT;
    int estimatedRTT;
    int deviation = 0;
    bool flag = false;
    while(true) {
        memset(buffer,0,sizeof(buffer));
        int in = recvfrom(sockFd, buffer, sizeof(buffer), MSG_WAITALL, 
                        ( struct sockaddr *) &servAddr, (socklen_t *)&addrLen); 
        //std::this_thread::sleep_for(std::chrono::microseconds(20));
	if(in <= 0) {
            higLog("%s"," recvfrom() failed");
        }

        buffer[in] = '\0';
        string protocolBuffer = buffer;
        MP::TcpMessage packet;
        packet.ParseFromString(protocolBuffer);
        currReceivedAck = (int)(packet.packetnum());

        higLog("Received ack packet with num : %d",currReceivedAck);


        auto currTime = chrono::high_resolution_clock::now();
        auto elapsedtime = chrono::
            duration_cast<chrono::microseconds>(currTime - startTime).count();

        sampleRTT = chrono::microseconds(elapsedtime).count() 
                    - sentTime[currReceivedAck - 1].count();
        cout <<"SampleRTT = "<<sampleRTT << endl;
	if(flag == false) {
            estimatedRTT = sampleRTT << 3;
            flag = true;
        }
	cout<<"Last estimatedRTT = "<< (estimatedRTT>>3) << endl;
        sampleRTT -= (estimatedRTT >> 3);
        estimatedRTT += sampleRTT;
        cout <<"New estimated RTT = "<< (estimatedRTT >> 3) << endl;
	if (sampleRTT < 0)
            sampleRTT = -sampleRTT;
        sampleRTT -= (deviation >> 3);
        deviation += sampleRTT;
	cout <<"deviation" << deviation << endl;
        timeOut = chrono::microseconds((estimatedRTT >> 3) + (deviation >> 1));

        long long x = timeOut.count();
        higLog(">>>>>>> New timeOut = %lld",x);

        int ackedPackets = currReceivedAck - lastReceivedAck;
        for(int packetNum = lastReceivedAck; packetNum <= currReceivedAck - 1; 
            packetNum++ ) {
            status[packetNum] = ACKED;
            fffout << chrono::microseconds(elapsedtime).count() <<" "<< packetNum << endl;
	}

        lastReceivedAck = currReceivedAck;

        unique_lock<mutex> lockerA(windowLock,defer_lock);

        lockerA.lock();
        higLog("ackedPackets = %d",ackedPackets);
        L = lastReceivedAck;
        W = W + (1.0/W) * (ackedPackets);
	R = L + floor(W); 
        higLog("L = %d  R = %d  W = %lf",L,R,W);
        lockerA.unlock();

	fout <<chrono::microseconds(elapsedtime).count() <<" "<<W<<endl;

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
