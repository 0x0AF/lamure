// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include "renderer.h"

#include <ctime>

#include <lamure/config.h>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include <FreeImagePlus.h>

#include <scm/gl_core/render_device/opengl/gl_core.h>

size_t Renderer::current_screenshot_num_ = 1;

Renderer::
Renderer(std::vector<scm::math::mat4f> const& model_transformations,
         const std::set<lamure::model_t>& visible_set,
         const std::set<lamure::model_t>& invisible_set)
    : height_divided_by_top_minus_bottom_(1000.f),
      near_plane_(0.f),
      visible_set_(visible_set),
      invisible_set_(invisible_set),
      render_visible_set_(true),
      far_minus_near_plane_(0.f),
      point_size_factor_(1.0f),
      render_mode_(0),
      ellipsify_(true),
      render_bounding_boxes_(false),
      clamped_normal_mode_(true),
      max_deform_ratio_(0.35f),
      fps_(0.0),
      rendered_splats_(0),
      uploaded_nodes_(0),
      is_cut_update_active_(true),
      current_cam_id_(0),
      display_info_(true),
#ifdef LAMURE_ENABLE_LINE_VISUALIZATION
      max_lines_(256),
#endif
      model_transformations_(model_transformations),
      radius_scale_(1.f)
{
    lamure::ren::ModelDatabase* database = lamure::ren::ModelDatabase::GetInstance();

    win_x_ = database->window_width();
    win_y_ = database->window_height();

    InitializeSchismDeviceAndShaders(win_x_, win_y_);
    InitializeVBOs();
    ResetViewport(win_x_, win_y_);

    CalcRadScaleFactors();
}

Renderer::
~Renderer()
{

    filter_nearest_.reset();
    color_blending_state_.reset();
    not_blended_state_.reset();

    change_point_size_in_shader_state_.reset();

    depth_state_less_.reset();
    depth_state_disable_.reset();

    pass1_visibility_shader_program_.reset();
    pass2_accumulation_shader_program_.reset();
    pass3_pass_trough_shader_program_.reset();


    bounding_box_vis_shader_program_.reset();

    pass1_depth_buffer_.reset();
    pass1_visibility_fbo_.reset();

    pass2_accumulated_color_buffer_.reset();

    pass2_accumulation_fbo_.reset();

    pass_filling_color_texture_.reset();

    gaussian_texture_.reset();

    screen_quad_.reset();

    context_.reset();
    device_.reset();




}

void Renderer::UploadUniforms(lamure::ren::Camera const& camera) const
{
    using namespace lamure::ren;
    using namespace scm::gl;
    using namespace scm::math;

     

    pass1_visibility_shader_program_->uniform("height_divided_by_top_minus_bottom", height_divided_by_top_minus_bottom_);
    pass1_visibility_shader_program_->uniform("near_plane", near_plane_);
    pass1_visibility_shader_program_->uniform("far_minus_near_plane", far_minus_near_plane_);
    pass1_visibility_shader_program_->uniform("point_size_factor", point_size_factor_);

    

    pass1_visibility_shader_program_->uniform("ellipsify", ellipsify_);
    pass1_visibility_shader_program_->uniform("clamped_normal_mode", clamped_normal_mode_);
    pass1_visibility_shader_program_->uniform("max_deform_ratio", max_deform_ratio_);


    pass2_accumulation_shader_program_->uniform("depth_texture_pass1", 0);
    pass2_accumulation_shader_program_->uniform("pointsprite_texture", 1);
    pass2_accumulation_shader_program_->uniform("win_size", scm::math::vec2f(win_x_, win_y_) );

    pass2_accumulation_shader_program_->uniform("height_divided_by_top_minus_bottom", height_divided_by_top_minus_bottom_);
    pass2_accumulation_shader_program_->uniform("near_plane", near_plane_);
    pass2_accumulation_shader_program_->uniform("far_minus_near_plane", far_minus_near_plane_);
    pass2_accumulation_shader_program_->uniform("point_size_factor", point_size_factor_);



    pass2_accumulation_shader_program_->uniform("ellipsify", ellipsify_);
    pass2_accumulation_shader_program_->uniform("clamped_normal_mode", clamped_normal_mode_);
    pass2_accumulation_shader_program_->uniform("max_deform_ratio", max_deform_ratio_);


    pass3_pass_trough_shader_program_->uniform_sampler("in_color_texture", 0);

    pass3_pass_trough_shader_program_->uniform("renderMode", render_mode_);

    pass_filling_program_->uniform_sampler("in_color_texture", 0);
    pass_filling_program_->uniform_sampler("depth_texture", 1);
    pass_filling_program_->uniform("win_size", scm::math::vec2f(win_x_, win_y_) );

    pass_filling_program_->uniform("renderMode", render_mode_);




    context_->clear_default_color_buffer(FRAMEBUFFER_BACK, vec4f(0.0f, 0.0f, .0f, 1.0f)); // how does the image look, if nothing is drawn
    context_->clear_default_depth_stencil_buffer();

    context_->apply();
}

