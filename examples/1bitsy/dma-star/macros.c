#include <math.h>

#define CAT2_H(a, b) a##b
#define CAT_H(a, b) CAT2_H(a, b)
#define TMPVAR_H() CAT_H(tmp__, __COUNTER__)
#define FLOORF_H(x, t) ({                               \
                          float t = (x);                \
                          t >= 0 ? (int)t : -(int)-t;   \
                        })
#define FLOORF(x) (FLOORF_H((x), TMPVAR_H()))

int f(float x);
int f(float x)
{
    return -FLOORF(x);
}

float sum_prod_vec(float k, float *v0, float *v1, size_t n);
float sum_prod_vec(float k, float *v0, float *v1, size_t n)
{
    float sum = 0;
    for (size_t i = 0; i < n; i++)
        sum += k * v0[i] + v1[i];
    return sum;
}
