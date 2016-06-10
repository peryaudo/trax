#ifndef TRAX_PERFT_H_
#define TRAX_PERFT_H_

// Do performance testing by counting all the possible moves
// within the given depth.
int Perft(int depth);

// Show results of perft between 0<=depth<=max_depth in readable format.
void ShowPerft(int max_depth);

#endif // TRAX_PERFT_H_

