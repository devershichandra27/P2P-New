#include <unistd.h>
#include <vector>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
#include <thread>

using namespace std;

#define MAXLINE 550000
#define TRACKERPORT 18005
#define CHUNKMAXSIZE 51200

int PEERPORT = 19000; //the peer will start acting as a server on current port plus 50
string USERNAME;
bool loginattempt = false;
bool loggedin = false;
string UPLOADFOLDER;
string DOWNLOADFOLDER;

unordered_map<string, string> filenametochunksmap; //a map to store the number of chunks associated with each file

int split(string filename)
{
    //this function will split the input file into little chunks of output files.
    int prefixnamecounter = 0;
    FILE *ifptr = fopen(filename.c_str(), "r");

    if (!ifptr)
    {
        cout << "File doesn't exist" << endl;
        return -1;
    }
    string numstr = "";

    do
    {
        prefixnamecounter++;
        cout << "Creating temp file number = " << prefixnamecounter << endl;
        string ofname = filename + to_string(prefixnamecounter);
        FILE *ofptr = fopen(ofname.c_str(), "w");
        long long int numcounter = 0;
        do
        {
            char ch = fgetc(ifptr);
            fprintf(ofptr, "%c", ch);
            numcounter++;
            // numstr.clear();
            if (feof(ifptr))
            {
                fclose(ofptr);
                fclose(ifptr);
                return prefixnamecounter;
            }
            if (numcounter == CHUNKMAXSIZE)
            {
                fclose(ofptr);
                break;
            }
        } while (1);
    } while (1);
    return prefixnamecounter;
}
vector<string> tokenize(string input)
{
    vector<string> tokens;
    string token;
    stringstream ss(input);
    while (getline(ss, token, ' '))
        tokens.push_back(token);
    return tokens;
}

int connecttodestionation(int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        cout << "Error occured in client while creating the socket to port " << port << endl;
        return -1;
    }

    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    string ip = "127.0.0.1";
    if (inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) <= 0)
    {
        cout << "Internet address translation error occured in peer" << endl;
        return -1;
    }
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        cout << "Connection failed in client\n";
        return -1;
    }

    //now we can use this sockfd to write and read data in the connected entitiy.
    return sockfd;
}

void downloadchunkfromport(int chunknum, string filename, int port)
{
    int sockfd = connecttodestionation(port);
    cout << "Attempting download chunknum " << chunknum << " of file " << filename << " from port " << port << endl;
    if (sockfd == -1)
    {
        cout << "Connection to the peer failed. chunknum = " << chunknum << " filename = " << filename << " at port = " << port << endl;
        close(sockfd);
        return;
    }

    char sendline[MAXLINE];
    bzero(sendline, MAXLINE);
    string chunkfilename = filename + to_string(chunknum); //this alone should suffice
    strcpy(sendline, chunkfilename.c_str());

    int sendbytes = strlen(sendline);
    int actualbytessent = 0;

    if ((actualbytessent = send(sockfd, sendline, sendbytes, 0)) != sendbytes)
    {
        cout << "Connectivity error occured while downloading. Couldn't contact peer for chunk number " << chunknum << " of file " << filename << endl;
        close(sockfd);
        return;
    }
    int readbytecounter = 0;
    char recvline[MAXLINE];
    memset(recvline, 0, MAXLINE);

    if ((readbytecounter = recv(sockfd, recvline, MAXLINE - 1, 0)))
    {
        // cout << "creating file:  " << chunkfilename << endl;
        FILE *ofptr = fopen((DOWNLOADFOLDER + "/"+chunkfilename).c_str(), "wb");
        if (ofptr == NULL)
        {
            cout << "Chunk file number " << chunknum << " couldn't be written here." << endl;
            return;
        }
        // cout << "RECEIVED FROM PEER CONTENT: " <<recvline << endl;
        int byteswritten = fwrite(recvline, sizeof(char), readbytecounter, ofptr);
        // cout << "Bytes written = " << byteswritten << endl;
        if (ferror(ofptr))
        {
            cout << "Error occured in writing the chunk number: " << chunknum << " of file " << filename << endl;
            return;
        }

        fclose(ofptr);
        cout << "Download successful of chunknum " << chunknum << " of file " << filename << " from port " << port << endl;
    }
    else
    {
        cout << "Readbyte error occured while downloading chunknum " << chunknum << " of file " << filename << " from port " << port << endl;
        return;
    }
}

string downloadfilefrompeer(int numberofchunks, string filename, vector<string> portvector)
{
    string response;
    //we will have to add 50 to each vector before callling.
    cout << "Starting downloadfilefrompeer function. numberofchunks = " << numberofchunks << " filename " << filename << endl;
    int numberofseeders = portvector.size();
    int seedercount = 0;
    thread *threadarray = new thread[numberofchunks + 1];
    cout << "Port list start" << endl;
    for(auto k : portvector){
        cout << k << " ";
    }cout << endl << "POrt list end\n";
    for (int chunknum = 1; chunknum <= numberofchunks; chunknum++)
    {
        int port = stoi(portvector[seedercount]) + 50;
        seedercount++;
        seedercount = seedercount % numberofseeders;
        threadarray[chunknum] = thread(downloadchunkfromport, chunknum, filename, port);
    }
    for (int chunknum = 1; chunknum <= numberofchunks; chunknum++)
    {
        threadarray[chunknum].join();
    }
    response = "Ended downloadfilefrompeer function. numberofchunks = " + to_string(numberofchunks) + " filename " + filename;
    return response;
}

