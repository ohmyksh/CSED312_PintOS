#ifndef FIXED_POINT_H
#define FIXED_POINT_H
#include <stdint.h>
#define F (1<<14)

int convert_n_to_fp (int n) {return n * F;}
int convert_x_to_int_zero(int x) {return x/F;}
int convert_x_to_int_nearest(int x){
    if (x>=0) return (x+F/2)/F;
    elif return (x-F/2)/F;
}
int add_x_y(int x, int y){return x+y;}
int sub_y_from_x(int x, int y){return x-y;}
int add_x_n(int x, int n) {return x+n*f;}
int sub_n_from_x(int x, int n) {return x-n*f;}
int mul_x_by_y(int x, int y){return ((int64_t)x)*y/f;}
int mul_x_by_n(int x, int n){return x*n;}
int div_x_by_y(int x, int y){return ((int64_t)x)*f/y;}
int div_x_by_n(int x, int n){return x/n;}

#endif /* threads/fixed_point.h */