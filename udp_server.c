/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

/** 
 * The following function is from this website
 * https://www.programmingsimplified.com/c/source-code/c-substring
 */
void substring(char s[], char sub[], int p, int l)
{
    int c = 0;

    while (c < l)
    {
        sub[c] = s[p + c];
        c++;
    }
    sub[c] = '\0';
}

int reliablyPutFiles(int packetCounter, int totalPackets, char filedata[][BUFSIZE], int sockfd, struct sockaddr_in clientaddr, int clientlen, char *buf, int n)
{
    int msec = 0, trigger = 30000; /* 30s */
    clock_t before = clock();
    while (packetCounter < totalPackets && msec < trigger)
    {
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");
        char toTokenize[strlen(buf)];
        strcpy(toTokenize, buf);
        int index = atoi(strtok(toTokenize, " "));
        strtok(NULL, " ");
        if (!strlen(filedata[index]))
        {
            packetCounter++;
            char *data = strtok(NULL, "");
            strcpy(filedata[index], data);
            printf("Received %d of %d packets\n", packetCounter, totalPackets);
        }
        clock_t difference = clock() - before;
        msec = difference * 1000 / CLOCKS_PER_SEC;
    }
    return packetCounter;
}

int main(int argc, char **argv)
{
    int sockfd;                    /* socket */
    int portno;                    /* port to listen on */
    int clientlen;                 /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp;         /* client host info */
    char buf[BUFSIZE];             /* message buf */
    char *hostaddrp;               /* dotted decimal host addr string */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */

    /* 
   * check command line arguments 
   */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /* 
   * socket: create the parent socket 
   */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval, sizeof(int));

    /*
   * build the server's Internet address
   */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
   * bind: associate the parent socket with a port 
   */
    if (bind(sockfd, (struct sockaddr *)&serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /* 
   * main loop: wait for a datagram, then echo it
   */
    clientlen = sizeof(clientaddr);
    while (1)
    {

        /*
     * recvfrom: receive a UDP datagram from a client
     */
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *)&clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");

        /* 
     * gethostbyaddr: determine who sent the datagram
     */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                              sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        printf("server received datagram from %s (%s)\n",
               hostp->h_name, hostaddrp);

        printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);

        char *newBuf = strtok(buf, "\n");

        char *command = strtok(newBuf, " ");
        char *fileName = strtok(NULL, " ");
        if (!strcmp(command, "get"))
        {
            if (fileName)
            {
                FILE *fp;
                fp = fopen(fileName, "rb");
                if (fp)
                {

                    fseek(fp, 0, SEEK_END);
                    long fsize = ftell(fp);
                    fseek(fp, 0, SEEK_SET); //same as rewind(f);
                    char fileChunk[fsize + 1];
                    fread(fileChunk, 1, fsize, fp);
                    fclose(fp);
                    fileChunk[fsize] = '\0';
                    int sizeOfPackets = BUFSIZE - 100;
                    int numPackets = (fsize / sizeOfPackets) + 1;
                    int index = 0;


                    while (index < numPackets)
                    {
                        char tmpSubStr[BUFSIZE];
                        char tmpstr[BUFSIZE];
                        bzero(tmpstr, BUFSIZE);
                        substring(fileChunk, tmpSubStr, index * sizeOfPackets, sizeOfPackets);
                        char numbToStr[BUFSIZE];
                        sprintf(numbToStr, "%d", index);
                        strcat(tmpstr, numbToStr);
                        strcat(tmpstr, " ");
                        sprintf(numbToStr, "%d", numPackets);
                        strcat(tmpstr, numbToStr);
                        strcat(tmpstr, " ");
                        strcat(tmpstr, tmpSubStr);
                        n = sendto(sockfd, tmpstr, sizeof(tmpstr), 0,
                                   (struct sockaddr *)&clientaddr, clientlen);
                        if (n < 0)
                            error("ERROR in sendto");
                        index++;
                        printf("Sent %d of %d packets\n", index, numPackets);
                    }
                }
                else
                {
                    char *errMsg = "The file could not be opened\n";
                    n = sendto(sockfd, errMsg, strlen(errMsg), 0,
                               (struct sockaddr *)&clientaddr, clientlen);
                    if (n < 0)
                        error("ERROR in sendto");
                }
            }
            else
            {
                char *errMsg = "Please enter a file name\n";
                n = sendto(sockfd, errMsg, strlen(errMsg), 0,
                           (struct sockaddr *)&clientaddr, clientlen);
                if (n < 0)
                    error("ERROR in sendto");
            }
        }
        else if (!strcmp(command, "put"))
        {
            char *ack = "ACK!\n";
            n = sendto(sockfd, ack, strlen(ack), 0,
                        (struct sockaddr *)&clientaddr, clientlen);
            if (n < 0)
                error("ERROR in sendto");

            n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *)&clientaddr, &clientlen);
            if (n < 0)
                error("ERROR in recvfrom");
            if (!strcmp(buf, "err!")){
                continue;
            }
            char toSplitString[strlen(buf)];
            strcpy(toSplitString, buf);
            char *indexString = strtok(toSplitString, " ");
            char *totalPacketsString = strtok(NULL, " ");
            int packetCounter = 1;
            int totalPackets = atoi(totalPacketsString);
            char filedata[totalPackets][BUFSIZE];
            int i;
            for (i = 0; i < totalPackets; i++)
            {
                strcpy(filedata[i], "");
            }
            strcpy(filedata[atoi(indexString)], strtok(NULL, ""));
            printf("Received %d of %d packets\n", packetCounter, totalPackets);

            packetCounter = reliablyPutFiles(packetCounter, totalPackets, filedata, sockfd, clientaddr, clientlen, buf, n);
            int sanityCounter = 0;
            while (packetCounter != totalPackets && sanityCounter < 1000)
            {
                n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientaddr, clientlen);
                if (n < 0)
                    error("ERROR in sendto");
                packetCounter = reliablyPutFiles(packetCounter, totalPackets, filedata, sockfd, clientaddr, clientlen, buf, n);
                sanityCounter++;
            }

            FILE *fp;
            /* open the file for writing*/
            fp = fopen("/tmp/testFile_put", "w");

            /* write 10 lines of text into the file stream*/
            for (i = 0; i < totalPackets; i++)
            {
                fwrite(filedata[i], 1, strlen(filedata[i]), fp);
            }
            /* close the file*/
            fclose(fp);
                        
        }
        else if (!strcmp(command, "ls"))
        {
            DIR *dp;
            struct dirent *ep;
            dp = opendir("./");

            if (dp != NULL)
            {
                int counter = 0;
                while (ep = readdir(dp))
                {
                    counter += strlen(ep->d_name) + 1;
                }
                char listOfFiles[counter + 2];
                rewinddir(dp);
                while (ep = readdir(dp))
                {
                    strcat(listOfFiles, ep->d_name);
                    strcat(listOfFiles, " ");
                }
                strcat(listOfFiles, "\n");
                (void)closedir(dp);
                n = sendto(sockfd, listOfFiles, strlen(listOfFiles), 0,
                           (struct sockaddr *)&clientaddr, clientlen);
                if (n < 0)
                    error("ERROR in sendto");
            }
            else
            {
                char *listOfFiles = "The directory can not be opened\n";
                n = sendto(sockfd, listOfFiles, strlen(listOfFiles), 0,
                           (struct sockaddr *)&clientaddr, clientlen);
                if (n < 0)
                    error("ERROR in sendto");
            }
        }
        else if (!strcmp(command, "delete"))
        {
            char *messageToReturn;
            if (fileName)
            {
                if (remove(fileName) == 0)
                {
                    messageToReturn = "Deleted the specified file\n";
                }
                else
                {
                    messageToReturn = "Could not delete the specified file\n";
                }
            }
            else
            {
                messageToReturn = "Please specify a fileName\n";
            }
            n = sendto(sockfd, messageToReturn, strlen(messageToReturn), 0,
                       (struct sockaddr *)&clientaddr, clientlen);
            if (n < 0)
                error("ERROR in sendto");
        }
        else if (!strcmp(command, "exit"))
        {
            char *toReturn = "Exit Called\n";
            printf("%s\n", toReturn);
            n = sendto(sockfd, toReturn, strlen(toReturn), 0,
                       (struct sockaddr *)&clientaddr, clientlen);
            if (n < 0)
                error("ERROR in sendto");
            close(sockfd);
            bzero(buf, BUFSIZE);
            break;
        }
        else
        {
            printf("none of these\n");
            /* 
       * sendto: echo the input back to the client 
       */
            n = sendto(sockfd, buf, strlen(buf), 0,
                       (struct sockaddr *)&clientaddr, clientlen);
            if (n < 0)
                error("ERROR in sendto");
        }
        /**
     * get
     * put
     */
    }
}
