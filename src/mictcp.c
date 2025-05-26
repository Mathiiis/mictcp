#include <mictcp.h>
#include <api/mictcp_core.h>
#include <pthread.h>

#define MAX_SOCKET 5
<<<<<<< Updated upstream
#define TIMEOUT 10
#define TAILLE_FENETRE 10
#define PERTES_TOLERES 2
=======
#define TIMEOUT 101
#define TAILLE 15
>>>>>>> Stashed changes

mic_tcp_sock sockets[MAX_SOCKET]; // table de sockets

int nb_fd = 0;
int PE = 0;
int PA = 0;
int fenetre[TAILLE_FENETRE] = {[0 ... TAILLE_FENETRE-1] = 1}; // succes 1, perte 0
int indice_fenetre = 0;

int tab[TAILLE] = {[0 ... TAILLE-1] = 1}; //febetre glissante
int indice_prochain_mesg = 0;
int perte_tolere = 10;
int perte_final = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER ;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER ; 

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    
    mic_tcp_sock sock;

    int result = initialize_components(sm); /* Appel obligatoire */
    
    if (result != -1) {
        set_loss_rate(20);
        sock.fd = nb_fd;
        sock.state = IDLE;
        nb_fd += 1;
        sockets[sock.fd] = sock;
        result = sock.fd;
        
        return result;

    } else {
        return -1;
    }
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */

