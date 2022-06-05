gcc -fsanitize=address -pthread client.c -o client
gcc -fsanitize=address -pthread server.c -o server
