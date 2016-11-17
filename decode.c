#include <assert.h>
#include <stdlib.h>
#include "decode.h"

#define NBASE 4


float decode_transducer(const Mat_rptr logpost, float skip_pen, int * seq){
	assert(NULL != logpost);
	assert(NULL != seq);
	const int nev = logpost->nc;
	const int nstate = logpost->nr;
	const int ldp = logpost->nrq * 4;
	const int nkmer = nstate - 1;
	const int kmer_mask = nkmer - 1;

	//  Forwards memory + traceback
	float * restrict score = calloc(nkmer, sizeof(float));
	float * restrict prev_score = calloc(nkmer, sizeof(float));
	int * traceback = calloc(nkmer * nev, sizeof(int));
	//  Initialise
	for( int i=0 ; i < nkmer ; i++){
		score[i] = logpost->data.f[i + 1];
	}

	//  Forwards Viterbi iteration
	for(int ev=1 ; ev < nev ; ev++){
		const int offsetT = ev * nkmer;
		const int offsetP = ev * ldp;
		// Swap score and previous score
		{
			float * tmp = score;
			score = prev_score;
			prev_score = tmp;
		}

		// Stay
		for(int i=0 ; i < nkmer ; i++){
			// Traceback for stay is negative
			score[i] = prev_score[i] + logpost->data.f[offsetP];
			traceback[offsetT + i] = -1;
		}
		// Step
		for(int i=0 ; i < nkmer ; i++){
			const int to = (i << 2) & kmer_mask;
			for(int b=0 ; b < NBASE ; b++){
				const float step_score = logpost->data.f[offsetP + to + b + 1]
					               + prev_score[i];
				if(score[to + b] < step_score){
					score[to + b] = step_score;
					traceback[offsetT + to + b] = i;
				}
			}
		}
		// Skip
		for(int i=0 ; i < nkmer ; i++){
			const int to = (i << 4) & kmer_mask;
			for(int b=0 ; b < NBASE * NBASE ; b++){
				const float step_score = logpost->data.f[offsetP + to + b + 1]
					               + prev_score[i] - skip_pen;
				if(score[to + b] < step_score){
					score[to + b] = step_score;
					traceback[offsetT + to + b] = i;
				}
			}
		}
	}

	//  Viterbi traceback
	float logscore = valmaxf(score, nkmer);
	seq[nev - 1] = argmaxf(score, nkmer);
	int pstate = seq[nev - 1];
	for(int ev=1 ; ev < nev ; ev++){
		const int iev = nev - ev;
		const int tstate = traceback[iev * nkmer + pstate];
		seq[iev - 1] = tstate;
		if(tstate >= 0){
			pstate = tstate;
		}
	}

	free(traceback);
	free(prev_score);
	free(score);
	return logscore;
}

int overlap(int k1, int k2, int nkmer){
	// Neither k1 nor k2 can be stays
	assert(k1 >= 0);
	assert(k2 >= 0);

	int kmer_mask = nkmer - 1;
	int overlap = 0;
	do {
		kmer_mask >>= 2;
		k1 &= kmer_mask;
		k2 >>= 2;
		overlap += 1;
	} while(k1 != k2);

	return overlap;
}


int position_highest_bit(int x){
	int i = 0;
	for( ; x != 0 ; i++, x>>=1);
	return i;
}

int first_nonnegative(const int * seq, int n){
	int st;
        for(st=0 ; st < n && seq[st] < 0; st++);
	return st;
}


const char base_lookup[4] = {'A', 'C', 'G', 'T'};
char * overlapper(const int * seq, int n, int nkmer){
	assert(NULL != seq);
	const int kmer_len = position_highest_bit(nkmer) / 2;


	//  Determine length of final sequence
	int length = kmer_len;
 	// Find first non-stay
        const int st = first_nonnegative(seq, n);
	assert(st != n);
	int kprev = seq[st];
	for(int k=st + 1 ; k < n ; k++){
		if(seq[k] < 0){
			// Short-cut stays
			continue;
		}
		assert(seq[k] >= 0);
		length += overlap(kprev , seq[k], nkmer);
		kprev = seq[k];
		assert(kprev >= 0);
	}

	// Initialise basespace sequence
	char * bases = calloc(length + 1, sizeof(char));
	// Fill with first kmer
	for(int kmer=seq[st], k=1 ; k <= kmer_len ; k++){
		int b = kmer & 3;
		kmer >>= 2;
		bases[kmer_len - k] = base_lookup[b];
	}



	for(int last_idx=kmer_len - 1, kprev=seq[st], k=st + 1 ; k < n ; k++){
		if(seq[k] < 0){
			// Short-cut stays
			continue;
		}
		int ol = overlap(kprev , seq[k], nkmer);
		kprev = seq[k];

                for(int kmer=seq[k], i=0 ; i<ol ; i++){
			int b = kmer & 3;
			kmer >>= 2;
			bases[last_idx + ol - i] = base_lookup[b];
		}
		last_idx += ol;
	}

	return bases;
}