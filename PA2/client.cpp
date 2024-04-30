#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

struct threader //for threading
{
    string inpStr; //the full unedited string
    int ord; //the number of the thread (and CPU)
    string outStr; //the final entropy string
    char* serverIP; // argv[1]
    char* portno; //argv[2]
    threader(string iS, int o, char* sIP, char* p) { //constructor
        inpStr = iS;
        ord = o;
        serverIP = sIP;
        portno = p;
    }
};

string output(int cpu, string input, vector<string> bufferVector)
{
    double utilization = stod(bufferVector[0]);
    int hyperperiod = stoi(bufferVector[1]);
    string schedulingDiagram = bufferVector[2];
    double threshold = stod(bufferVector[3]);
    string outputString = "CPU " + to_string(cpu+1), entropyString = "";
    outputString += "\nTask scheduling information: ";
    stringstream s(input);
    char task;
    int wcet;
    int period;
    while(s >> task >> wcet >> period)
    {
        outputString = outputString + task + " (WCET: " + to_string(wcet) + ", Period: " + to_string(period) + "), ";
    }
    outputString.pop_back();
    outputString.pop_back();
    ostringstream oss;
    oss << fixed << setprecision(2) << utilization;
    outputString += "\nTask set utilization: " + oss.str();
    outputString += "\nHyperperiod: " + to_string(hyperperiod);
    outputString += "\nRate Monotonic Algorithm execution for CPU" + to_string(cpu+1)+": ";
    if (utilization > 1)
    {
        outputString += "\nThe task set is not schedulable";
    }
    else if(utilization <= 1 && (utilization > threshold))
    { 
        outputString += "\nTask set schedulability is unknown"; 
    }
    else
    {
        outputString += "\nScheduling Diagram for CPU " + to_string(cpu+1) + ": ";
        outputString += schedulingDiagram;
    }
    return outputString;
}

//The threading function

void* threadInstructions(void* arg)
{
    //initializing variables and casting
    struct sockaddr_in serv_addr;
    struct hostent *server;
    threader* arg_ptr = (threader*) arg;
    int portno = atoi(arg_ptr->portno);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    string buffer = arg_ptr->inpStr;
    //creating socket
    if (sockfd < 0) { cerr << "ERROR opening socket" << endl; exit(0); }
    server = gethostbyname(arg_ptr->serverIP);
    if (server == NULL) { cerr << "ERROR, no such host" << endl; exit(0); }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) { cerr << "ERROR connecting" << endl; exit(0); }
    //writing to server
    int msgSize = buffer.size();
    if (write(sockfd,&msgSize,sizeof(int)) < 0)  { cerr << "ERROR writing size to socket" << endl; exit(0); }
    if (write(sockfd,buffer.c_str(),msgSize) < 0) { cerr << "ERROR writing string to socket" << endl; exit(0); }
    //reading from server
    if (read(sockfd,&msgSize,sizeof(int)) < 0) { cerr << "ERROR reading size from socket" << endl; exit(0); }
    char *tempBuffer = new char[msgSize+1];
    bzero(tempBuffer,msgSize+1);
    if (read(sockfd,tempBuffer,msgSize) < 0) { cerr << "ERROR reading string from socket" << endl; exit(0); }
    buffer = tempBuffer;
    vector<string> bufferVector;
    stringstream ss(buffer);
    string line;
    while(getline(ss, line))
    {
        bufferVector.push_back(line);
    }
    arg_ptr->outStr = output(arg_ptr->ord, arg_ptr->inpStr, bufferVector);
    delete [] tempBuffer;
    close(sockfd);
    return NULL;
}

int main(int argc, char *argv[])
{
    string input = "";
    vector<string> inputVector;
    vector<pthread_t> threadVector;
    vector<threader*> structVector;
    if (argc != 3) { cerr << "usage " << argv[0] << " hostname port" << endl; exit(0); }
    while(getline(cin, input))
    {
        if(input.empty()) { break; }
        inputVector.push_back(input);
    }
    for(int a = 0; a < inputVector.size(); a++) //variable-A threads
    {
        threader* newThread = new threader(inputVector[a], a, argv[1], argv[2]);
        pthread_t myThread;
        if(pthread_create(&myThread, NULL, threadInstructions, static_cast<void*> (newThread)))
        {
            cout << "ERROR";
            return -1;
        }
        structVector.push_back(newThread);
        threadVector.push_back(myThread);
    }
    for(int b = 0; b < threadVector.size(); b++) { pthread_join(threadVector[b], NULL); }
    for(int c = 0; c < structVector.size(); c++) { cout << structVector[c]->outStr << endl << endl; }
    return 0;
}