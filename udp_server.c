#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unistd.h> // voor de sleep functie
#define OSInit() WSADATA wsaData; WSAStartup( MAKEWORD( 2, 0 ), &wsaData );
#define OSCleanup() WSACleanup();
#define perror(string) fprintf( stderr, string ": WSA errno = %d\n", WSAGetLastError() )
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#define OSInit()
#define OSCleanup()
#endif

// Functie die een willekeurig getal genereert tussen 0 en 99
int generate_random_number() {
    srand(time(NULL)); // Initialiseer de willekeurige getalgenerator met de huidige tijd
    return rand() % 100; // Genereer een willekeurig getal tussen 0 en 99
}

// Functie om de gok van de speler te vergelijken met het juiste getal
void compare_guess_with_number(int guess, int number_to_guess, int client_socket, struct sockaddr *client_internet_address, socklen_t client_internet_address_length) {
    if (guess == number_to_guess) {
        printf("Je gok is correct! Je hebt gewonnen!\n");
        sendto(client_socket, "Je hebt gewonnen!", strlen("Je hebt gewonnen!"), 0, client_internet_address, client_internet_address_length); // Stuur "Je hebt gewonnen!" naar de speler
    } else {
        printf("Je gok is fout! Het juiste getal was: %d\n", number_to_guess);
        sendto(client_socket, "Je hebt verloren. Het juiste getal was: ", strlen("Je hebt verloren. Het juiste getal was: "), 0, client_internet_address, client_internet_address_length); // Stuur bericht met het juiste getal
    }
}

// Functie om de server te initialiseren
int initialization();

// Functie die het spel uitvoert
void execution(int internet_socket);

// Functie om de server resources op te ruimen
void cleanup(int internet_socket);

// Hoofdfunctie van het programma
int main(int argc, char *argv[]) {
    OSInit(); // Initialiseer Windows sockets als je Windows gebruikt

    int internet_socket = initialization(); // Initialiseer het netwerk en maak een socket

    while (1) {
        execution(internet_socket); // Voer het spel uit in een lus (voor meerdere rondes)
    }

    cleanup(internet_socket); // Ruim de server resources op

    OSCleanup(); // Maak Windows sockets schoon (indien nodig)

    return 0;
}

// Initialisatie van de server, maakt de socket aan
int initialization() {
    struct addrinfo internet_address_setup; 
    struct addrinfo *internet_address_result;
    memset(&internet_address_setup, 0, sizeof internet_address_setup); // Zet alle instellingen op nul

    internet_address_setup.ai_family = AF_UNSPEC; // Gebruik het juiste adresfamilie (IPv4 of IPv6)
    internet_address_setup.ai_socktype = SOCK_DGRAM; // Gebruik UDP (datagram socket)
    internet_address_setup.ai_flags = AI_PASSIVE; // Voor een server die bindt aan alle interfaces

    int getaddrinfo_return = getaddrinfo(NULL, "24042", &internet_address_setup, &internet_address_result); // Verkrijg adresinfo voor de server
    if (getaddrinfo_return != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return)); // Fout bij verkrijgen van adresinfo
        exit(1);
    }

    int internet_socket = -1;
    struct addrinfo *internet_address_result_iterator = internet_address_result;
    while (internet_address_result_iterator != NULL) {
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol); // Maak de socket aan
        if (internet_socket == -1) {
            perror("socket");
        } else {
            int bind_return = bind(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen); // Bind de socket aan een poort
            if (bind_return == -1) {
                close(internet_socket); // Sluit de socket als binden mislukt
                perror("bind");
            } else {
                break; // Als binden succesvol is, breek de lus
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }

    freeaddrinfo(internet_address_result); // Maak de geheugenruimte van de adresinfo vrij

    if (internet_socket == -1) {
        fprintf(stderr, "socket: geen geldige socket gevonden\n");
        exit(2); // Stop het programma als er geen geldige socket is
    }

    return internet_socket; // Retourneer de gemaakte socket
}

// De uitvoer van het spel (de ronde)
void execution(int internet_socket) {
    int number_to_guess = generate_random_number(); // Genereer een willekeurig getal voor deze ronde
    printf("Het te raden getal is: %d\n", number_to_guess);

    struct timeval timeout; 
    timeout.tv_sec = 16; // Begin met een timeout van 16 seconden
    timeout.tv_usec = 0;
    if (setsockopt(internet_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) { // Stel de timeout in
        perror("setsockopt");
    }

    int closest_guess = -1; // Variabele voor de dichtstbijzijnde gok
    int closest_distance = 100; // Begin met een grote afstand (meer dan 99)
    int winner_selected = 0; // Houd bij of er al een winnaar is geselecteerd

    while (!winner_selected) {
        int number_of_bytes_received = 0;
        char buffer[1000];
        struct sockaddr_storage client_internet_address;
        socklen_t client_internet_address_length = sizeof(client_internet_address);
        number_of_bytes_received = recvfrom(internet_socket, buffer, (sizeof buffer) - 1, 0, (struct sockaddr *)&client_internet_address, &client_internet_address_length); // Ontvang een gok van de speler
        if (number_of_bytes_received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout is opgetreden
                if (closest_guess != -1) { // Als er al een winnaar is geselecteerd
                    printf("Je hebt gewonnen?\n");
                    sendto(internet_socket, "Je hebt gewonnen!", strlen("Je hebt gewonnen!"), 0, (struct sockaddr *)&client_internet_address, client_internet_address_length);
                    winner_selected = 1; // Winnaar geselecteerd
                } else {
                    printf("Je hebt verloren?\n");
                    sendto(internet_socket, "Je hebt verloren. Het juiste getal was: ", strlen("Je hebt verloren. Het juiste getal was: "), 0, (struct sockaddr *)&client_internet_address, client_internet_address_length);
                    break; // Stop de ronde
                }
            } else {
                perror("recvfrom"); // Er ging iets mis bij het ontvangen van een bericht
            }
        } else {
            buffer[number_of_bytes_received] = '\0'; // Zet het ontvangen bericht om naar een string
            int client_guess = atoi(buffer); // Zet de ontvangen gok om naar een getal
            printf("Ontvangen gok van client: %d\n", client_guess);
            int distance = abs(client_guess - number_to_guess); // Bereken de afstand tot het juiste getal
            if (distance < closest_distance) { // Als deze gok dichterbij is dan de vorige
                closest_guess = client_guess;
                closest_distance = distance;
            }

            compare_guess_with_number(client_guess, number_to_guess, internet_socket, (struct sockaddr *)&client_internet_address, client_internet_address_length); // Vergelijk de gok met het juiste getal

            timeout.tv_sec /= 2; // Halveer de timeout
            if (setsockopt(internet_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) { // Stel de nieuwe timeout in
                perror("setsockopt");
            }
        }
    }
}

// Ruim de serverbronnen op na het beëindigen van het spel
void cleanup(int internet_socket) {
    close(internet_socket); // Sluit de socket af
}
