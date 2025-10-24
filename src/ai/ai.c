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
void find_solution_algorithm1(gate_t* init_data) {
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
	
	// Algorithm 1 is a width n + 1 search
	int w = init_data->num_pieces + 1;

	/* Simple BFS */
	int height = init_data->lines;
	int width = init_data->num_chars_map / init_data->lines;
	int atomCount = init_data->num_pieces;
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
				/* If identical, skip */
				bool identical = (memcmp(curPacked, childPacked, packedBytes) == 0);
				if (identical) {
					/* no state change - drop v */
					free_state(v, init_data);
					continue;
				}
				
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
	free(queue);
	if (curPacked) free(curPacked);
	if (childPacked) free(childPacked);

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
	// memoryUsage += queryRadixMemoryUsage(radixTree);
	// Algorithm 3: Memory usage, uncomment to add.
	// for(int i = 0; i < w; i++) {
	//	memoryUsage += queryRadixMemoryUsage(rts[i]);
	// }
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
	
	// Algorithm 1 is a width n + 1 search
	int w = init_data->num_pieces + 1;

	/*
	 * FILL IN: Algorithm 1 - 3.
	 */
	/* Simple BFS / Uniform Cost Search with duplicate detection using radix tree. */
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
	free(queue);
	if (rt) freeRadixTree(rt);
	if (curPacked) free(curPacked);
	if (childPacked) free(childPacked);

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
	// memoryUsage += queryRadixMemoryUsage(radixTree);
	// Algorithm 3: Memory usage, uncomment to add.
	// for(int i = 0; i < w; i++) {
	//	memoryUsage += queryRadixMemoryUsage(rts[i]);
	// }
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

void find_solution_algorithm3(gate_t* init_data) {
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
	
	// Algorithm 1 is a width n + 1 search
	int w = init_data->num_pieces + 1;

	/*
	 * FILL IN: Algorithm 1 - 3.
	 */
	/* Simple BFS / Uniform Cost Search with duplicate detection using radix tree. */
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
	free(queue);
	if (rt) freeRadixTree(rt);
	if (curPacked) free(curPacked);
	if (childPacked) free(childPacked);

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
	// memoryUsage += queryRadixMemoryUsage(radixTree);
	// Algorithm 3: Memory usage, uncomment to add.
	// for(int i = 0; i < w; i++) {
	//	memoryUsage += queryRadixMemoryUsage(rts[i]);
	// }
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

/**
 * Find a solution by exploring all possible paths
 */
void find_solution(gate_t* init_data)
{
	//find_solution_algorithm1(init_data);
	find_solution_algorithm2(init_data);
	// find_solution_algorithm3(init_data);
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
