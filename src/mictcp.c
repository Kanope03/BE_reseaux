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
int rate_loss = 1;
int perte_acceptee = 20;     //Représente le taux de perte en %, cad par exemple sur une 10 paquet envoyé avec un taux à 20%, on peut autoriser 2 paquet perdu

int nb_message_envoye = 0;
int nb_message_perdu = 0;



mic_tcp_sock socks[NB_MAX_SOCKET];

pthread_mutex_t mutex_compteur = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t mutex_cond = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond;

typedef struct {
    pthread_t thread_id;
    int fd;
} thread_fd_map;

thread_fd_map thread_map[NB_MAX_SOCKET];
int thread_map_count = 0;

pthread_mutex_t thread_map_mutex = PTHREAD_MUTEX_INITIALIZER;


int fenetre[100] ={1};
int taille_fenetre = 100;


//Fonctions qui serviront à gérer la perte grace à la fenetre
int calculer_taux_perte(){
    int taux = 0;

    if(nb_message_envoye < 100){
        return ((double) nb_message_perdu/nb_message_envoye) * 100;
    }

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

/*Fonction pour retrouver le file descriptor en fonction de l'id du thread. Renvoie -1 si aucun fd associé n'est trouvé*/
int get_fd_by_thread(pthread_t thread_id) {
    int fd = -1;

    pthread_mutex_lock(&thread_map_mutex);
    for (int i = 0; i < thread_map_count; i++) {
        if (pthread_equal(thread_map[i].thread_id, thread_id)) {
            fd = thread_map[i].fd;
            break;
        }
    }
    pthread_mutex_unlock(&thread_map_mutex);

    return fd;
}




/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    set_loss_rate(rate_loss);

    if (initialize_components(sm) == -1) {
        fprintf(stderr, "Échec d'initialize_components dans mic_tcp_socket\n");

        exit(EXIT_FAILURE);
    }

    // Récupération de l'ID du thread
    pthread_t thread_id = pthread_self();

    // Verrouiller l'accès
    pthread_mutex_lock(&thread_map_mutex);
    int fd = compteur_socket;

    socks[compteur_socket].fd = fd;
    socks[compteur_socket].state = IDLE;

    // Ajouter dans le tableau de correspondance
    thread_map[thread_map_count].thread_id = thread_id;
    thread_map[thread_map_count].fd = fd;
    thread_map_count++;

    compteur_socket++;
    pthread_mutex_unlock(&thread_map_mutex);

    return fd;
}


