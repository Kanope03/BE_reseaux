#include "../include/mictcp.h"
#include "api/mictcp_core.h"


#define NB_MAX_SOCKET 10

const long timeout = 10000;
const int limite_envoie = 10;

int compteur_socket = 0;
int seq_attendu = 0;
int seq_envoye = 0;

mic_tcp_sock sock;



/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   set_loss_rate(0);

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
	sock.state = IDLE;
    sock.remote_addr = *addr;
    //socket_local.remote_addr = *addr;
	return 0; 
	

    /*
    

    mic_tcp_pdu pdu_syn, pdu_synack, pdu_ack;
    int compteur_envoie = 10;
    //On se met en attente d'un SYN puis on recupere le SYN
    if(IP_recv(&pdu_syn, &socket_local.local_addr.ip_addr, &socket_distant_associe.local_addr.ip_addr, timeout)){printf("Error: IP_recv error for the first ack");return -1;}
    //On envoie un ack
    if(pdu_syn.header.syn == 1){
        pdu_synack.header.ack = 1;
        pdu_synack.header.syn = 1;
        if(IP_send(pdu_synack, socket_local.local_addr.ip_addr)){printf("Error: IP_send didn't send the synack");return -1;}
        //On se met en attente d'un ack
        for( int i=0; i<compteur_envoie; i++){
        if(IP_recv(&pdu_ack, &socket_local.local_addr.ip_addr, &socket_distant_associe.local_addr.ip_addr, timeout)){
            //Si le teimer expire, on renvoie le ack
            if(IP_send(pdu_synack, socket_local.local_addr.ip_addr)){printf("Error: IP_send didn't send the synack");return -1;}
        }
        //Si il accepte la connection
        if(pdu_ack.header.ack == 1){
            socket_local.remote_addr = *addr;
            return 0; 
        }
    }   

    }*/
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

    sock.remote_addr = addr;

    fprintf(stderr, " [CONNECT ] adresse sock distant %s \n", sock.remote_addr.ip_addr.addr);

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
	//Initialisation des headers
    pdu.header.source_port = sock.local_addr.port; 
	pdu.header.dest_port = sock.remote_addr.port;
    pdu.header.seq_num = seq_envoye;
    //seq_num a déjà été initialisé en global

    //On charge le message
	pdu.payload.data = mesg;
	pdu.payload.size = mesg_size;
	
    //envoie du PDU
	int effective_send = IP_send(pdu, sock.remote_addr.ip_addr);
	
    //Il se met en attente du pdu, avec le timer timeout

    int received, compteur = 0;
    do{
        do{
            if((received = IP_recv(&recv_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, timeout))==-1){
                printf("Sans surprise ça vaut %d\n\n", received);
                //Si le timer expire, on renvoie le pdu
                IP_send(pdu, sock.remote_addr.ip_addr);
                compteur++;
            }
        }while(received ==-1 && compteur <= limite_envoie); 

        fprintf(stderr, "Le numéro de séquence envoyé est le %d et l'ack reçu est le %d\n", seq_envoye, recv_pdu.header.ack_num);

    }while(recv_pdu.header.ack_num !=seq_envoye);
    
    

    seq_envoye = (seq_envoye + 1) % 2;
    return effective_send;

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
        fprintf(stderr, "Le port de destination du pdu n'est pas un port local attribué à un socket mictcp\n");
        return;
	}
    //fprintf(stderr, "jusqu'ici tout va bien \n");
    //exit(0);

    if(pdu.header.ack){
        if(sock.state == SYN_RECEIVED){

        sock.local_addr.ip_addr = local_addr;
        sock.remote_addr.ip_addr = remote_addr;

        sock.state = ESTABLISHED;

        }else if(sock.state == SYN_SENT){
            sock.remote_addr.ip_addr = remote_addr; 
            sock.remote_addr.port = pdu.header.dest_port;
            sock.state = ESTABLISHED;
            
            //IP_send();

        }
    }
    else if(pdu.header.fin){
        return;
    }
    else{

        if(seq_attendu == pdu.header.seq_num){
            mic_tcp_pdu ack_pdu;

            fprintf(stderr, " adresse sock distant %s \n", remote_addr.addr);
            fprintf(stderr, "port source %d, port dest : %d\n", sock.local_addr.port, sock.remote_addr.port);


            ack_pdu.header.source_port = sock.local_addr.port; 
            ack_pdu.header.dest_port = sock.remote_addr.port;
            
            ack_pdu.header.ack = 1;
            ack_pdu.header.ack_num = pdu.header.seq_num;

            IP_send(ack_pdu, remote_addr);
            
            seq_attendu = (seq_attendu + 1) % 2;

            app_buffer_put(pdu.payload);
        }

        
    }
    
}

char *number_to_state(protocol_state state){
        switch (state)
        {
        case IDLE:
            return "IDLE";
            break;
        
        case CLOSED:
            return "CLOSED"; 
            break;

        case SYN_SENT:
            return "SYN_SENT"; 
            break;

        case SYN_RECEIVED:
            return "SYN_RECEIVED"; 
            break;

        case ESTABLISHED:
            return "ESTABLISHED"; 
            break;

        case CLOSING:
            return "CLOSING"; 
            break;
        
        default:
            return "UNKNOWN STATE";
            break;
        }
        
    }
