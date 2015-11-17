// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#version 420 core

out VertexData {
	vec3 nor;
  float rad;
	float mv_vertex_depth;
} VertexOut;

uniform mat4 mvp_matrix;
uniform mat4 model_view_matrix;
uniform mat4 inv_mv_matrix;

uniform float height_divided_by_top_minus_bottom;

uniform float near_plane;

uniform float minSurfelSize;

uniform float QuantFactor;

uniform float point_size_factor;

uniform float rad_scale_fac;

uniform bool is_leaf;

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_r;
layout(location = 2) in float in_g;
layout(location = 3) in float in_b;
layout(location = 4) in float empty;
layout(location = 5) in float in_radius;
layout(location = 6) in vec3 in_normal;



//out float zDepth;



void main()
{

  if(in_radius == 0.0)
  {
   gl_Position = vec4(2.0,2.0,2.0,1.0);
  }
	else
  {

    float scaled_radius = rad_scale_fac * in_radius;

	  {

		vec4 pos_es = model_view_matrix * vec4(in_position, 1.0f);

		float ps = 3.0f*(scaled_radius) * point_size_factor * (near_plane/-pos_es.z) * height_divided_by_top_minus_bottom;


		if(false)
		{
			gl_Position = vec4(2.0,2.0,2.0,1.0);
		}
		else
		{
	      		gl_Position = mvp_matrix * vec4(in_position, 1.0);
		}



		  VertexOut.nor = (inv_mv_matrix * vec4(in_normal,0.0f)) .xyz;

		  gl_PointSize = ps;

		  VertexOut.rad = (scaled_radius) * point_size_factor;

		  VertexOut.mv_vertex_depth = pos_es.z;
	  }
	 }

}