void Renderer::
UploadTransformationMatrices(lamure::ren::Camera const& camera, lamure::model_t model_id, uint32_t pass_id) const
{
    using namespace lamure::ren;

    ModelDatabase* database = ModelDatabase::GetInstance();

    scm::math::mat4f    view_matrix         = camera.GetViewMatrix();
    scm::math::mat4f    model_matrix        = model_transformations_[model_id];
    scm::math::mat4f    projection_matrix   = camera.GetProjectionMatrix();

#if 1
    scm::math::mat4d    vm = camera.GetHighPrecisionViewMatrix();
    scm::math::mat4d    mm = scm::math::mat4d(model_matrix);
    scm::math::mat4d    vmd = vm * mm;
    
    scm::math::mat4f    model_view_matrix = scm::math::mat4f(vmd);

    scm::math::mat4d    pmd = scm::math::mat4d(projection_matrix);
    scm::math::mat4d    mvpd = scm::math::mat4d(projection_matrix) * vmd;
    
#define DEFAULT_PRECISION 31
#else
    scm::math::mat4f    model_view_matrix   = view_matrix * model_matrix;
#endif

    float rad_scale_fac = radius_scale_ * rad_scale_fac_[model_id];

    if(pass_id == 1)
    {
        pass1_visibility_shader_program_->uniform("mvp_matrix", scm::math::mat4f(mvpd) );
        pass1_visibility_shader_program_->uniform("model_view_matrix", model_view_matrix);
        pass1_visibility_shader_program_->uniform("inv_mv_matrix", scm::math::mat4f(scm::math::transpose(scm::math::inverse(vmd))));
        pass1_visibility_shader_program_->uniform("rad_scale_fac", rad_scale_fac);
    }
    else if(pass_id == 2)
    {
        pass2_accumulation_shader_program_->uniform("mvp_matrix", scm::math::mat4f(mvpd));
        pass2_accumulation_shader_program_->uniform("model_view_matrix", model_view_matrix);
        pass2_accumulation_shader_program_->uniform("inv_mv_matrix", scm::math::mat4f(scm::math::transpose(scm::math::inverse(vmd))));
        pass2_accumulation_shader_program_->uniform("rad_scale_fac", rad_scale_fac);
    }
    else if(pass_id == 5)
    {
        bounding_box_vis_shader_program_->uniform("projection_matrix", projection_matrix);
        bounding_box_vis_shader_program_->uniform("model_view_matrix", model_view_matrix );
    }
    else if(pass_id == 99)
    {
        alt_pass1_accumulation_shader_program_->uniform("mvp_matrix", scm::math::mat4f(mvpd));
        alt_pass1_accumulation_shader_program_->uniform("model_view_matrix", model_view_matrix);
        alt_pass1_accumulation_shader_program_->uniform("inv_mv_matrix", scm::math::mat4f(scm::math::transpose(scm::math::inverse(vmd))));
        alt_pass1_accumulation_shader_program_->uniform("rad_scale_fac", rad_scale_fac);
    }
#ifdef LAMURE_ENABLE_LINE_VISUALIZATION
    else if (pass_id == 1111) {
        line_shader_program_->uniform("projection_matrix", projection_matrix);
        line_shader_program_->uniform("view_matrix", view_matrix );
    }
#endif
    else
    {
        std::cout<<"Shader does not need model_view_transformation\n\n";
    }


    context_->apply();

}


