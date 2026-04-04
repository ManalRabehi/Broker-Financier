/*
 *  broker.c — Serveur Broker Financier
 *
 *  FONCTIONNALITÉS IMPLÉMENTÉES :
 *   Serveur simple : connexions, infos produits, logs
 *   Achat/Vente : portefeuille client, stock broker
 *   Robustesse : vérification fonds et quantités
 *   Optionnel : prix dynamiques selon les transactions
 */

/* ── Includes ─────────────────────────────────────────────── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <arpa/inet.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>

#define PORT         12345 
#define BUFFER_SIZE  1024 
#define NB_PRODUITS  4       
#define BROKER_FONDS 100000.0 

/* Produit financier */
typedef struct {
    char   nom[32];       
    float  prix; 
    int    quantite; 
} Produit;

/* ── Structure : le broker (état global) ─────────────────── */
typedef struct {
    Produit produits[NB_PRODUITS]; 
    float   fonds;  
} Broker;

/* ── Variable globale : état du broker ───────────────────── */
Broker broker = {
    /* initialisation des produits */
    .produits = {
        {"APPLE",   150.0f, 100},
        {"TESLA",   220.0f,  80},
        {"GOOGLE",  130.0f, 120},
        {"AMAZON",  175.0f,  60}
    },
    /* fonds initiaux du broker */
    .fonds = BROKER_FONDS
};

/* fichier de log */
FILE *log_file;

// FONCTION : log_message
void log_message(const char *message) {
    /* récupère l'heure courante */
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    /* ctime() ajoute un '\n' à la fin, on le supprime */
    timestamp[strlen(timestamp) - 1] = '\0';

    /* affichage dans le terminal */
    printf("[%s] %s\n", timestamp, message);

    /* écriture dans le fichier*/
    if (log_file != NULL) {
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fflush(log_file); /* écriture forcée sur le disque */
    }
}

// FONCTION : trouver_produit
// Cherche un produit par son nom dans le tableau du broker.
int trouver_produit(const char *nom) {
    for (int i = 0; i < NB_PRODUITS; i++) {
        /* strcmp retourne 0 si les chaînes sont identiques */
        if (strcmp(broker.produits[i].nom, nom) == 0) {
            return i; /* on retourne l'index du produit trouvé */
        }
    }
    return -1; /* produit non trouvé */
}

// FONCTION : cmd_info
// Traite la commande "INFO <PRODUIT>"
// Envoie au client le prix et la quantité disponible.
void cmd_info(const char *nom_produit, char *response) {
    int idx = trouver_produit(nom_produit);
    if (idx == -1) {
        /* produit non trouvé → message d'erreur */
        snprintf(response, BUFFER_SIZE,
            "ERREUR: Produit '%s' inconnu. Produits disponibles: APPLE, TESLA, GOOGLE, AMAZON",
            nom_produit);
    } else {
        /* produit trouvé */
        snprintf(response, BUFFER_SIZE,
            "INFO %s | Prix: %.2f$ | Quantite disponible: %d actions",
            broker.produits[idx].nom,
            broker.produits[idx].prix,
            broker.produits[idx].quantite);
    }
}

// FONCTION : cmd_liste
// Traite la commande "LISTE"
// Envoie au client la liste complète des produits disponibles.
void cmd_liste(char *response) {
    int offset = 0;
    offset += snprintf(response + offset, BUFFER_SIZE - offset,
        "=== ACTIONS DISPONIBLES ===\n");
    for (int i = 0; i < NB_PRODUITS; i++) {
        offset += snprintf(response + offset, BUFFER_SIZE - offset,
            "  %-8s | Prix: %7.2f$ | Stock: %d actions\n",
            broker.produits[i].nom,
            broker.produits[i].prix,
            broker.produits[i].quantite);
    }
    /* on ajoute les fonds du broker pour information */
    snprintf(response + offset, BUFFER_SIZE - offset,
        "Fonds Broker: %.2f$", broker.fonds);
}

/* ============================================================
 * FONCTION : cmd_acheter
 * Traite la commande "Acheter <PRODUIT> <QUANTITE>"
 * Vérifications (section 3.3) :
 *    - le produit existe
 *    - la quantité demandée est positive
 *    - le broker a assez de stock
 *  le prix augmente de 1% par tranche de 10
 *  actions achetées (la demande fait monter les prix).
 * ============================================================ */
