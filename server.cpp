#include "utils.h"
#include <semaphore.h>
#include <pthread.h>
#include <sstream>
#include <signal.h>

using namespace std;

queue<int> socketQueue;
sem_t mutex;
sem_t empty;
sem_t full;

void push(int skt);

int pop();

string filetypeHeader(string type);

string getExt(string fileName);

string generateFileName(char* line, string requestType);

void GetHeaderLines(vector<char *> &headerLines, int skt, bool envformat);

void HandleCGIConnection(int socket, string sstartLine, string contentType);

void *HandleConnection(void *param);

int main(int argc, char* argv[])
{
	int hSocket,hServerSocket;  /* handle to socket */
	struct hostent* pHostInfo;   /* holds info about a machine */
	struct sockaddr_in Address; /* Internet socket address stuct */
	int nAddressSize=sizeof(struct sockaddr_in);
	char pBuffer[BUFFER_SIZE];
	int nHostPort;
	vector<char *> headerLines;
	sem_init(&empty, PTHREAD_PROCESS_PRIVATE, QUEUE_SIZE);
	sem_init(&mutex, PTHREAD_PROCESS_PRIVATE, 1);
	sem_init(&full, PTHREAD_PROCESS_PRIVATE, 0);

	if(argc < 2)
	{
		printf("\nUsage: server host-port\n");
		return 0;
	}
	else
	{
		nHostPort=atoi(argv[1]);
	}
	signal(SIGPIPE, SIG_IGN);

	/* make a socket */
	hServerSocket=socket(AF_INET,SOCK_STREAM,0);
	if(hServerSocket == SOCKET_ERROR)
	{
		printf("\nCould not make a socket\n");
		return 0;
	}

	/* fill address struct */
	Address.sin_addr.s_addr=INADDR_ANY;
	Address.sin_port=htons(nHostPort);
	Address.sin_family=AF_INET;

	/* bind to a port */
	if(bind(hServerSocket,(struct sockaddr*)&Address,sizeof(Address)) 
		== SOCKET_ERROR)
	{
		printf("\nCould not connect to host\n");
		return 0;
	}
	/*  get port number */
	getsockname( hServerSocket, (struct sockaddr *) &Address,(socklen_t *)&nAddressSize);

	if(listen(hServerSocket,QUEUE_SIZE) == SOCKET_ERROR)
	{
		printf("\nCould not listen\n");
		return 0;
	}
	int optval = 1;
	setsockopt (hServerSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	
	pthread_t* threads;
	threads = new pthread_t[THREAD_COUNT];
	for(int i = 0;i<THREAD_COUNT;i++){
		threads[i] = pthread_t();
		pthread_create(&(threads[i]), NULL, &HandleConnection, NULL);
	}
	while(1)
	{
		/* get the connected socket */
		hSocket=accept(hServerSocket,(struct sockaddr*)&Address,(socklen_t *)&nAddressSize);
		push(hSocket);

	}
}

void *HandleConnection(void *param)
{
	while(1)
	{
		char buffer[BUFFER_SIZE];
		int skt = pop();
		char *startline = GetLine(skt);
		struct stat fileInfo;
		if(startline[0] == 'G')
		{
			//handle get
			string fileName = generateFileName(startline, "GET");
			if(stat(fileName.c_str(), &fileInfo) < 0)
			{
				//send 404
				strcpy(buffer, ERROR_MESSAGE);
				write(skt, buffer, strlen(buffer)+1);
			}
			else if(S_ISDIR(fileInfo.st_mode))
			{
				string indexFile = fileName + "/index.html";
				if(stat(indexFile.c_str(), &fileInfo) < 0)
				{
					struct dirent *drent;
					DIR *dir;
					dir = opendir(fileName.c_str());
					drent = readdir(dir);
					string directory = "<HTML>";
					directory += fileName;
					directory += "\n\n";
					while(1)
					{
						if(!drent)
						{
							break;
						}
						string dname = drent->d_name;
						if(dname == "." || dname == "..")
						{
							drent = readdir(dir);
							continue;
						}
						directory += "<p><a href = ";
						directory += drent->d_name;
						directory += ">";
						directory += drent->d_name;
						directory += "</a></p>\n";
						drent = readdir(dir);
					}
					directory += "</HTML>";
					string length = "";
					ostringstream stream;
					stream << "" << directory.size();
					length = stream.str();
					string header = "HTTP/1.1 200 OK\nContent-type: text/html\nContent-length: " + length + "\n\n";
					header += directory;
					send(skt, header.c_str(), header.length(), MSG_NOSIGNAL);
				}
				else
				{
					int fd_in;
					struct stat indexInfo;
					stat(indexFile.c_str(), &indexInfo);
					fd_in = open( indexFile.c_str(), O_RDONLY);
					off_t offest = 0 ;
					long size ;
					int t = fstat(fd_in, &indexInfo);
					string type = "html";
					type = getExt(indexFile);
					//send header here
					string length = "";
					ostringstream stream;
					stream << "" << indexInfo.st_size;
					length = stream.str();
					string header = "HTTP/1.1 200 OK\nContent-type: text/html\nContent-length: " + length + "\n\n";
					send(skt, header.c_str(), header.length(), MSG_NOSIGNAL);
					long ss = sendfile( skt , fd_in ,&offest ,indexInfo.st_size);
					close(fd_in);
				}
			}
			else if(S_ISREG(fileInfo.st_mode))
			{
				
				int fd_in;
				fd_in = open( fileName.c_str(), O_RDONLY);
				off_t offest = 0 ;
				long size ;
				int t = fstat(fd_in, &fileInfo);
				string type = "html";
				type = getExt(fileName);
				if(type == "cgi" || type == "pl")
				{
					handleCGIConnection(skt, startline, type);
					return NULL;
				}
				else if(type == "php")
				{
					HandlePHPConn(skt, startline, type);
					return NULL;
				}
				//send header here
				string length = "";
				ostringstream stream;
				stream << "" << fileInfo.st_size;
				length = stream.str();
				string header = "HTTP/1.1 200 OK\nContent-type: " + filetypeHeader(type) + "Content-length:" + length + "\n\n";
				send(skt, header.c_str(), header.length(), MSG_NOSIGNAL);
				
				long ss = sendfile( skt , fd_in ,&offest ,fileInfo.st_size);
				usleep(100);
				close(fd_in);
				if(type == "gif" || type == "jpg")
				{
					usleep(20000);
				}
				else
				{
					usleep(15000);
				}
			}
			else
			{
				//send 404
				strcpy(buffer, ERROR_MESSAGE);
				write(skt, buffer, strlen(buffer));
			}
		}
		else if(startline[0] == 'P')
		{
			string fileName = generateFileName(startline, "POST");
			string type = "html";
			type = getExt(fileName);
			if(type == "cgi" || type == "pl")
			{
				handleCGIConnection(skt, startline, type);
				return NULL;
			}
		}
		vector<char*> headerLines = vector<char*>();
		GetHeaderLines(headerLines, skt , false);
		
		//strcpy(buffer, "");
		write(skt, buffer, strlen(buffer));

		/* close socket */
		if(close(skt) == SOCKET_ERROR)
		{
			printf("\nCould not close socket\n");
		}
	}
}

void handleCGIConnection(int socket, string sstartLine, string contentType)
{
	//Send intital Headers
	char pBuffer[1000];
	strcpy(pBuffer,"HTTP/1.0 200 OK\r\n");
	strcat(pBuffer, "MIME-Version:1.0\r\n");
	write(socket,pBuffer,strlen(pBuffer));

//set up the filepath stuff
	//Make sure its a Post
	bool isPost = false;

	if (sstartLine.find("POST")+1)	
	{
		isPost = true;
	}
	string path;
	if (isPost)
	{
		path = sstartLine.substr(5, sstartLine.length() - 14);
	} 
	else
	{
		path = sstartLine.substr(4, sstartLine.length() - 13);
	}
	string absPath;
	// replace this with the public html folder
	absPath = "/users/ugrad/l/lloughra/public_html";

	string pathPart1;
	string pathPart2;

	if (path.find("?")+1 )
	{
		pathPart1 = path.substr(0,path.find("?"));
		pathPart2 = path.substr(path.find("?")+1 , path.size());
	}
	else
	{
		pathPart1 = path;
		pathPart2 = "";
	}

	//Set up the pipes
	int ServeToCGIpipefd[2];
	int CGIToServepipefd[2];
	pipe(ServeToCGIpipefd);
	pipe(CGIToServepipefd);

	//Set up env and Args
	vector<char *> headerLines;
	GetHeaderLines(headerLines, socket, true); // Read in the header lines, changing the format
	headerLines.push_back("GATEWAY_INTERFACE=CGI/1.1");

	char requestHeader[1000];
	sprintf(requestHeader,"REQUEST_URI=%s", pathPart1.c_str());
	headerLines.push_back(requestHeader);
	char requestHeader2[1000];
	sprintf(requestHeader2,"REQUEST_METHOD=%s", (isPost)? "POST":"GET");
	headerLines.push_back(requestHeader2);
	char requestHeader3[1000];
	sprintf(requestHeader3,"QUERY_STRING=%s", pathPart2.c_str());
	if (!isPost)
	{
		headerLines.push_back(requestHeader3);
	}

	char* args[2];
	char* env[headerLines.size()+1];
	//get content length from the request headers
	string contentlengthstring;
	for(int i =0; i<headerLines.size(); i++)
	{
		string curHeader(headerLines.at(i));
		if (curHeader.find("CONTENT_LENGTH=")+1)
		{
			contentlengthstring = curHeader.substr(15, curHeader.length());
		}
	}
	int contentlength = atoi(contentlengthstring.c_str());

// Set up the environment variable array
	int i;

	for (i = 0; i < headerLines.size(); i++)
	{
		string curHeader(headerLines.at(i));
		env[i] = headerLines.at(i);

		if (curHeader.find("CONTENT_LENGTH=")+1)
		{
			sscanf(env[i], "CONTENT_LENGTH=%d", &contentlength);
			//fprintf(stderr, "Scanned CONTENT_LENGTH is %d\n", contentlength);
		}

	}
	env[headerLines.size()] = (char* ) 0 ;


	char* filePath = (char*)(absPath + pathPart1).c_str();

	args[0] = filePath;
	args[1] = (char* ) 0 ;

	//fork the joint
	int child = fork();
	if (child == 0)
	{

		close(ServeToCGIpipefd[1]);    // close the write side of the pipe from the server
		dup2(ServeToCGIpipefd[0], 0);  // dup the pipe to stdin
		close(CGIToServepipefd[0]);    // close the read side of the pipe to the server
		dup2(CGIToServepipefd[1], 1);  // dup the pipe to stdout

		execve(filePath, args, env);

	} 
	else 
	{
		//do parent stuff
		close(ServeToCGIpipefd[0]);  // close the read side of the pipe to the CGI script
		close(CGIToServepipefd[1]);  // close the write side of the pipe from the CGI script
		if (isPost)
		{

	//DO THE READING AND WRITING
			int amt = 0;
			char buffer[1000];

			int amtread = 0;
			while (amtread < contentlength && (amt = read(socket, buffer, MAXBUFLEN)) ) 
			{
				amtread += amt;
				write(ServeToCGIpipefd[1], buffer, amt);
			}
		}

// Read from the CGIToServePipefd[0] until you get an error and write this data to the socket
		int amt = 0;
		char buffer[1000];
		int amtread = 0;
		do 
		{
			amt = read(CGIToServepipefd[0], buffer, MAXBUFLEN);
			amtread += amt;
			write(socket, buffer, amt);
		} while (amt > 0);
		close(ServeToCGIpipefd[1]); // all done, close the pipe
		close(CGIToServepipefd[0]); // all done, close the pipe
		wait(child);
		if(close(socket) == SOCKET_ERROR) 
		{
			printf("\nCould not close socket\n");
			return;
		}
	}
}

void HandlePHPConn(int socket, string sstartLine, string contentType)
{
	//Send intital Headers
	char pBuffer[1000];
	strcpy(pBuffer,"HTTP/1.0 200 OK\r\n");
	strcat(pBuffer, "MIME-Version:1.0\r\n");
	write(socket,pBuffer,strlen(pBuffer));

//set up the filepath stuff
	//Make sure its a Post
	bool isPost = false;

	if (sstartLine.find("POST")+1)	
	{
		isPost = true;
	}
	string path;
	if (isPost)
	{
		path = sstartLine.substr(5, sstartLine.length() - 14);
	} 
	else
	{
		path = sstartLine.substr(4, sstartLine.length() - 13);
	}
	string absPath;
	absPath = "/users/ugrad/l/lloughra/public_html";

	string pathPart1;
	string pathPart2;

	if (path.find("?")+1 )
	{
		pathPart1 = path.substr(0,path.find("?"));
		pathPart2 = path.substr(path.find("?")+1 , path.size());
	} 
	else 
	{
		pathPart1 = path;
		pathPart2 = "";
	}

	//Set up the pipes
	int ServeToCGIpipefd[2];
	int CGIToServepipefd[2];
	pipe(ServeToCGIpipefd);
	pipe(CGIToServepipefd);

	//Set up env and Args
	vector<char *> headerLines;
	GetHeaderLines(headerLines, socket, true); // Read in the header lines, changing the format
	headerLines.push_back("GATEWAY_INTERFACE=CGI/1.1");

	char requestHeader[1000];
	sprintf(requestHeader,"REQUEST_URI=%s", pathPart1.c_str());
	headerLines.push_back(requestHeader);
	char requestHeader2[1000];
	sprintf(requestHeader2,"REQUEST_METHOD=%s", (isPost)? "POST":"GET");
	headerLines.push_back(requestHeader2);
	char requestHeader3[1000];
	sprintf(requestHeader3,"QUERY_STRING=%s", pathPart2.c_str());
	if (!isPost)
	{
		headerLines.push_back(requestHeader3);
	}
	char requestHeader4[1000];
	sprintf(requestHeader4,"SCRIPT_FILENAME=%s", (char*)(absPath + pathPart1).c_str());
	headerLines.push_back(requestHeader4);
	char requestHeader5[1000];
	sprintf(requestHeader5,"SCRIPT_NAME=%s", pathPart1.c_str());
	headerLines.push_back(requestHeader5);
	headerLines.push_back("REDIRECT_STATUS=200");

	char* args[3];
	char* env[headerLines.size()+1];
	//get content length from the request headers
	string contentlengthstring;
	for(int i =0; i<headerLines.size(); i++)
	{
		string curHeader(headerLines.at(i));
		if (curHeader.find("CONTENT_LENGTH=")+1)
		{
			contentlengthstring = curHeader.substr(15, curHeader.length());
		}
	}
	int contentlength = atoi(contentlengthstring.c_str());

	// Set up the environment variable array
	int i;

	for (i = 0; i < headerLines.size(); i++)
	{
		string curHeader(headerLines.at(i));
		env[i] = headerLines.at(i);

		if (curHeader.find("CONTENT_LENGTH=")+1)
		{
			sscanf(env[i], "CONTENT_LENGTH=%d", &contentlength);
			fprintf(stderr, "Scanned CONTENT_LENGTH is %d\n", contentlength);
		}

	}
	env[headerLines.size()] = (char* ) 0 ;

	char* filePath = (char*)(absPath + pathPart1).c_str();

	args[0] = "/usr/bin/php-cgi";
	args[1] = filePath;
	args[2] = (char* ) 0 ;

	//fork the joint
	int child = fork();
	if (child == 0)
	{

		close(ServeToCGIpipefd[1]);    // close the write side of the pipe from the server
		dup2(ServeToCGIpipefd[0], 0);  // dup the pipe to stdin
		close(CGIToServepipefd[0]);    // close the read side of the pipe to the server
		dup2(CGIToServepipefd[1], 1);  // dup the pipe to stdout

		execve(args[0], args, env);
		printf("PAST EXECVE!!!");
	} 
	else 
	{
		//do parent stuff
		close(ServeToCGIpipefd[0]);  // close the read side of the pipe to the CGI script
		close(CGIToServepipefd[1]);  // close the write side of the pipe from the CGI script
		if (isPost)
		{

			//DO THE READING AND WRITING
			int i = 0;
			while(env[i] != (char*) 0)
			{
				string header = env[i];
				if(header.find("CONTENT_LENGTH")+1)
				{
					string lengthstring = header.substr(15, header.length());
					contentlength = atoi(lengthstring.c_str());
				}
			}
			
			int amt = 0;
			char buffer[1000];

			int amtread = 0;
			while (amtread < contentlength) 
			{
				amt = read(socket, buffer, MAXBUFLEN);
				amtread += amt;
				write(ServeToCGIpipefd[1], buffer, amt);
			}
		}

		int amt = 0;
		char buffer[1000];
		int amtread = 0;
		do 
		{
			amt = read(CGIToServepipefd[0], buffer, MAXBUFLEN);
			amtread += amt;
			write(socket, buffer, amt);
		} 
		while (amt > 0);
		close(ServeToCGIpipefd[1]); // all done, close the pipe
		close(CGIToServepipefd[0]); // all done, close the pipe
		wait(child);
		if(close(socket) == SOCKET_ERROR) 
		{
			printf("\nCould not close socket\n");
			return;
		}
	}
}

void GetHeaderLines(vector<char *> &headerLines, int skt, bool envformat)
{
    // Read the headers, look for specific ones that may change our responseCode
    char *line;
    char *tline;
    
    tline = GetLine(skt);
    while(strlen(tline) != 0)
    {
        if (strstr(tline, "Content-Length") || 
            strstr(tline, "Content-Type"))
        {
            if (envformat)
                line = FormatHeader(tline, "");
            else
                line = strdup(tline);
        }
        else
        {
            if (envformat)
                line = FormatHeader(tline, "HTTP_");
            else
            {
                line = (char *)malloc((strlen(tline) + 10) * sizeof(char));
                sprintf(line, "HTTP_%s", tline);                
            }
        }
        
        headerLines.push_back(line);
        free(tline);
        tline = GetLine(skt);
    }
    free(tline);
}

string generateFileName(char* line, string requestType)
{
	// replace this with whatever the public html folder is
	string base = "/users/ugrad/l/lloughra/public_html";
	
	string file = "";
	int i;
	if(requestType == "GET")
	{
		i = 4;
	}
	else if(requestType == "POST")
	{
		i = 5;
	}
	while(!isWhitespace(line[i] ) && line[i] != '?')
	{
		file += line[i];
		i++;
	}
	return base + file;
}

string getExt(string fileName)
{
	int length = fileName.length();
	unsigned found = fileName.find("txt");
	if(found != string::npos && found == length-3)
	{
		return "txt";
	}
	found = fileName.find("html");
	if(found != string::npos && found == length-4)
	{
		return "html";
	}
	found = fileName.find("gif");
	if(found != string::npos && found == length-3)
	{
		return "gif";
	}
	found = fileName.find("jpg");
	if(found != string::npos && found == length-3)
	{
		return "jpg";
	}
	found = fileName.find("php");
	if(found != string::npos && found == length-3)
	{
		return "php";
	}
	found = fileName.find(".cgi");
	if(found != string::npos)
	{
		return "cgi";
	}
	
	found = fileName.find(".pl");
	if(found != string::npos && found == length-3)
	{
		return "pl";
	}
}

string filetypeHeader(string type)
{
	string ft = "";
	if(type == "html")
	{
		ft = "Content-type: text/html\n";
	} 
	else if(type == "gif")
	{
		ft = "Content-type: image/gif\n";
	} 
	else if(type == "jpg")
	{
		ft = "Content-type: image/jpg\n";
	} 
	else if(type == "txt")
	{
		ft = "Content-type: text/plain\n";
	}
	return ft;
}

int pop()
{
	int skt;
	sem_wait(&full);
	sem_wait(&mutex);
	skt = socketQueue.front();
	socketQueue.pop();
	sem_post(&mutex);
	sem_post(&empty);
	return skt;
}

void push(int skt)
{
	sem_wait(&empty);
	sem_wait(&mutex);
	socketQueue.push(skt);
	sem_post(&mutex);
	sem_post(&full);
}
