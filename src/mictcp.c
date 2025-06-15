#include <mictcp.h>
#include <api/mictcp_core.h>
#include <pthread.h>

#define IP_ADDR_MAX_LEN 46

#define MAX_SOCKET 5
#define TIMEOUT 10
#define TAILLE_FENETRE 10

#define CLIENT_LOSS_RATE 0
#define SERVER_LOSS_RATE 10

mic_tcp_sock sockets[MAX_SOCKET] ; // table de sockets

int nb_fd = 0;

int PE = 0;
int PA = 0;

int fenetre[TAILLE_FENETRE] = {[0 ... TAILLE_FENETRE-1] = 1};
int indice_fenetre = 0;  
int perte_tolere = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
    mic_tcp_sock sock;
    
    int result = -1;
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    result = initialize_components(sm); /* Appel obligatoire */
    set_loss_rate(10);
    
    if (result != -1){
        sock.fd = nb_fd; // definir le numero de soc
        sock.state = IDLE; 
        nb_fd += 1;
        sockets[sock.fd] = sock; // mettre le sock dans la table de sockets.
        result = sock.fd;
    } 

    return result;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    for (int i = 0; i < MAX_SOCKET; i++){
        if (i != socket && sockets[socket].local_addr.port == addr.port){
            return -1;
        } 
    } 
    sockets[socket].local_addr = addr;
    return 0; 

}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    int result = -1;

    if (pthread_mutex_lock(&mutex)){
        printf("Erreur mutex lock\n");
        exit(-1);
    }

    // attendre le condition à réaliser
    pthread_cond_wait(&cond, &mutex);

    if (pthread_mutex_unlock(&mutex)){
        printf("Erreur mutex unlock\n");
        exit(-1);
    }

    // ACK bien reçu
    sockets[socket].state = CONNECTED;
    result = 0;
    return result;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    int result = -1;
    int ctrl_syn_ack = 0;

    sockets[socket].remote_addr  = addr;
    mic_tcp_sock sock = sockets[socket];   
    mic_tcp_sock_addr local_addr = sock.local_addr;
    mic_tcp_sock_addr remote_addr = sock.remote_addr;

    if (socket == sock.fd){

        // création pdu SYN
        mic_tcp_pdu syn;
        syn.header.source_port = local_addr.port;
        syn.header.dest_port = remote_addr.port;
        syn.header.ack = 0;
        syn.header.syn = 1;

        // mettre le choix de taux de pertes tolérés (loss_rate) de client dans le payload de SYN
        int client_loss_rate = CLIENT_LOSS_RATE;
        syn.payload.size = sizeof(int);
        syn.payload.data = malloc(sizeof(int));
        memcpy(syn.payload.data, &client_loss_rate, sizeof(int));

        // envoyer pdu SYN
        if (IP_send(syn, addr.ip_addr) == -1){
            printf("erreur a envoyer ack\n");
        } 

        // ouvrir des mémoire necessaire pour le récuperation de SYN_ACK
        mic_tcp_pdu syn_ack;
        mic_tcp_ip_addr local_ip;
        mic_tcp_ip_addr remote_ip;

        // allocation de mémoire
        local_ip.addr_size = IP_ADDR_MAX_LEN;
        local_ip.addr = malloc(IP_ADDR_MAX_LEN);
        remote_ip.addr_size = IP_ADDR_MAX_LEN;
        remote_ip.addr = malloc(IP_ADDR_MAX_LEN);
        syn_ack.payload.size = sizeof(int);
        syn_ack.payload.data = malloc(sizeof(int)); 

        while (ctrl_syn_ack == 0){
            // récuperer pdu SYN_ACK
            int recv_syn_ack = IP_recv(&syn_ack, &local_ip, &remote_ip, TIMEOUT);

            // vérifier s'il est bien SYN_ACK
            if ((recv_syn_ack != -1) && (syn_ack.header.ack == 1) && (syn_ack.header.syn == 1)){
                ctrl_syn_ack =1;

                // création pdu ACK
                mic_tcp_pdu ack;
                ack.header.source_port = local_addr.port;
                ack.header.dest_port = remote_addr.port;
                ack.header.ack = 1;
                ack.header.syn = 0;
                ack.payload.size = 0;
                ack.payload.data = NULL;

                // envoyer ACK
                if (IP_send(ack, addr.ip_addr) == -1){
                    printf("erreur a envoyer ack\n");
                } else {
                    result = 0;
                }

                // Continuer à écouter d'éventuels SYN-ACK dupliqués
                int wait = 1;
                int wait_count = 0;
                while (wait && wait_count < 5) { // 5 tentatives max
                    int again = IP_recv(&syn_ack, &local_ip, &remote_ip, TIMEOUT);
                    if ((again != -1) && (syn_ack.header.ack == 1) && (syn_ack.header.syn == 1)) {
                        // Renvoyer l'ACK si SYN-ACK dupliqué reçu
                        if (IP_send(ack, addr.ip_addr) == -1){
                            printf("erreur a renvoyer ack\n");
                        }
                        wait_count++;
                    } else {
                        wait = 0;
                    }
                }
            } else {
                // renvoyer SYN si pas reçu SYN_ACK
                if (IP_send(syn, addr.ip_addr) == -1){
                    printf("erreur a envoyer ack\n");
                } 
            }
        } 
    } else {
        result = -1;
    } 

    return result;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    int send = -1;
    int control = 0;

    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    // pdu ack
    mic_tcp_pdu ack;
    
    // socket
    mic_tcp_sock sock = sockets[mic_sock];
    mic_tcp_sock_addr local_addr = sock.local_addr;
    mic_tcp_sock_addr remote_addr = sock.remote_addr;

    // pdu message
    mic_tcp_pdu pdu;

    if (mic_sock == sock.fd){
        // definir le pdu à envoyer  
        pdu.header.dest_port = remote_addr.port;
        pdu.header.source_port = local_addr.port;
        pdu.header.seq_num = PE;
        pdu.header.ack = 0;
        pdu.header.syn = 0;
        pdu.header.fin = 0;

        // mettre le message dans pdu
        pdu.payload.size = mesg_size;
        pdu.payload.data = mesg;

        //PE = (PE + 1) % 2; // incrementer PE

        // envoyer le pdu a address remote ip
        if ((send = IP_send(pdu, remote_addr.ip_addr)) == -1){
            printf("error envoyer pdu\n");
        } 

        // adresse ip de local et remote pour ack
        mic_tcp_ip_addr local_addr_ip;
        mic_tcp_ip_addr remote_addr_ip;

        local_addr_ip.addr_size = IP_ADDR_MAX_LEN;
        local_addr_ip.addr = malloc(IP_ADDR_MAX_LEN);
        remote_addr_ip.addr_size = IP_ADDR_MAX_LEN;
        remote_addr_ip.addr = malloc(IP_ADDR_MAX_LEN);

        ack.payload.size = 0;

        // controle de recuperation ack
        while (control == 0){
            // si on a bien reçu pdu ack qufloat) vient de remote addr
            int ret = IP_recv(&ack, &local_addr_ip, &remote_addr_ip, TIMEOUT);
            if ((ret != -1) && (ack.header.ack == 1) && (ack.header.syn == 0) && (ack.header.ack_num != PE)){
                printf("ack bien reçu\n"); // affiche debug message
                fenetre[indice_fenetre] = 1;  // success
                indice_fenetre = (indice_fenetre + 1) % TAILLE_FENETRE;
                PE = (PE + 1) % 2;
                control = 1; // arreter boucle
                 

            } else if (ret == -1) { // expiration de timer

                fenetre[indice_fenetre] = 0; // pas de success
                
                // Calcul nb perte
                int pertes = 0;
                for (int i = 0; i < TAILLE_FENETRE; i++){
                    if (fenetre[i] == 0){
                        pertes++;
                    } 
                } 

                //comparer le taux de perte avec PERTES_TOLERES
                if (100*pertes/TAILLE_FENETRE <= perte_tolere){
                    indice_fenetre = (indice_fenetre + 1) % TAILLE_FENETRE;
                    control = 1;

                } else { // perte non tolere, renvoie pdu

                    if ((send = IP_send(pdu, remote_addr.ip_addr)) == -1){
                        printf("error envoyer pdu\n");
                    } 

                } 
            } 
        } 
    } else {
        send = -1;
    }  

    return send;
    
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    int recv = -1;
    mic_tcp_sock sock = sockets[socket];

    // message reçu
    mic_tcp_payload payload;
    payload.size = max_mesg_size;
    payload.data = mesg;

    // recevoir le message
    if (sock.fd == socket){
        recv = app_buffer_get(payload); 
    } 
    return recv;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    sockets[socket].state = CLOSED; 
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    int client_loss_rate;
    int server_loss_rate = SERVER_LOSS_RATE;
    int final_loss_rate;

    // vérifier si pdu reçu est SYN pendant la connexion
    if ((pdu.header.syn == 1) && (pdu.header.ack == 0)){

        // récuperer numéro de socket
        int sock = -1;
        for (int i = 0; i < MAX_SOCKET; i++){
            if (sockets[i].local_addr.port == pdu.header.dest_port){
                sock = i;
                break;
            } 
        } 

        if (sock != -1){
            // store adresse ip de client
            sockets[sock].remote_addr.ip_addr = remote_addr; 

            // creation pdu syn_ack
            mic_tcp_pdu syn_ack;
            syn_ack.header.dest_port = pdu.header.source_port;
            syn_ack.header.source_port = pdu.header.dest_port;
            syn_ack.header.ack = 1;
            syn_ack.header.syn = 1;
            syn_ack.payload.size = 0;
            syn_ack.payload.data = NULL;
                
            // envoyer SYN_ACK
            if ((IP_send(syn_ack,remote_addr)) == -1){
                printf("error envoyer pdu\n");
            } 

            // changement de l'état à WAIT_ACK
            sockets[sock].state = WAIT_ACK; 

            // récuperer loss_rate de client que SYN a apporté
            memcpy(&client_loss_rate, pdu.payload.data, sizeof(int));
            
            // comparaison des loss_rate et choisir final_loss_rate
            if (client_loss_rate <= server_loss_rate){
                final_loss_rate = client_loss_rate;
            } else {
                final_loss_rate = server_loss_rate;
            }
            
            // mettre le choix final comme perte tolere
            perte_tolere = final_loss_rate;
        } 

        
            
    }

    // vérifier si pdu reçu est ACK pendant la connexion
    else if ((pdu.header.syn == 0) && (pdu.header.ack == 1)){
        
        printf("ack recieved\n");
        // récuperer numéro de socket
        int sock = -1;
        for (int i = 0; i < MAX_SOCKET; i++){
            if (sockets[i].local_addr.port == pdu.header.dest_port){
                sock = i;
                break;
            }
        }

        // changer l'état à ACK_RECEIVED
        if (sock != -1){
            sockets[sock].state = ACK_RECEIVED;

            // signaler mutex en attente
            pthread_cond_broadcast(&cond);
        } 
    } 
    
    // cas message PDU
    else if ((pdu.header.syn == 0) && (pdu.header.ack == 0)){  

        mic_tcp_pdu ack;

        if (pdu.header.seq_num == PA){
            app_buffer_put(pdu.payload);
            PA = (PA + 1) % 2; 
        } 

        // création ACK
        ack.header.source_port = pdu.header.dest_port;
        ack.header.dest_port = pdu.header.source_port;
        ack.header.ack_num = PA;
        ack.header.ack = 1;
        ack.header.syn = 0;
        ack.payload.size = 0;
        ack.payload.data = NULL;

        // envoyer ACK
        if (IP_send(ack, remote_addr) == -1){
            printf("erreur a envoyer ack\n");
        } 
    } 
}