int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    
    for(int i = 0; i < MAX_SOCKET; i++) {
            if (i != socket && sockets[i].local_addr.port == addr.port) {       
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

    mic_tcp_sock sock = sockets[socket];

    if (socket == sock.fd){

        // Etape attente SYN
        sock.state = WAIT_SYN; // Mettre le socket en etat d'attente SYN

        if (pthread_mutex_lock(&mutex) != 0){
            printf("erreur de verrouillage");
        }

        while (sock.state == WAIT_SYN){
            pthread_cond_wait(&condition, &mutex);
        }

        if (pthread_mutex_unlock(&mutex) != 0){
            printf("erreur de deverrouillage");
        }

        // Creation de PDU SYN_ACK
        mic_tcp_pdu syn_ack;
        syn_ack.header.source_port = sock.remote_addr.port;
        syn_ack.header.dest_port = addr->port;
        syn_ack.header.syn = 1;
        syn_ack.header.ack = 1;
        syn_ack.header.seq_num = 0;
        syn_ack.header.ack_num = perte_tolere;
        syn_ack.payload.size = 0;

        // Envoi le PDU SYN_ACK
        if (IP_send(syn_ack, sock.local_addr.ip_addr) == -1){
            printf("erreur d'envoi'");
        }

        sock.state = WAIT_ACK;
        
        // Etape attente ACK
        if (pthread_mutex_lock(&mutex) != 0){
            printf("erreur de verrouillage");
        }

        while (sock.state == WAIT_ACK){
            pthread_cond_wait(&condition, &mutex);
        }

        if (pthread_mutex_unlock(&mutex) != 0){
            printf("erreur de deverrouillage");
        }

        return 0;
    } else {
        return -1;
    }  
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    if (socket > MAX_SOCKET) {
        return -1;
    }

    mic_tcp_sock sock = sockets[socket];
    mic_tcp_pdu syn, syn_ack, ack;
    int pourcentage_perte;

    sock.remote_addr = addr;

    syn.header.source_port = sock.local_addr.port;
    syn.header.dest_port = sock.remote_addr.port;
    syn.header.syn = 1;
    syn.header.ack = 0;
    syn.header.seq_num = 0;
    syn.header.ack_num = 0;
    syn.payload.size = 0;

    int syn_ack_control = 0;

    while (syn_ack_control == 0) {

        if (IP_send(syn, sock.remote_addr.ip_addr) == -1) {
            printf("erreur d'envoi");
        }

        sock.state = WAIT_SYN_ACK;

        if ((IP_recv(&syn_ack, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, TIMEOUT) != -1) 
                && (syn_ack.header.syn = 1) 
                && (syn_ack.header.ack = 1)) {

            syn_ack_control = 1; // bien recuperer syn_ack

            pourcentage_perte = syn_ack.header.ack_num;

            if (pourcentage_perte < perte_tolere) {
                perte_final = pourcentage_perte;
            } else {
                perte_final = perte_tolere ;
            }

            // Creation PDU ACK
            ack.header.source_port = sock.local_addr.port;
            ack.header.dest_port = sock.remote_addr.port;
            ack.header.seq_num = 0;
            ack.header.ack_num = perte_final;
            ack.header.syn = 0;
            ack.header.ack = 1;
            ack.header.fin = 0;
            ack.payload.size = 0;

            // Envoi PDU ACK
            if (IP_send(ack, sock.remote_addr.ip_addr) == -1) {
                printf("erreur de l'envoi de ACK");
            }
            sock.state = CONNECTED;
        }
    }
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    
    int sent = -1;
    int control = 0;

    mic_tcp_pdu pdu;
    mic_tcp_pdu ack;

    mic_tcp_sock sock = sockets[mic_sock];
    mic_tcp_sock_addr remote_addr = sockets[mic_sock].remote_addr;
    mic_tcp_sock_addr local_addr = sockets[mic_sock].local_addr;

    if (mic_sock == sock.fd){
        pdu.header.ack = 0;
        pdu.header.dest_port = remote_addr.port;
        pdu.header.seq_num = PE;

        pdu.payload.data = mesg;
        pdu.payload.size = mesg_size;

<<<<<<< Updated upstream
        PE = (PE + 1) % 2; // Incrémentation du PE
=======
        PE = (PE + 1) % 2; // incrementation de PE
>>>>>>> Stashed changes

        // Sinon, stop-and-wait
        sent = IP_send(pdu, remote_addr.ip_addr);

        while (control == 0){
<<<<<<< Updated upstream
            int ret = IP_recv(&(ack), &local_addr.ip_addr, &remote_addr.ip_addr, TIMEOUT);
            // ACK recu correspondant au pdu
            if(ret != -1 && (ack.header.ack == 1) && (ack.header.ack_num == PE)){
                control = 1;
                fenetre[indice_fenetre] = 1;
                indice_fenetre = (indice_fenetre + 1) % TAILLE_FENETRE; // Incrémenter indice de la fenetre
            }
            // Expiration du timer 
            else if (ret == -1){ 
                fenetre[indice_fenetre] = 0;
                
                // Calcul taux de perte
                int pertes = 0;
                for (int i = 0; i < TAILLE_FENETRE; i++) {
                    if (fenetre[i] == 0) pertes++;
                }

                // Si le taux de pertes esttoléré, envoyer sans attendre l'ACK
                if (pertes <= PERTES_TOLERES) {
                    PE = (PE + 1) % 2;
                    control = 1;
                    indice_fenetre = (indice_fenetre + 1) % TAILLE_FENETRE;

                } 
                // Perte non toléré, renvoi du PDU
                else {
                    sent = IP_send(pdu, remote_addr.ip_addr);
                }

=======
            int recv_result = IP_recv(&(ack), &local_addr.ip_addr, &remote_addr.ip_addr, TIMEOUT);
            if (recv_result != -1 && (ack.header.ack == 1) && (ack.header.ack_num == PE)){
                control = 1;
                tab[indice_prochain_mesg] = 1;
                indice_prochain_mesg = (indice_prochain_mesg + 1) % TAILLE;
            } else if (recv_result == -1) {
                tab[indice_prochain_mesg] = 0;
                int  nb_mesg_recus = 0;

                for (int i = 0; i < TAILLE; i++){
                    nb_mesg_recus += tab[i];
                }

                if ((float)(TAILLE - nb_mesg_recus)/(TAILLE * 100) <= perte_tolere){
                    control = 1;
                    PE = ( PE + 1 ) % 2 ;
                    indice_prochain_mesg = (indice_prochain_mesg + 1) % TAILLE;
                } else {
                    sent = IP_send(pdu, remote_addr.ip_addr);
                }                
            } else {
                printf("acquitement recue different de celui attendu");
>>>>>>> Stashed changes
            }
        }
    } else {
        return -1;
    }
    return sent;
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

    mic_tcp_sock sock = sockets[socket];
    mic_tcp_payload payload;

    payload.data = mesg;
    payload.size = max_mesg_size;

    int recv = -1;

    if (socket == sock.fd){
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
    
    mic_tcp_pdu ack;

    if(pdu.header.seq_num == PA){
        app_buffer_put(pdu.payload);
        PA = (PA + 1) % 2; // incrementation de PA
    }

    ack.header.dest_port = pdu.header.source_port;
    ack.header.source_port = pdu.header.dest_port;
    ack.header.ack = 1;
    ack.header.ack_num = PA;
    ack.payload.size = 0;
    ack.payload.data = NULL;

    if(IP_send(ack, remote_addr) == -1){
        printf("erreur ack");
    }
}
