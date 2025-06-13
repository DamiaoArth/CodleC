#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// Inclusões específicas para diferentes sistemas operacionais
#ifdef _WIN32
    #include <conio.h>      // Para funções de entrada de teclado no Windows
    #include <windows.h>    // Para funções específicas do Windows
#else
    #include <termios.h>    // Para controle de terminal no Unix/Linux
    #include <unistd.h>     // Para funções POSIX
    #include <sys/select.h> // Para função select() no Unix/Linux
#endif

// Códigos de Cores ANSI para colorir o terminal
#define RESET "\033[0m"                 // Reseta todas as formatações
#define GREEN "\033[42m\033[30m"        // Letra correta (fundo verde)
#define YELLOW "\033[43m\033[30m"       // Posição incorreta (fundo amarelo)
#define GRAY "\033[47m\033[30m"         // Letra incorreta (fundo cinza)
#define WHITE "\033[37m"                // Texto padrão
#define BOLD "\033[1m"                  // Texto em negrito
#define CLEAR_SCREEN "\033[2J\033[H"    // Limpa a tela e move cursor para início
#define BLUE "\033[46m\033[30m"         // Para dicas (fundo azul)

// Constantes do sistema de dicas
#define MAX_HINTS 4         // Máximo de dicas permitidas por jogo
#define HINT_DELAY 30       // Tempo em segundos entre dicas

// Constantes do jogo
#define MAX_WORD_LENGTH 6   // Comprimento máximo permitido para palavras
#define MAX_ATTEMPTS 7      // Número máximo de tentativas por jogo
#define WORD_LENGTH 5       // Comprimento padrão das palavras (Wordle clássico)
#define MAX_WORDS 30000     // Capacidade máxima para lista de palavras normais
#define HARD_MAX_WORDS 1000 // Capacidade máxima para lista de palavras difíceis

// Enumeração dos níveis de dificuldade
typedef enum {
    EASY = 1,    // Fácil - palavras mais comuns
    MEDIUM = 2,  // Médio - palavras de dificuldade média
    HARD = 3,    // Difícil - palavras menos comuns
    DEMO = 4     // Demonstração - para testes
} Difficulty;

// Estrutura principal que armazena todo o estado do jogo
typedef struct {
    char target_word[WORD_LENGTH + 1];              // Palavra secreta a ser adivinhada
    char guesses[MAX_ATTEMPTS][WORD_LENGTH + 1];    // Array com todas as tentativas do jogador
    int feedback[MAX_ATTEMPTS][WORD_LENGTH];        // Feedback para cada letra (0=incorreta, 1=posição errada, 2=correta)
    int current_attempt;                            // Tentativa atual (0 a MAX_ATTEMPTS-1)
    int max_attempts;                               // Número máximo de tentativas para este jogo
    int game_over;                                  // Flag indicando se o jogo terminou (0=não, 1=sim)
    int won;                                        // Flag indicando se o jogador venceu (0=não, 1=sim)
    Difficulty difficulty;                          // Nível de dificuldade atual
    int current_difficulty;                         // Dificuldade atual como inteiro
    int hints_used;                                 // Número de dicas já utilizadas
    time_t last_hint_time;                          // Timestamp da última dica solicitada
    char revealed_letters[WORD_LENGTH + 1];         // Letras reveladas pelas dicas
} GameState;

// Arrays globais para armazenar as listas de palavras
char word_list[MAX_WORDS][WORD_LENGTH + 1];           // Lista principal de palavras
char hard_word_list[HARD_MAX_WORDS][WORD_LENGTH + 1]; // Lista de palavras difíceis
int WORD_COUNT = 0;                                   // Contador de palavras carregadas na lista principal
int HARD_WORD_COUNT = 0;                             // Contador de palavras carregadas na lista difícil

// Protótipos das funções do sistema de dicas
int can_use_hint(GameState* game);      // Verifica se o jogador pode usar uma dica
void use_hint(GameState* game);         // Aplica uma dica ao jogo atual
void display_hints_info(GameState* game); // Mostra informações sobre dicas disponíveis

/**
 * Função multiplataforma para detectar se uma tecla foi pressionada
 * Retorna: 1 se há uma tecla disponível, 0 caso contrário
 */
int kbhit(void) {
    #ifdef _WIN32
        return _kbhit(); // Função nativa do Windows
    #else
        // Implementação para sistemas Unix/Linux usando select()
        struct timeval tv = { 0L, 0L }; // Timeout zero para não bloquear
        fd_set fds;
        FD_ZERO(&fds);                  // Limpa o conjunto de file descriptors
        FD_SET(0, &fds);               // Adiciona stdin (0) ao conjunto
        return select(1, &fds, NULL, NULL, &tv) > 0; // Verifica se stdin tem dados
    #endif
}

/**
 * Função para carregar palavras de um arquivo texto
 * @param destino: Array bidimensional onde as palavras serão armazenadas
 * @param nome_arquivo: Nome do arquivo a ser lido
 * @param is_hard_list: Flag indicando se é a lista de palavras difíceis (1) ou normal (0)
 * @return: Número de palavras carregadas com sucesso
 */
int carregar_palavras(char destino[][WORD_LENGTH + 1], const char* nome_arquivo, int is_hard_list) {
    FILE* file = fopen(nome_arquivo, "r");
    if (!file) {
        printf("Erro ao abrir arquivo: %s\n", nome_arquivo);
        return 0;
    }

    char buffer[100];                                           // Buffer para leitura de linha
    int count = 0;                                             // Contador de palavras válidas
    int max_words = is_hard_list ? HARD_MAX_WORDS : MAX_WORDS; // Define limite baseado no tipo de lista
    
    // Lê o arquivo linha por linha até o final ou até atingir o limite
    while (fgets(buffer, sizeof(buffer), file) && count < max_words) {
        // Processa e limpa cada palavra lida
        int valid = 1;                          // Flag para indicar se a palavra é válida
        char cleaned[WORD_LENGTH + 1] = {0};    // Array para palavra limpa (inicializado com zeros)
        
        // Remove caracteres não alfabéticos e converte para maiúscula
        for (int i = 0, j = 0; buffer[i] && j < WORD_LENGTH; i++) {
            if (isalpha(buffer[i])) {              // Verifica se é letra
                cleaned[j++] = toupper(buffer[i]); // Converte para maiúscula e adiciona
            }
        }

        // Só adiciona palavras que tenham exatamente o comprimento correto
        if (strlen(cleaned) == WORD_LENGTH) {
            strcpy(destino[count], cleaned);    // Copia palavra limpa para destino
            count++;                           // Incrementa contador
        }
    }

    fclose(file);
    printf("Carregadas %d palavras do arquivo: %s\n", count, nome_arquivo);
    return count;
}

