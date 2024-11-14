#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define YES 1
#define NO 0
#define BOF_INVALID -1

typedef struct
{
    int regLidos;
    int regBuscados;
    int regRemovidos;
} Header;

typedef struct
{
    char id_aluno[4];
    char sigla_disc[4];
    char nome_aluno[50];
    char nome_disc[50];
    float media;
    float freq;
} Hist;

typedef struct
{
    char id_aluno[4];
    char sigla_disc[4];
    int byte_offset;
} Key;

#define BUCKET_SIZE 2
#define HASH_TABLE_SIZE 13

typedef struct
{
    Key chave[BUCKET_SIZE];
} Bucket;

extern Bucket *hashTable[HASH_TABLE_SIZE];

int compararChaves(Key key1, Key key2);
void criar_hash();
int funcao_hash(Key *chave);
int inserirChave(Key *chave);
void carregarEmMemoria();
int inserir_registro();
int buscar();
void remover();

Bucket *hashTable[HASH_TABLE_SIZE];

/*---------------Funções para gerenciar Arquivos---------------*/

// Não esquecer de colocar na função carregarFiles() o template que a professora passar
void carregarFiles()
{
    FILE *fd;

    //////////////////////////////
    struct hist
    {
        char id_aluno[4];
        char sigla_disc[4];
        char nome_aluno[50];
        char nome_disc[50];
        float media;
        float freq;
    } vet[3] = {{"001", "001", "Nome-1", "Disc-001", 8.9, 80.3},
                {"002", "001", "Nome-2", "Disc-001", 3.3, 72.3},
                {"001", "002", "Nome-1", "Disc-002", 9.7, 73.7}};

    fd = fopen("insere.bin", "w+b");
    fwrite(vet, sizeof(vet), 1, fd);
    fclose(fd);

    //////////////////////////////
    struct busca
    {
        char id_aluno[4];
        char sigla_disc[4];
    } vet_b[2] = {{"001", "001"},
                  {"001", "002"}};

    fd = fopen("busca.bin", "w+b");
    fwrite(vet_b, sizeof(vet_b), 1, fd);
    fclose(fd);

    //////////////////////////////
    struct remove
    {
        char id_aluno[4];
        char sigla_disc[4];
    } vet_r[1] = {{"001", "001"}};

    fd = fopen("remove.bin", "w+b");
    fwrite(vet_r, sizeof(vet_r), 1, fd);
    fclose(fd);
}

FILE *abrirArquivo(char *filename)
{
    FILE *file;
    if ((file = fopen(filename, "r+b")) == NULL)
    {
        if ((file = fopen(filename, "w+b")) == NULL)
        {
            printf("O arquivo nao pode ser aberto [%s]", filename);
            exit(1);
        }
    }
    return file;
}

void abrirArquivoOut(FILE *out)
{
    Header header;
    if ((out = fopen("out.bin", "r+b")) == NULL)
    {
        if ((out = fopen("out.bin", "w+b")) != NULL)
        {
            header.regBuscados = 0;
            header.regLidos = 0;
            header.regRemovidos = 0;

            fwrite(&header, sizeof(header), 1, out);

            fclose(out);

            printf("Novo arquivo de dados criado com sucesso!\n");
        }
        else
        {
            printf("Nao foi possivel criar o arquivo dados.\n");
        }
    }
}

void abrirArquivoHash(FILE *hash)
{
    if ((hash = fopen("hash.bin", "r+b")) == NULL)
    {
        criar_hash();
    }
}

/*-----------------Funções para Hash----------------*/

int compararChaves(Key key1, Key key2)
{
    char id1[9], id2[9];
    sprintf(id1, "%s#%s", key1.id_aluno, key1.sigla_disc);
    sprintf(id2, "%s#%s", key2.id_aluno, key2.sigla_disc);

    return strcmp(id1, id2);
}

void carregarEmMemoria()
{
    FILE *hash = abrirArquivo("hash.bin");
    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        fread(&hashTable[i], sizeof(Bucket), 1, hash);
    }
    fclose(hash);
}

void criar_hash()
{
    FILE *hash;
    if ((hash = fopen("hash.bin", "r+b")) == NULL)
    {
        hash = fopen("hash.bin", "w+b");
        Bucket bucket;
        for (int i = 0; i < BUCKET_SIZE; i++)
        {
            strcpy(bucket.chave[i].id_aluno, "VVV");
            strcpy(bucket.chave[i].sigla_disc, "VVV");
            bucket.chave[i].byte_offset = BOF_INVALID;
        }
        rewind(hash);
        for (int i = 0; i < HASH_TABLE_SIZE; i++)
        {
            fwrite(&bucket, sizeof(Bucket), 1, hash);
        }
        printf("Arquivo hash.bin criado com sucesso\n");
        fclose(hash);
    }
    else
    {
        printf("O arquivo hash.bin ja existe\n");
    }
}

