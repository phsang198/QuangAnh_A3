#include <stdio.h>

// =============================
// Iterated Width Search (IW Search) - Algorithm 3
// =============================
// Hàm này là khung để bạn dễ dàng mở rộng thuật toán kiểm tra novelty (độ mới) của trạng thái.
// Có thể gọi hàm này thay cho find_solution để so sánh kết quả các thuật toán.
//
// width_limit: giá trị novelty tối đa cho phép (IW(1), IW(2), ...)
void find_solution_iw(gate_t* init_data, int width_limit) {
	typedef struct Node {
		gate_t* state;
		int cost;
		char* path;
	} Node;
	#define MAX_QUEUE 100000
	int packedBytes = getPackedSize(init_data);
	int max_width = width_limit;
	char* soln = NULL;
	int found_width = 0;
	int found_steps = 0;
	int found_dequeued = 0, found_enqueued = 0, found_duplicated = 0;
	double found_time = 0;
	int found_memory = 0;
	double start_time = now();

	for (int width = 1; width <= max_width; width++) {
		// Mỗi width là một lượt BFS mới
		Node* queue[MAX_QUEUE];
		int front = 0, rear = 0;
		struct radixTree* novelty_tree = getNewRadixTree(init_data->num_pieces, init_data->lines, init_data->num_chars_map / init_data->lines);
		Node* start = (Node*)malloc(sizeof(Node));
		start->state = duplicate_state(init_data);
		start->cost = 0;
		start->path = strdup("");
		queue[rear++] = start;
		int dequeued = 0, enqueued = 1, duplicatedNodes = 0;
		int solved = 0;

		while (front < rear) {
			Node* curr = queue[front++];
			dequeued++;
			unsigned char* packedMap = (unsigned char*)calloc(packedBytes, sizeof(unsigned char));
			packMap(curr->state, packedMap);
			// novelty kiểm tra: nếu tất cả tổ hợp size <= width đã từng xuất hiện thì bỏ qua
			if (checkPresentnCr(novelty_tree, packedMap, width)) {
				duplicatedNodes++;
				free(packedMap);
				free_state(curr->state, NULL);
				free(curr->path);
				free(curr);
				continue;
			}
			// Lưu tất cả tổ hợp size <= width
			insertRadixTreenCr(novelty_tree, packedMap, width);
			free(packedMap);

			if (winning_state(*(curr->state))) {
				soln = strdup(curr->path);
				found_width = width;
				found_steps = strlen(soln)/2;
				found_dequeued = dequeued;
				found_enqueued = enqueued;
				found_duplicated = duplicatedNodes;
				found_time = now() - start_time;
				found_memory = queryRadixMemoryUsage(novelty_tree);
				// Giải phóng queue còn lại
				for (int i = front; i < rear; i++) {
					free_state(queue[i]->state, NULL);
					free(queue[i]->path);
					free(queue[i]);
				}
				solved = 1;
				break;
			}
			for (int p = 0; p < curr->state->num_pieces; p++) {
				for (int d = 0; d < 4; d++) {
					gate_t* next_state = duplicate_state(curr->state);
					char piece = '0' + p;
					char dir = directions[d];
					next_state = move_location(*next_state, piece, dir);
					int changed = 0;
					for (int i = 0; i < next_state->lines; i++) {
						if (strcmp(next_state->map[i], curr->state->map[i]) != 0) {
							changed = 1;
							break;
						}
					}
					if (!changed) {
						free_state(next_state, NULL);
						continue;
					}
					size_t plen = strlen(curr->path);
					char* newpath = (char*)malloc(plen + 3);
					strcpy(newpath, curr->path);
					newpath[plen] = piece;
					newpath[plen+1] = dir;
					newpath[plen+2] = '\0';
					Node* newnode = (Node*)malloc(sizeof(Node));
					newnode->state = next_state;
					newnode->cost = curr->cost + 1;
					newnode->path = newpath;
					queue[rear++] = newnode;
					enqueued++;
				}
			}
			free_state(curr->state, NULL);
			free(curr->path);
			free(curr);
		}
		freeRadixTree(novelty_tree);
		if (solved) break;
	}

	// Xuất kết quả đúng định dạng
	printf("[IW Search] Solution path: %s\n", soln ? soln : "NO SOLUTION");
	printf("Execution time: %lf\n", found_time);
	printf("Expanded nodes: %d\n", found_dequeued);
	printf("Generated nodes: %d\n", found_enqueued);
	printf("Duplicated nodes: %d\n", found_duplicated);
	printf("Auxiliary memory usage (bytes): %d\n", found_memory);
	printf("Number of pieces in the puzzle: %d\n", init_data->num_pieces);
	printf("Number of steps in solution: %d\n", found_steps);
	int emptySpaces = 0;
	for (int i = 0; i < init_data->lines; i++) {
		for (int j = 0; init_data->map[i][j] != '\0'; j++) {
			if (init_data->map[i][j] == ' ') emptySpaces++;
		}
	}
	printf("Number of empty spaces: %d\n", emptySpaces);
	printf("Solved by IW(%d)\n", found_width);
	printf("Number of nodes expanded per second: %lf\n", (found_dequeued + 1) / (found_time > 0 ? found_time : 1));
	if (soln) free(soln);
	free_initial_state(init_data);
}

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