// Variável global para armazenar configurações originais do terminal (apenas Unix/Linux)
#ifndef _WIN32
struct termios orig_termios;
#endif

// Protótipos de funções - Interface e controle do jogo
void clear_screen(void);                                    // Limpa a tela do terminal
void init_game(GameState* game, Difficulty difficulty);     // Inicializa um novo jogo
void display_menu(void);                                    // Exibe o menu principal
void display_how_to_play(void);                            // Exibe as instruções do jogo
void display_results(void);                                 // Exibe resultados/estatísticas
void display_game_board(GameState* game);                  // Exibe o tabuleiro do jogo
void display_keyboard(GameState* game);                     // Exibe o teclado virtual com feedback
void process_guess(GameState* game, const char* guess);     // Processa uma tentativa do jogador
void calculate_feedback(GameState* game, const char* guess); // Calcula feedback para uma tentativa
void display_game_over(GameState* game, Difficulty dificuldade);// Exibe tela de fim de jogo

// Protótipos de funções - Controle de console e entrada
void setup_console(void);                                  // Configura o terminal para o jogo
void restore_console(void);                                // Restaura configurações originais do terminal
void display_pause_menu(void);                             // Exibe menu de pausa
char* get_guess_with_pause(GameState* game);               // Captura tentativa com opção de pausa
char get_char(void);                                       // Captura um caractere do teclado
int get_menu_choice(void);                                 // Captura escolha do menu principal
int get_difficulty_choice(void);                          // Captura escolha de dificuldade
int check_word_exists(GameState* game, const char* word);  // Verifica se palavra existe no dicionário
int get_pause_choice_robust(void);                        // Captura escolha do menu de pausa (robusto)
int handle_pause_menu(GameState* game);                   // Gerencia o menu de pausa
int is_position_solved(GameState* game, int position); // Adicionar com os outros protótipos

// Variáveis globais para manipulação do console

/*
    Função multiplataforma para pausar a execução por um tempo específico
    @param milliseconds: Tempo em milissegundos para pausar
*/
void sleep_ms(int milliseconds) {
    #ifdef _WIN32
        Sleep(milliseconds);                    // Função nativa do Windows (aceita milissegundos)
    #else
        usleep(milliseconds * 1000);          // Função Unix/Linux (aceita microssegundos, por isso *1000)
    #endif
}

// Configuração multiplataforma do console

/*
    Configura o terminal/console para o funcionamento adequado do jogo
    No Windows: Define codificação UTF-8 para suporte a caracteres especiais
    No Unix/Linux: Configura modo raw para captura imediata de teclas
*/
void setup_console(void) {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);                    // Define codificação UTF-8 para saída no console
    #else
    tcgetattr(STDIN_FILENO, &orig_termios);         // Salva configurações atuais do terminal
    struct termios new_termios = orig_termios;       // Cria cópia das configurações
    new_termios.c_lflag &= ~(ICANON | ECHO);        // Remove modo canônico e eco de caracteres
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios); // Aplica as novas configurações
    #endif
}

/*
    Restaura as configurações originais do terminal
    Importante chamar esta função antes de encerrar o programa
*/
void restore_console(void) {
   #ifndef _WIN32
       tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); // Restaura configurações originais do terminal
   #endif
}

/*
    Limpa a tela do terminal e move o cursor para o início
    Funciona em qualquer terminal que suporte códigos ANSI
*/
void clear_screen(void) {
   printf(CLEAR_SCREEN);   // Envia código ANSI para limpar tela e posicionar cursor
   fflush(stdout);         // Força a saída imediata dos dados do buffer
}

/*
    Inicializa um novo jogo com a dificuldade especificada
    @param game: Ponteiro para a estrutura do estado do jogo
    @param difficulty: Nível de dificuldade escolhido pelo jogador
*/
void init_game(GameState* game, Difficulty difficulty) {
    game->difficulty = difficulty;
    game->current_difficulty = difficulty; 
    game->current_attempt = 0;
    game->game_over = 0;
    game->won = 0;
   
   // Define número máximo de tentativas baseado na dificuldade
   switch (difficulty) {
       case EASY:
           game->max_attempts = 7;     // Fácil: 7 tentativas
           break;
       case MEDIUM:
           game->max_attempts = 6;     // Médio: 6 tentativas (padrão Wordle)
           break;
       case HARD:
       case DEMO:
           game->max_attempts = 5;     // Difícil/Demo: 5 tentativas
           break;
   }
   
   // Inicializa arrays de tentativas e feedback
   for (int i = 0; i < MAX_ATTEMPTS; i++) {
       strcpy(game->guesses[i], "");           // Limpa tentativas anteriores
       for (int j = 0; j < WORD_LENGTH; j++) {
           game->feedback[i][j] = -1;          // -1 = não definido/não usado
       }
   }
   
   // Seleciona palavra aleatória baseada na dificuldade
   srand((unsigned int)time(NULL));            // Inicializa gerador de números aleatórios
   if (difficulty == HARD) {
       // Modo difícil: usa lista de palavras mais complexas
       int idx = rand() % HARD_WORD_COUNT;
       strcpy(game->target_word, hard_word_list[idx]);
   } else {
       // Modos fácil/médio: usa lista principal de palavras
       int idx = rand() % WORD_COUNT;
       strcpy(game->target_word, word_list[idx]);
   }
   
   // Modo demonstração: usa palavra fixa para testes
   if (difficulty == DEMO) {
       strcpy(game->target_word, "TESTE");     // Palavra conhecida para debugging/demonstração
   }
   
   // Inicializa sistema de dicas
   game->hints_used = 0;                       // Nenhuma dica usada ainda
   game->last_hint_time = 0;                   // Timestamp da última dica (0 = nunca)
   strcpy(game->revealed_letters, "     ");    // Nenhuma letra revelada ainda (5 espaços)
}

/*
    Exibe o menu principal do jogo com arte ASCII e opções
*/
void display_menu(void) {
    clear_screen();
    printf("\n");
    // Arte ASCII do título do jogo
    printf(" ██████╗ ██████╗ ██████╗ ██╗     ███████╗ ██████╗ \n");
    printf("██╔════╝██╔═══██╗██╔══██╗██║     ██╔════╝██╔════╝\n");
    printf("██║     ██║   ██║██║  ██║██║     █████╗  ██║     \n");
    printf("██║     ██║   ██║██║  ██║██║     ██╔══╝  ██║     \n");
    printf("╚██████╗╚██████╔╝██████╔╝███████╗███████╗╚██████╗\n");
    printf(" ╚═════╝ ╚═════╝ ╚═════╝ ╚══════╝╚══════╝ ╚═════╝\n");
    printf("\n");
    printf("  %s=== Adivinhe a palavra ===%s\n\n", BOLD, RESET);
    printf("  1. %sJogar%s\n", WHITE, RESET);
    printf("  2. %sComo Jogar%s\n", WHITE, RESET);
    printf("  3. %sResultados%s\n", WHITE, RESET);
    printf("  4. %sSair%s\n\n", WHITE, RESET);
    printf("  Selecione uma opção (1-4): ");
}

