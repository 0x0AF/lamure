#ifndef LAMURE_POINT_H
#define LAMURE_POINT_H

#include <scm/core.h>
#include <scm/core/math.h>
#include "image.h"

using namespace std;
using namespace scm::math;

class point {
public:

    class measurement {
    public:
        measurement(const image &_still_image, const vec2f &_position);

        const image &get_still_image() const;

        void set_still_image(const image &_still_image);

        const vec2f &get_position() const;

        void set_position(const vec2f &_position);

    private:
        image _still_image;
        vec2f _position;
    };

private:

    vec3f _center;
    vec3i _color;
    vector<measurement> _measurements;

public:
    point();

    point(const vec3f &_center, const vec3i &_color, const vector<measurement> &_measurements);

    const vec3f &get_center() const;

    void set_center(const vec3f &_center);

    const vec3i &get_color() const;

    void set_color(const vec3i &_color);

    const vector<measurement> &get_measurements() const;

    void set_measurements(const vector<measurement> &_measurements);
};


#endif //LAMURE_POINT_H