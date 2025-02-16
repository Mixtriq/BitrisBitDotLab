#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"

#include "ws2812.pio.h" 

#define MAT_LED_PIN 7
#define BOTAO_A_PIN 5
#define BOTAO_B_PIN 6

#define NUM_LEDS 25
#define NUM_LINHAS 5
#define NUM_COLUNAS 5
#define LARG_PECAS 2
#define COMP_PECAS 2

int matriz[NUM_LINHAS][NUM_COLUNAS] = {};

int pecaHorizontal[LARG_PECAS][COMP_PECAS] = {{1,1},{0,0}};
int pecaVertical[LARG_PECAS][COMP_PECAS] = {{1,0},{1,0}};

int pecaAtiva[LARG_PECAS][COMP_PECAS] = {}; 

int x = 0;
int y = 0;

PIO pio;
uint sm;

// Mapeia a ordem dos LEDs para as posições equivalerem a matriz lógica do jogo
int mapearIndiceLED(int linha, int coluna) {
    int linhaInvertida = NUM_LINHAS - 1 - linha;

    if (linhaInvertida % 2 == 0) {
        return linhaInvertida * NUM_COLUNAS + (NUM_COLUNAS - 1 - coluna);
    } else {
        return linhaInvertida * NUM_COLUNAS + coluna;
    }
}

// Apaga todos os leds para ter uma janela de tempo de reiniciar a placa sem os leds ficarem acesos indefinidamente
void limparLeds() {
    for (int i = 0; i < NUM_LEDS; i++) {
        pio_sm_put_blocking(pio, sm, 0x000000);
    }
    sleep_ms(1);
}

// Reinicia a matriz lógica para recomeçar o jogo
void limparMatriz(){
    memset(matriz, 0, sizeof(matriz));
}

// Imprime a matriz do jogo no monitor serial
// Usada para debug, não é chamada nesta versão do código
void imprimirMatriz() {
    printf("\nEstado da Matriz:\n");
    for (int i = 0; i < NUM_LINHAS; i++) {
        for (int j = 0; j < NUM_COLUNAS; j++) {
            printf("%d ", matriz[i][j]);
        }
        printf("\n");
    }
}

void atualizarLeds() {
    uint32_t leds[NUM_LEDS] = {0};

    for (int i = 0; i < NUM_LINHAS; i++) {
        for (int j = 0; j < NUM_COLUNAS; j++) {
            int ledIndex = mapearIndiceLED(i, j);
            if (matriz[i][j] == 1) {
                leds[ledIndex] = 0x00FF00; // Vermelho para peças fixadas
            } else if (matriz[i][j] == 2) {
                leds[ledIndex] = 0xFF0000; // Verde para a peça em movimento
            } else {
                leds[ledIndex] = 0x000000; // Apagado
            }
        }
    }

    for (int i = 0; i < NUM_LEDS; i++) {
        pio_sm_put_blocking(pio, sm, leds[i]);
    }
}

// Verifica se uma peça esta colidindo com uma peça fixada ou com as bordas da matriz
bool colisao(int x, int y, int peca[LARG_PECAS][COMP_PECAS]) {
    for (int i = 0; i < LARG_PECAS; i++) {
        for (int j = 0; j < COMP_PECAS; j++) {
            if (peca[i][j] == 1) {
                if (// A parte verificada da peça está além da ou encostando na altura maxima da matriz?
                    y + i >= NUM_LINHAS ||
                    // A parte verificada da peça está na primeira coluna da matriz?
                    x + j < 0 ||
                    // A parte verificada da peça está além da ou encostando na ultima coluna da matriz?
                    x + j >= NUM_COLUNAS ||
                    // A parte verificada está encostando numa peça fixada na matriz?
                    matriz[y + i][x + j] == 1) {
                        printf("Colisao detectada\n");
                    return true;

                }
            }
        }
    }
    return false; 
}

void limparLinha() {
    // Verifica se alguma linha está preenchida
    for (int i = 0; i < NUM_LINHAS; i++) {
        bool linha_completa = true;
        for (int j = 0; j < NUM_COLUNAS; j++) {
            if (matriz[i][j] == 0 || matriz[i][j] == 2) {
                linha_completa = false;
                break;
            }
        }
        // Se sim, limpa a linha e desce as linhas acima
        if (linha_completa) {
            for (int j = 0; j < NUM_COLUNAS; j++) {
                matriz[i][j] = 0;
            }
            for (int k = i; k > 0; k--) {
                for (int j = 0; j < NUM_COLUNAS; j++) {
                    matriz[k][j] = matriz[k - 1][j];
                }
            }
        }
    }
}

// Transforma uma peça em movimento numa peça fixa na matriz
void fixar(int x, int y, int peca[LARG_PECAS][COMP_PECAS]) {
    for (int i = 0; i < LARG_PECAS; i++) {
        for (int j = 0; j < COMP_PECAS; j++) {
            if (peca[i][j] == 1) {
                matriz[y + i][x + j] = 1;
            }
        }
    }
    limparLinha();
}


