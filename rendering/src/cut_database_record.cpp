// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/ren/cut_database_record.h>


namespace lamure
{

namespace ren
{

CutDatabaseRecord::
CutDatabaseRecord(const context_t context_id)
    : context_id_(context_id),
    is_swap_required_(false),
    current_front_(RecordFront::FRONT_A),
    front_a_is_modified_(false),
    front_b_is_modified_(false),
    front_a_buffer_(TemporaryBuffer::BUFFER_A),
    front_b_buffer_(TemporaryBuffer::BUFFER_A) {

}

CutDatabaseRecord::
~CutDatabaseRecord() {

}

void CutDatabaseRecord::
SwapFront() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_swap_required_) {
        is_swap_required_ = false;

        if (current_front_ == RecordFront::FRONT_A) {
            current_front_ = RecordFront::FRONT_B;
            front_b_rendered_.clear();
        }
        else {
            current_front_ = RecordFront::FRONT_A;
            front_a_rendered_.clear();
        }
    }
}

void CutDatabaseRecord::
SetCamera(const view_t view_id, const Camera& camera) {
#ifdef LAMURE_DATABASE_SAFE_MODE
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    if (current_front_ == RecordFront::FRONT_A) {
        ExpandFrontA(view_id, 0);

        auto it = front_a_cameras_.find(view_id);

        if (it != front_a_cameras_.end()) {
            front_a_cameras_.erase(it);
        }

        front_a_cameras_.insert(std::make_pair(view_id, camera));

    }
    else {
        ExpandFrontB(view_id, 0);

        auto it = front_b_cameras_.find(view_id);

        if (it != front_b_cameras_.end()) {
            front_b_cameras_.erase(it);
        }

        front_b_cameras_.insert(std::make_pair(view_id, camera));

    }

}

void CutDatabaseRecord::
SetHeightDividedByTopMinusBottom(const view_t view_id, float const height_divided_by_top_minus_bottom) {
#ifdef LAMURE_DATABASE_SAFE_MODE
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    if (current_front_ == RecordFront::FRONT_A) {
        ExpandFrontA(view_id, 0);

        auto it = front_a_height_divided_by_top_minus_bottom_.find(view_id);

        if (it != front_a_height_divided_by_top_minus_bottom_.end()) {
            front_a_height_divided_by_top_minus_bottom_.erase(it);
        }

        front_a_height_divided_by_top_minus_bottom_.insert(std::make_pair(view_id, height_divided_by_top_minus_bottom));

    }
    else {
        ExpandFrontB(view_id, 0);

        auto it = front_b_height_divided_by_top_minus_bottom_.find(view_id);

        if (it != front_b_height_divided_by_top_minus_bottom_.end()) {
            front_b_height_divided_by_top_minus_bottom_.erase(it);
        }

        front_b_height_divided_by_top_minus_bottom_.insert(std::make_pair(view_id, height_divided_by_top_minus_bottom));

    }

}

void CutDatabaseRecord::
SetTransform(const view_t model_id, const scm::math::mat4f& transform) {
#ifdef LAMURE_DATABASE_SAFE_MODE
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    if (current_front_ == RecordFront::FRONT_A) {
        ExpandFrontA(0, model_id);

        auto it = front_a_transforms_.find(model_id);

        if (it != front_a_transforms_.end()) {
            front_a_transforms_.erase(it);
        }

        front_a_transforms_.insert(std::make_pair(model_id, transform));

    }
    else {
        ExpandFrontB(0, model_id);

        auto it = front_b_transforms_.find(model_id);

        if (it != front_b_transforms_.end()) {
            front_b_transforms_.erase(it);
        }

        front_b_transforms_.insert(std::make_pair(model_id, transform));

    }


}

void CutDatabaseRecord::
SetRendered(const model_t model_id) {
#ifdef LAMURE_DATABASE_SAFE_MODE
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    if (current_front_ == RecordFront::FRONT_A) {
        ExpandFrontA(0, model_id);

        auto it = front_a_rendered_.find(model_id);

        if (it != front_a_rendered_.end()) {
            front_a_rendered_.erase(it);
        }

        front_a_rendered_.insert(model_id);

    }
    else {
        ExpandFrontB(0, model_id);

        auto it = front_b_rendered_.find(model_id);

        if (it != front_b_rendered_.end()) {
            front_b_rendered_.erase(it);
        }

        front_b_rendered_.insert(model_id);

    }

}