/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    socks[socket].local_addr = addr;
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
    socks[socket].state = IDLE;

	return 0; 
    
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    socks[socket].remote_addr = addr;

    //Création du pdu contenant le syn pour demander l'établissement de la connexion.
    mic_tcp_pdu syn_pdu;

    syn_pdu.payload.size = 0;
    syn_pdu.header.syn = 1;
    syn_pdu.header.ack = 0;
    syn_pdu.header.fin = 0;
    syn_pdu.header.dest_port = socks[socket].remote_addr.port;
    syn_pdu.header.source_port = socks[socket].local_addr.port;
    syn_pdu.payload.size = 4;

    mic_tcp_pdu syn_ack_pdu;

    //On négocie le pourcentage de perte
    char pourcentage_perte[4];
    sprintf(pourcentage_perte, "%d", perte_acceptee);
    syn_pdu.payload.data = pourcentage_perte;


    syn_ack_pdu.payload.size = 0;

    
    IP_send(syn_pdu, addr.ip_addr);

    socks[socket].state = SYN_SENT;

    if(IP_recv(&syn_ack_pdu, &socks[socket].local_addr.ip_addr, &socks[socket].remote_addr.ip_addr, timeout) != -1){

        //Si l'on reçoit le syn_ack alors la connexion est établie (de notre pdv) et on envoie l'ack
        if(syn_ack_pdu.header.syn == 1 && syn_ack_pdu.header.ack == 1){
            socks[socket].state = ESTABLISHED;
            mic_tcp_pdu ack_pdu;
            ack_pdu.header.syn = 0;
            ack_pdu.header.ack = 1;
            ack_pdu.header.fin = 0;
            ack_pdu.header.dest_port = socks[socket].remote_addr.port;
            ack_pdu.header.source_port = socks[socket].local_addr.port;
            ack_pdu.payload.size = 0;
            IP_send(ack_pdu, addr.ip_addr);
        }
    }
    if(socks[socket].state != ESTABLISHED){
        printf("La connexion n'a pas été établie ...\n");
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
    recv_pdu.payload.size = 0;

    //Initialisation du message à envoyer (headers et payload)
    pdu.header.dest_port = socks[mic_sock].remote_addr.port;
    pdu.header.source_port = socks[mic_sock].local_addr.port;
    pdu.header.seq_num = seq_envoye;
    pdu.header.syn = 0;
    pdu.header.ack = 0;
    pdu.header.fin = 0;

    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    int effective_sent = 0;

    socks[mic_sock].remote_addr.ip_addr.addr_size = 0;
    socks[mic_sock].local_addr.ip_addr.addr_size = 0;

    int effective_received = -1;
    printf("Ack attendu normalement: %d\n", seq_envoye);


    //Si on n'a pas reçu de ack jusqu'à expiration du timeout
    while(effective_received == -1){
        effective_sent = IP_send(pdu, socks[mic_sock].remote_addr.ip_addr);

        
        effective_received = IP_recv(&recv_pdu, &socks[mic_sock].local_addr.ip_addr, &socks[mic_sock].remote_addr.ip_addr, timeout);

        if(effective_received != -1){
            //Si on reçoit un ack mais ce n'est pas le bon numéro d'acquittement, on applique le même principe que lors de l'expiration du timeout
            if(recv_pdu.header.ack_num != seq_envoye){
                printf("Le numero d'ack recu (%d) n'est pas le bon, celui attendu était %d, on renvoie la donnée\n ", recv_pdu.header.ack_num, seq_envoye);
                if(calculer_taux_perte()>perte_acceptee)
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

            if(calculer_taux_perte()>perte_acceptee){
                printf("La perte n'est pas acceptable (%d %%), on renvoie le PDU\n", calculer_taux_perte()); 
            }  
            else{
                printf("On accepte la perte (%d) et on passe à autre chose\n ", calculer_taux_perte());
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
    //On part pour l'instant du principe que l'application n'a qu'un seul socket...
    
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    int socket = get_fd_by_thread(pthread_self()); // On récupère le descripteur de fichier du socket grâce à son pthread_id. 

    if(socks[socket].state == IDLE){
        if(pdu.header.syn == 1){
            socks[socket].state = SYN_RECEIVED;

            printf("Le pourcentage de perte négocié est : %s\n", pdu.payload.data); //La source décide du pourcentage de perte. 

            mic_tcp_pdu syn_ack_pdu;
            syn_ack_pdu.header.syn = 1;
            syn_ack_pdu.header.ack = 1;
            syn_ack_pdu.header.fin = 0;
            syn_ack_pdu.header.dest_port = pdu.header.source_port;
            syn_ack_pdu.header.source_port = socks[socket].local_addr.port;
            syn_ack_pdu.payload.size = 0;

            IP_send(syn_ack_pdu, remote_addr);
        
        }

        

    }

    else if(socks[socket].state == SYN_RECEIVED){
        //Si on reçoit le ack du syn_ack alors la connexion est établie. 
        if(pdu.header.ack == 1){
            socks[socket].state = ESTABLISHED;
        }
    }
    

    else if(socks[socket].state == ESTABLISHED){
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
            app_buffer_put(pdu.payload);
            IP_send(ack_pdu, remote_addr);
            seq_attendu = (pdu.header.seq_num+1); 
            

        }
    }

    
    
}

