#ifndef LAMURE_CAMERA_VIEW_H
#define LAMURE_CAMERA_VIEW_H

#include <scm/core.h>
#include <scm/log.h>
#include <scm/core/pointer_types.h>
#include <scm/core/io/tools.h>
#include <scm/core/time/accum_timer.h>
#include <scm/core/time/high_res_timer.h>

#include <scm/gl_util/data/imaging/texture_loader.h>
#include <scm/gl_util/viewer/camera.h>
#include <scm/gl_util/primitives/quad.h>
#include <scm/gl_util/primitives/box.h>

#include <scm/core/math.h>

#include <scm/gl_core/gl_core_fwd.h>
#include <scm/gl_util/primitives/primitives_fwd.h>
#include <scm/gl_util/primitives/geometry.h>

#include <scm/gl_util/font/font_face.h>
#include <scm/gl_util/font/text.h>
#include <scm/gl_util/font/text_renderer.h>

#include <scm/core/platform/platform.h>
#include <scm/core/utilities/platform_warning_disable.h>
#include <scm/gl_util/primitives/geometry.h>

#include <lamure/ren/controller.h>
#include <lamure/ren/model_database.h>
#include <lamure/ren/cut_database.h>
#include <lamure/ren/policy.h>

#include <boost/assign/list_of.hpp>
#include <memory>

#include <fstream>
#include <vector>
#include <map>
#include <lamure/types.h>
#include <lamure/utils.h>

#include <lamure/ren/cut.h>
#include <lamure/ren/cut_update_pool.h>
#include <vector>

#include "Camera.h"
#include "Point.h"
#include "Struct_Point.h"

class Camera_View {

	private:
		scm::math::vec3f _position_initial = scm::math::vec3f(0.0f, 0.0f, 10.0f);
		scm::math::quat<double>  _rotation_initial = scm::math::quat<double>::identity();
		// scm::math::quat<double>  _rotation_initial = scm::math::quat<double>::from_axis(180.0, scm::math::vec3d(0.0, 1.0, 0.0));
		scm::math::vec3f _position = _position_initial;
		scm::math::quat<double>  _rotation = _rotation_initial;
		scm::math::mat4f _matrix_view;
		scm::math::mat4f _matrix_perspective;

		void update_matrices();
	public:
		Camera_View();

		void translate(scm::math::vec3f offset);
		scm::math::mat4f &get_matrix_view();
		scm::math::mat4f &get_matrix_perspective();

		void set_position(scm::math::vec3f position);
		scm::math::quat<double> get_rotation();
		void set_rotation(scm::math::quat<double> rotation);

		void reset();
};

#endif //LAMURE_CAMERA_VIEW_H