#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>
#include <algorithm>

#define MAXLINE 10000
#define TRACKERPORT 18005

using namespace std;

//GLOBAL DATA STRUCTURES BEGIN

typedef struct peerinfo
{ //this is a struct that we will pass to the handlerequest function
    //UPDATE I don't use it anymore. But I'm too lazy to remove it now.
    int port;
    int socketfiledescriptor;
} peerinfo;

typedef struct fileinfo
{
    int chunks;
    unordered_set<string> owners;
    string name;
    int roationcounter = 1; //for peerselection

} fileinfo;

typedef struct groupstruct
{
    string owner;
    string gid;
    unordered_set<string> members;
    unordered_set<string> requestlist;
    unordered_map<string, fileinfo> filenametofileinfomap;

} groupstruct;

unordered_map<string, string> usernamepasswordmap;          // to map username to password
unordered_map<string, string> usernameportmap;              // to map username to port
unordered_map<string, string> portusernamemap;              // to map port to username
unordered_map<string, groupstruct> groupidtogroupstructmap; //to map groupid to group struct
unordered_set<string> allusersset;                          //set of all users
unordered_set<string> onlineusersset;                       //set of online users

//GLOBAL DATA STRUCTURES END

vector<string> tokenizestring(string str)
{
    vector<string> tokens;
    string token;
    stringstream ss(str);
    while (getline(ss, token, ' '))
    {
        tokens.push_back(token);
    }
    return tokens;
}

string create_user(string u, string p)
{
    string response;
    if (usernamepasswordmap.count(u) != 0)
    {
        response = "There already exists a user with username " + u;
    }
    else
    {
        usernamepasswordmap[u] = p;
        allusersset.insert(u);
        response = "User successfully created. Now you may login.";
    }
    return response;
}

string login(string u, string p, string port)
{
    string response;
    if (usernamepasswordmap.count(u) == 0)
    {
        response = "No such user exists.";
    }
    else
    {
        if (usernamepasswordmap[u] == p)
        {
            onlineusersset.insert(u);
            usernameportmap[u] = port;
            portusernamemap[port] = u;
            response = "Login successful";
        }
        else
        {
            response = "Login Failed.";
        }
    }
    return response;
}

string create_group(string groupid, string ownerport)
{
    string response;
    if (groupidtogroupstructmap.count(groupid) != 0)
    {
        response = "A group with same name already exists. Try joining it.";
    }
    else
    {
        groupstruct gs;
        string username = portusernamemap[ownerport];
        gs.owner = username;
        gs.members.insert(username);
        gs.gid = groupid;
        groupidtogroupstructmap[groupid] = gs;
        response = "Group created.";
    }
    return response;
}

string join_group(string groupid, string port)
{
    string response;
    if ((portusernamemap.count(port) == 0) || (groupidtogroupstructmap.count(groupid) == 0))
    {
        response = "Value error. Either user or group doesn't exist";
    }
    else
    {
        string username = portusernamemap[port];
        groupstruct gs = groupidtogroupstructmap[groupid];
        if (gs.members.count(username) != 0)
        {
            response = "User " + username + " is already the member of group " + groupid;
        }
        else
        {
            groupidtogroupstructmap[groupid].requestlist.insert(username);
            response = "Your request has been registered.";
        }
    }
    return response;
}

string leave_group(string groupid, string userport)
{
    string response;
    if ((portusernamemap.count(userport) == 0) || (groupidtogroupstructmap.count(groupid) == 0))
    {
        response = "Value error. Either user or group doesn't exist";
    }
    else
    {
        string username = portusernamemap[userport];
        groupstruct gs = groupidtogroupstructmap[groupid];
        if (gs.members.count(username) == 0)
        {
            response = "User not even the part of group";
        }
        else
        {
            gs.members.erase(username);
            response = "User removed.";
            if (gs.owner == username)
            {
                response += " As the user was owner the group will also be deleted.";
                groupidtogroupstructmap.erase(groupid);
            }
        }
    }
    return response;
}

