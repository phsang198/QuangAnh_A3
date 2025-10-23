#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include "ai.h"
#include "gate.h"
#include "radix.h"
#include "utils.h"

#define DEBUG 0

#define UP 'u'
#define DOWN 'd'
#define LEFT 'l'
#define RIGHT 'r'
char directions[] = {UP, DOWN, LEFT, RIGHT};
char invertedDirections[] = {DOWN, UP, RIGHT, LEFT};
char pieceNames[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

/* ---------------- QUEUE IMPLEMENTATION ---------------- */

typedef struct QueueNode {
    gate_t *state;
    struct QueueNode *next;
} QueueNode;

typedef struct Queue {
    QueueNode *front;
    QueueNode *rear;
    int size;
} Queue;

Queue *createQueue() {
    Queue *q = malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

bool isQueueEmpty(Queue *q) {
    return (q->size == 0);
}

void enqueue(Queue *q, gate_t *state) {
    QueueNode *newNode = malloc(sizeof(QueueNode));
    newNode->state = state;
    newNode->next = NULL;
    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->size++;
}

gate_t *dequeue(Queue *q) {
    if (isQueueEmpty(q)) return NULL;
    QueueNode *temp = q->front;
    gate_t *state = temp->state;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    q->size--;
    return state;
}

void freeQueue(Queue *q) {
    while (!isQueueEmpty(q)) {
        gate_t *s = dequeue(q);
        free(s); // Không gọi free_state ở đây
    }
    free(q);
}

/* ---------------- STATE MANAGEMENT ---------------- */

gate_t* duplicate_state(gate_t* gate) {
    gate_t* duplicate = (gate_t*)malloc(sizeof(gate_t));
    assert(duplicate);

    memcpy(duplicate, gate, sizeof(gate_t));

    // Sao chép bản đồ
    duplicate->map = malloc(sizeof(char*) * gate->lines);
    for (int i = 0; i < gate->lines; i++) {
        duplicate->map[i] = strdup(gate->map[i]);
    }

    // Sao chép đường đi
    if (gate->soln)
        duplicate->soln = strdup(gate->soln);
    else
        duplicate->soln = strdup("");

    // Không cần buffer/map_save
    duplicate->buffer = NULL;
    duplicate->map_save = NULL;

    return duplicate;
}

void free_state(gate_t* stateToFree, __attribute__((unused)) gate_t *init_data) {
    if (!stateToFree) return;

    if (stateToFree->map) {
        for (int i = 0; i < stateToFree->lines; i++)
            free(stateToFree->map[i]);
        free(stateToFree->map);
    }
    if (stateToFree->soln)
        free(stateToFree->soln);

    free(stateToFree);
}

void free_initial_state(gate_t *init_data) {
    if (!init_data) return;

    if (init_data->buffer)
        free(init_data->buffer);

    if (init_data->map_save) {
        for (int i = 0; i < init_data->lines; i++)
            free(init_data->map_save[i]);
        free(init_data->map_save);
    }

    if (init_data->map) {
        for (int i = 0; i < init_data->lines; i++)
            free(init_data->map[i]);
        free(init_data->map);
    }
}

/* ---------------- PUZZLE SOLVER ---------------- */

/**
 * Given a game state, work out the number of bytes required to store the state.
*/
int getPackedSize(gate_t *gate) {
	int pBits = calcBits(gate->num_pieces);
    int hBits = calcBits(gate->lines);
    int wBits = calcBits(gate->num_chars_map / gate->lines);
    int atomSize = pBits + hBits + wBits;
	int bitCount = atomSize * gate->num_pieces;
	return bitCount;
}

/**
 * Store state of puzzle in map.
*/
void packMap(gate_t *gate, unsigned char *packedMap) {
	int pBits = calcBits(gate->num_pieces);
    int hBits = calcBits(gate->lines);
    int wBits = calcBits(gate->num_chars_map / gate->lines);
	int bitIdx = 0;
	for(int i = 0; i < gate->num_pieces; i++) {
		for(int j = 0; j < pBits; j++) {
			if(((i >> j) & 1) == 1) bitOn(packedMap, bitIdx);
			else bitOff(packedMap, bitIdx);
			bitIdx++;
		}
		for(int j = 0; j < hBits; j++) {
			if(((gate->piece_y[i] >> j) & 1) == 1) bitOn(packedMap, bitIdx);
			else bitOff(packedMap, bitIdx);
			bitIdx++;
		}
		for(int j = 0; j < wBits; j++) {
			if(((gate->piece_x[i] >> j) & 1) == 1) bitOn(packedMap, bitIdx);
			else bitOff(packedMap, bitIdx);
			bitIdx++;
		}
	}
}

/**
 * Check if the given state is in a won state.
 */
bool winning_state(gate_t gate) {
	for (int i = 0; i < gate.lines; i++) {
		for (int j = 0; gate.map_save[i][j] != '\0'; j++) {
			if (gate.map[i][j] == 'G' || (gate.map[i][j] >= 'I' && gate.map[i][j] <= 'Q')) {
				return false;
			}
		}
	}
	return true;
}

/* ---------------- FIND SOLUTION ---------------- */

void find_solution(gate_t* init_data) {
	int packedBytes = getPackedSize(init_data);
	unsigned char *packedMap = calloc(packedBytes, sizeof(unsigned char));
	assert(packedMap);

	int dequeued = 0;
	int enqueued = 0;
	int duplicatedNodes = 0;
	char *soln = "";
	double start = now(), elapsed;

	int w = init_data->num_pieces + 1;

	printf("Select algorithm (1=BFS, 2=BFS+Radix, 3=IW): ");
	int algo = 2; // đổi bằng input nếu muốn
	scanf("%d", &algo);

	Queue *queue = createQueue();
	enqueue(queue, init_data);
	enqueued++;

	// Radix Tree cho thuật toán 2 và 3
	struct radixTree *radixTree = NULL;
	struct radixTree **rts = NULL;
	if (algo == 2)
		radixTree = getNewRadixTree(init_data->num_pieces, init_data->lines,
			init_data->num_chars_map / init_data->lines);
	else if (algo == 3) {
		rts = malloc(sizeof(struct radixTree*) * w);
		for (int i = 0; i < w; i++)
			rts[i] = getNewRadixTree(init_data->num_pieces, init_data->lines,
				init_data->num_chars_map / init_data->lines);
	}

	bool found = false;
	while (!isQueueEmpty(queue) && !found) {
    gate_t *current = dequeue(queue);
    dequeued++;

    if (winning_state(*current)) {
        soln = current->soln;
        found = true;
        free_state(current, init_data);
        break;
    }

    for (int p = 0; p < current->num_pieces && !found; p++) {
        for (int d = 0; d < 4 && !found; d++) {
            gate_t moved_state = attempt_move(*current, pieceNames[p], directions[d]);
            gate_t *child = duplicate_state(&moved_state);

            // Kiểm tra có thực sự di chuyển không
            bool moved = false;
            for (int i = 0; i < current->lines; i++) {
                if (strcmp(current->map[i], child->map[i]) != 0) {
                    moved = true;
                    break;
                }
            }
            if (!moved) {
                free_state(child, init_data);
                continue;
            }

            // Cập nhật chuỗi di chuyển
            size_t len = strlen(current->soln) + 3;
            char *newSoln = malloc(len);
            snprintf(newSoln, len, "%s%c%c", current->soln, pieceNames[p], directions[d]);
            free(child->soln);
            child->soln = newSoln;

            // Kiểm tra trạng thái thắng
            if (winning_state(*child)) {
                soln = child->soln;
                found = true;
                free_state(current, init_data);
                free_state(child, init_data); // hoặc giữ lại nếu bạn muốn in bản đồ kết quả
                break;
            }

            // Nén trạng thái
            memset(packedMap, 0, packedBytes);
            packMap(child, packedMap);

            // Kiểm tra trùng tuỳ theo thuật toán
            bool duplicate = false;
            if (algo == 2 && checkPresent(radixTree, packedMap, init_data->num_pieces) == PRESENT)
                duplicate = true;
            else if (algo == 3) {
                for (int a = 0; a < w; a++) {
                    if (!checkPresentnCr(rts[a], packedMap, a + 1)) {
                        insertRadixTreenCr(rts[a], packedMap, a + 1);
                        duplicate = false;
                        break;
                    }
                    duplicate = true;
                }
            }

            if (duplicate) {
                duplicatedNodes++;
                free_state(child, init_data);
                continue;
            }

            if (algo == 2)
                insertRadixTree(radixTree, packedMap, init_data->num_pieces);

            enqueue(queue, child);
            enqueued++;
        }
    }
    free_state(current, init_data);
}
	elapsed = now() - start;
	printf("Solution path: %s\n", soln);
	printf("Execution time: %lf\n", elapsed);
	printf("Expanded nodes: %d\n", dequeued);
	printf("Generated nodes: %d\n", enqueued);
	printf("Duplicated nodes: %d\n", duplicatedNodes);
	int memoryUsage = 0;
	if (algo == 2)
		memoryUsage = queryRadixMemoryUsage(radixTree);
	else if (algo == 3) {
		for (int i = 0; i < w; i++)
			memoryUsage += queryRadixMemoryUsage(rts[i]);
	}
	printf("Auxiliary memory usage (bytes): %d\n", memoryUsage);
	printf("Number of pieces in the puzzle: %d\n", init_data->num_pieces);
	printf("Number of steps in solution: %ld\n", strlen(soln)/2);

	int emptySpaces = 0;
	for (int i = 0; i < init_data->lines; i++) {
		size_t len = strlen(init_data->map[i]);
		for (size_t j = 0; j < len; j++) {
			if (init_data->map[i][j] == '.')
				emptySpaces++;
		}
	}
	printf("Number of empty spaces: %d\n", emptySpaces);
	printf("Solved by IW(%d)\n", w);
	printf("Nodes expanded per second: %lf\n", (dequeued + 1) / elapsed);

	free(packedMap);
	if (radixTree)
		freeRadixTree(radixTree);
	if (rts) {
		for (int i = 0; i < w; i++)
			freeRadixTree(rts[i]);
		free(rts);
	}
	freeQueue(queue);
	free_initial_state(init_data);
}

/* ---------------- ENTRY POINT ---------------- */

void solve(char const *path) {
	gate_t gate = make_map(path, gate);
	map_check(gate);
	gate = find_player(gate);
	gate = find_pieces(gate);
	gate.base_path = path;
	find_solution(&gate);
}
