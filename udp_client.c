/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(0);
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

int reliablyGetFiles(int packetCounter, int totalPackets, char filedata[][BUFSIZE], int sockfd, struct sockaddr_in serveraddr, int serverlen, char *buf, int n)
{
    int msec = 0, trigger = 30000; /* 30s */
    clock_t before = clock();
    while (packetCounter < totalPackets && msec < trigger)
    {
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
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
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    while (1)
    {

        /* get a message from the user */
        bzero(buf, BUFSIZE);
        printf("Please enter msg: ");
        fgets(buf, BUFSIZE, stdin);

        /* send the message to the server */
        serverlen = sizeof(serveraddr);
        /* print the server's reply */
        char newBuf[strlen(buf)];
        strcpy(newBuf, buf);

        char *command = strtok(newBuf, " ");
        char *fileName = strtok(NULL, " ");
        if (!strcmp(command, "put"))
        {
            if (fileName == NULL)
            {
                printf("Please enter a file name\n");
                continue;
            }
        }
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
        if (n < 0)
            error("ERROR in sendto");

        if (!strcmp(command, "put"))
        {
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);

            if (n < 0)
                error("ERROR in recvfrom");

            if (fileName)
            {
                FILE *fp;
                fileName[strlen(fileName) - 1] = '\0';
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
                                   (struct sockaddr *)&serveraddr, serverlen);
                        if (n < 0)
                            error("ERROR in sendto");
                        index++;
                    }

                    // Listen for acknowledgement
                    // do it again
                }
                else
                {
                    printf("The file could not be opened\n");
                    char * tmpstr = "err!";
                    n = sendto(sockfd, tmpstr, sizeof(tmpstr), 0,
                                   (struct sockaddr *)&serveraddr, serverlen);
                    if (n < 0)
                        error("ERROR in sendto");
                }
            }
            else
            {
                printf("Please enter a file name\n");
                char * tmpstr = "err!";
                n = sendto(sockfd, tmpstr, sizeof(tmpstr), 0,
                                   (struct sockaddr *)&serveraddr, serverlen);
                if (n < 0)
                    error("ERROR in sendto");
            }
        }
        else if (!strcmp(command, "get"))
        {
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");
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

            packetCounter = reliablyGetFiles(packetCounter, totalPackets, filedata, sockfd, serveraddr, serverlen, buf, n);
            int sanityCounter = 0;
            while (packetCounter != totalPackets && sanityCounter < 1000)
            {
                n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
                if (n < 0)
                    error("ERROR in sendto");
                packetCounter = reliablyGetFiles(packetCounter, totalPackets, filedata, sockfd, serveraddr, serverlen, buf, n);
                sanityCounter++;
            }
            FILE *fp;

            /* open the file for writing*/
            fp = fopen("/tmp/testFile_server", "w");
            /* write 10 lines of text into the file stream*/
            for (i = 0; i < totalPackets; i++)
            {
                fwrite(filedata[i], 1, strlen(filedata[i]), fp);
            }
            /* close the file*/
            fclose(fp);
        }
        else if (!strcmp(command, "exit\n"))
        {
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");
            printf("Echo from server: %s\n", buf);
            close(sockfd);
            break;
        }
        else
        {
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR in recvfrom");
            printf("Echo from server: %s\n", buf);
        }
    }
    return 0;
}