string list_request(string groupid, string userport)
{
    string response;
    if ((portusernamemap.count(userport) == 0) || (groupidtogroupstructmap.count(groupid) == 0))
    {
        response = "Value error. Either user or group doesn't exist";
    }
    else
    {
        string username = portusernamemap[userport];
        groupstruct gs = groupidtogroupstructmap[groupid];
        if (gs.owner != username)
        {
            response = "Authorization Error. You are not the owner.";
        }
        else if (gs.requestlist.size())
        {
            response = "The list of pending approvals is ";
            unordered_set<string>::iterator iq = gs.requestlist.begin();
            while (iq != gs.requestlist.end())
            {
                response = response + (*iq) + ", ";
                iq++;
            }
        }
        else
        {
            response = "There is no active pending approval list present.";
        }
    }
    return response;
}

string accept_request(string groupid, string userid, string userport)
{
    string response;
    if ((portusernamemap.count(userport) == 0) || (groupidtogroupstructmap.count(groupid) == 0))
    {
        response = "Value error. Either user or group doesn't exist";
    }
    else
    {
        string username = portusernamemap[userport];
        groupstruct gs = groupidtogroupstructmap[groupid];
        if (gs.owner != username)
        {
            response = "Authorization Error. You are not the owner.";
        }
        else
        {
            if (gs.requestlist.count(userid) == 0)
            {
                response = "Please check the username entered.";
            }
            else
            {
                groupidtogroupstructmap[groupid].requestlist.erase(userid);
                groupidtogroupstructmap[groupid].members.insert(userid);
                response = "User " + userid + " succesfully added.";
            }
        }
    }
    return response;
}
string list_groups()
{
    string response;
    if (groupidtogroupstructmap.size() == 0)
    {
        response = "No groups present currently";
    }
    else
    {
        response = "Groups list are: ";
        unordered_map<string, groupstruct>::iterator iq = groupidtogroupstructmap.begin();
        while (iq != groupidtogroupstructmap.end())
        {
            response = response + iq->first + ", ";
            iq++;
        }
    }
    return response;
}

string list_files(string groupid)
{
    string response;
    if (groupidtogroupstructmap.count(groupid) == 0)
    {
        response = "No such group exists.";
    }
    else
    {
        groupstruct gs = groupidtogroupstructmap[groupid];
        if (gs.filenametofileinfomap.size() == 0)
        {
            response = "No sharable files found in the group";
        }
        else
        {
            response = "Files list begin: ";
            unordered_map<string, fileinfo>::iterator iq = gs.filenametofileinfomap.begin();
            while (iq != gs.filenametofileinfomap.end())
            {
                response = response + iq->first + "\n";
                iq++;
            }
        }
    }
    return response;
}

string upload_file(string filename, string groupid, string userport, string chunknumber)
{
    string response;
    string username = portusernamemap[userport];

    if (groupidtogroupstructmap.count(groupid) == 0)
    {
        response = "No group with the name " + groupid + " exists";
    }
    else if (groupidtogroupstructmap[groupid].members.count(username) == 0)
    {
        response = "You need to join the group before uploading file.";
    }
    else
    {
        groupstruct gs = groupidtogroupstructmap[groupid];

        if (gs.filenametofileinfomap.count(filename) == 0)
        {
            fileinfo finfo;
            finfo.name = filename;
            finfo.chunks = stoi(chunknumber);
            finfo.owners.insert(username);
            groupidtogroupstructmap[groupid].filenametofileinfomap[filename] = finfo;
            response = "You have started sharing this file in the group.";
        }
        else
        {
            groupidtogroupstructmap[groupid].filenametofileinfomap[filename].owners.insert(username);
            response = "You have been added to the list of people sharing the file in the group.";
        }
    }
    return response;
}

