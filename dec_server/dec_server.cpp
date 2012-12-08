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
#include 	<arpa/inet.h>
#include 	<sys/stat.h>

using namespace std;
#define BUFSIZE 1024
void usage(); // help function
void server_handle();
char events[26][26], file_buf[BUF_LEN], logfile[256];
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
//to print in log file
void log_status() {
	static int lcount = 0;
	if (lcount == 0) {
		logfile_fd = fopen(logfile, "w");
		lcount++;
	} else
		logfile_fd = fopen(logfile, "a");
	if (!logfile_fd) {
		perror("Error in opening log file");
	}
	fprintf(logfile_fd, "%s", file_buf);
	fclose(logfile_fd);
}
//check query for happened before relationship using Breadth First Search in adjacency matrix
int checkquery(int from, int to, char tevents[26][26]) {
	vector<char> que; //queue to keep track of BFS
	char temp;
	int i, front = 0, back = 0;
	que.push_back(from); //initialize queue
	back++;
	while (front != back) {
		temp = que[front]; //remove from front of queue
		front++;
		if (tevents[temp - 'A'][to - 'A'] == 1) //if found
			return 1;
		else {
			for (i = 0; i < 26; i++) { //push all the adjacent nodes in queue
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
//add event to graph(adjacency matrix)
int addtograph(char from, char to, char tevents[26][26]) {
	int check;
	check = checkquery(to, from, tevents);
	if (check == 0) {
		tevents[from - 'A'][to - 'A'] = 1;
		return 1;
	} else
		return 0;
}
int main(int argc, char *argv[]) {
	struct timeval optst, optend;
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
		logfile_fd = fopen(logfile, "w");
	}
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	printf("Waiting for connections\n");
	server_handle();
	return (0);
}

/*
 * server_handle() - set up server, accept client and process requests for event ordering
 */

void server_handle() {
	char tevents[26][26]; //temporary matrix to hold events while processing request
	struct sockaddr_in serv;
	struct servent *se;
	socklen_t len;
	fd_set ready;
	struct sockaddr_in msgfrom;
	socklen_t msgsize;
	struct in_addr ipv4addr;
	hostent *clientName;
	union {
		uint32_t addr;
		char bytes[4];
	} fromaddr;
	char buf[BUF_LEN], porint_buf[BUF_LEN], out_buf[BUF_LEN],
			clients[200][BUF_LEN], req_buf[BUF_LEN];

	fd_set master; // master file descriptor list
	fd_set read_fds; // temp file descriptor list for select()
	int fdmax; // maximum file descriptor number

	int sock; // newly accept()ed socket descriptor
	struct sockaddr_storage remote; // client address
	int nbytes;
	char remoteIP[INET6_ADDRSTRLEN];

	int yes = 1; // for setsockopt() SO_REUSEADDR, below
	int i, j, k, rv, add = 0, flagfrom, flagto;

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

		// close the and reuse the address if already in use
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(s, p->ai_addr, p->ai_addrlen) < 0) {
			close(s);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "Failed to bind\n");
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
								"New connection from %s on "
										"socket %d\n",
								inet_ntop(remote.ss_family,
										get_in_addr((struct sockaddr*) &remote),
										remoteIP, INET6_ADDRSTRLEN), sock);
						inet_pton(AF_INET, remoteIP, &ipv4addr);
						clientName = gethostbyaddr(&ipv4addr, sizeof ipv4addr,
								AF_INET);

						strcpy(clients[sock], clientName->h_name);
					}
				} else {
					// handle data from a client
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						// got error or connection closed by client
						if (nbytes == 0) {
							// connection closed
							printf("Socket %d hung up\n", i);
						} else {
							perror("recv");
						}
						close(i); // close the connection
						FD_CLR(i, &master);
						// remove from master set
					} else {
						// we got some data from a client
						if (lfile == 1)
							strcat(file_buf, buf);
						else
							write(fileno(stdout), buf, nbytes);
						char *tstr, command[BUFSIZE];
						//gets(buf);
						int len, start = 0, q, end, action;
						for (q = 0; buf[q] != '\r' && buf[q] != '\n' // read request
						&& buf[q] != '\0'; q++) {
						}
						len = q;
						q = 0;
						// initialize temporary matrix to events
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
								//get first command in a request
								for (q = start, k = 0; q < end; q++, k++)
									command[k] = buf[q];
								command[k] = '\0';
								sprintf(req_buf,
										"request received from %s:%s\n",
										clients[i], command);
								if (lfile == 1)
									strcpy(file_buf, req_buf);
								else
									write(fileno(stdout), req_buf,
											strlen(req_buf));
								start = end + 1;
								tstr = strtok(command, " ;"); //get the command type
								if (strcmp(tstr, "insert") == 0)
									action = 1;
								else if (strcmp(tstr, "query") == 0)
									action = 2;
								else if (strcmp(tstr, "reset") == 0) {
									action = 3;
									// reset matrices
									for (k = 0; k < 26; k++) {
										for (j = 0; j < 26; j++) {
											events[k][j] = 0;
											tevents[k][j] = 0;
										}
									}
									sprintf(out_buf, "RESET DONE\n");
									send(i, out_buf, strlen(out_buf), 0);
									if (lfile == 1)
										strcat(file_buf, out_buf);
									else
										write(fileno(stdout), out_buf,
												strlen(out_buf));
								} else {
									//if command type not specified or illegal treated as Wrong command
									sprintf(out_buf, "Wrong Command!\n");
									send(i, out_buf, strlen(out_buf), 0);
									if (lfile == 1)
										strcat(file_buf, out_buf);
									else
										write(fileno(stdout), out_buf,
												strlen(out_buf));
									close(i);
									FD_CLR(i, &master);
									break;
								}
								tstr = strtok(NULL, " ");
								char from, to;
								int cnt = 0, check;

								while (tstr != NULL) {
									//insert request
									if (action == 1) {
										//get events from and to
										from = tstr[0];
										to = tstr[3];
										add = addtograph(from, to, tevents);
										if (add == 0) {
											sprintf(
													out_buf,
													"CONFLICT DETECTED.INSERT FAILED\n");
											send(i, out_buf, strlen(out_buf),
													0);
											if (lfile == 1)
												strcat(file_buf, out_buf);
											else
												write(fileno(stdout), out_buf,
														strlen(out_buf));
											break;
										} else {
											sprintf(out_buf, "INSERT DONE\n");
										}
										send(i, out_buf, strlen(out_buf), 0);
										if (lfile == 1)
											strcat(file_buf, out_buf);
										else
											write(fileno(stdout), out_buf,
													strlen(out_buf));
									} else if (action == 2) { //query for order of events
										if (cnt == 0) {
											from = tstr[0];
											cnt++;
										} else {
											to = tstr[0];
											flagfrom = 0;
											flagto = 0;
											// check if event exists or not
											for (j = 0; j < 26; j++) {
												if (tevents[from - 'A'][j] == 1)
													flagfrom = 1;
												if (tevents[j][from - 'A'] == 1)
													flagfrom = 1;
												if (tevents[to - 'A'][j] == 1)
													flagto = 1;
												if (tevents[j][to - 'A'] == 1)
													flagto = 1;
											}
											if (flagfrom == 0) {
												sprintf(out_buf,
														"Event not found:%c\n",
														from);
												send(i, out_buf,
														strlen(out_buf), 0);
												if (lfile == 1)
													strcat(file_buf, out_buf);
												else
													write(fileno(stdout),
															out_buf,
															strlen(out_buf));
												break;
											}
											if (flagto == 0) {
												sprintf(out_buf,
														"Event not found:%c\n",
														to);
												send(i, out_buf,
														strlen(out_buf), 0);
												if (lfile == 1)
													strcat(file_buf, out_buf);
												else
													write(fileno(stdout),
															out_buf,
															strlen(out_buf));
												break;
											}
											//evaluate query
											check = checkquery(from, to,
													tevents);
											if (check == 0) {
												check = checkquery(to, from,
														tevents);
												if (check == 0) {
													sprintf(
															out_buf,
															"%c concurrent to %c\n",
															from, to);
												} else
													sprintf(
															out_buf,
															"%c happened before %c\n",
															to, from);
											} else if (check == 1) {
												sprintf(
														out_buf,
														"%c happened before %c\n",
														from, to);
											}
											send(i, out_buf, strlen(out_buf),
													0);
											if (lfile == 1)
												strcat(file_buf, out_buf);
											else
												write(fileno(stdout), out_buf,
														strlen(out_buf));
										}
									}
									tstr = strtok(NULL, " ");
								}
							} else {
								sprintf(out_buf, "Wrong Command!\n");
								if (lfile == 1)
									strcat(file_buf, out_buf);
								else
									write(fileno(stdout), out_buf,
											strlen(out_buf));
								send(i, out_buf, strlen(out_buf), 0);
								close(i);
								FD_CLR(i, &master);
								break;
							}
							if (lfile == 1)
								log_status(); //print log of request and response to file
							if (add == 0)
								break;
						}

						if (add != 0) { //if request successful then commit changes
							for (j = 0; j < 26; j++) {
								for (k = 0; k < 26; k++) {
									events[j][k] = tevents[j][k];
								}
							}
						}
					}
				}
			}
		}
	}
	fclose(logfile_fd);
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
