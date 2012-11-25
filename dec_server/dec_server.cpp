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
char events[26][26];
int portno;
extern char *optarg;
extern int optind;

FILE *logfile_fd = NULL;

int checkquery(int from, int to) {
	vector<char> que;
	char temp;
	int i, front = 0, back = 0, flag = 0;
	que.push_back(from);
	back++;
	while (front != back) {
		temp = que[front];
		front++;
		if (events[temp - 'A'][to - 'A'] == 1) {
			return 1;
			flag = 1;
		} else {
			for (i = 0; i < 26; i++) {
				if (events[temp - 'A'][i] == 1) {
					que.push_back(i + 'A');
					back++;
				}
			}
		}
	}
	if (flag == 0)
		return 0;
}

int addtograph(char from, char to) {
	int check = checkquery(from, to);
	if (check == 0) {
		check = checkquery(to, from);
		if (check == 0) {
			events[from - 'A'][to - 'A'] = 1;
			return 1;
		} else
			return 0;
	} else if (check == 1) {
		return 0;
	}
}
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
	char *tstr, command[BUFSIZE];
	gets(buf);
	i = 0;
	int len = strlen(buf), start = 0, j, k, end, action;
	while (i < len) {
		for (j = start; buf[j] != ';'; j++) {
		}
		end = j;
		for (j = start, k = 0; j < end; j++, k++)
			command[k] = buf[j];
		command[k] = '\0';
		start = end + 1;
		i = start;
		tstr = strtok(command, " ");
		if (strcmp(tstr, "insert") == 0)
			action = 1;
		else if (strcmp(tstr, "query") == 0)
			action = 2;
		else {
			cout << "Wrong Command!";
			usage();
		}
		tstr = strtok(NULL, " ");
		char from, to;
		int cnt = 0, add, check;
		while (tstr != NULL) {
			if (action == 1) {
				from = tstr[0];
				to = tstr[3];
				add = addtograph(from, to);
				if(add==0)
					cout<<"CONFLICT DETECTED.INSERT FAILED";
				else
					cout<<"INSERT DONE\n";
			} else if (action == 2) {
				if (cnt == 0) {
					from = tstr[0];
					cnt++;
				} else {
					to = tstr[0];
					check = checkquery(from, to);
					if (check == 0) {
						check = checkquery(to, from);
						if (check == 0) {
							cout << from << " concurrent to " << to;
						} else
							cout << to << " happened before " << from;
					} else if (check == 1) {
						cout<<from<<" happened before "<<to;
					}
				}
			}
			tstr = strtok(NULL, " ");
		}
	}

	return (0);
}

//print usage string and exit

void usage() {
	printf("dec_server [−h] [-p port-number] [−l file]\n");
	printf(	"−h : Print a usage summary with all options and exit."
			"-p port-number : Listen on the given port. If not provided, dec_server will listen on port 9090."
			"−l file : Log all requests and responses to the given file. If not, print all to stdout.\n");
	exit(1);
}