int funcao_hash(Key *chave)
{
    char chave_concat[9];
    strcpy(chave_concat, chave->id_aluno);
    strcat(chave_concat, chave->sigla_disc);
    int numero = atoi(chave_concat);
    return numero % HASH_TABLE_SIZE;
}

int inserirChave(Key *chave)
{
    int end_reg_eliminado = -1; // Variável para armazenar o endereço de um registro eliminado, se encontrado
    Bucket bucket;
    int count = 0; // Contador para tentativas de inserção em caso de colisão
    int inseriu = 0;
    Bucket bucketAux;
    Key chaveAux = *chave;
    FILE *out;
    Header header;
    int endereco_hash = funcao_hash(chave); // Calcula o endereço inicial baseado na função hash

    FILE *hash = abrirArquivo("hash.bin");
    out = abrirArquivo("out.bin");

    Key chaveEliminada; // Criação de uma chave marcadora para comparar com chaves eliminadas
    strcpy(chaveEliminada.id_aluno, "XXX");
    strcpy(chaveEliminada.sigla_disc, "XXX");
    chaveEliminada.byte_offset = BOF_INVALID;

    fread(&header, sizeof(Header), 1, out);

    printf("Chave: '%s%s'\n", chave->id_aluno, chave->sigla_disc);

    // Busca por espaços disponíveis ou registros eliminados
    while (YES)
    {
        fseek(hash, (endereco_hash * sizeof(bucketAux)), SEEK_SET);

        // Lê todas as chaves do bucket atual
        for (int i = 0; i < BUCKET_SIZE; i++)
        {
            fread(&bucketAux.chave[i], sizeof(Key), 1, hash);
        }

        // Procura por espaço vazio ou verifica se a chave já existe no bucket
        for (int i = 0; i < BUCKET_SIZE; i++)
        {
            // Verifica se há um registro eliminado
            if (compararChaves(bucketAux.chave[i], chaveEliminada) == 0)
            {
                // Salva o endereço do registro eliminado
                end_reg_eliminado = ftell(hash) - (BUCKET_SIZE - (i * sizeof(Key)));
                break;
            }

            // Verifica duplicação da chave
            if (compararChaves(bucketAux.chave[i], chaveAux) == 0)
            {
                // Se a chave já existe, exibe mensagem de duplicação e encerra
                printf("Chave '%s%s' duplicada\n", bucketAux.chave[i].id_aluno, bucketAux.chave[i].sigla_disc);

                header.regLidos++; // Atualiza o cabeçalho
                rewind(out);       // Retorna ao início do arquivo
                fwrite(&header, sizeof(header), 1, out);
                fclose(out); // Fecha o arquivo de registros
                return NO;   // Retorna falha
            }
        }

        // Avança para o próximo endereço em caso de colisão
        endereco_hash = (endereco_hash + 1) % HASH_TABLE_SIZE;

        // Se voltarmos ao endereço inicial, significa que a tabela foi percorrida por completo
        if (endereco_hash == funcao_hash(&chaveAux))
        {
            break;
        }
    }

    // Tentativa de inserir a chave no bucket encontrado
    while (YES)
    {
        printf("Endereco hash: %d\n", endereco_hash);
        fseek(hash, (endereco_hash * sizeof(Bucket)), SEEK_SET);
        fread(&bucket, sizeof(Bucket), 1, hash);

        inseriu = 0;

        // Procura por espaço vazio no bucket
        for (int i = 0; i < BUCKET_SIZE; i++)
        {
            if (bucket.chave[i].byte_offset == -1) // Verifica se há espaço vazio
            {
                bucket.chave[i] = *chave;
                fseek(hash, (endereco_hash * sizeof(Bucket)), SEEK_SET);
                fwrite(&bucket, sizeof(Bucket), 1, hash);
                printf("Chave: '%s%s' inserida com sucesso!\n", bucket.chave[i].id_aluno, bucket.chave[i].sigla_disc);
                inseriu = 1; // Marca que a chave foi inserida
                break;
            }
        }

        if (inseriu == 1) // Se a chave foi inserida, encerra o loop
        {
            break;
        }

        // Caso não haja espaço no bucket, ocorre uma colisão
        printf("Colisao\n");

        // Avança para o próximo endereço hash
        endereco_hash = (endereco_hash + 1) % HASH_TABLE_SIZE;

        // Caso exista um registro eliminado, utiliza o espaço dele
        if (end_reg_eliminado != -1)
        {
            fseek(hash, end_reg_eliminado, SEEK_SET);
            fwrite(&chaveAux, sizeof(Key), 1, hash);
            printf("Chave: '%s%s' inserida com sucesso!\n", chaveAux.id_aluno, chaveAux.sigla_disc);
            break;
        }

        // Se o hash foi completamente percorrido e não há espaço, a tabela está cheia
        if (endereco_hash == funcao_hash(chave))
        {
            printf("O hash esta cheio.\n");
            return NO;
        }

        printf("Tentativa: %d\n\n", ++count);
    }

    fclose(hash);
    return YES;
}

