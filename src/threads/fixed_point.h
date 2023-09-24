#ifndef FIXED_POINT_H
#define FIXED_POINT_H
#include <stdint.h>
#define F (1<<14)

int convert_n_to_fp (int n);
int convert_x_to_int_zero(int x);
int convert_x_to_int_nearest(int x);
int add_x_y (int x, int y);
int sub_y_from_x(int x, int y);
int add_x_n(int x, int n);
int sub_n_from_x(int x, int n);
int mul_x_by_y(int x, int y);
int mul_x_by_n(int x, int n);
int div_x_by_y(int x, int y);
int div_x_by_n(int x, int n);


int convert_n_to_fp (int n) {return n * F;}
int convert_x_to_int_zero(int x) {return x/F;}
int convert_x_to_int_nearest(int x){
    if (x>=0) return (x+F/2)/F;
    else return (x-F/2)/F;
}
int add_x_y (int x, int y){return x+y;}
int sub_y_from_x(int x, int y){return x-y;}
int add_x_n(int x, int n) {return x+n*F;}
int sub_n_from_x(int x, int n) {return x-n*F;}
int mul_x_by_y(int x, int y){return ((int64_t)x)*y/F;}
int mul_x_by_n(int x, int n){return x*n;}
int div_x_by_y(int x, int y){return ((int64_t)x)*F/y;}
int div_x_by_n(int x, int n){return x/n;}

#endif /* threads/fixed_point.h */