/*
    Exibe o menu de pausa durante o jogo
    Permite ao jogador continuar, reiniciar ou desistir
*/
void display_pause_menu(void) {
   clear_screen();
   printf("\n");
   // Arte ASCII para "PAUSA"
   printf(" ██████╗  █████╗ ██╗   ██╗███████╗ █████╗ \n");
   printf("██╔══██╗██╔══██╗██║   ██║██╔════╝██╔══██╗\n");
   printf("██████╔╝███████║██║   ██║███████╗███████║\n");
   printf("██╔═══╝ ██╔══██║██║   ██║╚════██║██╔══██║\n");
   printf("██║     ██║  ██║╚██████╔╝███████║██║  ██║\n");
   printf("╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝\n");
   printf("\n");
   printf("  %s=== JOGO PAUSADO ===%s\n\n", BOLD, RESET);
   printf("  1. %sContinuar Jogando%s\n", GREEN, RESET);    // Verde: ação positiva
   printf("  2. %sReiniciar Jogo%s\n", YELLOW, RESET);      // Amarelo: ação de mudança
   printf("  3. %sDesistir%s\n\n", GRAY, RESET);            // Cinza: ação de saída
   printf("  Selecione uma opção (1-3): ");
}

/*
    Exibe as instruções detalhadas de como jogar
    Inclui objetivos, níveis de dificuldade, codificação de cores e dicas
*/
void display_how_to_play(void) {
   clear_screen();
   printf("\n%s=== COMO JOGAR ===%s\n\n", BOLD, RESET);
   printf("🎯 %sObjetivo:%s Adivinhe a palavra secreta de 5 letras!\n\n", BOLD, RESET);
   
   printf("📏 %sNíveis de Dificuldade:%s\n", BOLD, RESET);
   printf("   • %sFácil:%s   7 tentativas para adivinhar\n", GREEN, RESET);
   printf("   • %sMédio:%s   6 tentativas para adivinhar\n", YELLOW, RESET);
   printf("   • %sDifícil:%s 5 tentativas para adivinhar\n", GRAY, RESET);
   printf("   • %sDemo:%s    5 tentativas (palavra: TESTE)\n\n", WHITE, RESET);
   
   printf("🎨 %sCódigo de Cores:%s\n", BOLD, RESET);
   printf("   %s V %s Letra correta na posição correta\n", GREEN, RESET);
   printf("   %s A %s Letra correta na posição errada\n", YELLOW, RESET);
   printf("   %s X %s Letra não está na palavra\n\n", GRAY, RESET);
   
   printf("💡 %sDicas:%s\n", BOLD, RESET);
   printf("   • Todas as palavras têm 5 letras\n");
   printf("   • Apenas palavras válidas em português são aceitas\n");
   printf("   • Letras podem aparecer múltiplas vezes\n");
   printf("   • Use o feedback para guiar sua próxima tentativa\n\n");
   
   printf("Pressione qualquer tecla para voltar ao menu...");
   get_char();
}

/*
    Exibe os resultados salvos dos jogos anteriores
    Lê o arquivo JSON com histórico, calcula estatísticas e mostra resumo
    Inclui: lista de jogos, total de vitórias e média de tentativas
*/
void display_results(void) {
    // Limpa a tela para exibir os resultados
    clear_screen();
    
    // Exibe o cabeçalho dos resultados do jogo
    printf("\n%s=== RESULTADOS DO JOGO ===%s\n\n", BOLD, RESET);
    // Tenta abrir o arquivo de resultados para leitura
    FILE* file = fopen("resultados.json", "r");
    if (!file) {
        // Se o arquivo não existe, informa que não há resultados salvos
        printf("Nenhum resultado salvo ainda.\n");
        printf("Pressione qualquer tecla para retornar ao menu...");
        get_char();
        return;
    }
    // Variáveis para armazenar dados temporários e estatísticas
    char linha[256];           // Buffer para ler cada linha do arquivo
    int total_games = 0;       // Contador do total de jogos
    int total_tentativas = 0;  // Soma total de tentativas
    int wins = 0;              // Contador de vitórias
    char dificuldade[20];      // Armazena a dificuldade do jogo
    
    printf("Resultados salvos:\n\n");
    // Lê o arquivo linha por linha
    while (fgets(linha, sizeof(linha), file)) {
        char palavra[WORD_LENGTH + 1];  // Armazena a palavra do jogo
        int tentativas;                 // Armazena o número de tentativas
        // Extrai a palavra e tentativas do formato JSON
        // Formato esperado: {"palavra": "XXXXX", "tentativas": N, "dificuldade": "XXXXX"}
        if (sscanf(linha, "{\"palavra\": \"%5[^\"]\", \"tentativas\": %d, \"dificuldade\": \"%19[^\"]\"}", palavra, &tentativas, dificuldade) == 3) {
            // Exibe os dados do jogo individual
            printf("  Palavra: %-5s | Tentativas: %d | Dificuldade: %-12s ", palavra, tentativas, dificuldade);
            
            if (strcmp(dificuldade, "FÁCIL") == 0) {
                printf("%s😊%s", GREEN, RESET);
            } else if (strcmp(dificuldade, "MÉDIO") == 0) {
                printf("%s😎%s", YELLOW, RESET);
            } else if (strcmp(dificuldade, "DIFÍCIL") == 0) {
                printf("%s🤯%s", GRAY, RESET);
            } else {
                printf("%s🤖%s", WHITE, RESET);
            }
            printf("\n");
            
            // Atualiza as estatísticas
            total_games++;
            total_tentativas += tentativas;
            wins++; // Considera que só salva se ganhou
        }
    }
    // Fecha o arquivo
    fclose(file);
    // Exibe o resumo das estatísticas
    printf("\nResumo:\n");
    printf("  • Jogos vencidos: %d\n", wins);
    printf("  • Total de jogos: %d\n", total_games);
    
    // Calcula e exibe a média de tentativas apenas se houver jogos
    if (total_games > 0)
        printf("  • Média de tentativas por vitória: %.2f\n", (float)total_tentativas / total_games);
    // Aguarda input do usuário para retornar ao menu
    printf("\nPressione qualquer tecla para retornar ao menu...");
    get_char();
}

