#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include "default.h"

unsigned int client_counter = 0;
static int uid = 10;

typedef struct channel_t {
	int channel_id;
	char name[32];
	int population;
} channel_t;

/* Client structure */
typedef struct client_t {
	struct sockaddr_in address;
	int socket_descriptor;
	int uid;
	int channel_id;
	int pair_uid;
	char username[32];
	int waiting;
} client_t;

client_t *clients[MAXCLIENTS];
channel_t channels[CHANNEL_COUNT];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void trim_newlinechar (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void print_client_addr (struct sockaddr_in addr) {
	printf("%s", inet_ntoa(addr.sin_addr));
}

void create_channel_data() {
	int i;
	for (i=0; i<CHANNEL_COUNT; i++) {
		channels[i].channel_id = i+1;
		channels[i].population = 0;
		switch (i) {
			case 0:
			sprintf(channels[i].name, "Cinema");
			break;
			case 1:
			sprintf(channels[i].name, "Video Games");
			break;
			case 2:
			sprintf(channels[i].name, "Role Playing Games");
			break;
			case 3:
			sprintf(channels[i].name, "Tech");
			break;
			case 4:
			sprintf(channels[i].name, "Comics");
			break;
			case 5:
			sprintf(channels[i].name, "TV Series");
			break;
			case 6:
			sprintf(channels[i].name, "Books");
			break;
			case 7:
			sprintf(channels[i].name, "General");
			break;
		}
	}
}

/* Add clients to queue */
void queue_add (client_t *client_to_add) {
	pthread_mutex_lock(&clients_mutex);
	int i;
	for (i=0; i < MAXCLIENTS; i++) {
		if (!clients[i]) {
			clients[i] = client_to_add;
			break;
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients from queue */
void queue_remove (int uid) {
	pthread_mutex_lock(&clients_mutex);
	int i;
	for (i=0; i < MAXCLIENTS; i++) {
		if (clients[i]) {
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

int catch_waiting_user() {

}

void send_channel_info(int uid) {
	if (uid > 0) {
		pthread_mutex_lock(&clients_mutex);
		int i, offset=0;
		char buffer[BUFFERSIZE];
		offset += sprintf(buffer+offset, "Hello! Welcome into the random chat. With this little IRC program, you can easily \nchat with some random guys, you just need to select a channel and u're set-up to start!\n The channels are rooms in which people start talking about a specific argument, like comics or cinema.\n\nTry out selecting now a channel!\n\n");
		for (i=0; i<CHANNEL_COUNT; i++) {
			offset += sprintf(buffer+offset, "\t%d.["BOLDYELLOW"%s"DEFAULT"] (%d)\n",
													channels[i].channel_id,
													channels[i].name,
													channels[i].population);
		}
		for (i=0; i < MAXCLIENTS; i++) {
			if (clients[i]) {
				if (clients[i]->uid == uid) {
					if (write(clients[i]->socket_descriptor, buffer, strlen(buffer)) < 0) {
						perror("ERROR: write to descriptor failed");
						break;
					}
				}
			}
		}
		pthread_mutex_unlock(&clients_mutex);
	}
}

int join_channel(int uid, char *channel) {
	if (uid > 0) {
		pthread_mutex_lock(&clients_mutex);
		int channel_id = atoi(channel);
		int i;
		channels[channel_id-1].population += 1;
		for (i=0; i < MAXCLIENTS; i++) {
			if (clients[i]) {
				if (clients[i]->uid == uid) {
					clients[i]->channel_id = channel_id;
					break;
				}
			}
		}
		pthread_mutex_unlock(&clients_mutex);
		return channel_id;
	}
	return 0;
}

void leave_channel(int channel_id) {
	if (channel_id > 0) {
		pthread_mutex_lock(&clients_mutex);
		channels[channel_id - 1].population -= 1;
		pthread_mutex_unlock(&clients_mutex);

	}
}


void send_message(char *s, int uid) {
	if (uid > 0) {
		pthread_mutex_lock(&clients_mutex);
		int i;
		for (i=0; i < MAXCLIENTS; i++) {
			if (clients[i]) {
				if (clients[i]->uid == uid) {
					if (write(clients[i]->socket_descriptor, s, strlen(s)) < 0) {
						perror("ERROR: write to descriptor failed");
						break;
					}
				}
			}
		}
		pthread_mutex_unlock(&clients_mutex);
	}
}

void unpair(int uid) {
	if (uid > 0) {
		pthread_mutex_lock(&clients_mutex);
		int i;
		for (i=0; i < MAXCLIENTS; i++) {
			if (clients[i]) {
				if (clients[i]->uid == uid) {
					clients[i]->waiting = 1;
					clients[i]->pair_uid = (-1 * clients[i]->pair_uid);
					printf("'%s' lost his pair and is currently waiting\n", clients[i]->username);
					break;
				}
			}
		}
		pthread_mutex_unlock(&clients_mutex);
	}
}

int attempt_pairing(int uid, int channel_id, int ignore) {
	if (uid > 0) {
		pthread_mutex_lock(&clients_mutex);
		int i;
		for (i=0; i< MAXCLIENTS; i++) {
			if (clients[i]) {
				/* If another client is in the same channel and is waiting */
				if (clients[i]->uid != uid &&
						clients[i]->channel_id == channel_id &&
						clients[i]->waiting == 1) {
					/* If the other client haven't matched with current user previously */
					if (clients[i]->pair_uid != (uid * -1)) {
						clients[i]->waiting = 0;
						clients[i]->pair_uid = uid;
						printf("User '%s' paired with ", clients[i]->username);
						pthread_mutex_unlock(&clients_mutex);
						return clients[i]->uid;
					}
				}
			}
		}
		pthread_mutex_unlock(&clients_mutex);
	}
	return -1;
}

void * client_communication_handler(void *arg) {
	char buffer[BUFFERSIZE];
	char username[32];
	char channel[32];
	int channel_id = 0;
	int pair_uid;
	int quit_flag = 0;

	client_counter++;
	client_t *client_user = (client_t *)arg;

	/* Username */
	if (recv(client_user->socket_descriptor, username, 32, 0) <= 0 ||
		strlen(username) < 2 || strlen(username) >= 32-1) {
			printf("Didn't enter the username.\n");
			quit_flag = 1;
	}
	else {
		strcpy(client_user->username, username);
	}

	memset(buffer, 0, BUFFERSIZE);
	printf("User '%s' joined the server\n", client_user->username);
	printf("Total Clients connected to server:  "BOLDBLUE" %d "DEFAULT"\n", client_counter);

	while (1) {
		if (channel_id == 0) {
			/* Channel has not been selected yet, sending channel selection message */
			send_channel_info(client_user->uid);
			if (recv(client_user->socket_descriptor, channel, 32, 0) <= 0 ||
				strlen(channel) < 1 || strlen(channel) >= 32-1) {
					printf("Didn't enter the channel.\n");
					quit_flag = 1;
			} else {
				channel_id = join_channel(client_user->uid, channel);
				client_user->waiting = 1;
				printf("User '%s' joined the channel: %s\n", client_user->username, channels[channel_id-1].name);
			}
		} else {
			if (client_user->waiting == 1) {
				pair_uid = attempt_pairing(client_user->uid, channel_id, client_user->pair_uid);
				if (pair_uid != -1) {
					client_user->waiting = 0;
					client_user->pair_uid = pair_uid;
					printf("'%s'\n", client_user->username);
				}
			} else {
				int receive = recv(client_user->socket_descriptor, buffer, BUFFERSIZE, 0);
				if (receive > 0) {
					if (strlen(buffer) > 0) {
						if (strcmp(buffer, "*back") == 0) {
							/* Return back to channel selection menu */
							memset(channel, 0, 32);
							leave_channel(channel_id);
							channel_id = 0;
							client_user->channel_id = 0;
							printf("User '%s' left channel\n", client_user->username);
							unpair(client_user->pair_uid);
							/* Negate the previous pair to prevent matching again */
							client_user->pair_uid = (-1 * client_user->pair_uid);
							client_user->waiting = 1;
						} else if (strcmp(buffer, "*quit") == 0) {
							quit_flag = 1;
						} else {
							send_message(buffer, client_user->pair_uid);
						}
					}
				}
			}
		}
		if (quit_flag) {
			break;
		}
	}
	/* Leave channel if still in */
	if (client_user->channel_id > 0) {
		leave_channel(client_user->channel_id);
	}
	/* Unpair partner if exists */
	if (client_user->pair_uid > 0) {
		unpair(client_user->pair_uid);
	}
	/* Delete client from queue and yield thread */
	close(client_user->socket_descriptor);
	queue_remove(client_user->uid);
	free(client_user);
	client_counter--;
	pthread_detach(pthread_self());
	printf("Total Clients connected to server:  "BOLDBLUE" %d "DEFAULT"\n", client_counter);
	return NULL;
}

int main(int argc, char **argv) {
	system("clear");
	create_channel_data();
	char *ip = "65.21.110.168";
	int option = 1;
	int server_socket_descriptor = 0, client_socket_descriptor = 0;
	struct sockaddr_in server;
	struct sockaddr_in client;
	pthread_t thread_id;

	/* Socket settings */
	server_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(ip);
	server.sin_port = htons(PORT);

	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	if (setsockopt(server_socket_descriptor, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR),
		((char *) &option), sizeof(option)) < 0) {
		perror("ERROR: setsockopt failed");
		return EXIT_FAILURE;
	}

	/* Bind */
	if (bind(server_socket_descriptor, ((struct sockaddr *) &server), sizeof(server)) < 0) {
		perror("ERROR: Socket binding failed");
		return EXIT_FAILURE;
	}

	/* Listen */
	if (listen(server_socket_descriptor, MAXCLIENTS) < 0) {
		perror("ERROR: Socket listening failed");
		return EXIT_FAILURE;
	}

	printf( BOLDGREEN "\t\t****** RANDOMCHAT SERVER ******\n" DEFAULT );

	while (1) {
		socklen_t clilen = sizeof(client);
		client_socket_descriptor = accept(server_socket_descriptor, (struct sockaddr*)&client, &clilen);

		/* Check if max client is reached */
		if ((client_counter + 1) == MAXCLIENTS) {
			printf("Max clients reached. Rejected: ");
			print_client_addr(client);
			printf(":%d\n", client.sin_port);
			close(client_socket_descriptor);
			continue;
		}

		/* Client settings */
		client_t *client_user = (client_t *)malloc(sizeof(client_t));
		client_user->address = client;
		client_user->socket_descriptor = client_socket_descriptor;
		client_user->uid = uid++;
		client_user->channel_id = 0;
		client_user->pair_uid = 0;

		/* Add client to queue and fork thread */
		queue_add(client_user);
		pthread_create(&thread_id, NULL, &client_communication_handler, ((void *)client_user));

		/* Reduce CPU usage */
		sleep(1);
	}

	return EXIT_SUCCESS;
}
