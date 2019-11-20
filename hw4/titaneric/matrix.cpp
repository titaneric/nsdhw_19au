#ifdef PYTHON_LIB
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#endif
#include <mkl.h>

#include <iostream>
#include <utility>
#include <cmath>
#include <map>
#include <vector>
#include <stdexcept>
#include <numeric>
#include <algorithm>

using namespace std;

#ifdef PYTHON_LIB
namespace py = pybind11;
#endif

class Matrix
{
public:
    Matrix() = default;
    Matrix(size_t r, size_t c) : m_nrow(r), m_ncol(c), data(r, vector<double>(c, 0)){};
    Matrix(size_t r, size_t c, vector<double> const &vec) : m_nrow(r), m_ncol(c), data(r, vector<double>(c, 0))
    {
        size_t vec_i = 0;
        for (size_t i = 0; i < nrow(); i++)
        {
            for (size_t j = 0; j < ncol(); j++)
            {
                data[i][j] = vec[vec_i++];
            }
        }
    };
    Matrix(size_t r, size_t c, double *flat) : m_nrow(r), m_ncol(c), data(r, vector<double>(c, 0))
    {
        size_t vec_i = 0;
        for (size_t i = 0; i < nrow(); i++)
        {
            for (size_t j = 0; j < ncol(); j++)
            {
                data[i][j] = flat[vec_i++];
            }
        }
    };
    Matrix(vector<vector<double>> const &d) : m_nrow(d.size()), m_ncol((d.size()) ? d[0].size() : 0), data(d)
    {
    }
    Matrix(const Matrix &) = default;
    Matrix(Matrix &&) = default;
    ~Matrix() = default;
    Matrix &operator=(const Matrix &) = default;
    Matrix &operator=(Matrix &&) = default;
    double operator()(size_t row, size_t col) const { return data[row][col]; }
    double &operator()(size_t row, size_t col)
    {
        return data[row][col];
    }
    bool operator==(const Matrix &other) const
    {
        return nrow() == other.nrow() && ncol() == other.ncol() && data == other.data;
    }

    bool operator!=(const Matrix &other) const
    {
        return !(*this == other);
    }
    bool is_same_size(const Matrix &other) const
    {
        return (nrow() == other.nrow()) && (ncol() == other.ncol());
    }
    Matrix &operator+=(const Matrix &other)
    {
        if (!is_same_size(other))
            throw out_of_range("Matrices can not be added!!!");

        for (size_t i = 0; i < nrow(); i++)
        {
            for (size_t j = 0; j < ncol(); j++)
            {
                data[i][j] += other(i, j);
            }
        }
        return *this;
    }

    double *getFlatData() const
    {
        double *flat = new double[nrow() * ncol()];
        size_t k = 0;
        for (size_t i = 0; i < nrow(); i++)
        {
            for (size_t j = 0; j < ncol(); j++)
            {
                flat[k++] = data[i][j];
            }
        }
        return flat;
    }

    vector<vector<double>> loadBlock(size_t u, size_t v, size_t tile, bool column_major = false) const
    {
        return loadBlock(u, v, tile, tile, tile, column_major);
    }

    vector<vector<double>> loadBlock(size_t u, size_t v,
                                     size_t u_tile, size_t v_tile,
                                     size_t tile_size, bool column_major = false) const
    {
        vector<vector<double>> block(tile_size, vector<double>(tile_size, 0));

        if (column_major)
        {
            // cout << u << ", " << v << endl;
            // cout << u_tile << ", " << v_tile << endl;
            for (size_t i = 0; i < u_tile; i++)
            {
                for (size_t j = 0; j < v_tile; j++)
                {
                    block[j][i] = data[u + i][v + j];
                }
            }
        }
        else
        {
            for (size_t i = 0; i < u_tile; i++)
            {
                for (size_t j = 0; j < v_tile; j++)
                {
                    block[i][j] = data[u + i][v + j];
                }
            }
        }
        return block;
    }

    void saveBlock(size_t u, size_t v, size_t ut, size_t vt,
                   size_t i_limit, size_t j_limit,
                   size_t rem_row, size_t rem_col,
                   Matrix const &block)
    {

        size_t save_row = (ut == (i_limit - 1)) ? rem_row : block.nrow();
        size_t save_col = (vt == (j_limit - 1)) ? rem_col : block.ncol();
        // cout << save_row << "," << save_col << endl;
        for (size_t i = 0; i < save_row; i++)
        {
            for (size_t j = 0; j < save_col; j++)
            {
                data[u + i][v + j] = block(i, j);
            }
        }
    }

#ifndef PYTHON_LIB
    friend ostream &operator<<(ostream &ostr, Matrix const &mat)
    {
        for (size_t i = 0; i < mat.nrow(); ++i)
        {
            ostr << endl
                 << " ";
            for (size_t j = 0; j < mat.ncol(); ++j)
            {
                ostr << mat(i, j);
            }
        }

        return ostr;
    }
#endif

public:
    size_t nrow() const { return m_nrow; };
    size_t ncol() const { return m_ncol; };

private:
    size_t m_nrow;
    size_t m_ncol;
    vector<vector<double>> data;
};