// Escolhe aleatóriamente uma das duas peças para criar
void criarNovaPeca() {

    x = 0;
    y = 0;

    int escolha = rand() % 2; 
    if (escolha == 0) {
        memcpy(pecaAtiva, pecaHorizontal, sizeof(pecaHorizontal));
    } else {
        memcpy(pecaAtiva, pecaVertical, sizeof(pecaVertical));
    }
    

    // Se a peça criada colidir com alguma peça fixada, ele da Game Over acendendo todos os leds por alguns segundos
    if (colisao(x, y, pecaAtiva)) {
        int tempo = 0;
        int tempoMaximo = 2000;
        limparMatriz();
        while (tempo <= tempoMaximo){
            for(int i = 0; i < NUM_LEDS; i++){
                pio_sm_put_blocking(pio, sm, 0xFFFFFF);
            }
            tempo += 1000;
            sleep_ms(1000);
        }
        atualizarLeds();
    }
    atualizarLeds(pio, sm);
}

// Atualiza a matriz com a peça em movimento, atribuindo o valor 2 as partes da matriz onde a peça atual está ocupando
void atualizarMatrizComPeca(int x, int y, int peca[LARG_PECAS][COMP_PECAS]) {
    // Primeiro apaga a posição antiga da peça
    for (int i = 0; i < NUM_LINHAS; i++) {
        for (int j = 0; j < NUM_COLUNAS; j++) {
            if (matriz[i][j] == 2) {
                matriz[i][j] = 0;
            }
        }
    }
    
    // Então atribui o valor 2 onde a peça vai ocupar
    for (int i = 0; i < LARG_PECAS; i++) {
        for (int j = 0; j < COMP_PECAS; j++) {
            // Verifica quais partes da matriz da peça são ocupadas
            //              para o caso de uma peça em formato de L que poderia ser: [1,1]
            // E verifica se a peça está numa posição dentro do campo                [0,1]
            if (peca[i][j] == 1 && y + i < NUM_LINHAS && x + j < NUM_COLUNAS) {
                matriz[y + i][x + j] = 2;
            }
        }
    }
}

// Desce a peça atual em 1 linha
void gravidade(int* x, int* y, int peca[LARG_PECAS][COMP_PECAS]) {
    if (!colisao(*x, *y + 1, peca)) {
        (*y)++;
        atualizarMatrizComPeca(*x, *y, pecaAtiva);
    } else {
        fixar(*x, *y, peca);
        criarNovaPeca(pio,sm);
    }
    atualizarLeds();
}

// Muda a posição da peça um espaço desejado.
// Ex: chamar moverPeca(2,0) muda a posição da peça duas casas para a direita
void moverPeca(int dx, int dy) {
    printf("Tentando mover a peça para x: %d, y: %d\n", x + dx, y + dy);
    if (!colisao(x + dx, y + dy, pecaAtiva)) {
        x += dx;
        y += dy;
        printf("Peça movida para x: %d, y: %d\n", x, y);
    } else {
        printf("Movimento bloqueado por colisão\n");
    }
    atualizarMatrizComPeca(x, y, pecaAtiva);
    atualizarLeds();
}

void botoes_interrupt_handler(uint gpio, uint32_t events) {
    printf("Interrupção acionada! GPIO: %d | Evento: %d\n", gpio, events);
    if (gpio == BOTAO_A_PIN) {
        printf("movendo a peça para esquerda\n");
        moverPeca(-1, 0);  
    } else if (gpio == BOTAO_B_PIN) {
        printf("movendo a peça para direita\n");
        moverPeca(1, 0);  
    }
    atualizarLeds();  
}

int main() {

    stdio_init_all();

    gpio_init(BOTAO_A_PIN);
    gpio_init(BOTAO_B_PIN);

    gpio_set_dir(BOTAO_A_PIN, GPIO_IN);
    gpio_set_dir(BOTAO_B_PIN, GPIO_IN);

    gpio_pull_up(BOTAO_A_PIN); 
    gpio_pull_up(BOTAO_B_PIN);

    pio = pio0; 
    sm = pio_claim_unused_sm(pio, true);


    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, MAT_LED_PIN, 800000, false);
    gpio_set_irq_enabled_with_callback(BOTAO_A_PIN, GPIO_IRQ_EDGE_FALL, true, &botoes_interrupt_handler);
    gpio_set_irq_enabled(BOTAO_B_PIN, GPIO_IRQ_EDGE_FALL, true);

    limparLeds();
    sleep_ms(5000);

    criarNovaPeca();
    sleep_ms(100);
    atualizarLeds();

    while (1) { 
        gravidade(&x, &y, pecaAtiva);
        sleep_ms(1000);
    }
}