int inserir_registro()
{
    FILE *insere;
    FILE *out;

    Hist registro;
    Header header;
    Key chave;

    Key nova_chave;

    int pos_reg = 0;
    int tam_reg = 0;

    char buffer[500];

    insere = abrirArquivo("insere.bin");
    out = abrirArquivo("out.bin");

    fread(&header, sizeof(Header), 1, out);

    fseek(insere, (header.regLidos) * sizeof(registro), SEEK_SET);

    if (fgetc(insere) == EOF)
    {
        printf("\nEsse indice nao esta disponivel\n");
        return NO;
    }

    fseek(insere, -1, SEEK_CUR);

    fread(&registro, sizeof(registro), 1, insere);
    snprintf(buffer, sizeof(buffer), "%s#%s#%s#%s#%.2f#%.2f", registro.id_aluno, registro.sigla_disc, registro.nome_aluno, registro.nome_disc, registro.media, registro.freq);

    strcpy(nova_chave.id_aluno, registro.id_aluno);
    strcpy(nova_chave.sigla_disc, registro.sigla_disc);

    fseek(insere, (header.regLidos) * sizeof(chave), SEEK_SET);

    fseek(out, 0, SEEK_END);

    pos_reg = (int)ftell(out);
    tam_reg = strlen(buffer);
    header.regLidos++;
    nova_chave.byte_offset = pos_reg;

    if (!inserirChave(&nova_chave))
    {
        printf("Nao foi possivel realizar a insercao\n");
        return NO;
    }

    fwrite(&tam_reg, sizeof(int), 1, out);
    fwrite(buffer, tam_reg, 1, out);

    rewind(out);
    fwrite(&header, sizeof(Header), 1, out);

    fclose(insere);
    fclose(out);
    return YES;
}

int buscar()
{
    Bucket bucket;
    int endereco_hash = 0;
    int count = 0; // Contador para o número de tentativas de busca
    int tam_reg = 0;
    int j = 0; // Contador de acessos realizados até encontrar a chave
    FILE *out = abrirArquivo("out.bin");
    FILE *busca = abrirArquivo("busca.bin");
    FILE *hash = abrirArquivo("hash.bin");
    Header header;
    Key chave;
    Key chaveIntocada; // Estrutura para identificar registros não usados ou eliminados
    char buffer[256];

    // Inicializa a chave "intocada" com valores padrão
    strcpy(chaveIntocada.id_aluno, "VVV");
    strcpy(chaveIntocada.sigla_disc, "VVV");
    chaveIntocada.byte_offset = BOF_INVALID;

    fread(&header, sizeof(header), 1, out);
    fseek(busca, (header.regBuscados) * (sizeof(chave) - sizeof(int)), SEEK_SET);

    if (fgetc(busca) == EOF)
    {
        printf("\nEsse indice nao esta disponivel\n");
        return NO;
    }
    fseek(busca, -1, SEEK_CUR);

    fread(&chave, sizeof(chave) - sizeof(int), 1, busca);
    printf("Chave: '%s%s'\n", chave.id_aluno, chave.sigla_disc);

    // Calcula o endereço inicial com base na função hash
    endereco_hash = funcao_hash(&chave);

    fseek(out, 0, SEEK_SET);
    header.regBuscados++;
    fwrite(&header, sizeof(header), 1, out);

    while (YES)
    {
        fseek(hash, (endereco_hash * sizeof(bucket)), SEEK_SET);

        // Lê todas as chaves do bucket atual
        for (int i = 0; i < BUCKET_SIZE; i++)
        {
            fread(&bucket.chave[i], sizeof(Key), 1, hash);
        }

        // Itera pelas chaves no bucket
        for (int i = 0; i < BUCKET_SIZE; i++)
        {
            j++;

            // Verifica se a chave atual do bucket é igual à chave buscada
            if (compararChaves(bucket.chave[i], chave) == 0)
            {
                fseek(out, bucket.chave[i].byte_offset, SEEK_SET);
                fread(&tam_reg, sizeof(int), 1, out);
                fread(&buffer, tam_reg, 1, out);
                buffer[tam_reg] = '\0';

                printf("Chave: '%s%s' encontrada no endereco %d, %d acessos\n", bucket.chave[i].id_aluno, bucket.chave[i].sigla_disc, endereco_hash, j);
                printf("Registro encontrado: %s\n", buffer);

                fclose(out); // Fecha o arquivo de registros
                return YES;  // Retorna sucesso
            }

            // Verifica se a chave atual é igual à chave "intocada"
            else if (compararChaves(chaveIntocada, bucket.chave[i]) == 0)
            {
                printf("Chave nao encontrada.\n");
                fclose(out);
                return NO;
            }
        }

        // Avança para o próximo endereço em caso de colisão
        endereco_hash = (endereco_hash + 1) % HASH_TABLE_SIZE;

        // Se voltarmos ao endereço inicial, a tabela foi completamente percorrida
        if (endereco_hash == funcao_hash(&chave))
        {
            printf("Chave nao encontrada.\n");
            fclose(out);
            return NO;
        }

        printf("Tentativa: %d\n\n", ++count);
    }

    fclose(hash);
}

