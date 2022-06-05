#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "default.h"

// Global variables
volatile sig_atomic_t quit_flag = 0;
int socket_descriptor = 0;
char username[32];
char channel_selection[32];
int channel_id = 0;

void overwrite_stdout() {
  printf("> ");
  fflush(stdout);
}

void trim_newlinechar (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void catch_ctrl_c_and_exit(int sig) {
    quit_flag = 1;
}

void select_username() {
  printf( FGLIGHTGREEN "\nOk, let's get started! Enter your username: " DEFAULT );
  do {
    fgets(username, 32, stdin);
    trim_newlinechar(username, strlen(username));

    if (strlen(username) > 32 || strlen(username) < 4){
      system("clear");
      printf( BOLDRED "The username must be less than 30 and more than 4 characters. Try another string.\n" DEFAULT );
      printf( FGLIGHTGREEN "\nEnter your username: " DEFAULT );
    }
  } while (strlen(username) > 32 || strlen(username) < 4);
}

int is_valid_channel() {
  return (strlen(channel_selection) == 1 && channel_selection[0] >= '1' && channel_selection[0] <= ('0' + CHANNEL_COUNT));
}

void select_channel() {
  memset(channel_selection, 0, 32);
  do {
    fgets(channel_selection, 32, stdin);
    trim_newlinechar(channel_selection, strlen(channel_selection));

    if (!is_valid_channel()) {
      printf( BOLDRED "The channel must be between 1 and %d. Try another string.\n" DEFAULT, CHANNEL_COUNT );
    }
  } while (!is_valid_channel());
  channel_id = channel_selection[0] - '0';
}

void send_message_handler() {
  char message[BUFFERSIZE] = {};
	char buffer[BUFFERSIZE + 32] = {};

  while(1) {
    if (channel_id == 0) {
      select_channel();
      send(socket_descriptor, channel_selection, strlen(channel_selection), 0);
    } else {
      overwrite_stdout();
      fgets(message, BUFFERSIZE, stdin);
      trim_newlinechar(message, BUFFERSIZE);
      if (strcmp(message, "*quit") == 0) {
        /* Quit program */
        quit_flag = 1;
        break;
      } else {
        if (strcmp(message, "*back") == 0) {
          /* Get back to channel selection */
          sprintf(buffer, "%s", message);
          channel_id = 0;
          system("clear");
        } else {
          sprintf(buffer, BOLDGREEN "%s:" DEFAULT BOLD " %s\n" DEFAULT , username, message);
        }
        send(socket_descriptor, buffer, strlen(buffer)+1, 0);
      }
    }
		memset(message, 0, BUFFERSIZE);
    memset(buffer, 0, BUFFERSIZE + 32);
  }
  catch_ctrl_c_and_exit(2);
}

void receive_message_handler() {
	char message[BUFFERSIZE] = {};
  while (1) {
		int receive = recv(socket_descriptor, message, BUFFERSIZE, 0);
    if (receive > 0) {
      printf("%s", message);
      overwrite_stdout();
    } else if (receive == 0) {
			break;
    } else {
			// -1
		}
		memset(message, 0, sizeof(message));
  }
}

int main(int argc, char **argv){
  system("clear");

	char *ip = "65.21.110.168";

	signal(SIGINT, catch_ctrl_c_and_exit);
  system("clear");
  printf( BOLDGREEN "\t\t****** RANDOMCHAT RUNNING ... ******\n" DEFAULT );
  sleep(1);
  printf("\n\nWelcome to Random Chat! This is a simple IRC application. \nRandom Chat is designed for group communication in discussion channels.");
  printf("\n\nDeveloped by Paolo Migliozzi & Roberto Cinque.");
  sleep(1);
  printf("\n\n Press any key to continue ...");
  getchar();

  system("clear");

  select_username();


	struct sockaddr_in server_addr;

	/* Socket settings */
	socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(PORT);


  /* Connect to Server */
  int err = connect(socket_descriptor, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (err == -1) {
		printf( BOLDRED "CONNECTION ERROR: Cannot establish the connection with the server.\n" DEFAULT );
		return EXIT_FAILURE;
	}

	/* Send username */
	send(socket_descriptor, username, 32, 0);

	printf( BOLDGREEN "\t\t****** WELCOME TO THE RANDOM CHAT! ******\n" DEFAULT );
  sleep(1);

	pthread_t send_message_thread;
  if(pthread_create(&send_message_thread, NULL, (void *) send_message_handler, NULL) != 0){
		printf( BOLDRED "THREAD ERROR: Cannot create thread with pthread_create." DEFAULT );
    return EXIT_FAILURE;
	}

	pthread_t receive_message_thread;
  if(pthread_create(&receive_message_thread, NULL, (void *) receive_message_handler, NULL) != 0){
		printf( BOLDRED "THREAD ERROR: Cannot create thread with pthread_create." DEFAULT );
		return EXIT_FAILURE;
	}

	while (1){
		if(quit_flag){
      printf("\nTerminating connection...\n");
      send(socket_descriptor, "*quit", strlen("*quit")+1, 0);
      sleep(1);
			printf( BOLDGREEN "\nSee ya!\n" DEFAULT );
			break;
    }
	}

	close(socket_descriptor);

	return EXIT_SUCCESS;
}
