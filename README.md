# BE RESEAU
## TPs BE Reseau - 3 MIC

=======


Ce projet a été réalié dans le cadre du bureau d'étude de réseau de la 3MIC/IR par GUILLIEN Corentin et RAMANGASON Notahiana Erwan. 

Il s'agit de créer un protocole mic-tcp, alternative de TCP pour le transfert de vidéo qui garantisse une certaine fiablilité, avec une tolérance de perte.

## Commandes

Ce programme est réalisé en langage C. Il suffit donc d'avoir un compilateur C. Pour le compiler, nous disposons d'un MakeFile qui se lance avec la commande 

    make

afin de générer les exécutables.

Les tests ont été effectué sur des transferts de textes et d'une vidéo "Star Wars". Les 2 applications fournies sont tsock_texte et tsock_video, elles peuvent être lancées soit en mode puits, soit en mode source selon la syntaxe suivante:

    Usage: ./tsock_texte [-p|-s destination] port
    Usage: ./tsock_video [[-p|-s] [-t (tcp|mictcp)]


## Versions

Afin d'avancer dans le projet, nous avons fournis différentes versions de notre protocole. Nos implémentations se trouvent principalement dans le fichier

    src/mictcp.c

### version v1.

Il s'agit de la première version dans laquelle notre protocole ne gère ni la fiabilité, ni établissement de connexion mais gère seulement l'envoie des données (sans reprise de perte).
La v1 est fonctionnelle (texte et vidéo)

### version v2

Il s'agit ici d'une extension de la v1 où on ne gère toujours pas la phase d'établissement de la connexion mais on a une garantie de fiablilté totale (avec un mécanisme de reprise de perte Stop and Wait)
La v2 est fonctionnelle (texte et vidéo)

### version v3

On ne gère toujours pas l'établissement de connection.
Il s'agit ici de garder une certaine fiabilité du transfert des données avec un mécanisme de reprise de perte qui autorise certains paquets perdu pour éviter que la vidéo ne s'arrête à chaque fois. Le calcul du taux de perte acceptable est faite grace à une fenêtre glissante et est déterminé de façon statique.
On a choisit une taille de 100 pour notre fenetre pour gérer plus aisément le % de perte.
La v3 est fonctionnelle (texte et vidéo)

A noté que dans ces 3 première version, on n'utilise qu'un socket par programme. On a donc pas eu besoin de créer un tableau de socket car chaque programme gère son propre socket

### version v4

#### v4.1

C'est ici qu'on gère la phase d'établissement de connexion. On négocie aussi le taux de pourcentage pendant cette phase
La v4.1 est fonctionnelle

#### v4.2

A continuer
Dans cette version, on gère plusieurs communication, c'est à dire que le puits (programme principal) se mettra toujours en attente et génèrera des threads à chaque nouvelle demande de connexion. 
On a mis en place en place le système pour récuperer le descripteur de fichier du socket à partir du tid du thread. Il reste à implémenter le lancement du thread côté puits qui va gérer la réception des données de la source tandis qu'un autre thread s'occupe des établissements de connexions. 

### Limites du projet:

La source devrait avoir un numéro de port associé puisqu'elle doit être en mesure de recevoir un ACK. Ici on envoie l'ack sans se préoccuper du numéro de port. 

Il n'y a qu'un seul buffer, le buffer n'est donc pas réellement associé à un socket mais directement à l'application. 