/**
 * Given a game state, work out the number of bytes required to store the state.
*/
int getPackedSize(gate_t *gate);

/**
 * Store state of puzzle in map.
*/
void packMap(gate_t *gate, unsigned char *packedMap);

/**
 * Check if the given state is in a won state.
 */
bool winning_state(gate_t gate);

gate_t* duplicate_state(gate_t* gate) {
       gate_t* duplicate = (gate_t*)malloc(sizeof(gate_t));
       if (!duplicate) return NULL;
       memcpy(duplicate, gate, sizeof(gate_t));
       // Deep copy buffer
       if (gate->buffer) {
	       size_t buflen = strlen(gate->buffer) + 1;
	       duplicate->buffer = (char*)malloc(buflen);
	       memcpy(duplicate->buffer, gate->buffer, buflen);
       }
       // Deep copy map
       if (gate->map) {
	       duplicate->map = (char**)malloc(sizeof(char*) * gate->lines);
	       for (int i = 0; i < gate->lines; i++) {
		       size_t len = strlen(gate->map[i]) + 1;
		       duplicate->map[i] = (char*)malloc(len);
		       memcpy(duplicate->map[i], gate->map[i], len);
	       }
       }
       // Deep copy map_save
       if (gate->map_save) {
	       duplicate->map_save = (char**)malloc(sizeof(char*) * gate->lines);
	       for (int i = 0; i < gate->lines; i++) {
		       size_t len = strlen(gate->map_save[i]) + 1;
		       duplicate->map_save[i] = (char*)malloc(len);
		       memcpy(duplicate->map_save[i], gate->map_save[i], len);
	       }
       }
       // Deep copy soln
       if (gate->soln) {
	       size_t slen = strlen(gate->soln) + 1;
	       duplicate->soln = (char*)malloc(slen);
	       memcpy(duplicate->soln, gate->soln, slen);
       }
       return duplicate;
}

/**
 * Without lightweight states, the second argument may not be required.
 * Its use is up to your decision.
 */
void free_state(gate_t* stateToFree, gate_t *init_data) {
       if (!stateToFree) return;
       if (stateToFree->map) {
	       for (int i = 0; i < stateToFree->lines; i++) {
		       if (stateToFree->map[i]) free(stateToFree->map[i]);
	       }
	       free(stateToFree->map);
       }
       if (stateToFree->map_save) {
	       for (int i = 0; i < stateToFree->lines; i++) {
		       if (stateToFree->map_save[i]) free(stateToFree->map_save[i]);
	       }
	       free(stateToFree->map_save);
       }
       if (stateToFree->buffer) free(stateToFree->buffer);
       if (stateToFree->soln) free(stateToFree->soln);
       free(stateToFree);
}

void free_initial_state(gate_t *init_data) {
       if (!init_data) return;
       if (init_data->map) {
	       for (int i = 0; i < init_data->lines; i++) {
		       if (init_data->map[i]) free(init_data->map[i]);
	       }
	       free(init_data->map);
       }
       if (init_data->map_save) {
	       for (int i = 0; i < init_data->lines; i++) {
		       if (init_data->map_save[i]) free(init_data->map_save[i]);
	       }
	       free(init_data->map_save);
       }
       if (init_data->buffer) free(init_data->buffer);
       if (init_data->soln) free(init_data->soln);
}

/**
 * Find a solution by exploring all possible paths
 */

