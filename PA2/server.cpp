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

void fireman(int) { while (waitpid(-1, NULL, WNOHANG) > 0) {;} }

int gcd(int a, int b)
{
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

string calculateHyperperiod(vector<tuple<char, int, int, int>> process)
{
    int result = get<2>(process[0]);
    for(int x = 1; x < process.size(); x++) { result = (result * get<2>(process[x]) / gcd(result, get<2>(process[x]))); }
    return to_string(result);
}

string calculateUtilization(vector<tuple<char, int, int, int>> process)
{
    double u = 0;
    for(int i = 0; i < process.size(); i++) { u = u + (static_cast<double>(get<1>(process[i])) / get<2>(process[i])); }
    return to_string(u);
}

bool compareProcesses(const tuple<char, int, int, int>& task1, const tuple<char, int, int, int>& task2) { return (get<2>(task1) > get<2>(task2) || (get<0>(task1) > get<0>(task2) && get<2>(task1) == get<2>(task2))); }

string calculateDiagram(vector<tuple<char, int, int, int>> process, int hyperperiod)
{
    string output = "";
    vector<tuple<char,bool,int,int>> intervals;
    priority_queue process_queue(process.begin(), process.end(), compareProcesses);
    process.clear();
    priority_queue pq2 = process_queue;
    while (!pq2.empty())
    {
        process.push_back(pq2.top());
        pq2.pop();
    }
    for (int i = 0; i < hyperperiod; i++)
    {
        for(int j = 0; j < process.size(); j++)
        {
            if(i > 0 && i % get<2>(process[j]) == 0)
            {
                get<1>(process[j]) = get<3>(process[j]);
                for(int k = 0; k < intervals.size(); k++) { get<1>(intervals[k]) = true; }
            }
        }
        for(auto& task : process)
        {
            if(i > 0 && i % get<2>(task) == 0) { get<1>(task) = get<3>(task); }
            if(get<1>(task) > 0)
            {
                if(get<1>(task) == get<3>(task)) { intervals.push_back(make_tuple(get<0>(task), false, i, i+1)); }
                else
                {
                    for(int l = intervals.size()-1; l < intervals.size(); l--)
                    {
                        if (get<0>(intervals[l]) == get<0>(task))
                        {
                            if(get<1>(intervals[l])) { intervals.push_back(make_tuple(get<0>(task), false, i, i+1)); }
                            else { ++get<3>(intervals[l]); }
                            break;
                        }
                    }
                }
                --get<1>(task);
                break;
            }
        }
    }
    for(int m = 0, last = 0; m < intervals.size(); m++)
    {
        tuple<char,bool,int,int> interval = intervals[m];
        if(get<2>(interval) > last) { output = output + "Idle(" + to_string(get<2>(interval) - last) + "), " + get<0>(interval) + "(" + to_string(get<3>(interval) - get<2>(interval)) + ")"; }
        else { output = output + get<0>(interval) + "(" + to_string(get<3>(interval) - get<2>(interval)) + ")"; }
        last = get<3>(interval);
        if(m < intervals.size()-1 || get<3>(interval) < hyperperiod) { output = output + ", "; }
        if(m == intervals.size()-1 && get<3>(interval) < hyperperiod) { output = output + "Idle(" + to_string(hyperperiod - get<3>(interval)) + ")"; }
    }
    return output;
}

int main(int argc, char *argv[])
{
   int sockfd, newsockfd, portno, clilen;
   struct sockaddr_in serv_addr, cli_addr;
   // Check the commandline arguments
   if (argc != 2) { cerr << "Port not provided" << endl; exit(0); }
   // Create the socket
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) { cerr << "Error opening socket" << endl; exit(0); }
   // Populate the sockaddr_in structure
   bzero((char *)&serv_addr, sizeof(serv_addr));
   portno = atoi(argv[1]);
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);
   // Bind the socket with the sockaddr_in structure
   if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { cerr << "Error binding" << endl; exit(0); }
   // Set the max number of concurrent connections
   listen(sockfd, 5);
   clilen = sizeof(cli_addr);
   signal(SIGCHLD, fireman); //call fireman to kill zombie processes
   while(true) //for however many threads we will be recieiving
   {
      // Accept a new connection
      newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen);
      if(fork() == 0) //fork for every new thread
      {
        if (newsockfd < 0) { cerr << "Error accepting new connections" << endl; exit(0); }
        int msgSize = 0;
        //read message from client
        if (read(newsockfd, &msgSize, sizeof(int)) < 0) { cerr << "Error reading size from socket" << endl; exit(0); }
        char *tempBuffer = new char[msgSize + 1];
        bzero(tempBuffer, msgSize + 1);
        if (read(newsockfd, tempBuffer, msgSize + 1) < 0) { cerr << "Error reading string from socket" << endl; exit(0); }
        string buffer = tempBuffer;
        delete[] tempBuffer;
        //preparing output to send to client
        stringstream s(buffer);    
        vector<tuple<char,int,int,int>> processVector;
        char task;
        int executionTime;
        int timePeriod;
        while(s >> task >> executionTime >> timePeriod) { processVector.push_back(make_tuple(task, executionTime, timePeriod, executionTime)); }
        string utilization = calculateUtilization(processVector);
        string hyperperiod = calculateHyperperiod(processVector);
        string threshold = to_string(processVector.size() * (pow(2, 1.0 / static_cast<double>(processVector.size())) - 1));
        string newBuffer = utilization + "\n" + hyperperiod + "\n" + calculateDiagram(processVector, stoi(hyperperiod)) + "\n" + threshold;
        msgSize = newBuffer.size();
        //write message to client
        if (write(newsockfd, &msgSize, sizeof(int)) < 0) { cerr << "Error writing size to socket" << endl; exit(0); }
        if (write(newsockfd, newBuffer.c_str(), msgSize) < 0) { cerr << "Error writing string to socket" << endl; exit(0); }
        close(newsockfd);
        _exit(0);
      }
   }
   close(sockfd);
   return 0;
}