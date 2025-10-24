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
	/* Copy non-pointer fields first */
	memcpy(duplicate, gate, sizeof(gate_t));
	/* Duplicate buffer if present */
	if (gate->buffer) {
		int blen = strlen(gate->buffer) + 1;
		duplicate->buffer = (char*)malloc(sizeof(char) * blen);
		memcpy(duplicate->buffer, gate->buffer, blen);
	} else {
		duplicate->buffer = NULL;
	}
	/* Duplicate map (array of strings) */
	if (gate->map) {
		duplicate->map = (char**)malloc(sizeof(char*) * gate->lines);
		for (int i = 0; i < gate->lines; i++) {
			if (gate->map[i]) {
				int len = strlen(gate->map[i]) + 1;
				duplicate->map[i] = (char*)malloc(sizeof(char) * len);
				memcpy(duplicate->map[i], gate->map[i], len);
			} else {
				duplicate->map[i] = NULL;
			}
		}
	} else {
		duplicate->map = NULL;
	}
	/* Duplicate map_save */
	if (gate->map_save) {
		duplicate->map_save = (char**)malloc(sizeof(char*) * gate->lines);
		for (int i = 0; i < gate->lines; i++) {
			if (gate->map_save[i]) {
				int len = strlen(gate->map_save[i]) + 1;
				duplicate->map_save[i] = (char*)malloc(sizeof(char) * len);
				memcpy(duplicate->map_save[i], gate->map_save[i], len);
			} else {
				duplicate->map_save[i] = NULL;
			}
		}
	} else {
		duplicate->map_save = NULL;
	}
	/* Duplicate solution string if present */
	if (gate->soln) {
		duplicate->soln = NULL;
		int slen = strlen(gate->soln) + 1;
		duplicate->soln = (char*)malloc(sizeof(char) * slen);
		memcpy(duplicate->soln, gate->soln, slen);
	} else {
		duplicate->soln = NULL;
	}
	/* base_path is const char* to file path; keep pointer */
	return duplicate;
}

/**
 * Without lightweight states, the second argument may not be required.
 * Its use is up to your decision.
 */
void free_state(gate_t* stateToFree, gate_t *init_data) {
	if (!stateToFree) return;
	/* Free map lines */
	if (stateToFree->map) {
		for (int i = 0; i < stateToFree->lines; i++) {
			if (stateToFree->map[i]) free(stateToFree->map[i]);
		}
		free(stateToFree->map);
	}
	/* Free map_save lines */
	if (stateToFree->map_save) {
		for (int i = 0; i < stateToFree->lines; i++) {
			if (stateToFree->map_save[i]) free(stateToFree->map_save[i]);
		}
		free(stateToFree->map_save);
	}
	/* Free buffer */
	if (stateToFree->buffer) free(stateToFree->buffer);
	/* Free solution */
	if (stateToFree->soln) free(stateToFree->soln);
	/* Finally free the struct */
	free(stateToFree);
}

void free_initial_state(gate_t *init_data) {
	/* Frees dynamic elements of initial state data - including 
		unchanging state. */
	/* 
	Hint:
	Unchanging state:
	buffer
	map_save
	*/
	if (!init_data) return;
	/* Free buffer */
	if (init_data->buffer) {
		free(init_data->buffer);
		init_data->buffer = NULL;
	}
	/* Free map_save (unchanging backup) */
	if (init_data->map_save) {
		for (int i = 0; i < init_data->lines; i++) {
			if (init_data->map_save[i]) free(init_data->map_save[i]);
		}
		free(init_data->map_save);
		init_data->map_save = NULL;
	}
	/* Free map (initial map) */
	if (init_data->map) {
		for (int i = 0; i < init_data->lines; i++) {
			if (init_data->map[i]) free(init_data->map[i]);
		}
		free(init_data->map);
		init_data->map = NULL;
	}
	/* Free soln if any */
	if (init_data->soln) {
		free(init_data->soln);
		init_data->soln = NULL;
	}
}
/*
 * Algorithm 1: Plain breadth-first search (BFS)
 * - Purpose: Find the shortest sequence of moves to solve the puzzle by
 *   exploring states in breadth-first order.
 * - Inputs: `init_data` (initial puzzle state)
 * - Outputs: assigns `init_data->soln` to a solution string on success.
 * - Notes: This implementation stores full states in a dynamic queue and
 *   does NOT detect duplicate states (no closed set), so may revisit states.
 * - Complexity: time and memory grow exponentially with solution depth; this
 *   is a baseline reference algorithm.
 */
