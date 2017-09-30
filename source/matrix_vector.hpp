
class vector {
private:
    float v[4];
public:
    float& operator[](int i)
    {
        return v[i];
    }
};


class matrix {
private:
    float m[16];
public:
    vector& operator[](int col)
    {
        return *(vector*)(&m[col*3]);
    }
};


vector matrix_vector_multiply(
    matrix& m,
    vector& v);