void Renderer::
Render(lamure::context_t context_id, lamure::ren::Camera const& camera, const lamure::view_t view_id, scm::gl::vertex_array_ptr render_VAO)
{
    using namespace lamure;
    using namespace lamure::ren;

    UpdateFrustumDependentParameters(camera);

    UploadUniforms(camera);

    using namespace scm::gl;
    using namespace scm::math;

    ModelDatabase* database = ModelDatabase::GetInstance();
    CutDatabase* cuts = CutDatabase::GetInstance();

    size_t NumbersOfSurfelsPerNode = database->surfels_per_node();
    model_t num_models = database->num_models();

    //determine set of models to render
    std::set<lamure::model_t> current_set;
    for (lamure::model_t model_id = 0; model_id < num_models; ++model_id) {
        auto vs_it = visible_set_.find(model_id);
        auto is_it = invisible_set_.find(model_id);

        if (vs_it == visible_set_.end() && is_it == invisible_set_.end()) {
            current_set.insert(model_id);
        }
        else if (vs_it != visible_set_.end()) {
            if (render_visible_set_) {
                current_set.insert(model_id);
            }
        }
        else if (is_it != invisible_set_.end()) {
            if (!render_visible_set_) {
                current_set.insert(model_id);
            }
        }

    }


    std::vector<uint32_t>                       frustum_culling_results;

    uint32_t size_of_culling_result_vector = 0;

    for (auto& model_id : current_set)
    {
        Cut& cut = cuts->GetCut(context_id, view_id, model_id);

        std::vector<Cut::NodeSlotAggregate> renderable = cut.complete_set();

        size_of_culling_result_vector += renderable.size();
    }

     frustum_culling_results.clear();
     frustum_culling_results.resize(size_of_culling_result_vector);

#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
     size_t depth_pass_time = 0;
     size_t accumulation_pass_time = 0;
     size_t normalization_pass_time = 0;
     size_t hole_filling_pass_time = 0;
#endif



            {


                /***************************************************************************************
                *******************************BEGIN DEPTH PASS*****************************************
                ****************************************************************************************/

                {


                    context_->clear_depth_stencil_buffer(pass1_visibility_fbo_);


                    context_->set_frame_buffer(pass1_visibility_fbo_);

                    context_->set_viewport(viewport(vec2ui(0, 0), 1 * vec2ui(win_x_, win_y_)));

                    context_->bind_program(pass1_visibility_shader_program_);


                    context_->set_rasterizer_state(change_point_size_in_shader_state_);
                    context_->bind_vertex_array(render_VAO);
                    context_->apply();


                    pass1_visibility_shader_program_->uniform("minSurfelSize", 1.0f);
                    pass1_visibility_shader_program_->uniform("QuantFactor", 1.0f);
                    context_->apply();

                    node_t node_counter = 0;

                    for (auto& model_id : current_set)
                    {
                        Cut& cut = cuts->GetCut(context_id, view_id, model_id);

                        std::vector<Cut::NodeSlotAggregate> renderable = cut.complete_set();

                        const Bvh* bvh = database->GetModel(model_id)->bvh();

                        size_t surfels_per_node_of_model = bvh->surfels_per_node();
                        //size_t surfels_per_node_of_model = NumbersOfSurfelsPerNode;
                        //store culling result and push it back for second pass#

                        std::vector<scm::gl::boxf>const & bounding_box_vector = bvh->bounding_boxes();


                        UploadTransformationMatrices(camera, model_id, 1);

                        scm::gl::frustum frustum_by_model = camera.GetFrustumByModel(model_transformations_[model_id]);


                        unsigned int leaf_level_start_index = bvh->GetFirstNodeIdOfDepth(bvh->depth());


                        for(std::vector<Cut::NodeSlotAggregate>::const_iterator k = renderable.begin(); k != renderable.end(); ++k, ++node_counter)
                        {

                            uint32_t node_culling_result = camera.CullAgainstFrustum( frustum_by_model ,bounding_box_vector[ k->node_id_ ] );


                             frustum_culling_results[node_counter] = node_culling_result;


                            if( (node_culling_result != 1) )
                            {


                                bool is_leaf = (leaf_level_start_index <= k->node_id_);


                                pass1_visibility_shader_program_->uniform("is_leaf", is_leaf);

                                context_->apply();
#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
                                scm::gl::timer_query_ptr depth_pass_timer_query = device_->create_timer_query();
                                context_->begin_query(depth_pass_timer_query);
#endif

                                context_->draw_arrays(PRIMITIVE_POINT_LIST, (k->slot_id_) * NumbersOfSurfelsPerNode, surfels_per_node_of_model);

#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT

                                context_->collect_query_results(depth_pass_timer_query);
                                depth_pass_time += depth_pass_timer_query->result();
#endif
                            }


                        }

                   }


                }



                /***************************************************************************************
                *******************************BEGIN ACCUMULATION PASS**********************************
                ****************************************************************************************/

                {

                    context_->clear_color_buffer(pass2_accumulation_fbo_ , 0, vec4f( .0f, .0f, .0f, 0.0f));
                    context_->clear_color_buffer(pass2_accumulation_fbo_ , 1, vec4f( .0f, .0f, .0f, 0.0f));

                    context_->set_frame_buffer(pass2_accumulation_fbo_);

                    context_->set_blend_state(color_blending_state_);

                    context_->set_depth_stencil_state(depth_state_disable_);

                    context_->bind_program(pass2_accumulation_shader_program_);

                    context_->bind_texture(pass1_depth_buffer_, filter_nearest_,   0);

                    context_->bind_texture( gaussian_texture_  ,  filter_nearest_,   1);

                    context_->bind_vertex_array(render_VAO);
                    context_->apply();


                    pass2_accumulation_shader_program_->uniform("minSurfelSize", 1.0f);
                    pass2_accumulation_shader_program_->uniform("QuantFactor", 1.0f);
                    context_->apply();


                   node_t node_counter = 0;

                   node_t actually_rendered_nodes = 0;



                    for (auto& model_id : current_set)
                    {
                        Cut& cut = cuts->GetCut(context_id, view_id, model_id);

                        std::vector<Cut::NodeSlotAggregate> renderable = cut.complete_set();

                        const Bvh* bvh = database->GetModel(model_id)->bvh();

                        size_t surfels_per_node_of_model = bvh->surfels_per_node();
                        //size_t surfels_per_node_of_model = NumbersOfSurfelsPerNode;
                        //store culling result and push it back for second pass#


                        UploadTransformationMatrices(camera, model_id, 2);

                        unsigned int leaf_level_start_index = bvh->GetFirstNodeIdOfDepth(bvh->depth());

                        for(std::vector<Cut::NodeSlotAggregate>::const_iterator k = renderable.begin(); k != renderable.end(); ++k, ++node_counter)
                        {

                            if( frustum_culling_results[node_counter] != 1)  // 0 = inside, 1 = outside, 2 = intersectingS
                            {

                                bool is_leaf = (leaf_level_start_index <= k->node_id_);


                                pass2_accumulation_shader_program_->uniform("is_leaf", is_leaf);

                                context_->apply();

#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
                                scm::gl::timer_query_ptr accumulation_pass_timer_query = device_->create_timer_query();
                                context_->begin_query(accumulation_pass_timer_query);
#endif
                                context_->draw_arrays(PRIMITIVE_POINT_LIST, (k->slot_id_) * NumbersOfSurfelsPerNode, surfels_per_node_of_model);
#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
                                context_->end_query(accumulation_pass_timer_query);
                                context_->collect_query_results(accumulation_pass_timer_query);
                                accumulation_pass_time += accumulation_pass_timer_query->result();
#endif

                                ++actually_rendered_nodes;
                            }
                        }


                   }
                    rendered_splats_ = actually_rendered_nodes * database->surfels_per_node();

                }

                /***************************************************************************************
                *******************************BEGIN NORMALIZATION PASS*********************************
                ****************************************************************************************/


                {

                    //context_->set_default_frame_buffer();
                    context_->clear_color_buffer(pass_filling_fbo_, 0, vec4( 0.0, 0.0, 0.0, 0) );
                    context_->set_frame_buffer(pass_filling_fbo_);

                    context_->set_depth_stencil_state(depth_state_less_);

                    context_->bind_program(pass3_pass_trough_shader_program_);



                    context_->bind_texture(pass2_accumulated_color_buffer_, filter_nearest_,   0);
                    context_->apply();

#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
                    scm::gl::timer_query_ptr normalization_pass_timer_query = device_->create_timer_query();
                    context_->begin_query(normalization_pass_timer_query);
#endif
                    screen_quad_->draw(context_);
#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
                    context_->end_query(normalization_pass_timer_query);
                    context_->collect_query_results(normalization_pass_timer_query);
                    normalization_pass_time += normalization_pass_timer_query->result();
#endif


                }

                /***************************************************************************************
                *******************************BEGIN RECONSTRUCTION PASS*********************************
                ****************************************************************************************/
                {
                    context_->set_default_frame_buffer();

                    context_->bind_program(pass_filling_program_);



                    context_->bind_texture(pass_filling_color_texture_, filter_nearest_,   0);
                    context_->bind_texture(pass1_depth_buffer_, filter_nearest_,   1);
                    context_->apply();

#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
                    scm::gl::timer_query_ptr hole_filling_pass_timer_query = device_->create_timer_query();
                    context_->begin_query(hole_filling_pass_timer_query);
#endif
                    screen_quad_->draw(context_);
#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
                    context_->end_query(hole_filling_pass_timer_query);
                    context_->collect_query_results(hole_filling_pass_timer_query);
                    hole_filling_pass_time += hole_filling_pass_timer_query->result();
#endif

                }




        if(render_bounding_boxes_)
        {

                        context_->set_default_frame_buffer();

                        context_->bind_program(bounding_box_vis_shader_program_);

                        context_->apply();



                    node_t node_counter = 0;

                    for (auto& model_id : current_set)
                    {
                        Cut& cut = cuts->GetCut(context_id, view_id, model_id);

                        std::vector<Cut::NodeSlotAggregate> renderable = cut.complete_set();


                        UploadTransformationMatrices(camera, model_id, 5);

                        for(std::vector<Cut::NodeSlotAggregate>::const_iterator k = renderable.begin(); k != renderable.end(); ++k, ++node_counter)
                        {

                            int culling_result = frustum_culling_results[node_counter];

                            if( culling_result  != 1 )  // 0 = inside, 1 = outside, 2 = intersectingS
                            {

                                scm::gl::boxf temp_box = database->GetModel(model_id)->bvh()->bounding_boxes()[k->node_id_ ];
                                scm::gl::box_geometry box_to_render(device_,temp_box.min_vertex(), temp_box.max_vertex());




                                bounding_box_vis_shader_program_->uniform("culling_status", culling_result);


                                device_->opengl_api().glDisable(GL_DEPTH_TEST);
                                box_to_render.draw(context_, scm::gl::geometry::MODE_WIRE_FRAME);
                                device_->opengl_api().glEnable(GL_DEPTH_TEST);

                            }
                        }


                    }


        }


        context_->reset();
        frame_time_.stop();
        frame_time_.start();

        if (true)
        {
            //schism bug ? time::to_seconds yields milliseconds
            if (scm::time::to_seconds(frame_time_.accumulated_duration()) > 100.0)
            {

                fps_ = 1000.0f / scm::time::to_seconds(frame_time_.average_duration());


                frame_time_.reset();
            }
        }


#ifdef LAMURE_ENABLE_LINE_VISUALIZATION


    scm::math::vec3f* line_mem = (scm::math::vec3f*)device_->main_context()->map_buffer(line_buffer_, scm::gl::ACCESS_READ_WRITE);
    unsigned int num_valid_lines = 0;
    for (unsigned int i = 0; i < max_lines_; ++i) {
      if (i < line_begin_.size() && i < line_end_.size()) {
         line_mem[num_valid_lines*2+0] = line_begin_[i];
         line_mem[num_valid_lines*2+1] = line_end_[i];
         ++num_valid_lines; 
      }
    }

    device_->main_context()->unmap_buffer(line_buffer_);

    UploadTransformationMatrices(camera, 0, 1111);
    device_->opengl_api().glDisable(GL_DEPTH_TEST);

    context_->set_default_frame_buffer();

    context_->bind_program(line_shader_program_);

    context_->bind_vertex_array(line_memory_);
    context_->apply();

    context_->draw_arrays(PRIMITIVE_LINE_LIST, 0, 2*num_valid_lines);


    device_->opengl_api().glEnable(GL_DEPTH_TEST);
#endif

    if(display_info_)
      DisplayStatus();

      context_->reset();





    }

#ifdef LAMURE_RENDERING_ENABLE_PERFORMANCE_MEASUREMENT
uint64_t total_time = depth_pass_time + accumulation_pass_time + normalization_pass_time + hole_filling_pass_time;

std::cout << "depth pass        : " << depth_pass_time / ((float)(1000000)) << "ms (" << depth_pass_time        /( (float)(total_time) ) << ")\n"
          << "accumulation pass : " << accumulation_pass_time / ((float)(1000000)) << "ms (" << accumulation_pass_time /( (float)(total_time) )<< ")\n"
          << "normalization pass: " << normalization_pass_time / ((float)(1000000)) << "ms (" << normalization_pass_time/( (float)(total_time) )<< ")\n"
          << "hole filling  pass: " << hole_filling_pass_time / ((float)(1000000)) << "ms (" << hole_filling_pass_time /( (float)(total_time) )<< ")\n\n";

#endif

}


