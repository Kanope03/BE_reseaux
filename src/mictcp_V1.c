#include "mictpc.h"

mic_tcp_socket mon_socket;

int compteur_socket = 0;
const int nb_max_socket = 10;


mic_tcp_socket *sockets = malloc(sizeof(mic_tcp_socket) * nb_max_socket); //A terme créer une structure de dictionnaire/hashMap
mic_tcp_socket *sockets_distant_associes = malloc(sizeof(mic_tcp_socket) * nb_max_socket);
/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm) {
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
   if(result){
   	fprintf(stderr, "initialize_components a échouée dans mic_tcp_socket lors de la création du socket %d \n", compteur_socket);
   	exit(EXIT_FAILURE);
   }

   // On choisi un numéro de port local associé à ce socket (>1024, unique)
   mon_socket.fd = compteur_socket++;
   return mon_socket.fd; 
}


/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr) {
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   sockets[socket].addr = addr;
   return 0;
}
/*
 * Met le socket en état d'acceptation de connexion
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr) {
	printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
	// on fait un mictcp sans connexion, donc on fait rien
	return 0; 
}
/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect (int socket, mic_tcp_sock_addr addr) {

	printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
	sockets_distant_associes[socket].addr = addr;
	
	return 0;
}
/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
 
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size) {
	printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
	Mic_tcp_pdu pdu;
	pdu.header.source_port = sockets[mic_sock].addr.port; 
	pdu.header.dest_port = sockets_distant_associes[mic_sock].addr.port;

	pdu.payload.data = mesg;
	pdu.payload.size = mesg_size;
	
	int effective_send = IP_send(pdu, sockets_distant_associes[mic_sock].addr.port);
	
        return effective_send;
}
/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
 
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size) {
	printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
	mic_tcp_payload payload;
	payload.data = mesg;
	payload.size = max_mesg_size;	
	int effectivement_lues_du_buffer = app_buffer_get(payload); //L'appel à app_buffer_get est déjà bloquant. 
	return effectivement_lues_du_buffer;
}
/*
 * Permet de traiter un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis d'insérer les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put(). Elle est appelée par initialize_components()
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr) {
   	printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
	// Vérifions : que le port destination du pdu est un port local attribué à un socket mictcp
	
	int est_port_associe_sock_local = 0;
	for(int i = 0; i < compteur_socket; i++){
		if(sockets[i].addr.port == pdr.header.dest_port) 
			est_port_associe_sock_local = 1;

		
	}

	//Vérifier des numéros de séquence => pas dans cette version
	
	app_buffer_put(pdu.payload);
	// Envoyer un ack => pas dans cette version
	
	/* A compléter dans la prochaine version. 
	Mic_tcp_pdu pdu_ack;
	Pdu_ack.header;
	Pdu_ack.payload.size =0;
	IP_send(pdu_ack, addr)
	*/
}
/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket) {
	printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
	return -1; }


/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
}
