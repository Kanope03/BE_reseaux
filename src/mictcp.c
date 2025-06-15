#include "../include/mictcp.h"
#include "api/mictcp_core.h"
#include <strings.h>
#include <time.h>


#define NB_MAX_SOCKET 10

const long timeout = 1000;
const int limite_envoie = 10;

int compteur_socket = 0;
int seq_attendu = 0;
int seq_envoye = 0;
int dernier_num_seq_traite = -1;
int rate_loss = 30;
int perte = 20;     //Représente le taux de perte en %, cad par exemple sur une 10 paquet envoyé avec un taux à 20%, on peut autoriser 2 paquet perdu

int nb_message_envoye = 0;
int nb_message_recu = 0;

int fenetre[10] ={1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int taille_fenetre = 10;

//Fonctions qui serviront à gérer la perte grace à la fenetre
int afficher_taux_perte(){
    int taux = 0;
    for(int i=0; i<taille_fenetre; i++)
        taux += fenetre[i];
    return 100-(taux*10); 
} 
void glisser(int recu){
    for (int i = 0; i<taille_fenetre-1; i++)
        fenetre[i] = fenetre[i+1];
    fenetre[taille_fenetre-1] = recu;   
} 
void affiche_fenetre(){
    printf("Fenetre glissante:{");
    for(int i=0; i<taille_fenetre; i++) 
        printf("%d, ", fenetre[i] );
    printf("}\n");
} 

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

   sock.fd = compteur_socket;   //Attribution d'un numero et modification de l'etat du socket
   sock.state = IDLE;

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
 // addr n'est pas utilisé dans server.c (ni dans gateaway). 
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");


	sock.state = IDLE;

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

    

    mic_tcp_pdu syn_pdu;

    syn_pdu.payload.size = 0;
    syn_pdu.header.syn = 1;
    syn_pdu.header.ack = 0;
    syn_pdu.header.fin = 0;
    syn_pdu.header.dest_port = sock.remote_addr.port;
    syn_pdu.header.source_port = sock.local_addr.port;
    syn_pdu.payload.size = 0;

    mic_tcp_pdu syn_ack_pdu;
    syn_ack_pdu.payload.size = 0;


    IP_send(syn_pdu, addr.ip_addr);

    sock.state = SYN_SENT;

    if(IP_recv(&syn_ack_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, timeout*3) != -1){
        //Si l'on reçoit le syn_ack alors la connexion est établie (de notre pdv) et on envoie l'ack
        if(syn_ack_pdu.header.syn == 1 && syn_ack_pdu.header.ack == 1){
            sock.state = ESTABLISHED;
            mic_tcp_pdu ack_pdu;
            ack_pdu.header.syn = 0;
            ack_pdu.header.ack = 1;
            ack_pdu.header.fin = 0;
            ack_pdu.header.dest_port = sock.remote_addr.port;
            ack_pdu.header.source_port = sock.local_addr.port;
            ack_pdu.payload.size = 0;

            IP_send(ack_pdu, addr.ip_addr);
        }
    }else{
        printf("\n");
    }

    if(sock.state != ESTABLISHED){
        return -1;
    }

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
    recv_pdu.payload.size = 0 ;

    //Initialisation du message à envoyer (headers et payload)
    pdu.header.dest_port = sock.remote_addr.port;
    pdu.header.source_port = sock.local_addr.port;
    pdu.header.seq_num = seq_envoye;
    pdu.header.syn = 0;
    pdu.header.ack = 0;
    pdu.header.fin = 0;

    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    int effective_sent = 0;

    sock.remote_addr.ip_addr.addr_size = 0;
    sock.local_addr.ip_addr.addr_size = 0;

    int effective_received = -1;
    printf("Ack attendu normalement: %d\n", seq_envoye);


    //Si on n'a pas reçu de ack jusqu'à expiration du timeout
    while(effective_received == -1){
        IP_send(pdu, sock.remote_addr.ip_addr);

        
        effective_received = IP_recv(&recv_pdu, &sock.local_addr.ip_addr, &sock.remote_addr.ip_addr, timeout);

        if(effective_received != -1){
            //Si on reçoit un ack mais ce n'est pas le bon numéro d'acquittement, on applique le même principe que lors de l'expiration du timeout
            if(recv_pdu.header.ack_num != seq_envoye){
                printf("Le numero d'ack recu (%d) n'est pas le bon, celui attendu était %d, on renvoie la donnée\n ", recv_pdu.header.ack_num, seq_envoye);
                if(afficher_taux_perte()>perte)
                    effective_received = -1;
                else{
                    fenetre[taille_fenetre-(abs(recv_pdu.header.ack_num - seq_envoye))] = 1; 
                    seq_envoye = (seq_envoye+1);
                    return effective_sent;
                }
            }

            //Sinon, on a reçu le ack et c'est bien le bon, on incrément et on met a jour la fenetre
            printf("ACK reçu: %d\n", recv_pdu.header.ack_num);
            seq_envoye = (seq_envoye+1);
            glisser(1);

        }
        else{
            printf("On n'a pas reçu l'ACK\t");
            //glisser(0);
            //affiche_fenetre();
            //Si la perte est acceptable, on tolère l'envoie du message, sinon on renvoie le message
            if(afficher_taux_perte()>perte){
                printf("La perte n'est pas acceptable (%d %%), on renvoie le PDU\n", afficher_taux_perte()); 
            }  
            else{
                printf("On accepte la perte (%d) et on passe à autre chose\n ", afficher_taux_perte());
                glisser(0);
                affiche_fenetre(); 
                seq_envoye++;
                return effective_sent;
            } 
        }
        
        
    }

    
    
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

    printf("remote_addr.addr_size vaut %d\n",remote_addr.addr_size);
    
    if(pdu.header.dest_port != sock.local_addr.port){
        fprintf(stderr, " %d != %d : Le port de destination du pdu n'est pas un port local attribué à un socket mictcp\n", pdu.header.dest_port, sock.local_addr.port);
        return;
	}

    if(sock.state == IDLE){
        if(pdu.header.syn == 1){
            sock.state = SYN_RECEIVED;
        }

        mic_tcp_pdu syn_ack_pdu;
        syn_ack_pdu.header.syn = 1;
        syn_ack_pdu.header.ack = 1;
        syn_ack_pdu.header.fin = 0;
        syn_ack_pdu.header.dest_port = pdu.header.source_port;
        syn_ack_pdu.header.source_port = sock.local_addr.port;
        syn_ack_pdu.payload.size = 0;

        IP_send(syn_ack_pdu, remote_addr);

    }

    else if(sock.state == SYN_RECEIVED){
        //Si on reçoit le ack du syn_ack alors la connexion est établie. 
        if(pdu.header.ack == 1){
            sock.state = ESTABLISHED;
        }
    }
    

    else if(sock.state == ESTABLISHED){
        //Initialisation du ack à envoyer
        mic_tcp_pdu ack_pdu;
        ack_pdu.header.syn = 0;
        ack_pdu.header.ack = 1;
        ack_pdu.header.fin = 0;
        ack_pdu.header.dest_port = pdu.header.source_port;
        ack_pdu.header.source_port = pdu.header.dest_port; 
        ack_pdu.payload.size = 0;


        //Si le message reçu a déjà été traité, on renvoie le ack correspondant
        if(pdu.header.seq_num == dernier_num_seq_traite){
            ack_pdu.header.ack_num = dernier_num_seq_traite;
            printf("[MIC-TCP processreceivedpdu] msg deja traité, envoie de l'ack %d vers l'addresse %s\n ", ack_pdu.header.ack_num, remote_addr.addr);
            IP_send(ack_pdu, remote_addr);
        }

        //Si on recoit le bon message attendu, on le met dans le buffer, on note que le message a déjà été traité, on envoie le ack et on incrémente le prochain numéro de séquence attendu
        else if(pdu.header.seq_num >= seq_attendu){
            
            ack_pdu.header.ack_num = pdu.header.seq_num;
            
            dernier_num_seq_traite = pdu.header.seq_num;
            printf("[MIC-TCP processreceivedpdu] message bien recu, envoie de l'ack %d vers l'addresse %s\n ", ack_pdu.header.ack_num, remote_addr.addr);
            IP_send(ack_pdu, remote_addr);
            seq_attendu = (pdu.header.seq_num+1); 
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