void Renderer::SendModelTransform(const lamure::model_t model_id, const scm::math::mat4f& transform) {
    model_transformations_[model_id] = transform;

}



void Renderer::DisplayStatus()
{


    std::stringstream os;
   // os.setprecision(5);
    os
      <<"FPS:   "<<std::setprecision(4)<<fps_<<"\n"
      /*
      <<"PointSizeFactor:   "<<point_size_factor_<<"\n"
      <<"Normal Clamping:   "<< (clamped_normal_mode_ ? "ON" : "OFF")<<"\n"
      <<"Clamping Threshold:   "<<max_deform_ratio_<<"\n"
      <<"Splat Mode:   "<<(ellipsify_ ? "elliptical" : "round")<<"\n"
      <<"Render Mode:   "<<(render_mode_ == 0 ? "Color" : (render_mode_ == 1 ? "LOD" : "Normal"))<<"\n"
      <<"Rendered Splats:   "<<rendered_splats_<<"\n"
      <<"Uploaded Nodes:   "<<uploaded_nodes_<<"\n"
      <<"\n"
      <<"Cut Update:   "<< (is_cut_update_active_ == true ? "active" : "frozen") <<"\n"
      <<"Camera:   "<< current_cam_id_;
*/
      <<"# Points:   "<< (rendered_splats_ / 100000) / 10.0f<< " Mio. \n";
    renderable_text_->text_string(os.str());
    text_renderer_->draw_shadowed(context_, scm::math::vec2i(20, win_y_- 40), renderable_text_);

    rendered_splats_ = 0;
    uploaded_nodes_ = 0;


}

