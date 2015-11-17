// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/ren/cut.h>


namespace lamure
{

namespace ren
{

Cut::
Cut()
: context_id_(invalid_context_t),
  view_id_(invalid_view_t),
  model_id_(invalid_model_t) {

}

Cut::
Cut(const context_t context_id, const view_t view_id, const model_t model_id)
: context_id_(context_id),
  view_id_(view_id),
  model_id_(model_id) {

}

Cut::
~Cut() {

}


} // namespace ren

} // namespace lamure


