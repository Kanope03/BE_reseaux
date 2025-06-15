# BE RESEAU
## TPs BE Reseau - 3 MIC

###Version 2
La version V2 est fonctionnelle. 
Problème restant : La vidéo semble refuser de ce lancer si le premier paquet est réenvoyé. 

###Version 3

###Version 4.1

Limite du projet:

La source devrait avoir un numéro de port associé puisqu'elle doit être en mesure de recevoir un ACK. Ici on envoie l'ack sans se préoccuper du numéro de port. 

Il n'y a qu'un seul buffer, le buffer n'est donc pas réellement associé à un socket mais directement à l'application. 