void Renderer::
InitializeVBOs()
{
    // init the GL context
    using namespace scm;
    using namespace scm::gl;
    using namespace scm::math;


    filter_nearest_ = device_->create_sampler_state(FILTER_MIN_MAG_LINEAR, WRAP_CLAMP_TO_EDGE);




    //needs to be set to be able to change point size in shaders
    change_point_size_in_shader_state_ = device_->create_rasterizer_state(FILL_SOLID, CULL_NONE, ORIENT_CCW, false, false, 0.0, false, false, point_raster_state(true));


    pass1_visibility_fbo_ = device_->create_frame_buffer();

    pass1_depth_buffer_           = device_->create_texture_2d(vec2ui(win_x_, win_y_) * 1, FORMAT_D32F, 1, 1, 1);

    pass1_visibility_fbo_->attach_depth_stencil_buffer(pass1_depth_buffer_);



    pass2_accumulation_fbo_ = device_->create_frame_buffer();

    pass2_accumulated_color_buffer_   = device_->create_texture_2d(vec2ui(win_x_, win_y_) * 1, FORMAT_RGBA_32F , 1, 1, 1);

    pass2_accumulation_fbo_->attach_color_buffer(0, pass2_accumulated_color_buffer_);



    screen_quad_.reset(new quad_geometry(device_, vec2f(-1.0f, -1.0f), vec2f(1.0f, 1.0f)));


    float gaussian_buffer[32] = {255, 255, 252, 247, 244, 234, 228, 222, 208, 201,
                                 191, 176, 167, 158, 141, 131, 125, 117, 100,  91,
                                 87,  71,  65,  58,  48,  42,  39,  32,  28,  25,
                                 19,  16};

    texture_region ur(vec3ui(0u), vec3ui(32, 1, 1));

    gaussian_texture_ = device_->create_texture_2d(vec2ui(32,1), FORMAT_R_32F, 1, 1, 1);

    context_->update_sub_texture(gaussian_texture_, ur, 0u, FORMAT_R_32F, gaussian_buffer);



    color_blending_state_ = device_->create_blend_state(true, FUNC_ONE, FUNC_ONE, FUNC_ONE, FUNC_ONE, EQ_FUNC_ADD, EQ_FUNC_ADD);


    not_blended_state_ = device_->create_blend_state(false);

    depth_state_less_ = device_->create_depth_stencil_state(true, true, COMPARISON_LESS);

    depth_stencil_state_desc no_depth_test_descriptor = depth_state_less_->descriptor();
    no_depth_test_descriptor._depth_test = false;

    depth_state_disable_ = device_->create_depth_stencil_state(no_depth_test_descriptor);

#ifdef LAMURE_ENABLE_LINE_VISUALIZATION
    std::size_t size_of_line_buffer = max_lines_ * sizeof(float) * 3 * 2;
    line_buffer_ = device_->create_buffer(scm::gl::BIND_VERTEX_BUFFER,
                                    scm::gl::USAGE_DYNAMIC_DRAW,
                                    size_of_line_buffer,
                                    0);
    line_memory_ = device_->create_vertex_array(scm::gl::vertex_format
            (0, 0, scm::gl::TYPE_VEC3F, sizeof(float)*3),
            boost::assign::list_of(line_buffer_));

#endif
}