void sendcommandtotracker(string input)
{
    int trackerfd = connecttodestionation(TRACKERPORT);

    if (trackerfd == -1)
    {
        cout << "Connection to tracker failed." << endl;
        close(trackerfd);
        return;
    }
    char sendline[MAXLINE];
    //ignoring the addition of EOF char here.
    vector<string> tokens = tokenize(input);

    if (tokens[0] == "login")
    {
        loginattempt = true;
    }
    else
    {
        loginattempt = false;
    }

    if ((tokens[0] == "create_group") && (loggedin == false))
    {
        cout << "You need to login to create a group" << endl;
        return;
    }

    if ((tokens[0] == "upload_file"))
    {
        if (loggedin == false)
        {
            cout << "You need to login to upload a file." << endl;
            return;
        }
        else
        {
            if (tokens.size() != 4)
            {
                cout << "Command Error. Too few or too many parameters passed.";
            }
            else
            {
                string filepath = UPLOADFOLDER + "/" + tokens[1];
                int chunkssize = split(filepath);
                if (chunkssize == -1)
                {
                    return;
                }
                else
                {
                    filenametochunksmap[filepath] = chunkssize;
                    input = input + " " + to_string(chunkssize);
                }
            }
        }
    }
    strcpy(sendline, input.c_str());

    int sendbytes = strlen(sendline);
    int actualbytessent = 0;
    if ((actualbytessent = send(trackerfd, sendline, sendbytes, 0)) != sendbytes)
    {
        cout << "Sending error occured in peer. Coudn't send all data." << endl;
        cout << "Could sent only " << actualbytessent << " of " << sendbytes << endl;
        close(trackerfd);
        return;
    }
    int readbytecounter = 0;
    char recvline[MAXLINE];
    memset(recvline, 0, MAXLINE);
    // while((readbytecounter = recv(trackerfd, recvline, MAXLINE -1, 0))){
    if ((readbytecounter = recv(trackerfd, recvline, MAXLINE - 1, 0)))
    {
        if (strcmp(recvline, "Login successful") == 0)
        {
            USERNAME = tokens[1];
            loginattempt = false;
            loggedin = true;
            UPLOADFOLDER = USERNAME+"/uploads";
            DOWNLOADFOLDER = USERNAME+"/downloads";
            mkdir(USERNAME.c_str(), S_IRWXG|S_IRWXO | S_IRWXU);
            mkdir(UPLOADFOLDER.c_str(), S_IRWXG|S_IRWXO | S_IRWXU );
            mkdir(DOWNLOADFOLDER.c_str(), S_IRWXG|S_IRWXO | S_IRWXU);
        }
        else
        {
            string receivedstring = recvline;
            if (receivedstring != "")
            {
                vector<string> tokens = tokenize(receivedstring);
                if (tokens[0] == "Seeder_list")
                {
                    //receivedstring = Seeder_list filename 2 port1 port2 ..etc
                    int numberofchunks = stoi(tokens[2]);
                    string filename = tokens[1];
                    vector<string> portvectorsforfile(tokens.begin() + 3, tokens.end());
                    downloadfilefrompeer(numberofchunks, filename, portvectorsforfile);
                }
            }
        }
        cout << "Tracker Response = " << recvline << endl;

        memset(recvline, 0, MAXLINE);
    }

    if (readbytecounter < 0)
    {
        cout << "Read error occured from tracker" << endl;
        close(trackerfd);
        return;
    }
    close(trackerfd);
    return;
}

void executecommand(string command)
{
    //will fill this later.
    command = command + " " + to_string(PEERPORT);
    sendcommandtotracker(command);
}

void startpeer()
{
    while (true)
    {
        string command;
        cout << "Input command" << endl;
        cout << "> ";
        getline(cin, command);
        if (command == "q")
        {
            cout << "Quitting. Bye" << endl;
            break;
        }
        else
        {
            executecommand(command);
        }
    }
}

void *handlerequest(void *connfdptr)
{
    cout << "File request arrived." << endl;
    int connfd = *((int *)connfdptr);
    free(connfdptr);
    char recvline[MAXLINE + 1];
    int n;

    while ((n = recv(connfd, recvline, MAXLINE - 1, 0)))
    {
        cout << "Received request for file: " << recvline << endl;

        string receivedstring =  UPLOADFOLDER + "/" + recvline;
        char sendarray[MAXLINE];
        FILE * ifstream = fopen(receivedstring.c_str(), "r");
        fread(sendarray, sizeof(char), MAXLINE, ifstream );
        fclose(ifstream);
        send(connfd, sendarray, strlen(sendarray), 0);
        memset(recvline, 0, MAXLINE);
    }
    cout << "Quitting handlerequest." << endl;
    close(connfd);
    return NULL;
}

void startserver()
{
    int PEERSERVERPORT = PEERPORT + 50;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd <= 0)
    {
        cout << "Error in starting server. Socket couldn't be created." << endl;
        return;
    }
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    servaddr.sin_port = htons(PEERSERVERPORT);

    if ((bind(listenfd, (sockaddr *)&servaddr, sizeof(servaddr))) < 0)
    {
        cout << "Binding Error in starting server in peer. Restarting may fix this problem.";
        return;
    }

    if (listen(listenfd, 5) < 0)
    {
        cout << "Listen error occured in server of peer." << endl;
        return;
    }

    for (;;)
    {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        cout << "Server is running on port " << PEERSERVERPORT << endl;
        int confd = accept(listenfd, (sockaddr *)&addr, &addr_len);

        int *pclient = (int *)malloc(sizeof(int));
        *pclient = confd;

        pthread_t t;
        pthread_create(&t, NULL, handlerequest, pclient);
    }
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        cout << "Usage: ./executable portnumber" << endl;
        return 1;
    }
    PEERPORT = atoi(argv[1]);
    thread t1(startpeer);
    thread t2(startserver);

    t1.join();
    t2.join();
    return 0;
}
