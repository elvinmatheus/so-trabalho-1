#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
 
int main()
 {
    printf("[Cliente] Processo Cliente iniciado. PID: %d\n", getpid());
    int tempo;
    srand( (unsigned)time(NULL) );
    int x = rand()%10;
    
    if(x == 0)
        tempo = 15;
    else if(x > 0 && x <= 3)
        tempo = 5;
    else
        tempo = 1;
        
    FILE* demanda = fopen("demanda.txt", "w+");
    fprintf(demanda, "%d", tempo);
    printf("[Cliente] Tempo de atendimento do cliente %d: %d\n", getpid(), tempo);
    fclose(demanda);   
    printf("[Cliente] Processo Cliente %d em modo espera...\n", getpid());
    raise(SIGSTOP);
    printf("[Cliente] Cliente %d acordado\n", getpid());
    // abre semáforo sem_atend em modo de leitura e escrita
    sem_t *sem = sem_open("/sem_atend", O_RDWR);
    printf("[Cliente] Cliente %d tentando acessar semáforo sem_atend\n", getpid());
    // bloqueia semáforo sem_atend
    if(sem != SEM_FAILED) sem_wait(sem);
    printf("[Cliente] Cliente %d em atendimento\n", getpid());
    usleep(tempo);
    printf("[Cliente] Cliente %d atendido\n", getpid());
    // libera semáforo sem_atend
    if(sem != SEM_FAILED) sem_post(sem);
    
    return 0;
    
}