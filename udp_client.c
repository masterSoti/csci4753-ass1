/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg)
{
  perror(msg);
  exit(0);
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

void reliablySendFiles(int sizeOfPackets, int fsize, int sockfd, struct sockaddr_in serveraddr, int serverlen, FILE *fp)
{
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
    int n = sendto(sockfd, fileChunk, sizeof(fileChunk), 0,
                   (struct sockaddr *)&serveraddr, serverlen);
    if (n < 0)
      error("ERROR in sendto");
    index++;
    printf("Sent %d of %d packets\n", index, numPackets);
  }
}

int reliablyGetFiles(int packetCounter, int totalPackets,
                     char filedata[][BUFSIZE - 100], int sockfd,
                     struct sockaddr_in serveraddr, int serverlen, char *buf,
                     int n, int *size)
{
  while (packetCounter < totalPackets)
  {
    bzero(buf, BUFSIZE);

    // if (packetCounter == totalPackets-1) {
    //   printf("%s\n", filedata[(packetCounter-1)*2]);
    // }
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr,
                 &serverlen);
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
  // for (int i = 0; i < totalPackets; i++)
  // {
  //   printf("%s\n", filedata[i]);
  // }
  // printf("%s\n", filedata[1]);

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
  bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr,
        server->h_length);
  serveraddr.sin_port = htons(portno);

  
  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                 sizeof(timeout)) < 0)
    error("setsockopt failed\n");

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
    n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr,
               serverlen);
    if (n < 0)
      error("ERROR in sendto");

    if (!strcmp(command, "put"))
    {
      n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr,
                   &serverlen);

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
          fseek(fp, 0, SEEK_SET); // same as rewind(f);
          int sizeOfPackets = BUFSIZE - 100;

          reliablySendFiles(sizeOfPackets, fsize, sockfd, serveraddr, serverlen, fp);

          char *getData[BUFSIZE];
          n = recvfrom(sockfd, getData, BUFSIZE, 0, (struct sockaddr *)&serveraddr,
                       &serverlen);
          while (!strcmp("sendAgain!", getData) || n<0)
          {
            reliablySendFiles(sizeOfPackets, fsize, sockfd, serveraddr, serverlen, fp);
            n = recvfrom(sockfd, getData, BUFSIZE, 0, (struct sockaddr *)&serveraddr,
                         &serverlen);
          }

          fclose(fp);
        }
        else
        {
          printf("The file could not be opened\n");
          char *tmpstr = "err!";
          n = sendto(sockfd, tmpstr, sizeof(tmpstr), 0,
                     (struct sockaddr *)&serveraddr, serverlen);
          if (n < 0)
            error("ERROR in sendto");
        }
      }
      else
      {
        printf("Please enter a file name\n");
        char *tmpstr = "err!";
        n = sendto(sockfd, tmpstr, sizeof(tmpstr), 0,
                   (struct sockaddr *)&serveraddr, serverlen);
        if (n < 0)
          error("ERROR in sendto");
      }
    }
    else if (!strcmp(command, "get"))
    {
      char getData[BUFSIZE];
      bzero(getData, BUFSIZE);
      n = recvfrom(sockfd, getData, BUFSIZE, 0, (struct sockaddr *)&serveraddr,
                   &serverlen);
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
      int packetCounter = reliablyGetFiles(1, totalPackets, filedata, sockfd,
                                           serveraddr, serverlen, getData, n, &size);
      int sanityCounter = 0;
      while (packetCounter < totalPackets && sanityCounter < 1000)
      {
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr,
                   serverlen);
        if (n < 0)
          error("ERROR in sendto");
        packetCounter =
            reliablyGetFiles(packetCounter, totalPackets, filedata, sockfd,
                             serveraddr, serverlen, getData, n, &size);
        sanityCounter++;
      }
      FILE *fp;
      /* open the file for writing*/
      fp = fopen("getFile", "wb");
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
    else if (!strcmp(command, "exit\n"))
    {
      n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr,
                   &serverlen);
      if (n < 0)
        error("ERROR in recvfrom");
      printf("Echo from server: %s\n", buf);
      close(sockfd);
      break;
    }
    else
    {
      n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr,
                   &serverlen);
      if (n < 0)
        error("ERROR in recvfrom");
      printf("Echo from server: %s\n", buf);
    }
  }
  return 0;
}
