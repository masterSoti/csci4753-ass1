/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */

#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg)
{
  perror(msg);
  exit(1);
}

int stripHeader(char buf[BUFSIZE], char file[BUFSIZE - 100], int *index,
                int *totalPackets)
{
  int i;
  for (i = 0; i < BUFSIZE - 100; i++)
  {
    file[i] = buf[i];
  }
  char tmpstr[100];
  for (i = 0; i < 100; i++)
  {
    tmpstr[i] = buf[BUFSIZE - 100 + i];
  }
  *index = atoi(strtok(tmpstr, " "));
  *totalPackets = atoi(strtok(NULL, " "));
  char *sz = strtok(NULL, " ");
  if (sz)
  {
    return atoi(sz);
  }
  else
  {
    return 0;
  }
}

int reliablyPutFiles(int packetCounter, int totalPackets,
                     char filedata[][BUFSIZE - 100], int sockfd,
                     struct sockaddr_in clientaddr, int clientlen, char *buf,
                     int n, int *size)
{
  while (packetCounter < totalPackets)
  {
    bzero(buf, BUFSIZE);

    // if (packetCounter == totalPackets-1) {
    //   printf("%s\n", filedata[(packetCounter-1)*2]);
    // }
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr,
                 &clientlen);
    if (n < 0){
      break;
    }
    char data[BUFSIZE - 100];
    int index, _;
    int tmpsize = stripHeader(buf, data, &index, &_);
    if (index * 2 == (totalPackets * 2) - 2)
    {
      *size = tmpsize;
    }
    if (filedata[index * 2][0] == '\0') // TODO: Binary null? filedata[index*2]
    {
      packetCounter++;
      memcpy(filedata[index * 2], data, sizeof(filedata[index]));

      printf("Received %d of %d packets\n", packetCounter, totalPackets);
    }
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
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
             sizeof(int));
  
  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;

  // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
  //                sizeof(timeout)) < 0)
  //   error("setsockopt failed\n");

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
  if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
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
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr,
                 &clientlen);
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
    printf("server received datagram from %s (%s)\n", hostp->h_name, hostaddrp);

    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    printf("HERE\n");

    char *newBuf = strtok(buf, "\n");

    char *command = strtok(newBuf, " ");
    char *fileName = strtok(NULL, " ");
    if (!strcmp(command, "get"))
    {
      if (fileName)
      {
        FILE *fp;
        fp = fopen(fileName, "rb"); //change
        if (fp)
        {

          fseek(fp, 0, SEEK_END);
          long fsize = ftell(fp);
          fseek(fp, 0, SEEK_SET); // same as rewind(f);
          int sizeOfPackets = BUFSIZE - 100;
          int numPackets = (fsize / sizeOfPackets) + 1;
          int index = 0;

          while (index < numPackets)
          {
            char fileChunk[BUFSIZE];
            bzero(fileChunk, BUFSIZE);
            fread(fileChunk, 1, sizeOfPackets, fp);
            /* this is the string for the frame */
            char tmpstr[100];
            bzero(tmpstr, 100);
            char numbToStr[BUFSIZE];
            sprintf(numbToStr, "%d", index);
            strcat(tmpstr, numbToStr);
            strcat(tmpstr, " ");
            sprintf(numbToStr, "%d", numPackets);
            strcat(tmpstr, numbToStr);
            strcat(tmpstr, " ");

            if (index == numPackets - 1)
            {
              sprintf(numbToStr, "%d", fsize);
              strcat(tmpstr, numbToStr);
              strcat(tmpstr, " ");
            }
            fsize -= sizeOfPackets;

            int i;
            for (i = 0; i < 99; i++)
            {
              fileChunk[sizeOfPackets + i] = tmpstr[i];
            }
            n = sendto(sockfd, fileChunk, sizeof(fileChunk), 0,
                       (struct sockaddr *)&clientaddr, clientlen);
            if (n < 0)
              error("ERROR in sendto");
            index++;
            printf("Sent %d of %d packets\n", index, numPackets);
          }
          fclose(fp);
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
      n = sendto(sockfd, ack, strlen(ack), 0, (struct sockaddr *)&clientaddr,
                 clientlen);
      if (n < 0)
        error("ERROR in sendto");
      char getData[BUFSIZE];
      bzero(getData, BUFSIZE);
      n = recvfrom(sockfd, getData, BUFSIZE, 0, (struct sockaddr *)&clientaddr,
                   &clientlen);
      if (n < 0)
        error("ERROR in recvfrom");
      if (!strcmp(getData, "The file could not be opened\n") || !strcmp(getData, "Please enter a file name\n"))
      {
        continue;
      }
      char file[BUFSIZE - 100];
      int index, totalPackets;
      stripHeader(getData, file, &index, &totalPackets);
      char filedata[totalPackets * 2][BUFSIZE - 100];
      int i;
      for (i = 0; i < totalPackets * 2; i++)
      {
        memcpy(filedata[i], "\0", sizeof(filedata[i]));
      }
      memcpy(filedata[index * 2], file, sizeof(filedata[index]));
      int size;
      printf("Received %d of %d packets\n", 1, totalPackets);
      int packetCounter = reliablyPutFiles(1, totalPackets, filedata, sockfd,
                                           clientaddr, clientlen, getData, n, &size);
      int sanityCounter = 0;
      strcpy(buf, "sendAgain!");
      while (packetCounter < totalPackets && sanityCounter < 1000)
      {
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientaddr,
                   clientlen);
        if (n < 0)
          error("ERROR in sendto");
        packetCounter =
            reliablyPutFiles(packetCounter, totalPackets, filedata, sockfd,
                             clientaddr, clientlen, getData, n, &size);
        sanityCounter++;
      }
      strcpy(buf, "ack!");
      n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientaddr,
                   clientlen);
      FILE *fp;
      /* open the file for writing*/
      fp = fopen("putFile", "wb");
      /* write 10 lines of text into the file stream*/
      for (i = 0; i < (totalPackets * 2) - 1; i++)
      {

        if (i % 2 == 0 && i < (totalPackets * 2) - 2)
        {
          /* printf("%d:===================================\n", i); */
          fwrite(filedata[i], 1, BUFSIZE - 100, fp);
        }
      }
      fwrite(filedata[i - 1], 1, size, fp);

      /* fwrite(filedata[0], 1, strlen(filedata[i]), fp); */
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
      n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientaddr,
                 clientlen);
      if (n < 0)
        error("ERROR in sendto");
    }
    /**
     * get
     * put
     */
  }
}