void cmd_acheter(const char *nom_produit, int quantite, char *response) {
    /* vérification de la quantité */
    if (quantite <= 0) {
        snprintf(response, BUFFER_SIZE, "ERREUR: La quantite doit etre positive.");
        return;
    }

    int idx = trouver_produit(nom_produit);
    if (idx == -1) {
        snprintf(response, BUFFER_SIZE, "ERREUR: Produit '%s' inconnu.", nom_produit);
        return;
    }

    /* vérification du stock du broker */
    if (broker.produits[idx].quantite < quantite) {
        snprintf(response, BUFFER_SIZE,
            "ERREUR: Stock insuffisant. Disponible: %d actions de %s.",
            broker.produits[idx].quantite,
            nom_produit);
        return;
    }
    /* calcul du coût total de la transaction */
    float cout_total = broker.produits[idx].prix * quantite;

    /* mise à jour du stock et des fonds du broker */
    broker.produits[idx].quantite -= quantite; /* le broker perd des actions */
    broker.fonds += cout_total;                /* le broker gagne de l'argent */

    /* ajustement dynamique du prix */
    /* +1% par tranche de 10 actions achetées (la demande fait monter les prix) */
    float hausse = (quantite / 10) * 0.01f;
    broker.produits[idx].prix *= (1.0f + hausse);

    snprintf(response, BUFFER_SIZE,
        "ACHAT OK | %d actions %s achetees pour %.2f$ (%.2f$/action) | "
        "Nouveau prix: %.2f$ | Stock restant: %d",
        quantite,
        nom_produit,
        cout_total,
        cout_total / quantite,
        broker.produits[idx].prix,
        broker.produits[idx].quantite);
}

/* ============================================================
 *  FONCTION : cmd_vendre
 *  Traite la commande "SELL <PRODUIT> <QUANTITE>"
 *  Vérifications (section 3.3) :
 *    - le produit existe
 *    - la quantité est positive
 *    - le broker a assez de fonds pour racheter
 *  le prix baisse de 1% par tranche de 10
 *  actions vendues (l'offre fait baisser les prix).
 * ============================================================ */
void cmd_vendre(const char *nom_produit, int quantite, char *response) {
    /* vérification de la quantité */
    if (quantite <= 0) {
        snprintf(response, BUFFER_SIZE, "ERREUR: La quantite doit etre positive.");
        return;
    }
 
    int idx = trouver_produit(nom_produit);
    if (idx == -1) {
        snprintf(response, BUFFER_SIZE, "ERREUR: Produit '%s' inconnu.", nom_produit);
        return;
    }
 
    /* calcul du montant que le broker doit payer */
    float montant = broker.produits[idx].prix * quantite;
 
    /* [3.3] vérification des fonds du broker */
    if (broker.fonds < montant) {
        snprintf(response, BUFFER_SIZE,
            "ERREUR: Broker sans fonds suffisants. Fonds disponibles: %.2f$, "
            "cout de rachat: %.2f$",
            broker.fonds,
            montant);
        return;
    }
 
    /* mise à jour du stock et des fonds du broker */
    broker.produits[idx].quantite += quantite; /* le broker gagne des actions */
    broker.fonds -= montant;                   /* le broker perd de l'argent */
 
    /* [3.4 OPTIONNEL] ajustement dynamique du prix */
    /* -1% par tranche de 10 actions vendues (l'offre fait baisser les prix) */
    float baisse = (quantite / 10) * 0.01f;
    broker.produits[idx].prix *= (1.0f - baisse);
    /* prix plancher : on ne descend pas en dessous de 1$ */
    if (broker.produits[idx].prix < 1.0f)
        broker.produits[idx].prix = 1.0f;
 
    snprintf(response, BUFFER_SIZE,
        "VENTE OK | %d actions %s vendues pour %.2f$ (%.2f$/action) | "
        "Nouveau prix: %.2f$ | Stock broker: %d",
        quantite,
        nom_produit,
        montant,
        montant / quantite,
        broker.produits[idx].prix,
        broker.produits[idx].quantite);
}

// FONCTION : handle_request
// Point d'entrée pour le traitement de toutes les commandes.
void handle_request(const char *request, char *response) {
    char commande[32];    
    char produit[32];   
    int  quantite = 0;

    sscanf(request, "%31s", commande);
    if (strcmp(commande, "LISTE") == 0) {
        cmd_liste(response);
    } else if (strcmp(commande, "INFO") == 0) {
        if (sscanf(request, "%*s %31s", produit) == 1) {
            cmd_info(produit, response);
        } else {
            strcpy(response, "ERREUR: Usage: INFO <PRODUIT>");
        }
    } else if (strcmp(commande, "BUY") == 0) {
        if (sscanf(request, "%*s %31s %d", produit, &quantite) == 2) {
            cmd_acheter(produit, quantite, response);
        } else {
            strcpy(response, "ERREUR: Usage: BUY <PRODUIT> <QUANTITE>");
        }
    } else if (strcmp(commande, "SELL") == 0) {
        if (sscanf(request, "%*s %31s %d", produit, &quantite) == 2) {
            cmd_vendre(produit, quantite, response);
        } else {
            strcpy(response, "ERREUR: Usage: SELL <PRODUIT> <QUANTITE>");
        }
    } else if (strcmp(commande, "AIDE") == 0) {
        strcpy(response,
            "Commandes disponibles:\n"
            "  LISTE                 - lister tous les produits\n"
            "  INFO <PRODUIT>        - infos sur un produit\n"
            "  BUY  <PRODUIT> <QTY> - acheter QTY actions\n"
            "  SELL <PRODUIT> <QTY> - vendre QTY actions\n"
            "  AIDE                  - afficher ce menu\n"
            "  QUIT                  - se deconnecter");
    } else {
        snprintf(response, BUFFER_SIZE,
            "ERREUR: Commande '%s' inconnue. Tapez AIDE pour la liste des commandes.",
            commande);
    }
}