void CutDatabaseRecord::
SetThreshold(const model_t model_id, const float threshold) {
#ifdef LAMURE_DATABASE_SAFE_MODE
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    if (current_front_ == RecordFront::FRONT_A) {
        ExpandFrontA(0, model_id);

        auto it = front_a_thresholds_.find(model_id);

        if (it != front_a_thresholds_.end()) {
            front_a_thresholds_.erase(it);
        }

        front_a_thresholds_.insert(std::make_pair(model_id, threshold));

    }
    else {
        ExpandFrontB(0, model_id);

        auto it = front_b_thresholds_.find(model_id);

        if (it != front_b_thresholds_.end()) {
            front_b_thresholds_.erase(it);
        }

        front_b_thresholds_.insert(std::make_pair(model_id, threshold));

    }

}



void CutDatabaseRecord::
ReceiveTransforms(std::map<model_t, scm::math::mat4f>& transforms) {
    //transforms.clear();

    std::lock_guard<std::mutex> lock(mutex_);

    if (current_front_ == RecordFront::FRONT_A) {
        for (const auto& trans_it : front_b_transforms_) {
            scm::math::mat4f transform = trans_it.second;
            model_t model_id = trans_it.first;
            transforms[model_id] = transform;
        }
    }
    else {
        for (const auto& trans_it : front_a_transforms_) {
            scm::math::mat4f transform = trans_it.second;
            model_t model_id = trans_it.first;
            transforms[model_id] = transform;
        }
    }
}

void CutDatabaseRecord::
ReceiveCameras(std::map<view_t, Camera>& cameras) {
    //cameras.clear();

    std::lock_guard<std::mutex> lock(mutex_);

    if (current_front_ == RecordFront::FRONT_A) {
        for (const auto& cam_it : front_b_cameras_) {
            Camera camera = cam_it.second;
            view_t view_id = cam_it.first;
            cameras[view_id] = camera;
        }
    }
    else {
        for (const auto& cam_it : front_a_cameras_) {
            Camera camera = cam_it.second;
            view_t view_id = cam_it.first;
            cameras[view_id] = camera;
        }
    }
}

void CutDatabaseRecord::
ReceiveHeightDividedByTopMinusBottoms(std::map<view_t, float>& height_divided_by_top_minus_bottoms) {
    //height_divided_by_top_minus_bottoms.clear();

    std::lock_guard<std::mutex> lock(mutex_);

    if (current_front_ == RecordFront::FRONT_A) {
        for (const auto& hdtmb_it : front_b_height_divided_by_top_minus_bottom_) {

            float hdtmb = hdtmb_it.second;
            view_t view_id = hdtmb_it.first;
            height_divided_by_top_minus_bottoms[view_id] = hdtmb;
        }
    }
    else {
        for (const auto& hdtmb_it : front_a_height_divided_by_top_minus_bottom_) {
            float hdtmb = hdtmb_it.second;
            view_t view_id = hdtmb_it.first;
            height_divided_by_top_minus_bottoms[view_id] = hdtmb;
        }
    }
}


void CutDatabaseRecord::
ReceiveRendered(std::set<model_t>& rendered) {
    //rendered.clear();

    std::lock_guard<std::mutex> lock(mutex_);

    if (current_front_ == RecordFront::FRONT_A) {
        rendered = front_b_rendered_;
    }
    else {
        rendered = front_a_rendered_;
    }

}

void CutDatabaseRecord::
ReceiveThresholds(std::map<model_t, float>& thresholds) {
    //thresholds.clear();

    std::lock_guard<std::mutex> lock(mutex_);

    if (current_front_ == RecordFront::FRONT_A) {
        for (const auto& threshold_it : front_b_thresholds_) {
            float threshold = threshold_it.second;
            view_t model_id = threshold_it.first;
            thresholds[model_id] = threshold;
        }

    }
    else {
        for (const auto& threshold_it : front_a_thresholds_) {
            float threshold = threshold_it.second;
            view_t model_id = threshold_it.first;
            thresholds[model_id] = threshold;
        }
    }

}


void CutDatabaseRecord::
ExpandFrontA(const view_t view_id, const model_t model_id) {
    while (model_id >= front_a_cuts_.size())
    {
        //expand cut front B
        front_a_cuts_.push_back(std::vector<Cut>());
        front_a_transforms_.insert(std::make_pair(model_id, scm::math::mat4f::identity()));
        front_a_thresholds_.insert(std::make_pair(model_id, LAMURE_DEFAULT_THRESHOLD));
    }

    view_t new_view_id = front_a_cuts_[model_id].size();

    while (view_id >= front_a_cuts_[model_id].size())
    {
        front_a_cuts_[model_id].push_back(Cut(context_id_, new_view_id, model_id));
        front_a_height_divided_by_top_minus_bottom_.insert(std::make_pair(new_view_id, 1000.0f));
        ++new_view_id;
    }

    assert(model_id < front_a_cuts_.size());
    assert(view_id < front_a_cuts_[model_id].size());

}


