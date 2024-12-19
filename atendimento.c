#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

// Semaforo
#include <semaphore.h>
#include <fcntl.h>

// Estrutura do cliente
typedef struct {
    pid_t pid;
    long long hora_chegada;
    int prioridade;
    int tempo_atendimento;
} Cliente;

// Fila de clientes
#define TAMANHO_MAXIMO 100
Cliente fila[TAMANHO_MAXIMO];

// variaveis 
int tamanho_fila = 0;
int n_clientes = 0;
int clientes_satisfeitos = 0;
int stop_flag = 0;
char input; 
int analista_pid;

// Semaforos
sem_t *sem_atend, *sem_block;

// Funções
long long tempo_atual_ms();

void *thread_recepcao(void *arg);
void criar_cliente();
void *thread_parar(void *arg);

void *thread_atendimento(void *arg);
Cliente remover_cliente();
void acordar_analista();

int main(int argc, char *argv[]) {
    
    sem_unlink("/sem_atend");
    sem_unlink("/sem_block");

    int N = atoi(argv[1]);
    int X = atoi(argv[2]);

    printf("Inicializando...\n");
    printf("N = %d\n", N);
    printf("X = %d\n", X);


    pthread_t recepcao, atendimento, parar;
    pthread_create(&recepcao, NULL, thread_recepcao, &N);
    pthread_create(&atendimento, NULL, thread_atendimento, &X);

    pid_t analista_pid = fork();
    if (analista_pid == 0) {
        printf("[Atendimento - main] Executando analista...\n");
        execl("./analista", "analista", NULL);
        perror("Erro ao executar o analista");
        exit(1);
    }

    waitpid(analista_pid, NULL, WUNTRACED);

    if (N == 0) {
        pthread_create(&parar, NULL, thread_parar, NULL);
    }

    long long inicio = tempo_atual_ms();
    if (N == 0) {
        pthread_join(parar, NULL);
    }

    pthread_join(recepcao, NULL);
    printf("[main] Recepção finalizada\n");
    pthread_join(atendimento, NULL);
    printf("[main] Atendimento finalizado\n");
    printf("[main] Todas as threads finalizadas\n");
    long long fim = tempo_atual_ms();

    kill(analista_pid, SIGTERM);
    // waitpid(analista_pid, NULL, 0);
    
    printf("[main] Analista finalizado\n");
    sem_close(sem_atend);
    sem_close(sem_block);
    sem_unlink("/sem_atend");
    sem_unlink("/sem_block");
    printf("[main] Semaforos fechados\n");
    double taxa_satisfacao = (double)clientes_satisfeitos / n_clientes;
    printf("Taxa de satisfação: %.2f\n", taxa_satisfacao * 100);
    printf("Tempo total de execução: %lld ms\n", fim - inicio);

    return 0;   
}

// thread para parar o programa caso N == 0
void *thread_parar(void* arg)
{
    int c;
	while((c = getchar()) != EOF) {
        if(c == 's' || c == 'S') {
            stop_flag = 1;
            break;
        }
    }
    return NULL;
}

// thread recepcao
void *thread_recepcao(void* arg)
{
    printf("[Atendimento - Recepção] Iniciando thread de recepção...\n");

    sem_atend = sem_open("/sem_atend", O_CREAT | O_EXCL, 0644, 1);
    sem_block = sem_open("/sem_block", O_CREAT | O_EXCL, 0644, 1);

    int N = *(int *)arg; // converte arg para um ponteiro para inteiro e acessa o valor
    int infinito = (N == 0);

    while ((infinito || n_clientes < N) && !stop_flag) {
        if (tamanho_fila < TAMANHO_MAXIMO) {
            printf("[Atendimento - Recepção] Criando cliente...\n");
            criar_cliente();
            n_clientes++;
            printf("[Atendimento - Recepção] Total de clientes: %d\n", n_clientes);
        }
    };
    printf("[Atendimento - Recepção] Fim da thread de recepção\n");
    stop_flag = 1;
    return NULL;
}