void find_solution_algorithm1(gate_t* init_data) {
	bool has_won = false;
    int dequeued = 0;
    int enqueued = 0;
    int duplicatedNodes = 0; // luôn 0 với BFS thuần
	/* duplicatedNodes remains 0 for plain BFS (no duplicate detection) */
    char *soln = NULL;

    double start = now();

	/* Dynamic queue */
    int qcap = 1024, qhead = 0, qtail = 0;
    gate_t **queue = (gate_t**)malloc(sizeof(gate_t*) * qcap);

	/* Initial state */
    gate_t *start_state = duplicate_state(init_data);

	if (!start_state->soln)
	{
		start_state->soln = (char*)malloc(1);
        start_state->soln[0] = '\0';
	}
	queue[qtail++] = start_state; enqueued++;


	while (qhead < qtail) {
		gate_t *u = queue[qhead++]; dequeued++;

		if (winning_state(*u)) {
			has_won = true;
			size_t L = strlen(u->soln);
			soln = (char*)malloc(L + 1);
			memcpy(soln, u->soln, L + 1);
			free_state(u, init_data);
			break;
		}


		for (int p = 0; p < init_data->num_pieces; ++p) {
			char piece = pieceNames[p];
			for (int d = 0; d < 4; ++d) {
				char dir = directions[d];

				// Create a copy of the parent node
				gate_t *v = duplicate_state(u);

				// Perform the move on the child node
				gate_t before = *v;
				*v = move_location(*v, piece, dir);

				// If the move did not change piece p's coordinates, skip
				if (v->piece_x[p] == u->piece_x[p] && v->piece_y[p] == u->piece_y[p]) {
					free_state(v, init_data);
					continue;
				}

				// Append the move to the solution string: soln(parent) + piece + dir
				size_t prevLen = u->soln ? strlen(u->soln) : 0;
				if (v->soln) free(v->soln);
				v->soln = (char*)malloc(prevLen + 3);
				if (prevLen) memcpy(v->soln, u->soln, prevLen);
				v->soln[prevLen]   = piece;
				v->soln[prevLen+1] = dir;
				v->soln[prevLen+2] = '\0';

				if (qtail >= qcap) {
					qcap *= 2;
					queue = (gate_t**)realloc(queue, sizeof(gate_t*) * qcap);
				}
				queue[qtail++] = v; enqueued++;
			}
		}

		// Finished expanding u
		free_state(u, init_data);
	}

	/* Clean up remaining states if loop exited early (found solution) */
    for (int i = qhead; i < qtail; ++i) {
        if (queue[i]) free_state(queue[i], init_data);
    }
    free(queue);

    double elapsed = now() - start;

	/* Print statistics for plain BFS */
    printf("Solution path: %s\n", soln ? soln : "Not Found");
    printf("Execution time: %lf\n", elapsed);
    printf("Expanded nodes: %d\n", dequeued);
    printf("Generated nodes: %d\n", enqueued);
	printf("Duplicated nodes: %d\n", duplicatedNodes);     /* always 0 for plain BFS */
	printf("Auxiliary memory usage (bytes): %d\n", 0);     /* no radix tree used */
    printf("Number of pieces in the puzzle: %d\n", init_data->num_pieces);
    printf("Number of steps in solution: %ld\n", soln ? (long)strlen(soln)/2 : 0L);

    int emptySpaces = 0;
    for (int i = 0; i < init_data->lines; ++i)
        for (int j = 0; init_data->map_save[i][j] != '\0'; ++j)
            if (init_data->map_save[i][j] == ' ') emptySpaces++;
    printf("Number of empty spaces: %d\n", emptySpaces);

	/* For algorithm 1 we don't use novelty/IW labels; print a placeholder if needed */
	printf("Solved by IW(%d)\n", init_data->num_pieces + 1);

    printf("Number of nodes expanded per second: %lf\n", (dequeued + 1) / elapsed);

    if (soln) {
        init_data->soln = soln;   /* giao lại cho caller để giải phóng sau */
    }

    /* Giải phóng initial state (nếu theo khung đề cần free ở đây) */
    free_initial_state(init_data);
}