Matrix multiply_naive(Matrix const &A, Matrix const &B, bool column_major = false)
{
    if (A.ncol() != B.nrow())
    {
        throw out_of_range("Matrices can not be multiplied!!!");
    }

    Matrix result = Matrix(A.nrow(), B.ncol());

    double value = 0;
    if (!column_major)
    {
        for (size_t i = 0; i < result.nrow(); i++)
        {
            for (size_t j = 0; j < result.ncol(); j++)
            {
                value = 0;
                for (size_t k = 0; k < A.ncol(); k++)
                {
                    value += A(i, k) * B(k, j);
                }
                result(i, j) = value;
            }
        }
    }
    else
    {
        for (size_t i = 0; i < result.nrow(); i++)
        {
            for (size_t j = 0; j < result.ncol(); j++)
            {
                value = 0;
                for (size_t k = 0; k < A.ncol(); k++)
                {
                    value += A(i, k) * B(j, k);
                }
                result(i, j) = value;
            }
        }
    }

    return result;
}

// https://software.intel.com/en-us/mkl-tutorial-c-multiplying-matrices-using-dgemm
Matrix multiply_mkl(Matrix const &A, Matrix const &B)
{
    size_t m = A.nrow(); // A: m x k
    size_t k = A.ncol(); // B: k x n
    size_t n = B.ncol(); // C: m x n
    double alpha = 1.0;
    double beta = 0.0;

    double *A_flat = A.getFlatData();
    double *B_flat = B.getFlatData();
    double *C_flat = new double[m * n];

    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                m, n, k, alpha, A_flat, k, B_flat, n, beta, C_flat, n);

    Matrix result(m, n, C_flat);

    delete A_flat;
    delete B_flat;
    delete C_flat;

    return result;
}
void make_padding(Matrix const &matrix, size_t tile_size,
                  size_t rem_row, size_t rem_col,
                  size_t num_tile_row, size_t num_tile_col,
                  map<pair<size_t, size_t>, Matrix> &padding_map, bool column_major = false)
{
    // cout << num_tile_row << "," << num_tile_col << endl;

    Matrix sub_matrix;

    // column padding
    size_t col_tile_index = num_tile_col - 1;
    size_t col_edge = col_tile_index * tile_size;

    for (size_t i = 0, sub_row_edge = 0; i < num_tile_row - 1; i++, sub_row_edge += tile_size)
    {
        sub_matrix = Matrix(matrix.loadBlock(sub_row_edge, col_edge, tile_size, rem_col, tile_size, column_major));
        padding_map[make_pair(sub_row_edge, col_edge)] = sub_matrix;
    }

    // row padding
    size_t row_tile_index = num_tile_row - 1;
    size_t row_edge = row_tile_index * tile_size;

    for (size_t j = 0, sub_col_edge = 0; j < num_tile_col - 1; j++, sub_col_edge += tile_size)
    {
        sub_matrix = Matrix(matrix.loadBlock(row_edge, sub_col_edge, rem_row, tile_size, tile_size, column_major));
        padding_map[make_pair(row_edge, sub_col_edge)] = sub_matrix;
    }

    // corner padding
    sub_matrix = Matrix(matrix.loadBlock(row_edge, col_edge, rem_row, rem_col, tile_size, column_major));
    padding_map[make_pair(row_edge, col_edge)] = sub_matrix;
}
Matrix determine_source_of_block(size_t i, size_t j,
                                 size_t tile_size, const Matrix &matrix,
                                 map<pair<size_t, size_t>, Matrix> &matrix_map,
                                 bool column_major = false)
{
    // cout << it << ":" << i_limit << "," << jt << ":" << j_limit << endl;
    auto block_index = make_pair(i, j);
    auto find_block = matrix_map.find(block_index);

    if (find_block != matrix_map.end())
    {
        return find_block->second;
    }

    matrix_map[block_index] = Matrix(matrix.loadBlock(i, j, tile_size, column_major));
    return matrix_map[block_index];
}
size_t power_of_2_tile_size(size_t tile_size)
{
    double pos = floor(log2((double)tile_size));
    return (size_t)pow(2, pos);
}
Matrix multiply_tile(Matrix const &A, Matrix const &B, size_t tile_size)
{
    if (A.ncol() != B.nrow())
    {
        throw out_of_range("Matrices can not be multiplied!!!");
    }
    auto matrices_sizes = {A.nrow(), A.ncol(), B.ncol()};
    size_t min_size = *min_element(matrices_sizes.begin(), matrices_sizes.end());
    if (min_size < tile_size)
    {
        throw out_of_range("Tile size is too big!!!");
    }

    // find suitable tile size which is the power of 2
    size_t new_tile_size = power_of_2_tile_size(tile_size);
    tile_size = new_tile_size;

#ifdef PYTHON_LIB
    tile_size = 32;
#endif

    // cout << tile_size << endl;

    div_t tile_rowA_result = div((int)A.nrow(), (int)tile_size);
    const size_t num_tile_rowA = (tile_rowA_result.rem) ? tile_rowA_result.quot + 1 : tile_rowA_result.quot;

    div_t tile_colA_result = div((int)A.ncol(), (int)tile_size);
    const size_t num_tile_colA = (tile_colA_result.rem) ? tile_colA_result.quot + 1 : tile_colA_result.quot;

    div_t tile_colB_result = div((int)B.ncol(), (int)tile_size);
    const size_t num_tile_colB = (tile_colB_result.rem) ? tile_colB_result.quot + 1 : tile_colB_result.quot;

    map<pair<size_t, size_t>, Matrix> matrix_mapA;
    map<pair<size_t, size_t>, Matrix> matrix_mapB;
    // map<pair<size_t, size_t>, Matrix> padding_mapA;
    // map<pair<size_t, size_t>, Matrix> padding_mapB;

    size_t rem_rowA = tile_rowA_result.rem;
    size_t rem_colA = tile_colA_result.rem;
    size_t rem_colB = tile_colB_result.rem;

    make_padding(A, tile_size, rem_rowA, rem_colA, num_tile_rowA, num_tile_colA, matrix_mapA);
    make_padding(B, tile_size, rem_colA, rem_colB, num_tile_colA, num_tile_colB, matrix_mapB, true);
    // make_padding(B, tile_size, rem_colA, rem_colB, num_tile_colA, num_tile_colB, matrix_mapB);

    Matrix result = Matrix(A.nrow(), B.ncol());
    size_t augA_col = num_tile_colA * tile_size;

    Matrix block = Matrix(tile_size, tile_size);

    for (size_t i = 0, it = 0; i < result.nrow(); i += tile_size, it++)
    {
        for (size_t j = 0, jt = 0; j < result.ncol(); j += tile_size, jt++)
        {
            Matrix block_result = block;
            for (size_t k = 0; k < augA_col; k += tile_size)
            {
                Matrix blockA = determine_source_of_block(i, k, tile_size, A, matrix_mapA);

                Matrix blockB = determine_source_of_block(k, j, tile_size, B, matrix_mapB, true);
                // Matrix blockB = determine_source_of_block(k, j, tile_size, B, matrix_mapB);

                // cout << blockA << "," << blockB << endl;
                Matrix partial_result = multiply_naive(blockA, blockB, true);
                // Matrix partial_result = multiply_mkl(blockA, blockB);

                // cout << partial_result << endl;
                block_result += partial_result;
            }
            // cout << block_result << endl;
            result.saveBlock(i, j, it, jt, num_tile_rowA, num_tile_colB, rem_rowA, rem_colB, block_result);
        }
    }

    return result;
}

