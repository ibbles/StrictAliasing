#include "matrix_vector.hpp"


int main()
{
    vector v;
    v[0] = 1.0f;
    v[1] = 2.0f;
    v[2] = 3.0f;
    v[3] = 4.0f;


    matrix m;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            m[col][row] = (float)row * 10.0f + (float)col;
        }
    }

    vector mv = matrix_vector_multiply(m, v);
}