/*
    Exibe o tabuleiro principal do jogo em tempo real
    Mostra tentativas anteriores com feedback colorido, tentativa atual,
    dicas reveladas e informações de status (dificuldade, tentativas restantes)
*/
void display_game_board(GameState* game) {
    clear_screen();
    printf("\n%s=== WORDLE GAME ===%s", BOLD, RESET);
    
    const char* diff_names[] = {
        "",      // Índice 0 não usado
        "FÁCIL", // Índice 1 (EASY)
        "MÉDIO", // Índice 2 (MEDIUM)
        "DIFÍCIL", // Índice 3 (HARD)
        "DEMO"   // Índice 4 (DEMO)
    };
    printf(" [%s%s%s] Attempt: %d/%d\n", 
           BOLD, diff_names[game->difficulty], RESET,
           game->current_attempt, game->max_attempts);
    
    // Instruções fixas
    printf("%sDigite 'P' para pausar | Digite 'H' para dica%s\n", WHITE, RESET);
    
    // Mostrar informações das dicas
    display_hints_info(game);
    printf("\n");
    
    // Display the grid
    for (int i = 0; i < game->max_attempts; i++) {
        printf("  ");
        for (int j = 0; j < WORD_LENGTH; j++) {
            char letter = ' ';
            const char* color = WHITE;
            
            if (i < game->current_attempt && strlen(game->guesses[i]) == WORD_LENGTH) {
                letter = game->guesses[i][j];
                switch (game->feedback[i][j]) {
                    case 2: color = GREEN; break;   // Correct position
                    case 1: color = YELLOW; break;  // Wrong position
                    case 0: color = GRAY; break;    // Not in word
                    default: color = WHITE; break;
                }
            } else if (i == game->current_attempt && strlen(game->guesses[i]) > j) {
                letter = game->guesses[i][j];
                color = WHITE;
            }
            // NOVO: Mostrar dicas reveladas diretamente no grid
            else if (game->revealed_letters[j] != ' ') {
                letter = game->revealed_letters[j];
                color = BLUE;
            }
            
            printf("%s %c %s ", color, letter, RESET);
        }
        printf("\n");
    }
    printf("\n");
}

/*
    Exibe o teclado virtual com status das letras já utilizadas
    Mostra feedback colorido: verde (posição correta), amarelo (letra existe),
    cinza (não existe na palavra), branco (ainda não testada)
*/
void display_keyboard(GameState* game) {
    // Layout do teclado QWERTY em 3 fileiras
    char keyboard[3][13] = {
        {'Q','W','E','R','T','Y','U','I','O','P','\0','\0','\0'},  // Fileira superior
        {'A','S','D','F','G','H','J','K','L','\0','\0','\0','\0'}, // Fileira do meio
        {'Z','X','C','V','B','N','M','\0','\0','\0','\0','\0','\0'}  // Fileira inferior
    };
    
    // Número de letras em cada fileira do teclado
    int lengths[3] = {10, 9, 7};
    
    printf("Status do Teclado:\n");
    
    // Percorre cada fileira do teclado
    for (int row = 0; row < 3; row++) {
        printf("  "); // Indentação base
        
        // Indentação adicional para simular layout de teclado real
        if (row == 1) printf(" ");    // Fileira do meio: 1 espaço extra
        if (row == 2) printf("  ");   // Fileira inferior: 2 espaços extras
        
        // Percorre cada letra da fileira atual
        for (int col = 0; col < lengths[row]; col++) {
            char letter = keyboard[row][col];
            const char* color = WHITE; // Cor padrão: branco (não testada)
            
            // Verifica o status da letra em todas as tentativas anteriores
            for (int i = 0; i < game->current_attempt; i++) {
                for (int j = 0; j < WORD_LENGTH; j++) {
                    // Se a letra foi usada em alguma tentativa
                    if (game->guesses[i][j] == letter) {
                        switch (game->feedback[i][j]) {
                            case 2: // Posição correta (sempre prevalece)
                                color = GREEN;
                                break;
                            case 1: // Letra existe mas posição errada
                                if (color != GREEN) color = YELLOW;
                                break;
                            case 0: // Letra não existe na palavra
                                if (color != GREEN && color != YELLOW) color = GRAY;
                                break;
                        }
                    }
                }
            }
            
            // Exibe a letra com a cor correspondente ao seu status
            printf("%s %c %s", color, letter, RESET);
        }
        printf("\n"); // Nova linha após cada fileira
    }
    printf("\n"); // Espaço após o teclado
}

/*
    Salva o resultado de uma partida vencedora no arquivo JSON
    Registra a palavra descoberta e o número de tentativas utilizadas
    Formato: uma linha JSON por vitória para fácil leitura posterior
*/
    
    const char* diff_names[] = {
        "",      // Índice 0 não usado
        "FÁCIL", // Índice 1 (EASY)
        "MÉDIO", // Índice 2 (MEDIUM)
        "DIFÍCIL", // Índice 3 (HARD)
        "DEMO"   // Índice 4 (DEMO)
    };
void salvar_resultado_json(const char* palavra, int tentativas, Difficulty dificuldade) {
    // Abre o arquivo em modo append (adicionar ao final)
    FILE* file = fopen("resultados.json", "a");
    
    if (file) {
        // Escreve uma linha JSON com os dados da partida
        // Formato: {"palavra": "XXXXX", "tentativas": N, "Dificuldade": "X"}
        fprintf(file, "{\"palavra\": \"%s\", \"tentativas\": %d, \"dificuldade\": \"%s\"}\n", 
               palavra, tentativas, diff_names[dificuldade]);
        
        // Fecha o arquivo para garantir que os dados sejam salvos
        fclose(file);
    }
    // Nota: Se falhar ao abrir o arquivo, a função termina silenciosamente
    // Em uma versão mais robusta, poderia exibir mensagem de erro
}