/*
 * Algorithm 2: BFS with compact state packing and radix-tree duplicate detection
 * - Purpose: Breadth-first exploration like Algorithm 1 but avoids revisiting
 *   previously seen states by packing each state's piece coordinates into a
 *   compact bit representation and tracking seen atoms in a radix tree.
 * - Inputs: `init_data` (initial puzzle state)
 * - Outputs: assigns `init_data->soln` to the found solution string.
 * - Notes: Packing reduces the cost of duplicate checks; radix tree stores
 *   seen packed states (or atom combinations). This reduces redundant work
 *   compared to plain BFS at the cost of additional memory for the radix tree.
 */
void find_solution_algorithm2(gate_t* init_data) {
	/* Location for packedMap. */
	int packedBytes = getPackedSize(init_data);
	unsigned char *packedMap = (unsigned char *) calloc(packedBytes, sizeof(unsigned char));
	assert(packedMap);

	bool has_won = false;
	int dequeued = 0;
	int enqueued = 0;
	int duplicatedNodes = 0;
	char *soln = NULL;
	double start = now();
	double elapsed;
	
	/* Algorithm 1 (plain BFS) is equivalent to a width w = n_pieces + 1 search; */
	int w = init_data->num_pieces + 1;

	int height = init_data->lines;
	int width = init_data->num_chars_map / init_data->lines;
	int atomCount = init_data->num_pieces;
	struct radixTree *rt = getNewRadixTree(atomCount, height, width);
	int packedSize = packedBytes;
	unsigned char *curPacked = (unsigned char*)calloc(packedBytes, sizeof(unsigned char));
	unsigned char *childPacked = (unsigned char*)calloc(packedBytes, sizeof(unsigned char));
	/* Create initial state copy that owns its resources for queue management */
	gate_t *start_state = duplicate_state(init_data);
	/* Ensure starting solution string exists */
	if (!start_state->soln) {
		start_state->soln = (char*)malloc(1);
		start_state->soln[0] = '\0';
	}
	/* Pack and insert start */
	packMap(start_state, curPacked);
	insertRadixTree(rt, curPacked, atomCount);
	/* Simple dynamic queue of gate_t* */
	int qcap = 1024;
	int qhead = 0, qtail = 0;
	gate_t **queue = (gate_t**)malloc(sizeof(gate_t*) * qcap);
	queue[qtail++] = start_state; enqueued++;
	/* Search loop */
	while(qhead < qtail) {
		gate_t *u = queue[qhead++];
		dequeued++;
		/* Check goal */
		if (winning_state(*u)) {
			has_won = true;
			/* Copy solution string out */
			if (u->soln) {
				if (soln) free(soln);
				soln = (char*)malloc(strlen(u->soln) + 1);
				strcpy(soln, u->soln);
			}
			/* free popped state and remaining queue items */
			free_state(u, init_data);
			break;
		}
		/* Generate successors: iterate pieces then directions */
		for (int p = 0; p < init_data->num_pieces; p++) {
			char piece = pieceNames[p];
			for (int d = 0; d < 4; d++) {
				char dir = directions[d];
				/* Create child by duplicating u */
				gate_t *v = duplicate_state(u);
				/* Append move to solution string: piece + dir */
				int prevLen = v->soln ? strlen(v->soln) : 0;
				char *newSol = (char*)malloc(prevLen + 3); /* two chars + terminator */
				if (prevLen > 0) memcpy(newSol, v->soln, prevLen);
				newSol[prevLen] = piece;
				newSol[prevLen+1] = dir;
				newSol[prevLen+2] = '\0';
				if (v->soln) free(v->soln);
				v->soln = newSol;
				/* Correct approach: perform move on a local copy and then overwrite v's contents. */
				gate_t tmp = *v; /* copy of current duplicated state */
				/* Actually perform move on tmp */
				tmp = move_location(tmp, piece, dir);
				/* Now compare packed representation to see if any change occurred */
				/* Pack u and tmp */
				packMap(u, curPacked);
				packMap(&tmp, childPacked);
				/* If identical, skip */
				bool identical = (memcmp(curPacked, childPacked, packedBytes) == 0);
				if (identical) {
					/* no state change - drop v */
					free_state(v, init_data);
					continue;
				}
				/* Check radix duplicate */
				if (checkPresent(rt, childPacked, atomCount) == PRESENT) {
					duplicatedNodes++;
					free_state(v, init_data);
					continue;
				}
				/* Not seen, insert and enqueue */
				insertRadixTree(rt, childPacked, atomCount);
				/* Overwrite v's dynamic data with tmp's results */
				/* Create new copies of tmp's maps first (tmp may share pointers with v)
				then free old storage and attach the new copies to v. */
				char **new_map = (char**)malloc(sizeof(char*) * tmp.lines);
				char **new_map_save = (char**)malloc(sizeof(char*) * tmp.lines);
				for (int i = 0; i < tmp.lines; i++) {
					new_map[i] = (char*)malloc(strlen(tmp.map[i]) + 1);
					strcpy(new_map[i], tmp.map[i]);
					new_map_save[i] = (char*)malloc(strlen(tmp.map_save[i]) + 1);
					strcpy(new_map_save[i], tmp.map_save[i]);
				}
				/* Now free previous v storage */
				for (int i = 0; i < v->lines; i++) { if (v->map[i]) free(v->map[i]); }
				for (int i = 0; i < v->lines; i++) { if (v->map_save[i]) free(v->map_save[i]); }
				free(v->map); free(v->map_save);
				/* Attach new copies */
				v->map = new_map;
				v->map_save = new_map_save;
				/* Copy scalar fields */
				v->lines = tmp.lines;
				v->player_x = tmp.player_x; v->player_y = tmp.player_y;
				v->num_chars_map = tmp.num_chars_map; v->num_pieces = tmp.num_pieces;
				for (int i = 0; i < v->num_pieces; i++) { v->piece_x[i] = tmp.piece_x[i]; v->piece_y[i] = tmp.piece_y[i]; }
				/* Enqueue v */
				/* Grow queue if necessary */
				if (qtail >= qcap) {
					qcap *= 2;
					queue = (gate_t**)realloc(queue, sizeof(gate_t*) * qcap);
				}
				queue[qtail++] = v; enqueued++;
			}
		}
		/* Free u after expansion */
		free_state(u, init_data);
	}
	/* Clean up queue remainder if not emptied by solution */
	for (int i = qhead; i < qtail; i++) {
		if (queue[i]) free_state(queue[i], init_data);
	}

	/* Output statistics */
	elapsed = now() - start;
	printf("Solution path: ");
	printf("%s\n", soln);
	printf("Execution time: %lf\n", elapsed);
	printf("Expanded nodes: %d\n", dequeued);
	printf("Generated nodes: %d\n", enqueued);
	printf("Duplicated nodes: %d\n", duplicatedNodes);
	int memoryUsage = 0;
	// Algorithm 2: Memory usage, uncomment to add.
	memoryUsage += queryRadixMemoryUsage(rt);

	printf("Auxiliary memory usage (bytes): %d\n", memoryUsage);
	printf("Number of pieces in the puzzle: %d\n", init_data->num_pieces);
	printf("Number of steps in solution: %ld\n", strlen(soln)/2);
	int emptySpaces = 0;
	/*
	 * FILL IN: Add empty space check for your solution.
	 */
	/* Count spaces in the initial map_save (or map if available) */
	for (int i = 0; i < init_data->lines; i++) {
		for (int j = 0; init_data->map_save[i][j] != '\0'; j++) {
			if (init_data->map_save[i][j] == ' ') emptySpaces++;
		}
	}
	printf("Number of empty spaces: %d\n", emptySpaces);
	printf("Solved by IW(%d)\n", w);
	printf("Number of nodes expanded per second: %lf\n", (dequeued + 1) / elapsed);

	
	free(queue);
	if (rt) freeRadixTree(rt);
	if (curPacked) free(curPacked);
	if (childPacked) free(childPacked);
	
	/* Free associated memory. */
	if(packedMap) {
		free(packedMap);
	}

	/* Assign solution back to the gate structure */
	if (soln) {
		init_data->soln = soln;
	}

	/* Free initial map. */
	free_initial_state(init_data);
}

