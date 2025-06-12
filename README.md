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

   
## Choix d'implémentation


## Bénéfices MICTCP-v4 par rapport à TCP & MICTCP-v2 