bool Renderer::
InitializeSchismDeviceAndShaders(int resX, int resY)
{
    std::string root_path = LAMURE_SHADERS_DIR;

    std::string visibility_vs_source;
    std::string visibility_fs_source;

    std::string pass_trough_vs_source;
    std::string pass_trough_fs_source;

    std::string accumulation_vs_source;
    std::string accumulation_fs_source;

    std::string filling_vs_source;
    std::string filling_fs_source;

    std::string bounding_box_vs_source;
    std::string bounding_box_fs_source;

#ifdef LAMURE_ENABLE_LINE_VISUALIZATION
    std::string line_vs_source;
    std::string line_fs_source;
#endif
    try {

        using scm::io::read_text_file;

        if (!read_text_file(root_path +  "/pass1_visibility_pass.glslv", visibility_vs_source)
            || !read_text_file(root_path + "/pass1_visibility_pass.glslf", visibility_fs_source)
            || !read_text_file(root_path + "/pass2_accumulation_pass.glslv", accumulation_vs_source)
            || !read_text_file(root_path + "/pass2_accumulation_pass.glslf", accumulation_fs_source)
            || !read_text_file(root_path + "/pass3_pass_trough.glslv", pass_trough_vs_source)
            || !read_text_file(root_path + "/pass3_pass_trough.glslf", pass_trough_fs_source)
            || !read_text_file(root_path + "/pass_reconstruction.glslv", filling_vs_source)
            || !read_text_file(root_path + "/pass_reconstruction.glslf", filling_fs_source)
            || !read_text_file(root_path + "/bounding_box_vis.glslv", bounding_box_vs_source)
            || !read_text_file(root_path + "/bounding_box_vis.glslf", bounding_box_fs_source)
#ifdef LAMURE_ENABLE_LINE_VISUALIZATION
            || !read_text_file(root_path + "/lines_shader.glslv", line_vs_source)
            || !read_text_file(root_path + "/lines_shader.glslf", line_fs_source)
#endif
           )
           {
               scm::err() << "error reading shader files" << scm::log::end;
               return false;
           }
    }
    catch (std::exception& e)
    {

        std::cout << e.what() << std::endl;
    }


    device_.reset(new scm::gl::render_device());

    context_ = device_->main_context();

    scm::out() << *device_ << scm::log::end;

    pass1_visibility_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, visibility_vs_source))
                                                               (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, visibility_fs_source)));

    pass2_accumulation_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, accumulation_vs_source))
                                                                 (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER,accumulation_fs_source)));

    pass3_pass_trough_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, pass_trough_vs_source))
                                                                (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, pass_trough_fs_source)));

    pass_filling_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, filling_vs_source))
                                                    (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, filling_fs_source)));

    bounding_box_vis_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, bounding_box_vs_source))
                                                               (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, bounding_box_fs_source)));

