#include <mictcp.h>
#include <api/mictcp_core.h>

#define MAX_SOCKET 5
#define TIMEOUT 10
#define TAILLE_FENETRE 100
#define PERTE_TOLERES 10

mic_tcp_sock sockets[MAX_SOCKET] ; // table de sockets

int nb_fd = 0;
int PE = 0;
int PA = 0;

int fenetre[TAILLE_FENETRE] = {[0 ... TAILLE_FENETRE-1] = 1};
int indice_fenetre = 0;  

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
    set_loss_rate(5);
    
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
    return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    if (socket > MAX_SOCKET){
        return -1;
    }  
    sockets[socket].remote_addr = addr; 
    return 0;
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
                if (100*pertes/TAILLE_FENETRE <= PERTE_TOLERES){
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

    // socket
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

    mic_tcp_pdu ack;

    if (pdu.header.seq_num == PA){
        app_buffer_put(pdu.payload);
        PA = (PA + 1) % 2;
        printf("pdu bien reçu\n");
    } 

    ack.header.source_port = pdu.header.dest_port;
    ack.header.dest_port = pdu.header.source_port;
    ack.header.ack_num = PA;
    ack.header.ack = 1;
    ack.header.syn = 0;

    ack.payload.size = 0;

    if (IP_send(ack, remote_addr) == -1){
        printf("erreur a envoyer ack\n");
    } 
}