// =============================
// 1. Uniform Cost Search (UCS) - KHÔNG loại duplicate
// =============================
void find_solution_ucs(gate_t* init_data) {
	typedef struct Node {
		gate_t* state;
		int cost;
		char* path;
	} Node;
	#define MAX_QUEUE 100000
	Node* queue[MAX_QUEUE];
	int front = 0, rear = 0;
	Node* start = (Node*)malloc(sizeof(Node));
	start->state = duplicate_state(init_data);
	start->cost = 0;
	start->path = strdup("");
	queue[rear++] = start;
	int dequeued = 0, enqueued = 1;
	char* soln = NULL;
	bool has_won = false;
	double start_time = now();
	double elapsed;
	while (front < rear) {
		Node* curr = queue[front++];
		dequeued++;
		if (winning_state(*(curr->state))) {
			has_won = true;
			soln = strdup(curr->path);
			for (int i = front; i < rear; i++) {
				free_state(queue[i]->state, NULL);
				free(queue[i]->path);
				free(queue[i]);
			}
			break;
		}
		for (int p = 0; p < curr->state->num_pieces; p++) {
			for (int d = 0; d < 4; d++) {
				gate_t* next_state = duplicate_state(curr->state);
				char piece = '0' + p;
				char dir = directions[d];
				next_state = move_location(*next_state, piece, dir);
				int changed = 0;
				for (int i = 0; i < next_state->lines; i++) {
					if (strcmp(next_state->map[i], curr->state->map[i]) != 0) {
						changed = 1;
						break;
					}
				}
				if (!changed) {
					free_state(next_state, NULL);
					continue;
				}
				size_t plen = strlen(curr->path);
				char* newpath = (char*)malloc(plen + 3);
				strcpy(newpath, curr->path);
				newpath[plen] = piece;
				newpath[plen+1] = dir;
				newpath[plen+2] = '\0';
				Node* newnode = (Node*)malloc(sizeof(Node));
				newnode->state = next_state;
				newnode->cost = curr->cost + 1;
				newnode->path = newpath;
				queue[rear++] = newnode;
				enqueued++;
			}
		}
		free_state(curr->state, NULL);
		free(curr->path);
		free(curr);
	}
	elapsed = now() - start_time;
	printf("[UCS] Solution path: %s\n", soln ? soln : "NO SOLUTION");
	printf("Execution time: %lf\n", elapsed);
	printf("Expanded nodes: %d\n", dequeued);
	printf("Generated nodes: %d\n", enqueued);
	printf("Duplicated nodes: N/A\n");
	printf("Auxiliary memory usage (bytes): N/A\n");
	printf("Number of pieces in the puzzle: %d\n", init_data->num_pieces);
	printf("Number of steps in solution: %ld\n", soln ? strlen(soln)/2 : 0);
	int emptySpaces = 0;
	for (int i = 0; i < init_data->lines; i++) {
		for (int j = 0; init_data->map[i][j] != '\0'; j++) {
			if (init_data->map[i][j] == ' ') emptySpaces++;
		}
	}
	printf("Number of empty spaces: %d\n", emptySpaces);
	printf("Solved by UCS\n");
	printf("Number of nodes expanded per second: %lf\n", (dequeued + 1) / elapsed);
	if (soln) free(soln);
	free_initial_state(init_data);
}

