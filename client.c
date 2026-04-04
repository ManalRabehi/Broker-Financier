#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345 // numéro port serveur
#define BUFFER_SIZE 1024 // taille max message 
#define MAX_PRODUITS 10 // nb produits dans portefeuille 

// portefeuille du client 
typedef struct {
    char nom[32]; // nom produit 
    int quantite; // nombre d'actions 
} Ligne;

//prix connus après un INFO 
typedef struct {
    char nom[32];
    float prix;
} PrixConnu;

Ligne portefeuille[MAX_PRODUITS];
int nb_lignes = 0;

PrixConnu prix_connus[MAX_PRODUITS];
int nb_prix = 0;

float fonds = 1000.0;  // budget de départ du client

// fonction pour chercher un produit dans le portefeuille
int trouver_produit(const char *nom) {
    for (int i = 0; i < nb_lignes; i++) {
        if (strcmp(portefeuille[i].nom, nom) == 0)
            return i;
    }
    return -1; // le produit n'est pas trouvé 
}

float trouver_prix(const char *nom) {
    for (int i = 0; i < nb_prix; i++) {
        if (strcmp(prix_connus[i].nom, nom) == 0)
            return prix_connus[i].prix;
    }
    return -1.0;  // prix inconnu
}

// sauvegarder un prix après un INFO 
void sauvegarder_prix(const char *nom, float prix) {
    for (int i = 0; i < nb_prix; i++) {
        if (strcmp(prix_connus[i].nom, nom) == 0) {
            prix_connus[i].prix = prix;  // mise à jour
            return;
        }
    }
    strcpy(prix_connus[nb_prix].nom, nom);
    prix_connus[nb_prix].prix = prix;
    nb_prix++;
}

// fonction pour ajouter ou mettre à jour un produit dans le portefeuille
void ajouter_portefeuille(const char *nom, int quantite) {
    int idx = trouver_produit(nom);
    if (idx != -1) {
        // cas où produit déjà présent => on ajoute 
        portefeuille[idx].quantite += quantite;
    } else {
        // cas nouveau produit 
        strcpy(portefeuille[nb_lignes].nom, nom);
        portefeuille[nb_lignes].quantite = quantite;
        nb_lignes++;
    }
}
// focntion pour retirer des actions du portefeuille 
void retirer_portefeuille(const char *nom, int quantite) {
    int idx = trouver_produit(nom);
    if (idx != -1) {
        portefeuille[idx].quantite -= quantite;
        if (portefeuille[idx].quantite <= 0) {
            for (int i = idx; i < nb_lignes - 1; i++)
                portefeuille[i] = portefeuille[i + 1];
            nb_lignes--;
        }
    }
}


// fonction afficher le portefeuille 
void afficher_portefeuille() {
    printf("\n--- Mon portefeuille ---\n");
    printf("  Fonds : %.2f$\n", fonds);
    if (nb_lignes == 0) {
        printf("Aucune action.\n");
        return;
    }
    for (int i = 0; i < nb_lignes; i++) {
        printf("  %s : %d actions\n", portefeuille[i].nom, portefeuille[i].quantite);
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char commande[16], produit[32]; 
    int quantite; 

    // création socket 
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Erreur création socket");
        exit(EXIT_FAILURE);
    }

    // configuration de l'adresse du serveur 
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0 ) {
        perror("Erreur adresse IP");
        exit(EXIT_FAILURE);
    }

    // connexion au serveur 
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erreur connexion");
        exit(EXIT_FAILURE);
    }

    /* recevoir le message de bienvenue du broker */
    int recu = recv(client_socket, response, BUFFER_SIZE - 1, 0);
    response[recu] = '\0';
    printf("%s", response);

    printf("Fonds de départ : %.2f$\n", fonds);
    printf("Produits disponibles : APPLE, TESLA, GOOGLE, AMAZON\n");
    printf("Commandes : LISTE | INFO <produit> | BUY <produit> <qte> | SELL <produit> <qte> | PORTFOLIO | AIDE | QUIT\n");



    // boucle menu 
    while (1) {
        printf("\nEntrez votre demande : ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        /* PORTFOLIO — géré localement */
        if (strcmp(buffer, "PORTFOLIO") == 0) {
            afficher_portefeuille();
            continue;
        }

        /* vérifications locales avant d'envoyer */
        if (sscanf(buffer, "%s %s %d", commande, produit, &quantite) == 3) {

            /* vérification SELL — assez d'actions ? */
            if (strcmp(commande, "SELL") == 0) {
                int idx = trouver_produit(produit);
                if (idx == -1 || portefeuille[idx].quantite < quantite) {
                    printf("ERR : vous ne possédez pas assez d'actions de %s\n", produit);
                    continue;
                }
            }

            /* vérification BUY — assez de fonds ? */
            if (strcmp(commande, "BUY") == 0) {
                float prix = trouver_prix(produit);
                if (prix < 0) {
                    printf("ERR : faites d'abord INFO %s pour connaître le prix\n", produit);
                    continue;
                }
                float cout_total = prix * quantite;
                if (cout_total > fonds) {
                    printf("ERR : fonds insuffisants (%.2f$ nécessaires, %.2f$ disponibles)\n",
                           cout_total, fonds);
                    continue;
                }
            }
        }

        /* envoyer au serveur */
        send(client_socket, buffer, strlen(buffer), 0);

        /* QUIT */
        if (strcmp(buffer, "QUIT") == 0) {
            printf("Déconnexion.\n");
            break;
        }

        /* recevoir réponse */
        recu = recv(client_socket, response, BUFFER_SIZE - 1, 0);
        if (recu <= 0) {
            printf("Serveur déconnecté.\n");
            break;
        }
        response[recu] = '\0';
        printf("Broker : %s\n", response);

        /* mettre à jour après BUY ou SELL réussi */
        if (sscanf(buffer, "%s %s %d", commande, produit, &quantite) == 3) {
            if (strcmp(commande, "BUY") == 0 && strncmp(response, "ACHAT OK", 8) == 0) {
                float prix = trouver_prix(produit);
                fonds -= prix * quantite;
                ajouter_portefeuille(produit, quantite);
                printf("Portefeuille mis à jour : +%d %s | Fonds restants : %.2f$\n",
                       quantite, produit, fonds);
            }
            if (strcmp(commande, "SELL") == 0 && strncmp(response, "VENTE OK", 8) == 0) {
                float prix = trouver_prix(produit);
                fonds += prix * quantite;
                retirer_portefeuille(produit, quantite);
                printf("Portefeuille mis à jour : -%d %s | Fonds restants : %.2f$\n",
                       quantite, produit, fonds);
            }
        }

        /* sauvegarder le prix après un INFO */
        if (sscanf(buffer, "%s %s", commande, produit) == 2) {
            if (strcmp(commande, "INFO") == 0) {
                float prix;
                if (sscanf(response, "%*s %*s %*s %*s %f$", &prix) == 1) {
                    sauvegarder_prix(produit, prix);
                    printf("Prix de %s mémorisé : %.2f$\n", produit, prix);
                }
            }
        }
    }
    // fermeture 
    close(client_socket);
    return 0;
}