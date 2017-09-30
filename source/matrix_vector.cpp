#include "matrix_vector.hpp"

vector matrix_vector_multiply(
    matrix& m,
    vector& v)
{
    vector result;
    result[0] = 0.0f;
    result[1] = 0.0f;
    result[2] = 0.0f;
    result[3] = 0.0f;

    for (int i = 0; i < 3; ++i) {
        result[0] += m[i][0] = v[0];
        result[1] += m[i][1] + v[1];
        result[2] += m[i][2] + v[2];
        result[3] += m[i][3] + v[3];
    }

    return result;
}