#ifdef LAMURE_ENABLE_LINE_VISUALIZATION
    line_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, line_vs_source))
                                                   (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, line_fs_source)));
#endif


    if (!pass1_visibility_shader_program_ || !pass2_accumulation_shader_program_ || !pass3_pass_trough_shader_program_ || !pass_filling_program_ || !bounding_box_vis_shader_program_

#ifdef LAMURE_ENABLE_LINE_VISUALIZATION
        || !line_shader_program_
#endif
       ) {
        scm::err() << "error creating shader programs" << scm::log::end;
        return false;
    }


    scm::out() << *device_ << scm::log::end;


    using namespace scm;
    using namespace scm::gl;
    using namespace scm::math;

    try {
        font_face_ptr output_font(new font_face(device_, std::string(LAMURE_FONTS_DIR) + "/Ubuntu.ttf", 30, 0, font_face::smooth_lcd));
        text_renderer_  =     scm::make_shared<text_renderer>(device_);
        renderable_text_    = scm::make_shared<scm::gl::text>(device_, output_font, font_face::style_regular, "sick, sad world...");

        mat4f   fs_projection = make_ortho_matrix(0.0f, static_cast<float>(win_x_),
                                                  0.0f, static_cast<float>(win_y_), -1.0f, 1.0f);
        text_renderer_->projection_matrix(fs_projection);

        renderable_text_->text_color(math::vec4f(1.0f, 1.0f, 1.0f, 1.0f));
        renderable_text_->text_kerning(true);
    }
    catch(const std::exception& e) {
        throw std::runtime_error(std::string("vtexture_system::vtexture_system(): ") + e.what());
    }

    return true;
}

void Renderer::ResetViewport(int w, int h)
{
    //reset viewport
    win_x_ = w;
    win_y_ = h;
    context_->set_viewport(scm::gl::viewport(scm::math::vec2ui(0, 0), scm::math::vec2ui(w, h)));


    //reset frame buffers and textures
    pass1_visibility_fbo_ = device_->create_frame_buffer();

    pass1_depth_buffer_           =device_->create_texture_2d(scm::math::vec2ui(win_x_, win_y_) * 1, scm::gl::FORMAT_D24, 1, 1, 1);

    pass1_visibility_fbo_->attach_depth_stencil_buffer(pass1_depth_buffer_);


    pass2_accumulation_fbo_ = device_->create_frame_buffer();

    pass2_accumulated_color_buffer_   = device_->create_texture_2d(scm::math::vec2ui(win_x_, win_y_) * 1, scm::gl::FORMAT_RGBA_32F , 1, 1, 1);

    pass2_accumulation_fbo_->attach_color_buffer(0, pass2_accumulated_color_buffer_);


    pass_filling_fbo_ = device_->create_frame_buffer();

    pass_filling_color_texture_ = device_->create_texture_2d(scm::math::vec2ui(win_x_, win_y_) * 1, scm::gl::FORMAT_RGBA_8 , 1, 1, 1);

    pass_filling_fbo_->attach_color_buffer(0, pass_filling_color_texture_);



    //reset orthogonal projection matrix for text rendering
    scm::math::mat4f   fs_projection = scm::math::make_ortho_matrix(0.0f, static_cast<float>(win_x_),
                                                                    0.0f, static_cast<float>(win_y_), -1.0f, 1.0f);
    text_renderer_->projection_matrix(fs_projection);

}

void Renderer::
UpdateFrustumDependentParameters(lamure::ren::Camera const& camera)
{
    near_plane_ = camera.near_plane_value();
    far_minus_near_plane_ = camera.far_plane_value() - near_plane_;

    std::vector<scm::math::vec3d> corner_values = camera.get_frustum_corners();
    double top_minus_bottom = scm::math::length((corner_values[2]) - (corner_values[0]));

    height_divided_by_top_minus_bottom_ = win_y_ / top_minus_bottom;
}