/*
    Calcula o feedback colorido para uma tentativa do jogador
    Compara a palavra tentada com a palavra-alvo usando algoritmo de duas passadas
    Retorna: 2=posição correta, 1=letra existe mas posição errada, 0=não existe
*/
void calculate_feedback(GameState* game, const char* guess) {
    int attempt = game->current_attempt;
    char target[WORD_LENGTH + 1];      // Cópia da palavra-alvo para manipulação
    char guess_copy[WORD_LENGTH + 1];  // Cópia da tentativa para manipulação
    
    // Cria cópias das strings para não modificar as originais
    strcpy(target, game->target_word);
    strcpy(guess_copy, guess);
    
    // Inicializa todo o feedback como 0 (letra não existe na palavra)
    for (int i = 0; i < WORD_LENGTH; i++) {
        game->feedback[attempt][i] = 0; // Padrão: não está na palavra
    }
    
    // PRIMEIRA PASSADA: marca posições corretas (prioridade máxima)
    for (int i = 0; i < WORD_LENGTH; i++) {
        if (guess_copy[i] == target[i]) {
            game->feedback[attempt][i] = 2; // Posição correta (verde)
            target[i] = '*';                // Marca como usada na palavra-alvo
            guess_copy[i] = '*';            // Marca como processada na tentativa
        }
    }
    
    // SEGUNDA PASSADA: marca posições erradas (letra existe mas local errado)
    for (int i = 0; i < WORD_LENGTH; i++) {
        if (guess_copy[i] != '*') { // Se ainda não foi processada
            // Procura a letra em outras posições da palavra-alvo
            for (int j = 0; j < WORD_LENGTH; j++) {
                if (target[j] == guess_copy[i]) {
                    game->feedback[attempt][i] = 1; // Posição errada (amarelo)
                    target[j] = '*';                // Marca como usada
                    break; // Para evitar múltiplas marcações da mesma letra
                }
            }
        }
    }
    
    // Resultado final no array feedback[attempt]:
    // 2 = Verde: letra correta na posição correta
    // 1 = Amarelo: letra existe na palavra mas posição errada  
    // 0 = Cinza: letra não existe na palavra-alvo
}

/*
    Limpa e padroniza uma palavra removendo caracteres inválidos
    Remove espaços, números, símbolos e converte para maiúsculas
    Mantém apenas letras alfabéticas para validação posterior
*/
void clean_word(char *word) {
    // Remove espaços e caracteres especiais
    int j = 0; // Índice para a posição de escrita na string limpa
    
    // Percorre cada caractere da palavra original
    for (int i = 0; word[i]; i++) {
        // Verifica se o caractere é uma letra alfabética
        if (isalpha(word[i])) {
            // Converte para maiúscula e adiciona na posição correta
            word[j++] = toupper(word[i]);
        }
        // Caracteres não-alfabéticos são ignorados (espaços, números, símbolos)
    }
    
    // Adiciona terminador nulo na nova posição final
    word[j] = '\0';
    
    // Resultado: palavra contém apenas letras maiúsculas
    // Exemplo: "ca$a 123" → "CASA"
}

/*
    Revela uma letra da palavra-alvo como dica para o jogador
    Escolhe aleatoriamente uma posição ainda não revelada ou acertada
    Atualiza estatísticas de uso de dicas e marca o tempo da última dica
*/
void use_hint(GameState* game) {
    // Encontrar posições disponíveis (não reveladas E não acertadas)
    int available_positions[WORD_LENGTH];  // Array para armazenar posições válidas
    int count = 0;                         // Contador de posições disponíveis
    
    // Percorre todas as posições da palavra
    for (int i = 0; i < WORD_LENGTH; i++) {
        // Verifica se a posição está disponível para dica
        if (game->revealed_letters[i] == ' ' && !is_position_solved(game, i)) {
            available_positions[count++] = i;  // Adiciona à lista de disponíveis
        }
    }
    
    // Verifica se ainda há posições disponíveis para dicas
    if (count == 0) {
        printf("Todas as letras disponíveis já foram reveladas ou acertadas!\n");
        printf("Pressione qualquer tecla para continuar...");
        get_char();
        return;  // Sai da função se não há dicas possíveis
    }
    
    // Escolher uma posição aleatória entre as disponíveis
    int pos = available_positions[rand() % count];
    
    // Revela a letra na posição escolhida
    game->revealed_letters[pos] = game->target_word[pos];
    
    // Atualiza estatísticas de dicas
    game->hints_used++;                    // Incrementa contador de dicas usadas
    game->last_hint_time = time(NULL);     // Registra o tempo da última dica
    
    // Exibe mensagem de confirmação da dica revelada
    printf("%sDica revelada!%s Letra na posição %d: %s%c%s\n", 
           BLUE, RESET, pos + 1, BOLD, game->target_word[pos], RESET);
    printf("Pressione qualquer tecla para continuar...");
    get_char();
    
    // A letra revelada aparecerá em azul no tabuleiro na próxima atualização
}

/*
    Exibe uma barra de progresso visual para o tempo de espera das dicas
    Mostra countdown em formato gráfico com blocos preenchidos e vazios
    Formato: [████████░░░░░░░] 08s (exemplo com 8 segundos restantes)
*/
void display_timer_bar(int remaining) {
    int total_bars = 15;  // REDUZIDO: Barra menor para melhor visualização
    
    // Calcula quantas barras devem estar preenchidas proporcionalmente
    int filled_bars = (remaining * total_bars) / HINT_DELAY;
    
    printf("[");  // Início da barra visual
    
    // Desenha a barra de progresso
    for (int i = 0; i < total_bars; i++) {
        if (i < filled_bars) {
            printf("█");  // Bloco preenchido (tempo restante)
        } else {
            printf("░");  // Bloco vazio (tempo decorrido)
        }
    }
    
    // Finaliza com o tempo numérico em segundos
    printf("] %02ds", remaining);
    
    // Resultado visual: barra diminui conforme o tempo passa
    // Exemplo: [█████████░░░░░░] 06s → [████░░░░░░░░░░░] 03s
}

/*
    Exibe informações sobre o sistema de dicas no tabuleiro
    Mostra contador de dicas usadas, disponibilidade e tempo de cooldown
    Formato: "Dicas: 2/3 usadas - Próxima dica: [████░░░] 05s"
*/
void display_hints_info(GameState* game) {
    // Exibe contador básico de dicas utilizadas
    printf("Dicas: %d/%d usadas", game->hints_used, MAX_HINTS);
    
    // Se ainda há dicas disponíveis para usar
    if (game->hints_used < MAX_HINTS) {
        
        // Se já usou pelo menos uma dica (há tempo de cooldown)
        if (game->hints_used > 0) {
            time_t now = time(NULL);  // Tempo atual
            
            // Calcula tempo restante para próxima dica
            int remaining = HINT_DELAY - (int)(now - game->last_hint_time);
            
            if (remaining > 0) {
                // Ainda em cooldown - mostra barra de progresso
                printf(" - Próxima dica: ");
                display_timer_bar(remaining);
            } else {
                // Cooldown terminado - dica disponível
                printf(" - %s✓ DISPONÍVEL%s", GREEN, RESET);
            }
        } else {
            // Primeira dica - sempre disponível (sem cooldown)
            printf(" - %s✓ DISPONÍVEL%s", GREEN, RESET);
        }
    }
    // Se atingiu o limite máximo de dicas, não exibe status adicional
    
    // Estados possíveis:
    // "Dicas: 0/3 usadas - ✓ DISPONÍVEL"
    // "Dicas: 1/3 usadas - Próxima dica: [███░░] 03s"
    // "Dicas: 1/3 usadas - ✓ DISPONÍVEL"
    // "Dicas: 3/3 usadas" (sem status adicional)
}

