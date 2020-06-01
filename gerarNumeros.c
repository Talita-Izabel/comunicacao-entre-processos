#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/stat.h>

#define CARACTERE "/c"
#define TRUE 1

typedef struct {
    unsigned quantidadeNumeros;
    unsigned quantidadeLotes;
    unsigned valorInicial;
    unsigned valorFinal;
    unsigned tamanhoLote;
    unsigned ordenacao;
    unsigned numeroArquivo;
}Tipo_elementos;

//Função para ordenar os números do lote.
void selecao(int * v, unsigned n){
    int i, j, Min;
    int aux;
    for (i = 0; i < n - 1; i++){
        Min = i;
        for (j = i + 1 ; j < n; j++)
            if ( v[j] < v[Min])
                Min = j;
        if (i != Min){
            aux = v[Min];
            v[Min] = v[i];
            v[i] = aux;
        }
    }
}

void produzir(int* shared_memory, Tipo_elementos e){
    int i, j, aux;

    //Fica gerando números aleatórios até o tamanho do lote especificado e insere esses números na memória compartilhada.
    while(TRUE){
        //Inicializa o gerador de números aleatórioscom o valor da função time(NULL).
        srand(time(NULL)); 
        for (i=0; i < e.tamanhoLote; i++){
            aux = e.valorInicial + rand() % (e.valorFinal - e.valorInicial);
            //Verifica se o numero já foi gerado dentro do lote
            for(j = 0; j < i; j++)
                if(shared_memory[j] == aux) break;
            //Se não é um numero repetido o adiciona no buffer
            if(j == i)
                shared_memory[i] = aux;
            else
                i--;
        }
        //Da um tempo antes do consumidor consumir os números.
        sleep(1);
    }
}

void consumir (int* shared_memory, Tipo_elementos e){
    int i = 0;
    FILE *arq;

    //Enquanto não houver a quantidade de lotes necessários, ele continua consumindo e inserindo no arquivo.
    while(e.numeroArquivo != e.quantidadeLotes + 1){

        //Determina se o lote precisa ser ordenado.
        if(e.ordenacao == TRUE){
            selecao(shared_memory,e.tamanhoLote);
        }

        //Determina nome do arquivo.
        char nome[20] = "Lote";
        int qt = e.quantidadeLotes;
        if(qt < 10)
            sprintf(nome,"Lote%.1d",e.numeroArquivo);
        else if (qt < 100)
            sprintf(nome,"Lote%.2d",e.numeroArquivo);
        else if (qt < 1000)
            sprintf(nome,"Lote%.3d",e.numeroArquivo);
        else
            sprintf(nome,"Lote%.4d",e.numeroArquivo);

        //Abre o arquivo no modo de escrita.
        arq = fopen(nome, "w"); 

        //Imprime o lote nos arquivos. Se for o último elemento, não coloca a vírgula.
        for(i=0; i<e.tamanhoLote; i++){
            if(i == e.tamanhoLote-1){
                fprintf(arq,"%d. ", shared_memory[i]);
            }else{
                fprintf(arq,"%d, ", shared_memory[i]);
            }
        }

        //Fecha o arquivo.
        fclose(arq);
        //Incrementa o numero do arquivo.
        e.numeroArquivo++;
        sleep(1); // Para dar tempo ao produtor para produzir mais números.
    }
}


//Função para guardar as informações necessárias na estrutura Tipo_elementos.
void guardarElementos(int argc, char *argv[], Tipo_elementos *e){
    e->numeroArquivo = 1;
    e->quantidadeNumeros = atoi(argv[1]);
    e->ordenacao = 0;

    //Caso seja todas as opções, guarda os valores dentro da estrutura Tipo_elementos
    if(argc == 6){
        e->valorInicial = atoi(argv[2]);
        e->valorFinal = atoi(argv[3]);
        e->tamanhoLote = atoi(argv[4]);
        e->ordenacao = 1;
    }
    //Caso tenha só 5 comandos, verifica se o parâmetro ocultado é do valor incial ou de ordenação.
    if(argc == 5){
        //Não tem valor inicial e ordena.
        if(!strcmp(argv[4],CARACTERE)){ //if(strcmp(argv[4],CARACTERE)==0)
            e->valorInicial = 1;
            e->valorFinal = atoi(argv[2]);
            e->tamanhoLote = atoi(argv[3]);
            e->ordenacao = 1;
        }else{
            //Não ordena e atribui valor inicial.
            e->valorInicial = atoi(argv[2]);
            e->valorFinal = atoi(argv[3]);
            e->tamanhoLote = atoi(argv[4]);
        }
    }

    //Sem valor inicial e ordena.
    if(argc == 4){
        e->valorInicial = 1;
        e->valorFinal = atoi(argv[2]);
        e->tamanhoLote = atoi(argv[3]);
    }
    e->quantidadeLotes = e->quantidadeNumeros/e->tamanhoLote;
}

int iniciar(int argc, char *argv[]){
    //Referente aos processos
    pid_t produtor, consumidor;
    int status;

    //Referente a area de memoria compartilhada
    int segment_id;
	int* shared_memory;
	struct shmid_ds shmbuffer;
	int segment_size;
	const int shared_segment_size = 0x6400;

    //Se a quantidade de argumentos for mais que 6 ou menor que 4, já retorna 0, pois é inválido.
    if(argc>6 || argc <4){
        printf("\nErro");
        return 0;
    }
    Tipo_elementos e;
    guardarElementos(argc,argv, &e);

    /*Aloca o espaço de memoria compartilhada.
    IPC_CREATE: Indica que um novo segmento deve ser criado.
    IPC_EXCL: Faz com que o sistema falhe se uma chave de segmento especificada já existir.
    S_IRUSRand S_IWUSR: especificam permissões de leitura e gravação para o proprietário do segmento de memória compartilhada.
    */
    segment_id = shmget (IPC_PRIVATE, shared_segment_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    //Aloca a memória compartilhada.
    shared_memory = (int*) shmat (segment_id, 0, 0); 
    //Determina o tamanho do segmento.
	shmctl (segment_id, IPC_STAT, &shmbuffer); 
	segment_size = shmbuffer.shm_segsz;  


    //Executa e cria os processos produtor e consumidor.
    produtor = fork();
    if(produtor < 0){
        printf("Falha ao executar produtor!!");
    }

    //O código aqui dentro será executado no processo produtor.
    if(produtor == 0){
        produzir(shared_memory, e);
    }
    
    //O código neste trecho será executado no processo pai.
    if(produtor > 0){
        consumidor = fork();
        if(consumidor < 0){
            printf("Falha ao executar consumidor!!");
        }
        //Código referente ao processo consumidor.
        if(consumidor == 0)
            consumir(shared_memory, e);
        else{
            waitpid(consumidor, &status, 0);
            /* Desaloca o segmento de memória compartilhada. */
            shmctl (segment_id, IPC_RMID, 0);
            printf("Foram gerados %d números no intervalo de %d a %d organizados em %d lotes.\n",e.quantidadeNumeros,e.valorInicial,e.valorFinal,e.quantidadeNumeros/e.tamanhoLote);
        }
    }
}

int main(int argc, char *argv[]) {
    return iniciar(argc,argv);
}