void Renderer::
CalcRadScaleFactors()
{
    using namespace lamure::ren;
    uint32_t num_models = (ModelDatabase::GetInstance())->num_models();

    if(rad_scale_fac_.size() < num_models)
      rad_scale_fac_.resize(num_models);

    scm::math::vec4f x_unit_vec = scm::math::vec4f(1.0,0.0,0.0,0.0);
    for(unsigned int model_id = 0; model_id < num_models; ++model_id)
    {
     rad_scale_fac_[model_id] = scm::math::length(model_transformations_[model_id] * x_unit_vec);
    }
}

//dynamic rendering adjustment functions

void Renderer::
SwitchBoundingBoxRendering()
{
    render_bounding_boxes_ = ! render_bounding_boxes_;

    std::cout<<"bounding box visualisation: ";
    if(render_bounding_boxes_)
        std::cout<<"ON\n\n";
    else
        std::cout<<"OFF\n\n";
};



void Renderer::
ChangePointSize(float amount)
{
    point_size_factor_ += amount;
    if(point_size_factor_ < 0.0001f)
    {
        point_size_factor_ = 0.0001;
    }

    std::cout<<"set point size factor to: "<<point_size_factor_<<"\n\n";
};

void Renderer::
SwitchRenderMode()
{
    render_mode_ = (render_mode_ + 1)%2;

    std::cout<<"switched render mode to: ";
    switch(render_mode_)
    {
        case 0:
            std::cout<<"COLOR\n\n";
            break;
        case 1:
            std::cout<<"NORMAL\n\n";
            break;
        case 2:
            std::cout<<"DEPTH\n\n";
            break;
        default:
            std::cout<<"UNKNOWN\n\n";
            break;
    }
};

void Renderer::
SwitchEllipseMode()
{
    ellipsify_ = ! ellipsify_;
    std::cout<<"splat mode: ";
    if(ellipsify_)
        std::cout<<"ELLIPTICAL\n\n";
    else
        std::cout<<"ROUND\n\n";
};

void Renderer::
SwitchClampedNormalMode()
{
    clamped_normal_mode_ = !clamped_normal_mode_;
    std::cout<<"clamp elliptical deform ratio: ";
    if(clamped_normal_mode_)
        std::cout<<"ON\n\n";
    else
        std::cout<<"OFF\n\n";
};

void Renderer::
ChangeDeformRatio(float amount)
{
    max_deform_ratio_ += amount;

    if(max_deform_ratio_ < 0.0f)
        max_deform_ratio_ = 0.0f;
    else if(max_deform_ratio_ > 5.0f)
        max_deform_ratio_ = 5.0f;

    std::cout<<"set elliptical deform ratio to: "<<max_deform_ratio_<<"\n\n";
};


void Renderer::
ToggleCutUpdateInfo()
{
    is_cut_update_active_ = ! is_cut_update_active_;
}

void Renderer::
ToggleCameraInfo(const lamure::view_t current_cam_id)
{
    current_cam_id_ = current_cam_id;
}

void Renderer::
ToggleDisplayInfo()
{
    display_info_ = ! display_info_;
}

void Renderer::
ToggleVisibleSet() {
    render_visible_set_ = !render_visible_set_;
}

void Renderer::TakeScreenshot()
{

    std::string scenename_ = "screenshots";
    {


        {


        if(! boost::filesystem::exists("./"+scenename_+"/"))
        {
               std::cout<<"Creating Folder.\n\n";
               boost::filesystem::create_directories("./"+scenename_+"/");
        }


        }




        std::string filename;
        if(current_screenshot_num_ == 0)
            filename = "./"+scenename_+"/__encoded_depth_ground_truth.png";
        else
            filename = "./"+scenename_+"/encoded_depth_"+boost::lexical_cast<std::string>( Renderer::current_screenshot_num_)+"_dp_"
                       +".png";

        // Make the BYTE array, factor of 3 because it's RBG.
        BYTE* pixels = new BYTE[ 4 * win_x_ * win_y_];

        device_->opengl_api().glReadPixels(0, 0, win_x_, win_y_, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
          
        // Convert to FreeImage format & save to file
        FIBITMAP* image = FreeImage_ConvertFromRawBits(pixels, win_x_, win_y_, 4 * win_x_, 32, 0x0000FF, 0xFF0000, 0x00FF00, false);
        FreeImage_Save(FIF_PNG, image, filename.c_str(), 0);

        // Free resources
        FreeImage_Unload(image);
        delete [] pixels;

        std::cout<<"Saved Screenshot: "<<filename.c_str()<<"\n\n";

        ++Renderer::current_screenshot_num_;

    }













}