// auxiliar para criar um cliente
void criar_cliente() {
    Cliente cliente;
    cliente.pid = fork();
    
    if (cliente.pid == 0) {
        // executa o cliente
        printf("[Atendimento - Recepção - Filho] Iniciando processo cliente...\n");
        execl("./cliente", "cliente", NULL);
        exit(0);
    
    } else if (cliente.pid > 0) {
        // define o tempo de chegada do cliente e a prioridade
        cliente.hora_chegada = time(NULL);
        cliente.prioridade = (rand() % 2 == 0) ? 1 : 0;

        // Espera o cliente dormir
        printf("[Atendimento - Recepção - Pai] Esperando cliente %d dormir...\n", cliente.pid);
        waitpid(cliente.pid, NULL, WUNTRACED);
        printf("[Atendimento - Recepção - Pai] Cliente %d dormindo...\n", cliente.pid);

        // Lê o tempo de atendimento do cliente
        FILE *demanda = fopen("demanda.txt", "r");
        fscanf(demanda, "%d", &cliente.tempo_atendimento);
        fclose(demanda);

        // Adiciona o cliente na fila
        fila[tamanho_fila] = cliente;
        tamanho_fila++;
        printf("[Atendimento - Recepção - Pai] Cliente adicionado à fila. Tamanho da fila: %d\n", tamanho_fila);
    }
};

// auxiliar para obter o tempo atual em milissegundos
long long tempo_atual_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
};


void *thread_atendimento(void *arg) {
    int X = *(int *)arg;
    printf("[Atendimento - Atendente] Iniciando thread de atendimento...\n");
    while (stop_flag == 0) {
        if (tamanho_fila > 0) {
            printf("[Atendimento - Atendente] Removendo cliente da fila...\n");
            // Pega o cliente da fila e remove        
            Cliente cliente = remover_cliente();

            // Acorda o cliente
            printf("[Atendimento - Atendente] Acordando cliente %d\n", cliente.pid);
            kill(cliente.pid, SIGCONT);
            waitpid(cliente.pid, NULL, WUNTRACED);
            // Espera sem_atend abrir
            printf("[Atendimento - Atendente] Esperando semáforo sem_atend abrir...\n");
            sem_wait(sem_atend);
            printf("[Atendimento - Atendente] Semáforo sem_atend aberto\n");
            
            // Espera o semáforo ficar verde para continuar. Antes de entrar na região crítica, coloca vermelho (0)
            sem_wait(sem_block);

            // ***Início da região crítica***,
            // Escreve pid no arquivo lng.txt
            FILE *lng = fopen("lng.txt", "a");
            fprintf(lng, "%d\n", cliente.pid);
            fclose(lng);
            printf("[Atendimento - Atendente] Cliente %d adicionado ao arquivo lng.txt\n", cliente.pid);
            // Abre semaforo sem_block
            sem_post(sem_block);
            sem_post(sem_atend);

            // Calcula satisfacao do cliente
            long long tempo_atual = tempo_atual_ms();
            int tempo_espera = tempo_atual - cliente.hora_chegada;
            printf("[Atendimento - Atendente] Tempo de espera: %d ms\n", tempo_espera);
            printf("[Atendimento - Atendente] Tempo atual: %lld\n", tempo_atual);
            int paciencia = (cliente.prioridade == 1) ? X/2 : X;
            printf("[Atendimento - Atendente] Tempo de espera: %d ms\n", tempo_espera);
            // Se o cliente estiver satisfeito, incrementa o contador
            if (tempo_espera <= paciencia) {
                printf("[Atendimento - Atendente] Cliente satisfeito\n");
                clientes_satisfeitos++;
            }

            acordar_analista();

            waitpid(cliente.pid, NULL, 0);
        };
    };

    return NULL;
}

Cliente remover_cliente() {
    Cliente cliente = fila[0];
    for (int i = 0; i < tamanho_fila - 1; i++) {
        fila[i] = fila[i+1];
    }
    tamanho_fila--;
    return cliente;
};

void acordar_analista() {
    static int contador_acordar = 0;
    contador_acordar++;

    if (contador_acordar % 10 == 0) {
        FILE *pid_analista = fopen("analista_pid.txt", "r");

        fscanf(pid_analista, "%d", &analista_pid);
        fclose(pid_analista);

        kill(analista_pid, SIGCONT);
    }
};