static inline void append_move(gate_t *child, const char *parent_soln, char piece, char dir) {
    size_t L = parent_soln ? strlen(parent_soln) : 0;
    child->soln = (char*)malloc(L + 3);
    if (L) memcpy(child->soln, parent_soln, L);
    child->soln[L]   = piece;
    child->soln[L+1] = dir;
    child->soln[L+2] = '\0';
}

void find_solution_algorithm3(gate_t* init_data) {
	 /*
	  * Algorithm 3: Iterative Width (IW) / novelty-based search
	  * - Purpose: Perform a novelty-driven breadth-first search with increasing
	  *   width (w). For each width, nodes are kept if they introduce a novel
	  *   combination of atom-values (nCr of size k) for some k <= w.
	  * - Inputs: `init_data` (initial puzzle state)
	  * - Outputs: assigns `init_data->soln` to the found solution string.
	  * - Notes: This method stores a radix tree for each k to record seen
	  *   combinations and prunes nodes that are not novel up to width w. It is
	  *   much more selective than plain BFS and can dramatically reduce search
	  *   effort for many domains.
	  */

	 /* packedBytes: getPackedSize currently returns a bit-count; code keeps
		 the existing approach and allocates bytes accordingly (may over-allocate) */
    int packedBytes = getPackedSize(init_data);
    unsigned char *packedMap = (unsigned char*)calloc(packedBytes, 1);
    assert(packedMap);

    int wmax = init_data->num_pieces + 1;
    int height = init_data->lines;
    int width  = init_data->num_chars_map / init_data->lines;

	/* For each k we maintain a radixTree that stores seen atom-combinations of size k */
	struct radixTree **rts = (struct radixTree**)calloc(wmax + 1, sizeof(*rts));
    assert(rts);

    int dequeued_total = 0;
    int enqueued_total = 0;
    int duplicated_total = 0;     /* số node bị loại do “không novel” */
    char *soln = NULL;
    int solved_w = 0;

    double start = now();

	/* Iterate width from 1 to wmax */
	for (int w = 1; w <= wmax; ++w) {

		/* Create novelty trees for all k <= w if they don't exist yet */
		for (int k = 1; k <= w; ++k) {
			if (!rts[k]) rts[k] = getNewRadixTree(init_data->num_pieces, height, width);
		}

		/* Dynamic queue */
        int qcap = 1024, qhead = 0, qtail = 0;
        gate_t **queue = (gate_t**)malloc(sizeof(gate_t*) * qcap);

		/* Load root (initial state copy) */
        gate_t *root = duplicate_state(init_data);
		free(root->soln);
		root->soln = NULL;
        if (!root->soln) { root->soln = (char*)malloc(1); root->soln[0] = '\0'; }

        memset(packedMap, 0, packedBytes);
        packMap(root, packedMap);
		/* Insert root into all novelty levels k where it is not present */
		for (int k = 1; k <= w; ++k) {
			if (checkPresentnCr(rts[k], packedMap, k) == NOTPRESENT) {
				insertRadixTreenCr(rts[k], packedMap, k);
			}
		}

        queue[qtail++] = root;
        int dequeued = 0, enqueued = 1, duplicated = 0;
        int found = 0;

        while (qhead < qtail) {
            gate_t *u = queue[qhead++]; dequeued++;

            if (winning_state(*u)) {
                size_t L = strlen(u->soln);
                soln = (char*)malloc(L + 1);
                memcpy(soln, u->soln, L + 1);
                free_state(u, init_data);
                found = 1; solved_w = w;
                break;
            }

				/* Generate children: iterate pieces then {u,d,l,r} */
            for (int p = 0; p < init_data->num_pieces; ++p) {
                char piece = pieceNames[p];
                for (int di = 0; di < 4; ++di) {
                    char dir = directions[di];

                    gate_t moved = move_location(*u, piece, dir);

						/* Non-move: piece p coordinates unchanged */
						if (moved.piece_x[p] == u->piece_x[p] &&
							moved.piece_y[p] == u->piece_y[p]) {
							continue;
						}

                    gate_t *child = duplicate_state(&moved);
					free(child->soln);
					child->soln = NULL;
                    append_move(child, u->soln, piece, dir);

                    memset(packedMap, 0, packedBytes);
                    packMap(child, packedMap);

					/* Novelty check: is there a k <= w such that some nCr combination
					   of size k has NOT been seen? If none, the node is not novel and
					   can be pruned. */
					int novel_k = 0;
					for (int k = 1; k <= w; ++k) {
						if (checkPresentnCr(rts[k], packedMap, k) == NOTPRESENT) {
							novel_k = k; break;
						}
					}
					if (!novel_k) {
						/* Not novel for any k <= w: prune this child */
						duplicated++;
						free_state(child, init_data);
						continue;
					}

                    /* Insert vào cây tại kích thước novel_k */
                    insertRadixTreenCr(rts[novel_k], packedMap, novel_k);

					/* Enqueue the novel child */
                    if (qtail >= qcap) {
                        qcap *= 2;
                        queue = (gate_t**)realloc(queue, sizeof(gate_t*) * qcap);
                    }
                    queue[qtail++] = child; enqueued++;
                }
            }

			free_state(u, init_data);
        }

		/* Clean up remaining queue entries if search broke due to finding a solution */
		for (int i = qhead; i < qtail; ++i) if (queue[i]) free_state(queue[i], init_data);
        free(queue);

        dequeued_total   += dequeued;
        enqueued_total   += enqueued;
        duplicated_total += duplicated;

		if (found) break; /* solution found at width w */
    }

	/* Memory statistics for novelty trees */
    int memoryUsage = 0;
    for (int k = 1; k <= wmax; ++k) {
        if (rts[k]) {
            memoryUsage += queryRadixMemoryUsage(rts[k]);
            freeRadixTree(rts[k]);
        }
    }
    free(rts);
    free(packedMap);

    double elapsed = now() - start;

    /* In thống kê theo format của bạn */
    printf("Solution path: %s\n", soln ? soln : "Not Found");
    printf("Execution time: %lf\n", elapsed);
    printf("Expanded nodes: %d\n", dequeued_total);
    printf("Generated nodes: %d\n", enqueued_total);
    printf("Duplicated nodes: %d\n", duplicated_total);
    printf("Auxiliary memory usage (bytes): %d\n", memoryUsage);
    printf("Number of pieces in the puzzle: %d\n", init_data->num_pieces);
    printf("Number of steps in solution: %ld\n", soln ? (long)strlen(soln)/2 : 0L);

    int emptySpaces = 0;
    for (int i = 0; i < init_data->lines; ++i)
        for (int j = 0; init_data->map_save[i][j] != '\0'; ++j)
            if (init_data->map_save[i][j] == ' ') emptySpaces++;
    printf("Number of empty spaces: %d\n", emptySpaces);

    printf("Solved by IW(%d)\n", solved_w ? solved_w : (init_data->num_pieces + 1));
    printf("Number of nodes expanded per second: %lf\n", (dequeued_total + 1) / elapsed);

    if (soln) {
        init_data->soln = soln;   /* giao lại cho caller để free sau */
    }

  
    if (init_data->soln) { free(init_data->soln); init_data->soln = NULL; } 
	free_initial_state(init_data);
}

/**
 * Find a solution by exploring all possible paths
 */
void find_solution(gate_t* init_data)
{
	//find_solution_algorithm1(init_data);
	find_solution_algorithm2(init_data);
	//find_solution_algorithm3(init_data);
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
	/* Ensure initial soln pointer is initialized */
	gate.soln = NULL;

	find_solution(&gate);
	
	/* Free the solution string if it exists */
	if (gate.soln) {
		free(gate.soln);
		gate.soln = NULL;
	}

}
