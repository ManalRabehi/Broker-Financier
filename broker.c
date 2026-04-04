#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>

#define PORT 12345
#define BUFFER_SIZE 1024

void handle_request(const char *request, char *response) {
    if (strncmp(request, "INFO APPLE", 10) == 0) {
        strcpy(response, "APPLE: prix=150$");
    } else if (strncmp(request, "INFO TESLA", 10) == 0) {
        strcpy(response, "TESLA: prix=220$");
    } else {
        strcpy(response, "Produit inconnu");
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];  // ← ajouté

    // 1. création du socket TCP
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // 2. configuration de l'adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 3. lier le socket à l'adresse et au port
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erreur lors du bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 4. mettre le serveur en mode écoute
    if (listen(server_socket, 5) == -1) {
        perror("Erreur lors de l'écoute");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Serveur en attente sur le port %d...\n", PORT);

    // 5. boucle principale : accepter les clients un par un
    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
            perror("Erreur lors de l'acceptation");
            continue;  // on réessaie plutôt que de quitter
        }
        printf("Client connecté : %s:%d\n",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port));

        // Créer un processus fils pour gérer ce client
        pid_t pid = fork();

        if (pid == -1){
            perror("Erreur fork");
            close(client_socket);
            continue;
        }
        if (pid == 0){
            // fils qui gère ce client
            close(server_socket); // le fils n'a pas besoin du socket serveur

            // 6. boucle de communication avec ce client
            while (1) {
                int received_bytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (received_bytes <= 0) {
                    printf("Client déconnecté.\n");
                    break;
                }
                buffer[received_bytes] = '\0';
                printf("Message reçu : %s\n", buffer);

                handle_request(buffer, response);
                send(client_socket, response, strlen(response), 0);
            }
            close(client_socket);
            exit(0); // le fils termine
        
        }else{
            // père: retourne attendre un nouveau client
            close(client_socket);
        }
        

    }


}