string download_file(string gid, string fname, string dpath, string uport)
{
    //this will just send the list of users (port) with files back to peer.
    string response;
    string username = portusernamemap[uport];
    if (groupidtogroupstructmap[gid].members.count(username) == 0)
    {
        response = "You are not the memeber of this group. You need to join the group before downloading the file.";
    }
    else if (groupidtogroupstructmap[gid].filenametofileinfomap.count(fname) == 0)
    {
        response = "No such downloadable file exists in the group.";
    }
    else
    {
        response = "Seeder_list " + fname + " ";
        int numberofchunks = groupidtogroupstructmap[gid].filenametofileinfomap[fname].chunks;
        response = response + to_string(numberofchunks);
        unordered_set<string>::iterator iq = groupidtogroupstructmap[gid].filenametofileinfomap[fname].owners.begin();
        vector<string> seedvectors;
        while(iq!=groupidtogroupstructmap[gid].filenametofileinfomap[fname].owners.end()){
            if(onlineusersset.count(*iq) != 0 ){
                seedvectors.push_back(*iq);
            }
            iq++;
        }
        // vector<string> seedvectors(groupidtogroupstructmap[gid].filenametofileinfomap[fname].owners.begin(), groupidtogroupstructmap[gid].filenametofileinfomap[fname].owners.end());
        rotate(seedvectors.begin(), seedvectors.begin() + groupidtogroupstructmap[gid].filenametofileinfomap[fname].roationcounter, seedvectors.end());
        cout << "NUmber of seeders of this file: = " << seedvectors.size() << endl;
        for (string STR : seedvectors)
        {
            string port = usernameportmap[(STR)];
            response = response + " " + port;
        }
        groupidtogroupstructmap[gid].filenametofileinfomap[fname].roationcounter++;
    }
    return response;
}
void parsestring(string command, int sockfd)
{
    cout << "Parsing command " << command << " from fd = " << sockfd << endl;
    string response;
    if (command == "")
    {
        response = "No Command entered.";
    }
    else
    {
        vector<string> tokens = tokenizestring(command);
        if (tokens[0] == "create_user")
        {
            if (tokens.size() != 4)
            {
                response = "Command error. Too few or too many parameters passed.";
            }
            else
            {
                string username = tokens[1];
                string password = tokens[2];
                response = create_user(username, password);
            }
        }
        else if (tokens[0] == "login")
        {
            if (tokens.size() != 4)
            {
                response = "Command error. Too few or too many parameters passed.";
            }
            else
            {
                string username = tokens[1];
                string password = tokens[2];
                string userport = tokens[3];
                response = login(username, password, userport);
            }
        }
        else if (tokens[0] == "create_group")
        {
            if (tokens.size() != 3)
            {
                response = "Command Error. Too few or too many parameters passed.";
            }
            else
            {
                string groupid = tokens[1];
                string ownerportnumber = tokens[2];
                response = create_group(groupid, ownerportnumber);
            }
        }
        else if (tokens[0] == "join_group")
        {
            if (tokens.size() != 3)
            {
                response = "Command error. Too few or too many parameters passed.";
            }
            else
            {
                string groupid = tokens[1];
                string userport = tokens[2];
                response = join_group(groupid, userport);
            }
        }
        else if (tokens[0] == "leave_group")
        {
            if (tokens.size() != 3)
            {
                response = "Command error. Too few or too many parameters passed.";
            }
            else
            {
                string groupid = tokens[1];
                string userport = tokens[2];
                response = leave_group(groupid, userport);
            }
        }
        else if (tokens[0] == "list_requests")
        {
            if (tokens.size() != 3)
            {
                response = "Command error. Too few or too many parameters passed.";
            }
            else
            {
                string groupid = tokens[1];
                string userport = tokens[2];
                response = list_request(groupid, userport);
            }
        }
        else if (tokens[0] == "accept_request")
        {
            if (tokens.size() != 4)
            {
                response = "Command error. Too few or too many parameters passed.";
            }
            else
            {
                string groupid = tokens[1];
                string userid = tokens[2];
                string userport = tokens[3];
                response = accept_request(groupid, userid, userport);
            }
        }
        else if (tokens[0] == "list_groups")
        {
            if (tokens.size() != 2)
            {
                response = "Command error. Too many parameters passed.";
            }
            else
            {
                response = list_groups();
            }
        }
        else if (tokens[0] == "list_files")
        {
            if (tokens.size() != 3)
            {
                response = "Command Error. Too few or too many parameters passed.";
            }
            else
            {
                string groupid = tokens[1];
                response = list_files(groupid);
            }
        }
        else if (tokens[0] == "upload_file")
        {
            if (tokens.size() != 5)
            {
                response = "Command Error. Too few or too many parameters passed.";
            }
            else
            {
                string filename = tokens[1];
                string groupid = tokens[2];
                string userport = tokens[3];
                string chunknumber = tokens[4];
                response = upload_file(filename, groupid, userport, chunknumber);
            }
        }
        else if (tokens[0] == "download_file")
        {
            if (tokens.size() != 5)
            {
                response = "Command Error. Too few or too many parameters passed.";
            }
            else
            {
                string groupid = tokens[1];
                string filename = tokens[2];
                string destinationpath = tokens[3];
                string userport = tokens[4];

                response = download_file(groupid, filename, destinationpath, userport);
            }
        }
        else if (tokens[0] == "logout")
        {
            if (tokens.size() != 2)
            {
                response = "Command Error. Too few or too many parameters passed.";
            }
            else
            {
                string port = tokens[1];
                string username = portusernamemap[port];
                onlineusersset.erase(username);
                response = "Successfully Logged out.";
            }
        }
        else
        {
            response = "Please enter a valid command";
        }
    }
    send(sockfd, response.c_str(), strlen(response.c_str()), 0);
    return;
}

