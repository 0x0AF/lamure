#ifndef REN_UTILS_H_
#define REN_UTILS_H_

#include "camera.h"
#include "point.h"
#include <iostream>
#include <scm/core/math.h>

namespace utils {
    bool read_nvm(ifstream &in,
                                 vector<camera> &camera_vec,
                                 vector<point> &point_vec,
                                 vector<image> &images);

    template<typename T>
    vec<T, 2> pair_to_vec2(T *arr);

    template<typename T>
    vec<T, 3> arr3_to_vec3(T arr[3]);

    template<typename T>
    mat<T, 3, 3> arr9_to_mat3(T arr[9]);
};

#endif