void remover()
{
    Bucket bucket;                         
    FILE *hash = abrirArquivo("hash.bin"); 

    int endereco_hash = 0;                     
    int count = 0;                             // Contador para o número de tentativas de busca
    FILE *out = abrirArquivo("out.bin");       
    FILE *remove = abrirArquivo("remove.bin"); 
    Header header;                             
    Key chave;                                 
    Key nova_chave;                            
    int pos_inicio_registro = 0;               

    fread(&header, sizeof(header), 1, out);
    fseek(remove, (header.regRemovidos) * (sizeof(Key) - sizeof(int)), SEEK_SET);

    if (fgetc(remove) == EOF)
    {
        return;
    }

    fseek(remove, -1, SEEK_CUR); 
    fread(&chave, sizeof(chave) - sizeof(int), 1, remove);

    // Calcula o endereço inicial com base na função hash
    endereco_hash = funcao_hash(&chave);

    while (YES)
    {
        fseek(hash, (endereco_hash * sizeof(bucket)), SEEK_SET);

        pos_inicio_registro = ftell(hash);

        // Lê todas as chaves do bucket atual
        for (int i = 0; i < BUCKET_SIZE; i++)
        {
            fread(&bucket.chave[i], sizeof(bucket.chave[i]), 1, hash);
        }

        // Itera pelas chaves no bucket
        for (int i = 0; i < BUCKET_SIZE; i++)
        {
            // Verifica se a chave atual do bucket é igual à chave a ser removida
            if (compararChaves(bucket.chave[i], chave) == 0)
            {
                fseek(hash, pos_inicio_registro + (i * sizeof(chave)), SEEK_SET);
                strcpy(nova_chave.id_aluno, "XXX");
                strcpy(nova_chave.sigla_disc, "XXX");
                nova_chave.byte_offset = BOF_INVALID;
                fwrite(&nova_chave, sizeof(nova_chave), 1, hash);

                fseek(out, 0, SEEK_SET);
                header.regRemovidos++;
                fwrite(&header, sizeof(header), 1, out);

                fclose(out);
                fclose(hash);

                printf("Chave removida no endereco: %d; posicao: %d.\n", endereco_hash, i);
                return;
            }
        }

        // Avança para o próximo endereço em caso de colisão
        endereco_hash = (endereco_hash + 1) % HASH_TABLE_SIZE;

        // Se voltarmos ao endereço inicial, a tabela foi completamente percorrida
        if (endereco_hash == funcao_hash(&chave))
        {
            printf("Chave nao encontrada.\n");

            fseek(out, 0, SEEK_SET);
            header.regRemovidos++;
            fwrite(&header, sizeof(header), 1, out);

            fclose(hash);
            fclose(out);
            return;
        }

        // Exibe o número de tentativas realizadas
        printf("Tentativa: %d\n\n", ++count);
    }

    fclose(hash);
}

/*--------------------MAIN----------------------*/
int main()
{
    carregarFiles();

    int op;

    FILE *out = NULL;
    FILE *hash = NULL;

    abrirArquivoOut(out);
    abrirArquivoHash(hash);

    carregarEmMemoria();

    printf("\n---------------MENU---------------\n\n");
    printf("1 - Inserir\n");
    printf("2 - Buscar\n");
    printf("3 - Remover\n");
    printf("4 - Sair\n\n");

    while (YES)
    {
        printf("Selecione a opcao desejada: ");
        scanf("%d", &op);

        if (op == 1)
        {
            inserir_registro();
        }
        else if (op == 2)
        {
            buscar();
        }
        else if (op == 3)
        {
            remover();
        }
        else if (op == 4)
        {
            break;
        }
        else
            printf("\nOpcao invalida, insira novamente.");
    }

    fclose(out);
    fclose(hash);
    return 0;
}