// FONCTION : gerer_client
// Boucle de communication avec un client connecté.
void gerer_client(int client_socket, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];  
    char log_buf[BUFFER_SIZE]; 

    /* log de connexion avec l'IP et le port du client */
    snprintf(log_buf, sizeof(log_buf), ">> Nouveau client : %s:%d",
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port));
    log_message(log_buf);
    const char *bienvenue =
        "=== BROKER FINANCIER ===\n"
        "Bienvenue ! Tapez AIDE pour la liste des commandes.\n";
    send(client_socket, bienvenue, strlen(bienvenue), 0);

    /* ── boucle principale de communication ── */
    while (1) {
        /* recv() bloque jusqu'à recevoir un message.
         * Retourne 0 si le client s'est déconnecté,
         * -1 en cas d'erreur réseau. */
        int received_bytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (received_bytes <= 0) {
            /* déconnexion inattendue ou erreur réseau */
            snprintf(log_buf, sizeof(log_buf), "<< Client %s:%d deconnecte.",
                inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));
            log_message(log_buf);
            break; /* on sort de la boucle */
        }
        buffer[received_bytes] = '\0';
        if (buffer[received_bytes - 1] == '\n')
            buffer[received_bytes - 1] = '\0';

        /* log du message reçu */
        snprintf(log_buf, sizeof(log_buf), "   [%s:%d] -> %s",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port),
            buffer);
        log_message(log_buf);

        /* le client demande à se déconnecter */
        if (strcmp(buffer, "QUIT") == 0) {
            const char *au_revoir = "Au revoir !\n";
            send(client_socket, au_revoir, strlen(au_revoir), 0);
            snprintf(log_buf, sizeof(log_buf), "<< Client %s:%d a quitte volontairement.",
                inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));
            log_message(log_buf);
            break;
        }
        memset(response, 0, BUFFER_SIZE);
        handle_request(buffer, response);

        send(client_socket, response, strlen(response), 0);

        /* log de la réponse envoyée */
        snprintf(log_buf, sizeof(log_buf), "   [%s:%d] <- %s",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port),
            response);
        log_message(log_buf);
    }
}

// FONCTION : nettoyer_fils
// Gestionnaire du signal SIGCHLD.
void nettoyer_fils(int sig) {
    (void)sig; /* on ignore le paramètre, on en a pas besoin */
    /* WNOHANG = ne pas bloquer si aucun fils n'est terminé */
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

//main
int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char log_buf[BUFFER_SIZE];

    // Ouverture du fichier de log
    log_file = fopen("broker.log", "a");
    if (log_file == NULL) {
        perror("Erreur ouverture broker.log");
        exit(EXIT_FAILURE);
    }

    // Gestionnaire SIGCHLD pour éviter les zombies
    signal(SIGCHLD, nettoyer_fils);

    // Création du socket TCP 
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Erreur creation socket");
        exit(EXIT_FAILURE);
    }

    // option SO_REUSEADDR : permet de relancer le serveur immédiatement
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //Configuration de l'adresse du serveur 
    server_addr.sin_family      = AF_INET;    
    server_addr.sin_addr.s_addr = INADDR_ANY;   
    server_addr.sin_port        = htons(PORT); 

    //Bind : associe le socket à l'adresse/port
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erreur bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen : mise en écoute
    if (listen(server_socket, 5) == -1) {
        perror("Erreur listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    snprintf(log_buf, sizeof(log_buf),
        "Broker demarré | Port: %d | %d produits | Fonds: %.2f$",
        PORT, NB_PRODUITS, broker.fonds);
    log_message(log_buf);
    log_message("En attente de connexions clients...");

    //Boucle principale : accept + fork
    while (1) {
        // accept() bloque jusqu'à l'arrivée d'un client
        client_socket = accept(server_socket,
                               (struct sockaddr*)&client_addr,
                               &client_addr_len);
        if (client_socket == -1) {
            perror("Erreur accept");
            continue; 
        }

        // fork() crée un processus fils.
        pid_t pid = fork();

        if (pid == -1) {
            // échec du fork : on ferme ce client et on continue
            perror("Erreur fork");
            close(client_socket);
            continue;
        }
        if (pid == 0) {
            close(server_socket);

            gerer_client(client_socket, client_addr);

            close(client_socket);
            fclose(log_file);
            exit(0);

        } else {
            close(client_socket);
        }
    }
    fclose(log_file);
    close(server_socket);
    return 0;
}