void CutDatabaseRecord::
ExpandFrontB(const view_t view_id, const model_t model_id) {
    while (model_id >= front_b_cuts_.size())
    {
        //expand cut front B
        front_b_cuts_.push_back(std::vector<Cut>());
        front_b_transforms_.insert(std::make_pair(model_id, scm::math::mat4f::identity()));
        front_b_thresholds_.insert(std::make_pair(model_id, LAMURE_DEFAULT_THRESHOLD));
    }

    view_t new_view_id = front_b_cuts_[model_id].size();

    while (view_id >= front_b_cuts_[model_id].size())
    {
        front_b_cuts_[model_id].push_back(Cut(context_id_, new_view_id, model_id));
        front_b_height_divided_by_top_minus_bottom_.insert(std::make_pair(new_view_id, 1000.0f));
        ++new_view_id;
    }

    assert(model_id < front_b_cuts_.size());
    assert(view_id < front_b_cuts_[model_id].size());


}

Cut& CutDatabaseRecord::
GetCut(const view_t view_id, const model_t model_id) {
    if (current_front_ == RecordFront::FRONT_A) {
        ExpandFrontA(view_id, model_id);
        return front_a_cuts_[model_id][view_id];
    }
    else {
        ExpandFrontB(view_id, model_id);
        return front_b_cuts_[model_id][view_id];
    }

}

void CutDatabaseRecord::
SetCut(const view_t view_id, const model_t model_id, Cut& cut) {
    is_swap_required_ = true;

    if (current_front_ == RecordFront::FRONT_A) {
        ExpandFrontB(view_id, model_id);
        front_b_cuts_[model_id][view_id] = cut;
    }
    else {
        ExpandFrontA(view_id, model_id);
        front_a_cuts_[model_id][view_id] = cut;
    }

}

std::vector<CutDatabaseRecord::SlotUpdateDescr>& CutDatabaseRecord::
GetUpdatedSet() {
    if (current_front_ == RecordFront::FRONT_A) {
        return front_a_updated_set_;
    }
    else {
        return front_b_updated_set_;
    }

}

void CutDatabaseRecord::
SetUpdatedSet(std::vector<CutDatabaseRecord::SlotUpdateDescr>& updated_set) {
    is_swap_required_ = true;

    if (current_front_ == RecordFront::FRONT_A) {
        front_b_updated_set_ = updated_set;
    }
    else {
        front_a_updated_set_ = updated_set;
    }

}

const bool CutDatabaseRecord::
IsFrontModified() const {
    if (current_front_ == RecordFront::FRONT_A) {
        return front_a_is_modified_;
    }
    else {
        return front_b_is_modified_;
    }

};

void CutDatabaseRecord::
SetIsFrontModified(const bool front_modified) {
    is_swap_required_ = true;

    if (current_front_ == RecordFront::FRONT_A) {
        front_b_is_modified_ = front_modified;
    }
    else {
        front_a_is_modified_ = front_modified;
    }
};

void CutDatabaseRecord::
SignalUploadComplete() {
    if (current_front_ == RecordFront::FRONT_A) {
        front_a_is_modified_ = false;
    }
    else {
        front_b_is_modified_ = false;
    }
}

const bool CutDatabaseRecord::
IsSwapRequired() const {
    return is_swap_required_;
};

void CutDatabaseRecord::
SetIsSwapRequired(const bool swap_required) {
    is_swap_required_ = true;
};

const CutDatabaseRecord::TemporaryBuffer CutDatabaseRecord::
GetBuffer() const {
    if (current_front_ == RecordFront::FRONT_A) {
        return front_a_buffer_;
    }
    else {
        return front_b_buffer_;
    }

};

void CutDatabaseRecord::
SetBuffer(const CutDatabaseRecord::TemporaryBuffer buffer) {
    is_swap_required_ = true;

    if (current_front_ == RecordFront::FRONT_A) {
        front_b_buffer_ = buffer;
    }
    else {
        front_a_buffer_ = buffer;
    }
};

void CutDatabaseRecord::
LockFront() {
    mutex_.lock();
}

void CutDatabaseRecord::
UnlockFront() {
    mutex_.unlock();
}


} // namespace ren

} // namespace lamure