/*
    Processa uma tentativa válida do jogador no jogo
    Registra a palavra, calcula feedback, verifica vitória e atualiza estado
    Determina se o jogo continua, foi vencido ou perdido
*/
void process_guess(GameState* game, const char* guess) {
    // Registra a tentativa atual no histórico do jogo
    strcpy(game->guesses[game->current_attempt], guess);
    
    // Calcula o feedback colorido para a tentativa
    calculate_feedback(game, guess);
    
    // Verifica se o jogador acertou a palavra (vitória)
    int correct_count = 0;  // Contador de letras na posição correta
    
    for (int i = 0; i < WORD_LENGTH; i++) {
        // Conta quantas letras estão na posição correta (feedback = 2)
        if (game->feedback[game->current_attempt][i] == 2) {
            correct_count++;
        }
    }
    
    // Se todas as letras estão corretas = vitória
    if (correct_count == WORD_LENGTH) {
        game->won = 1;        // Marca como vencido
        game->game_over = 1;  // Finaliza o jogo
    }
    
    // Avança para a próxima tentativa
    game->current_attempt++;
    
    // Verifica se esgotou todas as tentativas (derrota)
    if (game->current_attempt >= game->max_attempts) {
        game->game_over = 1;  // Finaliza o jogo por falta de tentativas
        // Nota: game->won permanece 0 (não venceu)
    }
    
    // Estados possíveis após esta função:
    // 1. Jogo continua: game_over=0, won=0
    // 2. Vitória: game_over=1, won=1  
    // 3. Derrota: game_over=1, won=0
}

/*
    Exibe a tela final do jogo com resultado da partida
    Mostra mensagem de vitória ou derrota, palavra-alvo e oferece salvamento
    Permite ao jogador salvar resultado em caso de vitória
*/
void display_game_over(GameState* game, Difficulty dificuldade) {
    printf("\n%s=== FIM DE JOGO ===%s\n\n", BOLD, RESET);
    
    // Verifica se o jogador venceu a partida
    if (game->won) {
        // Mensagem de vitória com celebração
        printf("%s🎉 Parabéns! Você venceu! 🎉%s\n", GREEN, RESET);
        printf("Você adivinhou a palavra %s%s%s em %d tentativa(s)!\n\n",
               BOLD, game->target_word, RESET, game->current_attempt);

        // Oferece opção para salvar o resultado da vitória
        printf("Deseja salvar o resultado? (S/N): ");
        char c = toupper(get_char());  // Converte para maiúscula
        
        if (c == 'S') {
            // Salva no arquivo JSON com palavra e número de tentativas
            salvar_resultado_json(game->target_word, game->current_attempt, dificuldade);
            printf("\nResultado salvo com sucesso!\n");
        }
        // Se escolher 'N' ou qualquer outra tecla, não salva
        
    } else {
        // Mensagem de derrota com encorajamento
        printf("%s😔 Mais sorte na próxima vez! 😔%s\n", GRAY, RESET);
        printf("A palavra era: %s%s%s\n\n", BOLD, game->target_word, RESET);
        // Em caso de derrota, não oferece salvamento
    }

    // Aguarda input para retornar ao menu principal
    printf("Pressione qualquer tecla para retornar ao menu...");
    get_char();
    
    // Funcionalidades desta tela:
    // - Feedback visual diferenciado (vitória vs derrota)
    // - Revelação da palavra-alvo
    // - Sistema opcional de salvamento de resultados
    // - Retorno controlado ao menu principal
}

/*
    Obtém palpite do jogador com atualização em tempo real da tela
    
    Gerencia entrada de texto, comandos especiais e timer de dicas
    
    Retorna string com palpite ou NULL se jogo foi pausado
*/
char* get_guess_with_pause(GameState* game) {
    static char guess[WORD_LENGTH + 1];  // Buffer estático para armazenar palpite
    char input;                          // Caractere atual digitado pelo usuário
    int pos = 0;                        // Posição atual no buffer de entrada
    time_t last_update = time(NULL);    // Timestamp da última atualização de tela
    
    // Solicita entrada do usuário
    printf("Digite seu palpite: ");
    fflush(stdout);  // Força impressão imediata na tela
    
    while (1) {
        // Atualiza a tela a cada segundo se o timer de dicas estiver ativo
        time_t now = time(NULL);
        if (now != last_update && game->hints_used > 0 && game->hints_used < MAX_HINTS) {
            int remaining = HINT_DELAY - (int)(now - game->last_hint_time);
            // CORREÇÃO: Verifica se ainda há tempo restante OU se já passou do tempo limite
            if (remaining != (int)(last_update - game->last_hint_time) - HINT_DELAY) {
                // Redesenha completamente a tela com o timer atualizado
                display_game_board(game);
                display_keyboard(game);
                printf("Digite seu palpite: ");
                // Reimprime caracteres já digitados pelo usuário
                for (int i = 0; i < pos; i++) {
                    printf("%c", guess[i]);
                }
                fflush(stdout);  // Garante que tudo seja exibido imediatamente
            }
            last_update = now;  // Atualiza timestamp da última atualização
        }
        
        // Verifica se há input disponível do teclado
        if (kbhit()) {
            input = get_char();  // Obtém caractere digitado
            
            // Processa Enter/Return - finaliza entrada ou executa comandos
            if (input == '\n' || input == '\r') {
                if (pos == WORD_LENGTH) {
                    // Palpite completo com 5 letras - finaliza entrada
                    guess[pos] = '\0';  // Adiciona terminador de string
                    printf("\n");
                    return guess;
                } else if (pos == 1 && (guess[0] == 'P' || guess[0] == 'p')) {
                    // Comando de pausa - abre menu de pausa
                    printf("\n");
                    if (handle_pause_menu(game)) {
                        return NULL;  // Retorna NULL se jogo foi encerrado
                    }
                    // Retorna ao jogo - redesenha tela e reseta entrada
                    display_game_board(game);
                    display_keyboard(game);
                    pos = 0;
                    printf("Digite seu palpite: ");
                    fflush(stdout);
                } else if (pos == 1 && (guess[0] == 'H' || guess[0] == 'h')) {
                    // Comando de dica - tenta usar uma dica
                    printf("\n");
                    if (can_use_hint(game)) {
                        use_hint(game);  // Usa dica se disponível
                    } else {
                        // Informa por que a dica não pode ser usada
                        time_t now = time(NULL);
                        int remaining = HINT_DELAY - (int)(now - game->last_hint_time);
                        if (game->hints_used >= MAX_HINTS) {
                            printf("Você já usou todas as %d dicas disponíveis!\n", MAX_HINTS);
                        } else if (remaining > 0) {
                            printf("Aguarde %d segundos para usar outra dica.\n", remaining);
                        } else {
                            printf("Dica disponível! Tente novamente.\n");
                        }
                        printf("Pressione qualquer tecla para continuar...");
                        get_char();  // Aguarda confirmação do usuário
                    }
                    // Redesenha tela e reseta entrada após usar/tentar usar dica
                    display_game_board(game);
                    display_keyboard(game);
                    pos = 0;
                    printf("Digite seu palpite: ");
                    fflush(stdout);
                } else {
                    // Entrada inválida - solicita novo palpite
                    printf("\nPalpite deve ter exatamente 5 letras. Tente novamente: ");
                    fflush(stdout);
                    pos = 0;  // Reseta posição para nova entrada
                }
            } else if (input == '\b' || input == 127) { // Processa Backspace
                if (pos > 0) {
                    pos--;  // Remove último caractere do buffer
                    printf("\b \b");  // Apaga caractere da tela (volta, espaço, volta)
                    fflush(stdout);
                }
            } else if (isalpha(input) && pos < WORD_LENGTH) {
                // Adiciona letra válida ao palpite (apenas letras, máximo 5)
                guess[pos] = toupper(input);  // Converte para maiúscula
                printf("%c", guess[pos]);     // Exibe na tela
                fflush(stdout);
                pos++;  // Avança posição no buffer
            }
            // Ignora outros caracteres não-alfabéticos ou quando buffer está cheio
        } else {
            // Pequeno delay para não consumir CPU desnecessariamente
            // Evita loop infinito que sobrecarregaria o processador
            sleep_ms(100);  // Pausa de 100 milissegundos
        }
    }
}

