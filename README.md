# BE RESEAU
## TPs BE Reseau - 3 MIC-E : Mathis Morra-Fischer & Efe Inkaya

## Compilation du protocole mictcp et lancement des applications de test fournies

Pour compiler mictcp et générer les exécutables des applications de test depuis le code source fourni, taper :

    make

Deux applicatoins de test sont fournies, tsock_texte et tsock_video, elles peuvent être lancées soit en mode puits, soit en mode source selon la syntaxe suivante:

    Usage: ./tsock_texte [-p|-s destination] port
    Usage: ./tsock_video [[-p|-s] [-t (tcp|mictcp)]

Seul tsock_video permet d'utiliser, au choix, votre protocole mictcp ou une émulation du comportement de tcp sur un réseau avec pertes.

## Ce qui fonctionne et ne fonctionne pas

Tous fonctionne correctement.

   
## Choix d'implémentation

### Stop-and-Wait avec fiabilité partielle

Nous avons choisi d’implémenter un protocole de type stop-and-wait, adapté pour la fiabilité partielle. À chaque envoi de PDU, l’émetteur attend un ACK correspondant avant de passer au suivant. Si l’ACK n’arrive pas, le PDU est retransmis, sauf si le taux de pertes toléré est atteint.

Pour mesurer ce taux de pertes, nous utilisons une fenêtre glissante de taille fixe (définie par `TAILLE_FENETRE`). Cette fenêtre est implémentée comme un buffer circulaire qui enregistre le succès ou l’échec des derniers envois. Cela permet de calculer dynamiquement le taux de pertes sur les transmissions récentes, ce qui évite d’accepter trop d’erreurs consécutives et améliore la qualité de service, notamment pour la vidéo.

### Négociation du taux de pertes

Lors de l’établissement de la connexion, le client et le serveur proposent chacun un taux de pertes maximal acceptable (respectivement `CLIENT_LOSS_RATE` et `SERVER_LOSS_RATE`). Le serveur choisit la valeur la plus contraignante (la plus faible) et l’applique pour la session. Ce choix garantit que la contrainte la plus stricte est respectée des deux côtés.

### Gestion de la connexion (SYN, SYN-ACK, ACK)

La connexion suit un schéma inspiré de TCP : le client envoie un SYN, le serveur répond par un SYN-ACK, puis le client termine avec un ACK. Nous avons ajouté une gestion robuste des duplications et pertes de paquets :  
- Le serveur renvoie un SYN-ACK à chaque SYN reçu (même dupliqué).
- Le client renvoie un ACK à chaque SYN-ACK reçu (même dupliqué), ce qui permet de gérer la perte de paquets lors de l’établissement de la connexion.

### Asynchronisme serveur

Pour gérer l’asynchronisme entre le thread applicatif (accept) et le thread réceptif (réception des PDU), nous utilisons un mutex et une variable de condition (`pthread_cond_t`). Le thread applicatif reste bloqué dans `mic_tcp_accept` tant qu’aucune connexion n’est établie, et il est réveillé par le thread réceptif dès qu’un ACK de connexion est reçu.

## Bénéfices de notre MICTCP-v4.2

Notre version de MICTCP permet une fiabilité partielle configurable, ce qui est particulièrement adapté aux applications multimédia (vidéo, audio temps réel) où la fluidité prime sur la fiabilité absolue. En tolérant un certain taux de pertes, on évite les blocages et les délais dus aux retransmissions systématiques, ce qui améliore l’expérience utilisateur par rapport à TCP ou à une version de MICTCP-v2 sans gestion fine des pertes.

