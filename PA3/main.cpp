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
#include <pthread.h>
#include <unistd.h>

using namespace std;

struct thread
{
    vector<tuple<char,int,int,int>> process;
    string input;
    int cpu;
    string output;
    int* counter;
    pthread_mutex_t *mutex;
    pthread_mutex_t *mutex2;
    pthread_cond_t *condition;
};

int gcd(int a, int b)
{
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int calculateHyperperiod(vector<tuple<char, int, int, int>> process)
{
    int result = get<2>(process[0]);
    for(int x = 1; x < process.size(); x++) { result = (result * get<2>(process[x]) / gcd(result, get<2>(process[x]))); }
    return result;
}

double calculateUtilization(vector<tuple<char, int, int, int>> process)
{
    double u = 0;
    for(int i = 0; i < process.size(); i++) { u = u + (static_cast<double>(get<1>(process[i])) / get<2>(process[i])); }
    return u;
}

bool compareProcesses(const tuple<char, int, int, int>& task1, const tuple<char, int, int, int>& task2) { return (get<2>(task1) > get<2>(task2) || (get<0>(task1) > get<0>(task2) && get<2>(task1) == get<2>(task2))); }

string calculateDiagram(vector<tuple<char, int, int, int>> process, int hyperperiod)
{
    string output = "";
    vector<tuple<char,bool,int,int>> intervals;
    // sort the vector by period in descending order
    priority_queue process_queue(process.begin(), process.end(), compareProcesses);
    process.clear();
    priority_queue pq2 = process_queue;
    while (!pq2.empty())
    {
        process.push_back(pq2.top());
        pq2.pop();
    }
    // begin the scheduling diagram
    for (int i = 0; i < hyperperiod; i++)
    {
        for(int j = 0; j < process.size(); j++)
        {
            if(i > 0 && i % get<2>(process[j]) == 0)
            {
                get<1>(process[j]) = get<3>(process[j]);
                for(int k = 0; k < intervals.size(); k++)
                { 
                    get<1>(intervals[k]) = true;
                }
            }
        }
        for(auto& task : process)
        {
            if(i > 0 && i % get<2>(task) == 0)
            { 
                get<1>(task) = get<3>(task);
            }
            if(get<1>(task) > 0)
            {
                if(get<1>(task) == get<3>(task))
                {
                    intervals.push_back(make_tuple(get<0>(task), false, i, i+1));
                }
                else
                {
                    for(int l = intervals.size()-1; l < intervals.size(); l--)
                    {
                        if (get<0>(intervals[l]) == get<0>(task))
                        {
                            if(get<1>(intervals[l]))
                            {
                                intervals.push_back(make_tuple(get<0>(task), false, i, i+1));
                            }
                            else
                            {
                                ++get<3>(intervals[l]);
                            }
                            break;
                        }
                    }
                }
                --get<1>(task);
                break;
            }
        }
    }
    // prepare output to send back to output() function
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

void* threadInstructions(void* arg)
{
    thread arg_ptr = *(thread*) arg;
    vector<tuple<char,int,int,int>> localProcess = arg_ptr.process;
    string localInput = arg_ptr.input;
    int localCPU = arg_ptr.cpu;
    pthread_mutex_unlock(arg_ptr.mutex);
    double utilization = calculateUtilization(localProcess);
    int hyperperiod = calculateHyperperiod(localProcess);
    string outputString = "CPU " + to_string(localCPU+1), entropyString = "";
    outputString += "\nTask scheduling information: ";
    stringstream s(localInput);
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
    outputString += "\nRate Monotonic Algorithm execution for CPU" + to_string(localCPU+1)+": ";
    if (utilization > 1)
    {
        outputString += "\nThe task set is not schedulable";
    }
    if(utilization <= 1 && (utilization > localProcess.size() * (pow(2, 1.0 / static_cast<double>(localProcess.size())) - 1))) { outputString += "\nTask set schedulability is unknown"; }
    else
    {
        outputString += "\nScheduling Diagram for CPU " + to_string(localCPU+1) + ": ";
        outputString += calculateDiagram(localProcess, hyperperiod);
    }
    pthread_mutex_lock(arg_ptr.mutex2); //get ready to output by locking down all other threads
    while(*arg_ptr.counter!=localCPU) { pthread_cond_wait(arg_ptr.condition, arg_ptr.mutex2); }
    pthread_mutex_unlock(arg_ptr.mutex2); //all threads are asleep now we can print
    cout << outputString << endl << endl; //PRINT OUTSIDE CRITICAL SECTION
    pthread_mutex_lock(arg_ptr.mutex2); //get next CPU ready
    (*arg_ptr.counter)++;
    pthread_cond_broadcast(arg_ptr.condition); //wake up threads
    pthread_mutex_unlock(arg_ptr.mutex2);
    return NULL;
}

int main()
{
    string input = "";
    vector<string> inputVector;
    vector<vector<tuple<char,int,int,int>>> allThreads;
    vector<pthread_t> threadVector;
    pthread_mutex_t mutex; //initialize first semaphore
    pthread_mutex_init(&mutex, nullptr);
    pthread_mutex_t mutex2; //initialize second semaphore
    pthread_mutex_init(&mutex2, nullptr);
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER; //initialize first conditional variable
    static int counter = 0; //keep track of what order we print
    while(getline(cin, input))
    {
        if(input.empty()) {break;}
        inputVector.push_back(input);
        stringstream s(input);    
        vector<tuple<char,int,int,int>> processVector;
        char task;
        int executionTime;
        int timePeriod;
        while(s >> task >> executionTime >> timePeriod) { processVector.push_back(make_tuple(task, executionTime, timePeriod, executionTime)); }
        allThreads.push_back(processVector);
    }
    thread newThread; //only using 1 struct
    //initializing struct constants
    newThread.counter = &counter;
    newThread.mutex = &mutex;
    newThread.condition = &condition;
    newThread.mutex2 = &mutex2;
    for(int i = 0; i < inputVector.size(); i++)
    {
        pthread_t myThread;
        pthread_mutex_lock(&mutex);
        newThread.cpu = i;
        newThread.input = inputVector[i];
        newThread.process = allThreads[i];
        if(pthread_create(&myThread, NULL, threadInstructions, &newThread))
        { cout << "ERROR!"; return -1; }
        threadVector.push_back(myThread);
    }
    for(int j = 0; j < threadVector.size(); j++) { pthread_join(threadVector[j], NULL); }
    return 0;
}