/*
    Captura um caractere do teclado de forma multiplataforma
    @return: Caractere digitado pelo usuário
*/
char get_char(void) {
   #ifdef _WIN32
       return _getch();    // Windows: captura caractere sem echo e sem necessidade de Enter
   #else
       return getchar();   // Unix/Linux: captura caractere (funciona em modo raw configurado)
   #endif
}

/*
    Obtém escolha do usuário no menu de pausa de forma robusta
    
    Temporariamente muda modo do terminal para entrada segura de números
    
    Retorna número válido entre 1 e 3 escolhido pelo usuário
*/
int get_pause_choice_robust(void) {
    #ifndef _WIN32
        // Temporariamente restaura o modo canônico para entrada segura de números
        // Modo canônico permite edição de linha e validação antes de enviar
        struct termios temp_termios = orig_termios;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &temp_termios);
    #endif

    char input[10];     // Buffer para armazenar entrada do usuário (tamanho generoso)
    int choice = -1;    // Inicializa com valor inválido para entrar no loop
    
    // Loop até obter escolha válida entre 1 e 3
    while (choice < 1 || choice > 3) {
        printf("Digite sua escolha (1-3): ");
        fflush(stdout);  // Força exibição imediata do prompt
        
        // Lê linha completa de entrada do usuário
        if (fgets(input, sizeof(input), stdin)) {
            choice = atoi(input);  // Converte string para inteiro
            if (choice < 1 || choice > 3) {
                printf("Opção inválida! ");  // Informa erro sem quebrar linha
            }
        }
        // Se fgets falhar, choice permanece -1 e loop continua
    }

    #ifndef _WIN32
        // Volta para o modo não-canônico após obter entrada válida
        // Modo não-canônico permite captura imediata de teclas individuais
        struct termios new_termios = orig_termios;
        new_termios.c_lflag &= ~(ICANON | ECHO);  // Remove flags canônico e echo
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    #endif

    return choice;  // Retorna escolha válida (1, 2 ou 3)
}

/*
    Gerencia o menu de pausa do jogo
    
    Exibe opções de continuar, reiniciar ou desistir da partida
    
    Retorna 0 para continuar jogo ou 1 para sair ao menu principal
*/
int handle_pause_menu(GameState* game) {
    int choice;  // Armazena escolha do usuário no menu
    
    // Loop infinito até que uma ação válida seja executada
    while (1) {
        display_pause_menu();              // Mostra opções do menu de pausa
        choice = get_pause_choice_robust(); // Obtém escolha válida do usuário
        
        switch (choice) {
            case 1: // Opção: Continuar jogo atual
                return 0; // Retorna 0 para voltar direto ao jogo sem mensagens
                
            case 2: // Opção: Reiniciar partida atual
                // Reinicializa jogo com mesma dificuldade selecionada
                init_game(game, game->difficulty);
                clear_screen();
                printf("\n%sJogo reiniciado!%s\n", GREEN, RESET);
                printf("Nova palavra selecionada. Boa sorte!\n");
                printf("Pressione qualquer tecla para continuar...");
                get_char();  // Aguarda confirmação antes de continuar
                return 0;    // Retorna 0 para continuar com novo jogo
                
            case 3: // Opção: Desistir da partida
                clear_screen();
                // Revela a palavra-alvo com formatação especial
                printf("\n%s😔 Que pena! A palavra era: %s%s%s%s\n", 
                       GRAY, BOLD, game->target_word, RESET, RESET);
                printf("Não desista! Tente novamente!\n");
                printf("Pressione qualquer tecla para voltar ao menu...");
                get_char();  // Aguarda confirmação antes de sair
                return 1;    // Retorna 1 para sinalizar saída ao menu principal
                
            default:
                // Caso de erro inesperado (não deveria acontecer devido à validação)
                printf("Erro inesperado. Tente novamente.\n");
                break;  // Continua no loop para nova tentativa
        }
    }
}

/*
   Verifica se uma posição específica da palavra já foi descoberta
   
   Analisa tentativas anteriores para ver se a posição tem feedback verde
   
   Retorna 1 se posição foi resolvida, 0 caso contrário
*/
int is_position_solved(GameState* game, int position) {
   // Percorre todas as tentativas já realizadas pelo jogador
   for (int attempt = 0; attempt < game->current_attempt; attempt++) {
       if (game->feedback[attempt][position] == 2) { // Verde = letra correta na posição correta
           return 1;  // Posição já foi descoberta em tentativa anterior
       }
   }
   return 0;  // Posição ainda não foi descoberta
}

/*
    Obtém escolha do usuário no menu principal

    Captura um único caractere e valida se está entre 1 e 4

    Retorna número da opção escolhida ou -1 se inválida
*/
int get_menu_choice(void) {
   char input = get_char();  // Captura um único caractere do usuário
   if (input >= '1' && input <= '4') {
       return input - '0';  // Converte caractere para número inteiro
   }
   return -1;  // Retorna -1 para indicar entrada inválida
}

