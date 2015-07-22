/*--------------------------------------------------------------------*/
/* global.c                                                           */
/* Author: Rongxin Fang                                               */
/* E-mail: r3fang@ucsd.edu                                            */
/* Pair wise global alignment with affine gap penality.               */
/*--------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "utils.h"
#include "kseq.h"
#include "kstring.h"

KSEQ_INIT(gzFile, gzread);

//--------------
#define GL_ERR_NONE              0

// penality
//--------------
#define GAP                     -3.0
#define EXTENSION               -1.0
#define MATCH                    1.0
#define MISMATCH                -1.0

// state
//--------------
#define LOW                     100
#define MID                     200
#define UPP                     300

// DP matrix
typedef struct {
  unsigned int m;
  unsigned int n;
  double **L;
  double **M;
  double **U;
  int **pointerL;
  int **pointerM;
  int **pointerU;
} matrix_t;

/*
 * initilize matrix.	
 */
matrix_t *create_matrix(size_t m, size_t n){
	size_t i, j; 
	matrix_t *S = mycalloc(1, matrix_t);
	S->m = m;
	S->n = n;
	S->L = mycalloc(m, double*);
	S->M = mycalloc(m, double*);
	S->U = mycalloc(m, double*);
	S->pointerL = mycalloc(m, int*);
	S->pointerM = mycalloc(m, int*);
	S->pointerU = mycalloc(m, int*);
	for (i = 0; i < m; i++) {
		S->L[i] = mycalloc(n, double);
		S->M[i] = mycalloc(n, double);
		S->U[i] = mycalloc(n, double);
		S->pointerL[i] = mycalloc(n, int);
		S->pointerM[i] = mycalloc(n, int);
		S->pointerU[i] = mycalloc(n, int);
	}
	for(i=0; i<S->m; i++){
		for(j=0; j<S->n; j++){
			S->L[i][j] = -INFINITY;
			S->M[i][j] = -INFINITY;
			S->U[i][j] = -INFINITY;
		}
	}
	
	// initlize DP matrix
	S->M[0][0] = 0.0;
	S->L[0][0] = S->U[0][0] = GAP;
	// initlize 0 column
	for(i=1; i<S->m; i++){
		S->L[i][0] = GAP + EXTENSION*(i);
		S->M[i][0] = -INFINITY;
		S->U[i][0] = -INFINITY;
	}
	for(j=1; j<S->n; j++){
		S->L[0][j] = -INFINITY;
		S->M[0][j] = -INFINITY;
		S->U[0][j] = GAP + EXTENSION*(j);
	}
	return S;
}

/*
 * reverse a string
 */
char* strrev(char *s){
	if(s == NULL) return NULL;
	int l = strlen(s);
	char *ss = strdup(s);
	free(s);
	s = mycalloc(l, char);
	int i; for(i=0; i<l; i++){
		s[i] = ss[l-i-1];
	}
	s[l] = '\0';
	return s;
}

/*
 * destory matrix
 */
int destory_matrix(matrix_t *S){
	if(S == NULL) die("destory_matrix: parameter error\n");
	int i, j;
	for(i = 0; i < S->m; i++){
		free(S->L[i]);
		free(S->M[i]);
		free(S->U[i]);
		free(S->pointerL[i]);
		free(S->pointerM[i]);
		free(S->pointerU[i]);
	}
	free(S);
	return GL_ERR_NONE;
}

int max3(double *res, double a1, double a2, double a3){
	*res = -INFINITY;
	int ind;
	if(a1 > *res){*res = a1; ind = 1;}
	if(a2 > *res){*res = a2; ind = 2;}
	if(a3 > *res){*res = a3; ind = 3;}
	return ind;
}

void trace_back(matrix_t *S, kstring_t *s1, kstring_t *s2, kstring_t *res_ks1, kstring_t *res_ks2, int state){
	if(S == NULL || s1 == NULL || s2 == NULL || res_ks1 == NULL || res_ks2 == NULL) die("trace_back: paramter error");
	int i = s1->l; int j = s2->l;
	int cur = 0; 
	while(i > 0 && j > 0){
		switch(state){
			case LOW:
				state = S->pointerL[i][j]; // change to next state
				res_ks1->s[cur] = s1->s[--i];
				res_ks2->s[cur++] = '-';
				break;
			case MID:
				state = S->pointerM[i][j]; // change to next state
                res_ks1->s[cur] = s1->s[--i];
                res_ks2->s[cur++] = s2->s[--j];
				break;
			case UPP:
				state = S->pointerU[i][j];
				res_ks1->s[cur] = '-';
            	res_ks2->s[cur++] = s2->s[--j];
				break;
			default:
				break;
			}
	}
	if(j>0){while(j>0){
			res_ks1->s[cur] = '-';	
			res_ks2->s[cur++] = s2->s[--j];				
		}
	}
	if(i>0){while(i>0){
			res_ks2->s[cur] = '-';	
			res_ks1->s[cur++] = s1->s[--i];				
		}
	}	
	res_ks1->l = cur;
	res_ks2->l = cur;	
	res_ks1->s = strrev(res_ks1->s);
	res_ks2->s = strrev(res_ks2->s);
}