#ifdef PYTHON_LIB
PYBIND11_MODULE(_matrix, m)
{
    m.doc() = "pybind11 matrix plugin"; // optional module docstring
    m.def("multiply_mkl", &multiply_mkl, "A function which calculates product of two matrices using MKL");
    m.def("multiply_naive", &multiply_naive, "A function which calculates product of two matrices using naive method",
          py::arg("A"), py::arg("B"), py::arg("column_major") = false);
    m.def("multiply_tile", &multiply_tile, "A function which calculates product of two matrices using tile method");
    py::class_<Matrix>(m, "Matrix")
        .def(py::init<size_t, size_t>())
        .def(py::init<size_t, size_t, vector<double>>())
        .def(py::init<vector<vector<double>>>())
        .def_property_readonly("nrow", &Matrix::nrow)
        .def_property_readonly("ncol", &Matrix::ncol)
        .def("__eq__", [](const Matrix &self, const Matrix &other) { return self == other; })
        .def("__ne__", [](const Matrix &self, const Matrix &other) { return self != other; })
        .def("__getitem__", [](const Matrix &self, pair<size_t, size_t> idx) { return self(idx.first, idx.second); })
        .def("__setitem__", [](Matrix &self, pair<size_t, size_t> idx, double val) { self(idx.first, idx.second) = val; });
}
#else
int main()
{
    // Matrix A({{1, 0},
    //           {0, 1}});
    // Matrix B({{1, 2},
    //           {3, 4}});

    // cout << A << endl;
    // cout << B << endl;
    // cout << multiply_tile(A, B, 1) << endl;
    // cout << multiply_mkl(A, B) << endl;

    Matrix A({{1, 0, 0},
              {0, 1, 0},
              {0, 0, 1}});
    Matrix B({{1, 2, 3},
              {4, 5, 6},
              {7, 8, 9}});
    cout << A << endl;
    cout << B << endl;
    cout << multiply_tile(A, B, 2) << endl;
    return 0;
}
#endif