/* wubusolver.h — linear algebra solver core (powers spreadsheet Solver/
 * Goal-seek for linear systems). Solves A·x = b by Gaussian elimination with
 * partial pivoting. Determinant + inverse also available. */
#ifndef WUBUSOLVER_H
#define WUBUSOLVER_H

/* Solve the n×n system A[][n] * x = b[n]. A is row-major, n×n. On success
 * writes x and returns 0. Returns -1 on singular matrix / bad args. */
int wubusolver_solve(const double *A, int n, const double *b, double *x);

/* Compute determinant of an n×n row-major matrix. Returns 0 on singular. */
double wubusolver_det(const double *A, int n);

/* Invert an n×n row-major matrix into inv[][n]. Returns 0 on success, -1 on
 * singular. */
int wubusolver_inv(const double *A, int n, double *inv);

/* Solve a small 2×2 system {a11 x + a12 y = b1, a21 x + a22 y = b2}. */
int wubusolver_solve2(double a11, double a12, double a21, double a22,
                      double b1, double b2, double *x, double *y);

#endif
