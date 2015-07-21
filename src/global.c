/*--------------------------------------------------------------------*/
/* global.c 		                                                  */
/* Author: Rongxin Fang                                               */
/* E-mail: r3fang@ucsd.edu                                            */
/* Pair wise global alignment without affine gap.                     */
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
#define GL_ERR_NONE 			0
#define GAP 					-1.0
#define MATCH 					2.0
#define MISMATCH 				-0.5

// POINTER STATE
#define LEFT 					100
#define DIAGONAL 				200
#define RIGHT	 				300

typedef enum {true, false} bool;

typedef struct {
  unsigned int m;
  unsigned int n;
  double **score;
  int  **pointer;
} matrix_t;

matrix_t *create_matrix(size_t m, size_t n){
	size_t i, j; 
	matrix_t *S = mycalloc(1, matrix_t);
	S->m = m;
	S->n = n;
	S->score = mycalloc(m, double*);
	for (i = 0; i < m; i++) {
      S->score[i] = mycalloc(n, double);
    }	
	
	S->pointer = mycalloc(m, int*);
	for (i = 0; i < m; i++) {
      S->pointer[i] = mycalloc(n, int);
    }	
	
	return S;
}


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
	
int destory_matrix(matrix_t *S){
	if(S == NULL) die("destory_matrix: parameter error\n");
	int i, j;
	for(i = 0; i < S->m; i++){
		free(S->score[i]);
	}
	for(i = 0; i < S->m; i++){
		free(S->pointer[i]);
	}
	free(S);
	return GL_ERR_NONE;
}

int max3(double *res, double a1, double a2, double a3){
	*res = DBL_MIN;
	int state;
	if(a1 >= *res){*res = a1; state = LEFT;}
	if(a2 >= *res){*res = a2; state = DIAGONAL;}
	if(a3 >= *res){*res = a3; state = RIGHT;}	
	return state;
}

void trace_back(matrix_t *S, kstring_t *ks1, kstring_t *ks2, kstring_t *res_ks1, kstring_t *res_ks2){
	if(S == NULL || ks1 == NULL || ks2 == NULL || res_ks1 == NULL || res_ks2 == NULL) die("trace_back: parameter error");
	int m = 0; 
	int i = ks1->l;
	int j = ks2->l;
	
	while(i>0 || j >0){
		switch(S->pointer[i][j]){
			case LEFT:
				res_ks2->s[m] = ks2->s[--j];
				res_ks1->s[m++] = '-';
				break;
			case DIAGONAL:
				res_ks1->s[m] = ks1->s[--i];
				res_ks2->s[m++] = ks2->s[--j];
				break;
			case RIGHT:
				res_ks1->s[m] = ks1->s[--i];
				res_ks2->s[m++] = '-';
				break;
			default:
				break;	
		}
	}
	res_ks1->l = m;
	res_ks2->l = m;	
	res_ks1->s = strrev(res_ks1->s);
	res_ks2->s = strrev(res_ks2->s);
}

double global(kstring_t *s1, kstring_t *s2, kstring_t *r1, kstring_t *r2){
	if(s1 == NULL || s2 == NULL || r1 == NULL || r2 == NULL) die("global: parameter error\n");
	size_t m   = s1->l + 1;
	size_t n   = s2->l + 1;
	matrix_t *S = create_matrix(m, n);
	size_t i, j, k, l;
	for(i=0; i < S->m; i++) S->score[i][0] = 0.0;
	for(j=0; j < S->n; j++) S->score[0][j] = 0.0;

	for(i=0; i < S->m; i++) S->pointer[i][0] = RIGHT;
	for(j=0; j < S->n; j++) S->pointer[0][j] = LEFT;
	
	for(i = 1; i <= s1->l; i++){
		for(j = 1; j <= s2->l; j++){
	        double new_score = (strncmp(s1->s+(i-1), s2->s+(j-1), 1) == 0) ? MATCH : MISMATCH;
			S->pointer[i][j] = max3(&S->score[i][j], S->score[i][j-1] + GAP, S->score[i-1][j-1] + new_score, S->score[i-1][j] + GAP);
		}
	}
	double res = S->score[s1->l][s2->l];
	trace_back(S, s1, s2, r1, r2);
	if(destory_matrix(S) != GL_ERR_NONE) die("smith_waterman: fail to destory matrix");
	return res;
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

void kstring_read(char* fname, kstring_t *str1, kstring_t *str2){
	gzFile fp;
	kseq_t *seq;
	fp = gzopen(fname, "r");
	seq = kseq_init(fp);
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
	printf("score=%f\n", global(ks1, ks2, r1, r2));
	printf("%s\n%s\n", r1->s, r2->s);
	kstring_destory(ks1);
	kstring_destory(ks2);
	kstring_destory(r1);
	kstring_destory(r2);
	return 0;
}