/*
   Exibe menu de seleção de dificuldade e obtém escolha do usuário
   
   Mostra opções de dificuldade com número de tentativas disponíveis
   
   Retorna número da dificuldade escolhida ou -1 se inválida
*/
int get_difficulty_choice(void) {
   clear_screen();  // Limpa tela para exibir menu limpo
   printf("\n%s=== SELECIONAR DIFICULDADE ===%s\n\n", BOLD, RESET);
   printf("  1. %sFácil%s   - 7 tentativas\n", GREEN, RESET);
   printf("  2. %sMédio%s   - 6 tentativas\n", YELLOW, RESET);
   printf("  3. %sDifícil%s - 5 tentativas\n", GRAY, RESET);
   printf("  4. %sDemo%s    - 5 tentativas (palavra: TESTE)\n\n", WHITE, RESET);
   printf("  Selecione a dificuldade (1-4): ");
   
   char input = get_char();  // Captura caractere digitado pelo usuário
   if (input >= '1' && input <= '4') {
       return input - '0';  // Converte caractere para número inteiro
   }
   return -1;  // Retorna -1 para indicar entrada inválida
}

/*
   Verifica se uma palavra existe nas listas de palavras válidas
   
   Converte palavra para maiúsculas e busca nas listas apropriadas
   
   Retorna 1 se palavra existe, 0 caso contrário
*/
int check_word_exists(GameState* game, const char* word) {
   char upper_word[WORD_LENGTH + 1];  // Buffer para palavra em maiúsculas
   
   // Converte toda a palavra para maiúsculas para comparação padronizada
   for (int i = 0; i < WORD_LENGTH; i++) {
       upper_word[i] = toupper(word[i]);
   }
   upper_word[WORD_LENGTH] = '\0';  // Adiciona terminador de string

   // Verifica primeiro na lista normal de palavras
   for (int i = 0; i < WORD_COUNT; i++) {
       if (strcmp(upper_word, word_list[i]) == 0) {
           return 1;  // Palavra encontrada na lista normal
       }
   }

   // Só verifica na lista difícil se estiver no modo difícil
   if (game->difficulty == HARD) {
       for (int i = 0; i < HARD_WORD_COUNT; i++) {
           if (strcmp(upper_word, hard_word_list[i]) == 0) {
               return 1;  // Palavra encontrada na lista difícil
           }
       }
   }

   return 0;  // Palavra não encontrada em nenhuma lista
}

/*
   Verifica se o jogador pode usar uma dica no momento atual
   
   Checa limite máximo de dicas e tempo de espera entre dicas
   
   Retorna 1 se pode usar dica, 0 caso contrário
*/
int can_use_hint(GameState* game) {
   if (game->hints_used >= MAX_HINTS) return 0;  // Já usou todas as dicas disponíveis
   if (game->hints_used == 0) return 1;          // Primeira dica sempre disponível
   
   time_t now = time(NULL);  // Obtém timestamp atual
   return (now - game->last_hint_time) >= HINT_DELAY;  // Verifica se passou tempo mínimo
}

/*
    Função principal do jogo - ponto de entrada da aplicação
    
    Inicializa sistema, carrega palavras e gerencia loop principal do menu
    
    Retorna 0 em caso de saída normal
*/
int main(void) {
    setup_console();  // Configura console para captura de teclas e cores

    // Carrega listas de palavras dos arquivos de texto
    WORD_COUNT = carregar_palavras(word_list, "palavras.txt", 0);
    HARD_WORD_COUNT = carregar_palavras(hard_word_list, "palavras_dificeis.txt", 1);
    
    // Verifica se carregou quantidade mínima de palavras necessárias
    if (WORD_COUNT < 100 || HARD_WORD_COUNT < 10) {
        printf("Erro: Bancos de palavras não carregados adequadamente\n");
        exit(1);  // Encerra programa se não conseguiu carregar palavras
    }

    // Declaração de variáveis para controle do jogo
    int choice;              // Escolha do usuário no menu principal
    int difficulty_choice;   // Escolha de dificuldade selecionada
    GameState game;         // Estado atual do jogo
    char* guess;            // Palpite atual do jogador
    
    // Loop principal do programa - executa até usuário escolher sair
    while (1) {
        display_menu();           // Exibe menu principal
        choice = get_menu_choice(); // Obtém escolha do usuário
        
        switch (choice) {
            case 1: // Opção: Jogar
                difficulty_choice = get_difficulty_choice(); // Seleciona dificuldade
                if (difficulty_choice >= 1 && difficulty_choice <= 4) {
                    init_game(&game, (Difficulty)difficulty_choice); // Inicializa nova partida
                    
                   // Loop principal da partida - executa até jogo terminar
                   while (!game.game_over) {
                        display_game_board(&game);  // Mostra tabuleiro atual
                        display_keyboard(&game);    // Mostra teclado virtual
                        
                        guess = get_guess_with_pause(&game); // Obtém palpite do jogador
                        
                        // Se o jogador desistiu no menu de pausa, sair do loop
                        if (guess == NULL) {
                            break;  // Encerra partida atual
                        }
                        
                        // Verifica se palavra digitada existe no dicionário
                        if (!check_word_exists(&game, guess)) {
                            printf("Palavra não encontrada no dicionário. Tente novamente.\n");
                            printf("Pressione qualquer tecla para continuar...");
                            get_char();  // Aguarda confirmação do usuário
                            continue;    // Volta ao início do loop sem processar palpite
                        }
                        
                        process_guess(&game, guess); // Processa palpite válido
                    }
                    
                    // Só mostra tela final se jogador não desistiu
                    if (guess != NULL) {
                        display_game_board(&game);  // Mostra estado final do tabuleiro
                        display_keyboard(&game);    // Mostra teclado final
                        display_game_over(&game, game.difficulty);   // Exibe resultado da partida
                    }
                }
                break;
                
            case 2: // Opção: Como Jogar
                display_how_to_play(); // Exibe instruções do jogo
                break;
                
            case 3: // Opção: Resultados
                display_results(); // Mostra histórico de resultados salvos
                break;
                
            case 4: // Opção: Sair
                clear_screen();
                printf("Obrigado por jogar! Até logo!\n");
                restore_console(); // Restaura configurações originais do console
                return 0;          // Encerra programa normalmente
                
            default:
                // Trata escolhas inválidas no menu
                printf("Escolha inválida. Pressione qualquer tecla para continuar...");
                get_char();
                break; // Volta ao menu principal
        }
    }
    
    restore_console(); // Restaura console (caso de segurança, nunca executado)
    return 0;
}