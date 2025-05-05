// UDP-server voor "Wie is het dichtste?" spel
// Elke speler stuurt een gok (getal tussen 0–99) in ASCII
// De server duidt enkel de dichtste gok aan als winnaar na een time-out

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unistd.h>
#define OSInit() WSADATA wsaData; WSAStartup(MAKEWORD(2, 0), &wsaData)
#define OSCleanup() WSACleanup()
#define perror(msg) fprintf(stderr, msg ": WSA errno = %d\n", WSAGetLastError())
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#define OSInit()
#define OSCleanup()
#endif

#define PORT "24042"
#define MAX_BUFFER 1000

// Vraag 1: UDP SOCKETS correct
int create_udp_socket() {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_DGRAM;   // UDP
    hints.ai_flags = AI_PASSIVE;      // Voor server

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    freeaddrinfo(res);
    return sockfd;
}

// Vraag 2: willekeurig getal tussen 0 en 99
int generate_secret_number() {
    return rand() % 100;
}

// Vraag 6: late messages – stuur "You lost!" naar alle late berichten
void handle_late_message(int sockfd, struct sockaddr *client_addr, socklen_t addrlen) {
    char message[] = "You lost!";
    sendto(sockfd, message, strlen(message), 0, client_addr, addrlen);
}

// Vraag 3, 4 en 5
void play_round(int sockfd) {
    int secret = generate_secret_number();
    printf("Nieuw getal: %d\n", secret);  // Voor debug

    struct sockaddr_storage winner_addr;
    socklen_t winner_addrlen = sizeof(winner_addr);
    int closest_guess = -1;
    int closest_diff = 100;

    int round_done = 0;
    int timeout_sec = 16;

    while (!round_done && timeout_sec > 0) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = timeout_sec;
        timeout.tv_usec = 0;

        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select");
            break;
        } else if (activity == 0) {
            // Time-out
            if (closest_guess != -1) {
                char msg[] = "You won!";
                sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr*)&winner_addr, winner_addrlen);
                printf("Winnaar verstuurd!\n");
            }
            round_done = 1;
        } else {
            // Data beschikbaar
            char buffer[MAX_BUFFER];
            struct sockaddr_storage client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int bytes = recvfrom(sockfd, buffer, MAX_BUFFER - 1, 0, (struct sockaddr*)&client_addr, &addrlen);

            if (bytes < 0) {
                perror("recvfrom");
                continue;
            }

            buffer[bytes] = '\0';
            int guess = atoi(buffer);
            printf("Gok ontvangen: %d\n", guess);

            int diff = abs(secret - guess);
            if (diff < closest_diff) {
                closest_diff = diff;
                closest_guess = guess;
                memcpy(&winner_addr, &client_addr, addrlen);
                winner_addrlen = addrlen;
            }

            // Vraag 4: halveer de time-out
            timeout_sec /= 2;
        }
    }

    // Vraag 6: late berichten opvangen
    struct timeval grace;
    grace.tv_sec = 2;
    grace.tv_usec = 0;

    fd_set late_fds;
    FD_ZERO(&late_fds);
    FD_SET(sockfd, &late_fds);

    while (select(sockfd + 1, &late_fds, NULL, NULL, &grace) > 0) {
        char buffer[MAX_BUFFER];
        struct sockaddr_storage late_addr;
        socklen_t addrlen = sizeof(late_addr);
        int bytes = recvfrom(sockfd, buffer, MAX_BUFFER - 1, 0, (struct sockaddr*)&late_addr, &addrlen);

        if (bytes > 0) {
            handle_late_message(sockfd, (struct sockaddr*)&late_addr, addrlen);
        }

        FD_ZERO(&late_fds);
        FD_SET(sockfd, &late_fds);
    }
}

// Vraag 8: CONTINUOUS – speel meerdere rondes
int main() {
    OSInit();
    srand(time(NULL));

    int sockfd = create_udp_socket();  // Vraag 1

    while (1) {
        play_round(sockfd);  // Vraag 8
        printf("Nieuwe ronde start...\n");
    }

    close(sockfd);
    OSCleanup();
    return 0;
}