// =============================
// 2. Uniform Cost Search + Duplicate Detection (RadixTree)
// =============================
void find_solution_ucs_nodup(gate_t* init_data) {
	typedef struct Node {
		gate_t* state;
		int cost;
		char* path;
	} Node;
	#define MAX_QUEUE 100000
	Node* queue[MAX_QUEUE];
	int front = 0, rear = 0;
	int packedBytes = getPackedSize(init_data);
	struct radixTree* visited = getNewRadixTree(init_data->num_pieces, init_data->lines, init_data->num_chars_map / init_data->lines);
	Node* start = (Node*)malloc(sizeof(Node));
	start->state = duplicate_state(init_data);
	start->cost = 0;
	start->path = strdup("");
	queue[rear++] = start;
	int dequeued = 0, enqueued = 1, duplicatedNodes = 0;
	char* soln = NULL;
	bool has_won = false;
	double start_time = now();
	double elapsed;
	while (front < rear) {
		Node* curr = queue[front++];
		dequeued++;
		unsigned char* packedMap = (unsigned char*)calloc(packedBytes, sizeof(unsigned char));
		packMap(curr->state, packedMap);
		if (checkPresent(visited, packedMap, curr->state->num_pieces)) {
			duplicatedNodes++;
			free(packedMap);
			free_state(curr->state, NULL);
			free(curr->path);
			free(curr);
			continue;
		}
		insertRadixTree(visited, packedMap, curr->state->num_pieces);
		free(packedMap);
		if (winning_state(*(curr->state))) {
			has_won = true;
			soln = strdup(curr->path);
			for (int i = front; i < rear; i++) {
				free_state(queue[i]->state, NULL);
				free(queue[i]->path);
				free(queue[i]);
			}
			break;
		}
		for (int p = 0; p < curr->state->num_pieces; p++) {
			for (int d = 0; d < 4; d++) {
				gate_t* next_state = duplicate_state(curr->state);
				char piece = '0' + p;
				char dir = directions[d];
				next_state = move_location(*next_state, piece, dir);
				int changed = 0;
				for (int i = 0; i < next_state->lines; i++) {
					if (strcmp(next_state->map[i], curr->state->map[i]) != 0) {
						changed = 1;
						break;
					}
				}
				if (!changed) {
					free_state(next_state, NULL);
					continue;
				}
				size_t plen = strlen(curr->path);
				char* newpath = (char*)malloc(plen + 3);
				strcpy(newpath, curr->path);
				newpath[plen] = piece;
				newpath[plen+1] = dir;
				newpath[plen+2] = '\0';
				Node* newnode = (Node*)malloc(sizeof(Node));
				newnode->state = next_state;
				newnode->cost = curr->cost + 1;
				newnode->path = newpath;
				queue[rear++] = newnode;
				enqueued++;
			}
		}
		free_state(curr->state, NULL);
		free(curr->path);
		free(curr);
	}
	elapsed = now() - start_time;
	int memoryUsage = 0;
	memoryUsage += queryRadixMemoryUsage(visited);
	printf("[UCS+NoDup] Solution path: %s\n", soln ? soln : "NO SOLUTION");
	printf("Execution time: %lf\n", elapsed);
	printf("Expanded nodes: %d\n", dequeued);
	printf("Generated nodes: %d\n", enqueued);
	printf("Duplicated nodes: %d\n", duplicatedNodes);
	printf("Auxiliary memory usage (bytes): %d\n", memoryUsage);
	printf("Number of pieces in the puzzle: %d\n", init_data->num_pieces);
	printf("Number of steps in solution: %ld\n", soln ? strlen(soln)/2 : 0);
	int emptySpaces = 0;
	for (int i = 0; i < init_data->lines; i++) {
		for (int j = 0; init_data->map[i][j] != '\0'; j++) {
			if (init_data->map[i][j] == ' ') emptySpaces++;
		}
	}
	printf("Number of empty spaces: %d\n", emptySpaces);
	printf("Solved by UCS+NoDup\n");
	printf("Number of nodes expanded per second: %lf\n", (dequeued + 1) / elapsed);
	if (soln) free(soln);
	freeRadixTree(visited);
	free_initial_state(init_data);
}

// =============================
// 3. Iterated Width Search (IW Search) - đã có khung
// =============================
// Hàm find_solution_iw đã có phía trên.

// =============================
// Hàm solve: chọn thuật toán để kiểm tra
// =============================
void solve(char const *path)
{
	gate_t gate = make_map(path, gate);
	map_check(gate);
	gate = find_player(gate);
	gate = find_pieces(gate);
	gate.base_path = path;

	// Đổi tên hàm dưới đây để kiểm tra từng thuật toán:
	// find_solution_ucs(&gate);           // UCS không loại duplicate
	find_solution_ucs_nodup(&gate);        // UCS + loại duplicate (mặc định)
	// find_solution_iw(&gate, 2);         // IW Search với width = 2 (ví dụ)
}

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
			if(((i >> j) & 1) == 1) {
				bitOn( packedMap, bitIdx );
			} else {
				bitOff( packedMap, bitIdx );
			}
			bitIdx++;
		}
		for(int j = 0; j < hBits; j++) {
			if(((gate->piece_y[i] >> j) & 1) == 1) {
				bitOn( packedMap, bitIdx );
			} else {
				bitOff( packedMap, bitIdx );
			}
			bitIdx++;
		}
		for(int j = 0; j < wBits; j++) {
			if(((gate->piece_x[i] >> j) & 1) == 1) {
				bitOn( packedMap, bitIdx );
			} else {
				bitOff( packedMap, bitIdx );
			}
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

void solve(char const *path)
{
	/**
	 * Load Map
	*/
	gate_t gate = make_map(path, gate);
	
	/**
	 * Verify map is valid
	*/
	map_check(gate);

	/**
	 * Locate player x, y position
	*/
	gate = find_player(gate);

	/**
	 * Locate each piece.
	*/
	gate = find_pieces(gate);
	
	gate.base_path = path;

	find_solution(&gate);

}
