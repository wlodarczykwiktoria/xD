#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_PLAYERS 10 // maksymalna liczba graczy
#define NUMBERS_TO_PICK 6 // liczba liczb do wylosowania
#define BET_COST 3 // koszt zakładu
#define RESIGNATION_PROBABILITY 10 // prawdopodobieństwo rezygnacji gracza w procentach

void player_process(int player_id, int write_fd) {
    int numbers[NUMBERS_TO_PICK] = {0}; // tablica na wytypowane liczby

    // Losowanie, czy gracz zrezygnuje z dalszej gry
    if (rand() % 100 < RESIGNATION_PROBABILITY) {
        printf("[%d]: This is a waste of money\n", player_id);
        exit(EXIT_SUCCESS);
    }

    // Wybór losowych liczb przez gracza
    srand(player_id); // ustawienie ziarna generatora liczb losowych
    for (int i = 0; i < NUMBERS_TO_PICK; i++) {
        numbers[i] = rand() % 49 + 1; // losowanie liczby z zakresu 1-49
    }

    // Wysłanie wytypowanych liczb do Totalizatora Sportowego
    write(write_fd, numbers, sizeof(numbers));

    printf("[%d]: I'm going to play Lotto\n", player_id);
}

void totalizator_process(int read_fds[][2], int N, int T) {
    int total_bets = 0; // łączna kwota wydana na zakłady
    int total_rewards = 0; // łączna kwota wygranych

    for (int week = 0; week < T; week++) {
        int lucky_numbers[NUMBERS_TO_PICK] = {0}; // tablica na wylosowane liczby

        printf("\nWEEK %d\n", week+1);
        // Odbieranie zakładów od graczy i wypisywanie komunikatu
        for (int i = 0; i < N; i++) {
            int player_numbers[NUMBERS_TO_PICK];
            read(read_fds[i][0], player_numbers, sizeof(player_numbers));
            printf("Totalizator Sportowy: [%d] bet: [%d %d %d %d %d %d]$\n",
                   i + 1, player_numbers[0], player_numbers[1], player_numbers[2], player_numbers[3], player_numbers[4], player_numbers[5]);
            total_bets += BET_COST;
        }

        // Losowanie liczb
        srand(getpid() * week); // ustawienie ziarna generatora liczb losowych
        printf("Totalizator Sportowy: ");
        for (int i = 0; i < NUMBERS_TO_PICK; i++) {
            lucky_numbers[i] = rand() % 49 + 1; // losowanie liczby z zakresu 1-49
            printf("%d ", lucky_numbers[i]);
        }
        printf("are today’s lucky numbers\n");

        // Wysłanie wylosowanych liczb do wszystkich graczy
        for (int i = 0; i < N; i++) {
            write(read_fds[i][1], lucky_numbers, sizeof(lucky_numbers));
        }

        // Odbieranie wygranych graczy i wypisywanie komunikatu
        for (int i = 0; i < N; i++) {
            int reward;
            read(read_fds[i][0], &reward, sizeof(reward));
            if (reward > 0) {
                printf("[%d]: I won %dzł\n", i + 1, reward);
                total_rewards += reward;
            }
        }
    }

    printf("\nTotalizator Sportowy: Total bets: %dzł, Total rewards: %dzł\n", total_bets, total_rewards);
}

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s N T\n", name);
    fprintf(stderr, "N: N >= 1 - number of players\n");
    fprintf(stderr, "T: T >= 1 - number of weeks (iterations)\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int N = atoi(argv[1]); // liczba graczy
    int T = atoi(argv[2]); // liczba tygodni

    if (N < 1 || T < 1 || N > MAX_PLAYERS) {
        printf("Błędne parametry\n");
        return 1;
    }

    pid_t pid = getpid();
    srand(pid);
    int read_fds[N][2]; // tablica łączy pipe do odczytu

    // Tworzenie łączy pipe dla każdego gracza
    for (int i = 0; i < N; i++) {
        if (pipe(read_fds[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Tworzenie procesów graczy
    for (int player_id = 1; player_id <= N; player_id++) {
        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // proces potomny
            // Zamknięcie końca do zapisu
            close(read_fds[player_id - 1][0]);
            player_process(player_id, read_fds[player_id - 1][1]);
            exit(EXIT_SUCCESS);
        }
    }

    // Proces Totalizatora Sportowego
    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // proces potomny
        // Zamknięcie wszystkich końcówek do zapisu
        for (int i = 0; i < N; i++) {
            close(read_fds[i][1]);
        }
        totalizator_process(read_fds, N, T);
        exit(EXIT_SUCCESS);
    }

    // Zamknięcie wszystkich końcówek łączy pipe w procesie głównym
    for (int i = 0; i < N; i++) {
        close(read_fds[i][0]);
        close(read_fds[i][1]);
    }

    // Oczekiwanie na zakończenie wszystkich procesów potomnych (graczy i Totalizatora Sportowego)
    for (int i = 0; i < N + 1; i++) {
        wait(NULL);
    }

    return 0;
}
