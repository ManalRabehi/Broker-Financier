#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 1024

void handle_request(const char *request, char *response){
    if (strncmp(request, "INFO APPLE", 10)==0){
        strcpy(response, "APPLE: prix=150$");
    }
    else if (strncmp(request, "INFO TESSLA", 10)==0){
        strcpy(response, "TESSLA: prix=220$");
    }
    else {
        strcpy(response, "produit inconnu");
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // 1. création du socket TCP
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0))== -1){
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // 2. configuration de l'adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 3. lier le socket à l'adresse et au port
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))== -1){
        perror("Erreur lors du bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 4. mettre le serveur en mode écoute
    if (listen(server_socket, 5)== -1){
        perror("Erreur lors de l'écoute");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Serveur TCP en attente de connexions sur le port %d...\n", PORT);

    // 5. accepter une connexion entrante 
    if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len))== -1){
        perror("Erreur lors de l'acceptation de la connexion");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Client connecté : %s:%d\n", 
        inet_ntoa(client_addr.sin_addr), 
        ntohs(client_addr.sin_port));

    // boucle de communication avec le client
    while(1){
        // 6. réception du message du client
        int received_bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (received_bytes <= 0){
            printf("Client déconnecté.\n");
            break;
        }
        buffer[received_bytes] = '\0';
        printf("Message reçu du client: %s\n", buffer);
        // traiter la demande
        handle_request(buffer, response);
        // envoi de la réponse
        send(client_socket, response, strlen(response), 0);
    }
    // 7. fermeture des sockets
    close(client_socket);
    close(server_socket);
    return 0;
}