#include <stdio.h>
#include <sys/types.h>  // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h> // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h> // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>  //O_WRONLY
#include <unistd.h> //write(), close()
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include "LRULinkedList.h"
#include <pthread.h>

#define MAX_THREAD 1024
void error(char *);
char *getCurrentTime();

pthread_mutex_t cache_mutex;
pthread_mutex_t log_mutex;

LRU cache;
typedef struct thread_info{
    int client_fd;
    struct sockaddr_in client_addr;
} THREAD_INFO;

void *thread_proxy(void *arg);
void set_info(THREAD_INFO *, struct sockaddr_in);

int main(int argc, char *argv[])
{
    int proxy_fd, client_fd, host_fd;
    int portno;

    struct sockaddr_in proxy_addr, client_addr;

    socklen_t client_len;

    pthread_t thread_arr[MAX_THREAD];
    int thread_i=0;

    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    //클라이언트로 부터 요청을 받을 소켓 생성
    proxy_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (proxy_fd < 0)
        error("ERROR on opening socket");
    bzero((char *)&proxy_addr, sizeof(proxy_addr));
    portno = atoi(argv[1]); //portno를 넣어줌

    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    proxy_addr.sin_port = htons(portno); //host에서 network byte order로 전환

    if (bind(proxy_fd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) //socket에 ip, port 할당
        error("ERROR on binding");

    //커널에 개통 요청
    listen(proxy_fd, 5);
    client_len = sizeof(client_addr);

    init(&cache);

    while (1)
    {
        THREAD_INFO thread_info;
        fprintf(stdout, "\n\n############# Request Waiting ##############\n");
        client_fd = accept(proxy_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
            error("ERROR on accpet");
        thread_info.client_fd = client_fd;
        set_info(&thread_info, client_addr);
        if (pthread_create(&thread_arr[thread_i], NULL, thread_proxy, (void *)&thread_info) != 0) error("pthread_create()");
        pthread_detach(thread_arr[thread_i]);
        thread_i++;
        thread_i %= MAX_THREAD;
    }
    return 0;
}
void error(char *msg)
{
    perror(msg);
    exit(1);
}

char *getCurrentTime()
{
    time_t mytime;
    mytime = time(NULL);
    char *returnTime = ctime(&mytime);
    returnTime[strlen(returnTime) - 1] = '\0';
    return returnTime;
}

void set_info(THREAD_INFO *th_info, struct sockaddr_in client_addr){
    th_info->client_addr.sin_family = client_addr.sin_family;
    th_info->client_addr.sin_port = client_addr.sin_port;
    th_info->client_addr.sin_addr.s_addr = client_addr.sin_addr.s_addr;
}

void *thread_proxy(void *arg)
{
    char method[20];
    char buf[BUFFER_SIZE*2];
    int i = 0;
    struct sockaddr_in host_addr;
    int client_fd, host_fd, n, len;

    memset(buf, '\0', BUFFER_SIZE*2);

    client_fd =((THREAD_INFO*)arg)->client_fd;
    if ( (len =read(client_fd, buf, sizeof(buf)))<= 0){close(client_fd); pthread_exit(NULL);}
    fprintf(stdout, "############## Request message ##############\n");
    fprintf(stdout, "%s\n", buf);
    for(i = 0; buf[i] != ' '; i++) method[i] = buf[i];
    char *token, *cmd;
    cmd = strtok(buf, " ");
    token = strtok(NULL, " ");
    char *request_addr;
    request_addr = malloc(BUFFER_SIZE);
    memset(request_addr, '\0', BUFFER_SIZE);
    sprintf(request_addr, "%s", token);

    char *host_url;
    host_url = malloc(BUFFER_SIZE);
    memset(host_url, '\0', BUFFER_SIZE);
    strtok(token, "//");
    host_url = strtok(NULL, "//");

    char requestMsgToHost[BUFFER_SIZE * 20];

    if(strcmp(cmd,"GET") == 0){
        sprintf(requestMsgToHost, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", request_addr,host_url);
        printf("\n$$$$$$$ RequestMsgToEndServer $$$$$$$\n%s", requestMsgToHost);

        pthread_mutex_lock(&cache_mutex);
        Node *cachedData = search_url(&cache, request_addr);
        pthread_mutex_unlock(&cache_mutex);

        char *cacheURL = malloc(sizeof(char) * BUFFER_SIZE*2);
        char *cacheData = malloc(sizeof(char) * MAX_OBJECT_SIZE);

        memset(cacheURL, '\0', BUFFER_SIZE*2);
        memset(cacheData, '\0', MAX_OBJECT_SIZE);

        int dataSize = 0;

        if (cachedData != NULL)
        {
            printf("=============HIT=============\n");
            printf("\nHit URL: %s \n", cachedData->url);
            printf("\n=============================\n\n");

            pthread_mutex_lock(&cache_mutex);
            if ((n = write(client_fd, cachedData->object, cachedData->dataSize)) < 0)
            {
                error("ERROR on Cache Write ");
            }
            pthread_mutex_unlock(&cache_mutex);
        }
        else
        {
            int i = 0;
            printf("=============NO HIT=============\n");
            host_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (host_fd < 0)
                error("Error on opening socket");

            struct hostent *host_entry = gethostbyname(host_url);
            if (host_entry == NULL)
            {
                perror("ERROR on gethostbyname");
                pthread_exit(0);
            }

            bzero((char *)&host_addr, sizeof(host_addr));
            host_addr.sin_family = AF_INET;
            memmove(&host_addr.sin_addr.s_addr, host_entry->h_addr, host_entry->h_length);
            host_addr.sin_port = htons(80); //host에서 network byte order로 전환

            if (connect(host_fd, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0) //establish a connection to the server
            {   perror("ERROR on connecting");
                pthread_exit(NULL);
            }
            n = write(host_fd, requestMsgToHost, strlen(requestMsgToHost));

            if (n < 0)
                error("ERROR writing on socket");
            char read_buf[10240];
            memset(read_buf, '\0', BUFFER_SIZE*10);

            while ((n = read(host_fd, read_buf, BUFFER_SIZE * 5)) > 0)
            {
                dataSize += write(client_fd, read_buf, n);
                if (dataSize <= MAX_OBJECT_SIZE){
                    strcat(cacheData, read_buf);
                }
                memset(read_buf, '\0', BUFFER_SIZE * 10);
            }
            if (dataSize <= MAX_OBJECT_SIZE)
            {
                pthread_mutex_lock(&cache_mutex);
                printf("\n####################Data enters in Cache####################\n\n");
                Node *temp = newNode(request_addr, cacheData, dataSize);
                addLast(&cache, temp);
                printf("%s %i\n", request_addr, dataSize);
                printf("\n############################################################\n\n");
                pthread_mutex_unlock(&cache_mutex);
            }
        }
        pthread_mutex_lock(&cache_mutex);
        printf("##################### Least Recently-Used #####################\n\n");
        print_list(&cache);
        printf("\n##################### ################### #####################\n\n");
        pthread_mutex_unlock(&cache_mutex);

        int ip_Addr = ((THREAD_INFO *)arg)->client_addr.sin_addr.s_addr;
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_Addr, clientIP, INET_ADDRSTRLEN); //2진수 IP를 10진수 IP주소로 변환하는 함수 clientIP에 저장된다.

        char logContent[BUFFER_SIZE];
        memset(logContent, '\0', BUFFER_SIZE);
        sprintf(logContent, "%s %s %s %d\n", getCurrentTime(), clientIP, request_addr, dataSize);

        printf("%s", logContent);

        pthread_mutex_lock(&log_mutex);
        int proxyLog;
        if ((proxyLog = open("proxy.log", O_CREAT | O_RDWR | O_APPEND, 0644)) < 0)
            error("ERROR on file ");
        n = write(proxyLog, logContent, sizeof(logContent));
        if (n < 0)
            error("ERROR on file write ");

        pthread_mutex_unlock(&log_mutex);
        fprintf(stdout, "\n#######################################################\n");

        close(proxyLog);
        close(host_fd);
    }
    close(client_fd);

    pthread_exit(0);
        
}
