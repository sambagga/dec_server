#define	BUF_LEN	8192
#include	<iostream>
#include	<vector>
#include	<map>
#include	<stdio.h>
#include 	<syslog.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/time.h>
#include	<time.h>

using namespace std;
#define BUFSIZE 1024
void usage();

int portno;
extern char *optarg;
extern int optind;

FILE *logfile_fd = NULL;

int main(int argc, char *argv[]) {
	struct timeval optst, optend;
	char logfile[256];
	char buf[BUF_LEN];
	int lfile = 0, ch;
	portno = 5;
	//check various options
	while ((ch = getopt(argc, argv, "hp:l:")) != -1)
		switch (ch) {
		case 'p': //port no
			portno = atoi(optarg);
			break;
		case 'l': //address of log file
			lfile = 1;
			strncpy(logfile, optarg, sizeof(logfile));
			break;
		case 'h': //help
		default:
			usage();
			break;
		}
	argc -= optind;
	if (argc != 0)
		usage();
	if (portno < 1) {
		cout << "\nInvalid port no!";
		usage();
	}
	int i = 0, temp;
	//Open the file to log request and responses
	if (lfile == 1) {
		logfile_fd = fopen(logfile, "r");

		fclose(logfile_fd);
	}
	char *tstr,command[BUFSIZE];
	gets(buf);
	i = 0;
	int len = strlen(buf),start=0,j,k,end,action;
	while (i < len) {
		for(j=start;buf[j]!=';';j++){}
		end=j;
		for(j=start,k=0;j<end;j++,k++)
			command[k]=end[j];
		command[k]='\0';
		start=end+1;
		i=start;
		tstr = strtok(command, " ");
		if(strcmp(tstr,"insert")==0)
			action=1;
		else if(strcmp(tstr,"query")==0)
			action=2;
		else{
			cout<<"Wrong Command!";
			usage();
		}
		while (tstr != NULL) {
			if(action==1){

			}
			else if(action==2){

			}
			tstr = strtok(NULL, " ");
		}
	}


	return (0);
}

//print usage string and exit

void usage() {
	printf("dec_server [−h] [-p port-number] [−l file]\n");
	printf(
			"−h : Print a usage summary with all options and exit."
					"-p port-number : Listen on the given port. If not provided, dec_server will listen on port 9090."
					"−l file : Log all requests and responses to the given file. If not, print all to stdout.\n");
	exit(1);
}