/*
 * Global alignment with affine gap penality
 */
double align(kstring_t *s1, kstring_t *s2, kstring_t *r1, kstring_t *r2){
	if(s1 == NULL || s2 == NULL || r1 == NULL || r2 == NULL) die("align: parameter error\n");
	size_t m   = s1->l + 1; size_t n   = s2->l + 1;
	matrix_t *S = create_matrix(m, n);
	int i, j;
	int ind;
	// recurrance relation
	for(i=1; i<=s1->l; i++){
		for(j=1; j<=s2->l; j++){
			// MID
			double new_score = (strncmp(s1->s+(i-1), s2->s+(j-1), 1) == 0) ? MATCH : MISMATCH;
			ind = max3(&S->M[i][j], S->L[i-1][j-1]+new_score, S->M[i-1][j-1]+new_score, S->U[i-1][j-1]+new_score);
			if(ind==1)  S->pointerM[i][j] = LOW;
			if(ind==2)  S->pointerM[i][j] = MID;
			if(ind==3)  S->pointerM[i][j] = UPP;
			// LOW
			ind = max3(&S->L[i][j], S->L[i-1][j]+EXTENSION, S->M[i-1][j]+GAP+EXTENSION, -INFINITY);
			if(ind==1)	S->pointerL[i][j] = LOW; // stay in state LOW
			if(ind==2)	S->pointerL[i][j] = MID; // jump to state MID
			// UPP
			ind = max3(&S->U[i][j], S->U[i][j-1]+EXTENSION, S->M[i][j-1]+GAP+EXTENSION, -INFINITY);
			if(ind==1)  S->pointerU[i][j] = UPP;
			if(ind==2)	S->pointerU[i][j] = MID;
		}
	}
	double max_score;
	int max_state;
	//printf("L=%f\tM=%f\tU=%f\n", S->L[s1->l][s2->l], S->M[s1->l][s2->l], S->U[s1->l][s2->l]);
	//for(i=0; i<=s1->l; i++){
	//	for(j=0; j<=s2->l; j++){
	//		printf("%d\t", S->pointerM[i][j]);
	//	}
	//	printf("\n");
	//}
	ind = max3(&max_score, S->L[s1->l][s2->l], S->M[s1->l][s2->l], S->U[s1->l][s2->l]);
	if(ind == 1) max_state = LOW;
	if(ind == 2) max_state = MID;
	if(ind == 3) max_state = UPP;
	trace_back(S, s1, s2, r1, r2, max_state);	
	return max_score;
}

char* str_toupper(char* s){
	char *r = mycalloc(strlen(s), char);
	int i = 0;
	char c;
	while(s[i])
	{
		r[i] = toupper(s[i]);
		i++;
	}
	r[strlen(s)] = '\0';
	return r;
}
/*
 * read in kstring_t from fasta file 
 */
void kstring_read(char* fname, kstring_t *str1, kstring_t *str2){
	if(fname == NULL || str1 == NULL || str2 == NULL) die("kstring_read: parameter error");
	gzFile fp;
	kseq_t *seq;
	if((fp = gzopen(fname, "r")) == NULL) die("kstring_read: gzopen fails\n");
	if((seq = kseq_init(fp)) == NULL) die("kseq_init: gzopen fails\n");
	int i, l;
	char **tmp = mycalloc(3, char*);
	i = 0;
	while((l=kseq_read(seq)) >= 0){
		if(i >= 2) die("input fasta file has more than 2 sequences");
		tmp[i++] = str_toupper(seq->seq.s);
	}
	if(tmp[0] == NULL || tmp[1] == NULL) die("read_kstring: fail to read sequence");
	(str1)->s = strdup(tmp[0]); (str1)->l = strlen((str1)->s);
	(str2)->s = strdup(tmp[1]); (str2)->l = strlen((str2)->s);
	for(; i >=0; i--){if(tmp[i]) free(tmp[i]);} free(tmp);
	kseq_destroy(seq);
	gzclose(fp);
}
/*
 * destory kstring_t
 */
void kstring_destory(kstring_t *ks){
	free(ks->s);
	free(ks);
}

/* main function. */
int main(int argc, char *argv[]) {
	kstring_t *ks1, *ks2; 
	ks1 = mycalloc(1, kstring_t);
	ks2 = mycalloc(1, kstring_t);
	if (argc == 1) {
		fprintf(stderr, "Usage: %s <in.seq>\n", argv[0]);
		return 1;
	}
	kstring_read(argv[1], ks1, ks2);
	if(ks1->s == NULL || ks2->s == NULL) die("fail to read sequence\n");
	kstring_t *r1 = mycalloc(1, kstring_t);
	kstring_t *r2 = mycalloc(1, kstring_t);
	r1->s = mycalloc(ks1->l + ks2->l, char);
	r2->s = mycalloc(ks1->l + ks2->l, char);
	printf("score=%f\n", align(ks1, ks2, r1, r2));
	printf("%s\n%s\n", r1->s, r2->s);
	kstring_destory(ks1);
	kstring_destory(ks2);
	kstring_destory(r1);
	kstring_destory(r2);
	return 0;
}