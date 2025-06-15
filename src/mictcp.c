#include "../include/mictcp.h"
#include "api/mictcp_core.h"
#include <strings.h>


#define NB_MAX_SOCKET 10

const long timeout = 1000;
const int limite_envoie = 10;

int compteur_socket = 0;
int seq_attendu = 0;
int seq_envoye = 0;
int dernier_num_seq_traite = -1;
int rate_loss = 5;

mic_tcp_sock sock;

//Dans cette v2, on ne gère pas encore l'établissement de connexion

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   set_loss_rate(rate_loss);

   if(initialize_components(sm) == -1){
   	fprintf(stderr, "initialize_components a échouée dans mic_tcp_socket lors de la création du socket %d \n", compteur_socket);
   	exit(EXIT_FAILURE);
   }
   compteur_socket++;

   if(compteur_socket>=NB_MAX_SOCKET){
    fprintf(stderr, "Trop de demande de connexion");
   	exit(EXIT_FAILURE);
   }
   sock.fd = compteur_socket;   //Attribution d'un numero et modification de l'etat du socket
   sock.state = CLOSED;

   return sock.fd;
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   sock.local_addr = addr;

    //fprintf(stderr, "Le num de port est %d\n", sock.local_addr.port);

   return 0;    
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    //Dans la v2, on ne traite pas encore la phase d'établissement de connexion
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

    sock.remote_addr = addr;
	return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
	
    mic_tcp_pdu pdu, recv_pdu;

    recv_pdu.payload.size = 0;

    //Initialisation du message à envoyer (headers et pauload)
    pdu.header.dest_port = sock.remote_addr.port;
    pdu.header.seq_num = seq_envoye;
    pdu.header.syn = 0;
    pdu.header.ack = 0;
    pdu.header.fin = 0;

    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    int effective_sent = 0;

    sock.remote_addr.ip_addr.addr_size = 0;
    sock.local_addr.ip_addr.addr_size = 0;

    effective_sent = IP_send(pdu, sock.remote_addr.ip_addr);

    //Si on n'a pas reçu de ack jusqu'à expiration du timeout, on renvoie le message
    if(IP_recv(&recv_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, timeout) == -1){
        return mic_tcp_send(sock.fd, mesg, mesg_size);
    }

    //Si on reçoit un ack mais ce n'est pas le bon numéro d'acquittement, on renvoie toujours le message
	if(recv_pdu.header.ack_num != seq_envoye){
        printf("Le numero d'ack recu (%d) n'est pas le bon, celui attendu était %d, on renvoie la donnée", recv_pdu.header.ack_num, seq_envoye);
        return mic_tcp_send(sock.fd, mesg, mesg_size);
    }

    //Sinon, on a reçu le ack et c'est bien le bon, on incrément et on renvoie la taille du message envoyé
    seq_envoye = (seq_envoye+1)%2;

    return effective_sent;
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
	mic_tcp_payload payload;
	payload.data = mesg;
	payload.size = max_mesg_size;	
	int effectivement_lues_du_buffer = app_buffer_get(payload); //L'appel à app_buffer_get est déjà bloquant. 
	return effectivement_lues_du_buffer;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    return -1;
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
    
    if(pdu.header.dest_port != sock.local_addr.port){
        fprintf(stderr, " %d != %d : Le port de destination du pdu n'est pas un port local attribué à un socket mictcp\n", pdu.header.dest_port, sock.local_addr.port);
        return;
	}
    
    //Initialisation du ack à envoyer
    mic_tcp_pdu ack_pdu;
    ack_pdu.header.syn = 0;
    ack_pdu.header.ack = 1;
    ack_pdu.header.fin = 0;
    ack_pdu.header.dest_port = sock.remote_addr.port;
    ack_pdu.header.source_port = sock.local_addr.port;
    ack_pdu.payload.size = 0;
    

    //Si le message reçu a déjà été traité, on renvoie le ack correspondant
    if(pdu.header.seq_num == dernier_num_seq_traite){
        ack_pdu.header.ack_num = dernier_num_seq_traite;
        IP_send(ack_pdu, remote_addr);
    }

    //Si on recoit le bon message attendu, on le met dans le buffer, on note que le message a déjà été traité, on envoie le ack et on incrémente le prochain numéro de séquence attendu
    else if(pdu.header.seq_num == seq_attendu){
        ack_pdu.header.ack_num = seq_attendu;
        app_buffer_put(pdu.payload);
        dernier_num_seq_traite = seq_attendu;
        IP_send(ack_pdu, remote_addr);
        seq_attendu = (seq_attendu+1) %2; 

    }
    
}