void *handlerequest(void *connfdptr)
{
    cout << "Request arrived" << endl;
    // int connfd = *(int *)connfdptr;
    peerinfo pinfo = *((peerinfo *)connfdptr);
    free(connfdptr);
    char recvline[MAXLINE + 1];
    int connfd = pinfo.socketfiledescriptor;
    int cport = pinfo.port;
    int n;

    //zero out the receive buffer to make sure that it is null terminated
    while ((n = recv(connfd, recvline, MAXLINE - 1, 0)))
    {
        cout << "Read " << n << " lines from the client on port " << cport << endl;
        cout << recvline << endl;
        //ignoring the EOF file charreader

        string receivedstring = recvline;
        string sentmsg = "DONE";
        parsestring(receivedstring, connfd);
        memset(recvline, 0, MAXLINE);
    }
    cout << "Quitting handlerequest." << endl;
    close(connfd);
    return NULL;
}

int main(int argc, char const *argv[])
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd <= 0)
    {
        cout << "Error in starting socket in tracker" << endl;
        return -1;
    }
    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TRACKERPORT);

    if ((bind(listenfd, (sockaddr *)&servaddr, sizeof(servaddr))) < 0)
    {
        cout << "Error occured in tracker while binding. Restart may fix this problem." << endl;
        return -1;
    }

    if (listen(listenfd, 5) < 0)
    {
        cout << "listen error occured" << endl;
        return -1;
    }

    for (;;)
    {
        struct sockaddr_in addr;

        socklen_t addr_len = sizeof(addr);

        cout << "Waiting for connection on port " << TRACKERPORT << endl;

        int confd = accept(listenfd, (sockaddr *)&addr, &addr_len);

        int conport = addr.sin_port;

        peerinfo *peerinfoptr = new peerinfo;

        peerinfoptr->port = conport;
        peerinfoptr->socketfiledescriptor = confd;

        // int * pclient = (int *)malloc(sizeof(int));

        // *pclient = confd;

        pthread_t t;

        // pthread_create(&t, NULL, handlerequest, pclient);
        pthread_create(&t, NULL, handlerequest, peerinfoptr);
    }
    return 0;
}
