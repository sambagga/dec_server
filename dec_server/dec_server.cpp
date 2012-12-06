#define	BUF_LEN	8192
#include	<iostream>
#include	<vector>
#include	<map>
#include	<stdio.h>
#include 	<syslog.h>
#include	<unistd.h>
#include	<dirent.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include 	<termios.h>
#include	<fcntl.h>
#include	<assert.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<inttypes.h>
#include	<pthread.h>
#include	<semaphore.h>
#include	<time.h>
#include 	<arpa/inet.h>
#include 	<sys/stat.h>

using namespace std;
#define BUFSIZE 1024
void usage();
void setup_server();
char events[26][26];
char *port = "9090", *progname;
int s, sock, ch, server, done;
char *host = NULL;
extern char *optarg;
extern int optind;
int lfile = 0;
FILE *logfile_fd = NULL;
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}
int checkquery(int from, int to, char tevents[26][26]) {
	vector<char> que;
	char temp;
	int i, front = 0, back = 0;
	que.push_back(from);
	back++;
	while (front != back) {
		temp = que[front];
		front++;
		if (tevents[temp - 'A'][to - 'A'] == 1)
			return 1;
		else {
			for (i = 0; i < 26; i++) {
				if (tevents[temp - 'A'][i] == 1) {
					que.push_back(i + 'A');
					back++;
				}
			}
		}
	}
	que.clear();
	return 0;
}

int addtograph(char from, char to, char tevents[26][26]) {
	int check = checkquery(from, to, tevents);
	if (check == 0) {
		check = checkquery(to, from, tevents);
		if (check == 0) {
			tevents[from - 'A'][to - 'A'] = 1;
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
	int ch;
	//check various options
	while ((ch = getopt(argc, argv, "hp:l:")) != -1)
		switch (ch) {
		case 'p': //port no
			port = optarg;
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
	int i = 0, temp;
	//Open the file to log request and responses
	if (lfile == 1) {
		logfile_fd = fopen(logfile, "r");

		fclose(logfile_fd);
	}
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	setup_server();
	return (0);
}

/*
 * setup_server() - set up socket for mode of soc running as a server.
 */

void setup_server() {
	char tevents[26][26];
	struct sockaddr_in serv;
	struct servent *se;
	socklen_t len;
	fd_set ready;
	struct sockaddr_in msgfrom;
	socklen_t msgsize;
	union {
		uint32_t addr;
		char bytes[4];
	} fromaddr;
	char buf[BUF_LEN], out_buf[BUF_LEN];

	fd_set master; // master file descriptor list
	fd_set read_fds; // temp file descriptor list for select()
	int fdmax; // maximum file descriptor number

	int sock; // newly accept()ed socket descriptor
	struct sockaddr_storage remote; // client address
	int nbytes;

	char remoteIP[INET6_ADDRSTRLEN];

	int yes = 1; // for setsockopt() SO_REUSEADDR, below
	int i, j, k, rv, add = 0;

	struct addrinfo hints, *ai, *p;

	FD_ZERO(&master);
	// clear the master and temp sets
	FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}

	for (p = ai; p != NULL; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (s < 0) {
			continue;
		}

		// lose the pesky "address already in use" error message
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(s, p->ai_addr, p->ai_addrlen) < 0) {
			close(s);
			continue;
		}

		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL) {
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this
	//fprintf(stderr, "Port number is %d\n", ntohs(remote.sin_port));

	// listen
	if (listen(s, 10) == -1) {
		perror("listen");
		exit(3);
	}

	// add the listener to the master set
	FD_SET(s, &master);

	// keep track of the biggest file descriptor
	fdmax = s; // so far, it's this one

	// main loop
	for (;;) {
		read_fds = master; // copy it
		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}

		// run through the existing connections looking for data to read
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				if (i == s) {
					// handle new connections
					len = sizeof remote;
					sock = accept(s, (struct sockaddr *) &remote, &len);

					if (sock == -1) {
						perror("accept");
					} else {
						FD_SET(sock, &master);
						// add to master set
						if (sock > fdmax) { // keep track of the max
							fdmax = sock;
						}
						printf(
								"selectserver: new connection from %s on "
										"socket %d\n",
								inet_ntop(remote.ss_family,
										get_in_addr((struct sockaddr*) &remote),
										remoteIP, INET6_ADDRSTRLEN), sock);
					}
				} else {
					// handle data from a client
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						// got error or connection closed by client
						if (nbytes == 0) {
							// connection closed
							printf("selectserver: socket %d hung up\n", i);
						} else {
							perror("recv");
						}
						close(i); // bye!
						FD_CLR(i, &master);
						// remove from master set
					} else {
						// we got some data from a client
						write(fileno(stdout), buf, nbytes);
						char *tstr, command[BUFSIZE];
						//gets(buf);
						int len, start = 0, q, end, action;
						for (q = 0;
								buf[q] != '\r' && buf[q] != '\n'
										&& buf[q] != '\0'; q++) {
						}
						len = q;
						q = 0;
						for (j = 0; j < 26; j++) {
							for (k = 0; k < 26; k++) {
								tevents[j][k] = events[j][k];
							}
						}
						char *bound;
						while (start < len - 1) {
							bound = strchr(buf, ';');
							if (bound != NULL) {
								for (q = start; buf[q] != ';'; q++) {
								}
								end = q;
								for (q = start, k = 0; q < end; q++, k++)
									command[k] = buf[q];
								command[k] = '\0';
								start = end + 1;
								tstr = strtok(command, " ");
								if (strcmp(tstr, "insert") == 0)
									action = 1;
								else if (strcmp(tstr, "query") == 0)
									action = 2;
								else if (strcmp(tstr, "reset") == 0)
									action = 3;
								else {
									sprintf(out_buf, "Wrong Command!");
									send(i, out_buf, strlen(out_buf), 0);
									close(i);
									FD_CLR(i, &master);
									break;
								}
								tstr = strtok(NULL, " ");
								char from, to;
								int cnt = 0, check;

								while (tstr != NULL) {
									if (action == 1) {
										from = tstr[0];
										to = tstr[3];
										add = addtograph(from, to, tevents);
										if (add == 0) {
											sprintf(
													out_buf,
													"CONFLICT DETECTED.INSERT FAILED");
											send(i, out_buf, strlen(out_buf),
													0);
											write(fileno(stdout), out_buf,
													strlen(out_buf));
											break;
										} else {
											sprintf(out_buf, "INSERT DONE");
										}
										write(fileno(stdout), out_buf,
												strlen(out_buf));
									} else if (action == 2) {
										if (cnt == 0) {
											from = tstr[0];
											cnt++;
										} else {
											to = tstr[0];
											check = checkquery(from, to,
													tevents);
											if (check == 0) {
												check = checkquery(to, from,
														tevents);
												if (check == 0) {
													sprintf(
															out_buf,
															"%c concurrent to %c",
															from, to);
												} else
													sprintf(
															out_buf,
															"%c happened before %c",
															to, from);
											} else if (check == 1) {
												sprintf(out_buf,
														"%c happened before %c",
														from, to);
											}
											send(i, out_buf, strlen(out_buf),
													0);
											write(fileno(stdout), out_buf,
													strlen(out_buf));
										}
									} else if (action == 3) {
										for (i = 0; i < 26; i++)
											for (j = 0; j < 26; j++)
												events[i][j] = 0;
									}
									tstr = strtok(NULL, " ");
								}
							} else {
								sprintf(out_buf, "Wrong Command!");
								send(i, out_buf, strlen(out_buf), 0);
								close(i);
								FD_CLR(i, &master);
								break;
							}

							if (add == 0)
								break;
						}
						if (add != 0) {
							for (j = 0; j < 26; j++) {
								for (k = 0; k < 26; k++) {
									events[j][k] = tevents[j][k];
								}
							}
						}
					}
				} // END handle data from client insert E->F A->B B->C C->D; query B F;
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!

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
