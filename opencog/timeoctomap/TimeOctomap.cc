/*
 * Copyright (c) 2016, Mandeep Singh Bhatia, OpenCog Foundation
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the
 * exceptions at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "TimeOctomap.h"
#include "opencog/util/oc_assert.h"

using namespace opencog;

void TimeSlice::insert_atom(const point3d& location, const Handle& ato)
{
    map_tree.updateNode(location, true);
    map_tree.setNodeData(location, ato);
}

void TimeSlice::remove_atom(const Handle& ato)
{
    point3d_list pl;
    for (AtomOcTree::tree_iterator it2 = map_tree.begin_tree(),
                endit2 = map_tree.end_tree();
                it2 != endit2;
                ++it2)
    {
        if (it2->getData() == ato)
        {
            pl.push_back(it2.getCoordinate());
            it2->setData(UndefinedHandle);
        }
    }

    for (auto& p : pl)
        map_tree.deleteNode(p);
}

void TimeSlice::remove_atoms_at_location(const point3d& location)
{
    map_tree.updateNode(location, false);
}

Handle TimeSlice::get_atom_at_location(const point3d& location)
{
    OcTreeNode* result = map_tree.search(location);
    if (result == nullptr) return Handle();

    return (static_cast<AtomOcTreeNode*>(result))->getData();
}

/// get_locations -- get zero, one or more locations (3D coordinates)
/// of an atom in this time-slice.  A time-slice does allow a single
/// atom to be present at multiple locations, and this will return all
/// of them.  This returns an empty list if teh atom does not appear
/// in the timeslice.
point3d_list TimeSlice::get_locations(const Handle& ato)
{
    point3d_list pl;
    for (AtomOcTree::tree_iterator ita =
        map_tree.begin_tree(),
        end = map_tree.end_tree();
        ita != end;
        ++ita)
    {
        if (ita->getData() == ato)
            pl.push_back(ita.getCoordinate());
    }
    return pl;
}


// ================================================================

TimeOctomap::TimeOctomap(unsigned int num_time_units,
                         double map_res_meters,
                         duration_c time_resolution) :
  map_res(map_res_meters),
  time_res(time_resolution),
  time_circle(num_time_units),
  auto_step(false)
{
    curr_time = std::chrono::system_clock::now();

    TimeSlice tu(curr_time, time_res);
    tu.map_tree.setResolution(map_res);
    time_circle.push_back(tu);
}

TimeOctomap::~TimeOctomap()
{
    auto_step_time(false);
}

double TimeOctomap::get_space_resolution()
{
    return map_res;
}

duration_c TimeOctomap::get_time_resolution()
{
    return time_res;
}

void
TimeOctomap::step_time_unit()
{
    std::lock_guard<std::mutex> lgm(mtx);

    curr_time += time_res;
    TimeSlice tu(curr_time, time_res);
    tu.map_tree.setResolution(map_res);
    time_circle.push_back(tu);
}

bool
TimeOctomap::is_auto_step_time_on()
{
  return auto_step;
}

void
TimeOctomap::auto_step_time(bool astep)
{
    std::lock_guard<std::mutex> t_mtx(mtx_auto);
    if (auto_step == astep) return;
    auto_step = astep;
    if (astep) auto_timer();
    else g_thread.join();
}

void
TimeOctomap::auto_timer()
{
    duration_c tr = time_res;

    // In a separate thread, step the circular buffer.
    g_thread = std::thread(
        [tr, this] ()
        {
            while (this->is_auto_step_time_on())
            {
                std::this_thread::sleep_for(tr);
                this->step_time_unit();
            }
        });
}

TimeSlice& TimeOctomap::get_current_timeslice()
{
    // XXX FIXME - can't we just use size() always ???
    int i = time_circle.capacity() - 1;
    if (time_circle.size() < time_circle.capacity())
        i = time_circle.size() - 1;
    return time_circle[i];
}

void TimeOctomap::insert_atom(const point3d& location, const Handle& ato)
{
    std::lock_guard<std::mutex> lgm(mtx);
    TimeSlice& tu = get_current_timeslice();
    tu.insert_atom(location, ato);
}

void TimeOctomap::remove_atoms_at_location(const point3d& location)
{
    std::lock_guard<std::mutex> lgm(mtx);
    TimeSlice& tu = get_current_timeslice();
    tu.remove_atoms_at_location(location);
}

void TimeOctomap::remove_atom_at_time_by_location(time_pt tp,
                                const point3d& location)
{
    std::lock_guard<std::mutex> lgm(mtx);
    auto tu = find(tp);
    if (tu == nullptr) return;
    tu->remove_atoms_at_location(location);
}

Handle TimeOctomap::get_atom_at_location(const point3d& location)
{
    std::lock_guard<std::mutex> lgm(mtx);
    TimeSlice& tu = get_current_timeslice();
    return tu.get_atom_at_location(location);
}

Handle TimeOctomap::get_atom_at_time_by_location(const time_pt& time_p,
                                                 const point3d& location)
{
    std::lock_guard<std::mutex> lgm(mtx);
    auto tu = find(time_p);
    if (tu == nullptr) return Handle();
    return tu->get_atom_at_location(location);
}

time_list
TimeOctomap::get_times_of_atom_occurence_at_location(
                                                 const point3d& location,
                                                 const Handle& ato)
{
    std::lock_guard<std::mutex> lgm(mtx);
    time_list tl;
    for (auto& tu : time_circle)
    {
        Handle ato_t = tu.get_atom_at_location(location);
        if (ato_t != ato) continue;

        tl.push_back(tu.t);
    }
    return tl;
}

/// get_timeline - Get the sequence of points in time at which the
/// atom appears in the map.  There will be one time-point for each
/// time-slice in which the atom appears.
time_list TimeOctomap::get_timeline(const Handle& ato)
{
    std::lock_guard<std::mutex> lgm(mtx);
    time_list tl;
    for (auto& tu : time_circle)
    {
        // Go through all nodes of the octomap, searching for the atom
        // FIXME -- the octomap should provide this as a method.
        // We should not have to search for this ourselves.
        for (auto& nod : tu.map_tree)
            if (nod.getData() == ato)
            {
                tl.push_back(tu.t);
                break;
            }
    }
    return tl;
}

//get the first atom from the elapse from now
//FIXME: check time point within time duration and not just greater or less
bool TimeOctomap::get_oldest_time_elapse_atom_observed(const Handle& ato,
                                            const time_pt& from_d,
                                            time_pt& result)
{
    time_list tl = get_timeline(ato);

    // XXX FIXME when/why would this ever NOT be sorted?
    tl.sort();

    for (auto& tp : tl)
    {
       if (tp >= from_d)
       {
         result = tp;
         return true;
       }
    }
    return false;
}

point3d_list TimeOctomap::get_oldest_locations(const Handle& ato,
                                            const time_pt& from_d)
{
    time_pt tpt;
    if (not get_oldest_time_elapse_atom_observed(ato, from_d, tpt))
        return point3d_list();
    return get_locations_of_atom_at_time(tpt, ato);
}

/// get_last_time_elapse_atom_observed -- get the latest time that
/// the atom was obsserved, as long as it was observed more recently
/// than `from_d`.  XXX FIXME -- this is a kind-of pointless
/// operation to do, as the user is quite capable of doing the math,
/// themselves, to perform the subtraction.  So this method, and the
/// others like it, should be removed from the API.  This will simplify
/// the API a lot. todo -- fixme later.
bool TimeOctomap::get_last_time_elapse_atom_observed(const Handle& ato,
                                            const time_pt& from_d,
                                            time_pt& result)
{
    time_list tl = get_timeline(ato);
    if (0 == tl.size()) return false;

    // XXX FIXME -- when would this ever NOT be sorted??
    tl.sort();
    if (from_d > tl.back() and tl.back() != from_d)
    {
        return false;
    }
    result = tl.back();
    return true;
}

bool TimeOctomap::get_last_time_before_elapse_atom_observed(const Handle& ato,
                                            const time_pt& till_d,
                                            time_pt& result)
{
    time_list tl = get_timeline(ato);
    if (0 == tl.size()) return false;
    tl.sort();

    if (till_d < tl.front()) return false;

    for (auto& tp : tl)
    {
        if (tp <= till_d)
        {
            result = tp;
            return true;
        }
    }
    return false;
}

point3d_list TimeOctomap::get_newest_locations(const Handle& ato,
                                               const time_pt& till_d)
{
  time_pt tpt;
  if (not get_last_time_elapse_atom_observed(ato, till_d, tpt))
     return point3d_list();
  return get_locations_of_atom_at_time(tpt, ato);
}

point3d_list TimeOctomap::get_locations_of_atom(const Handle& ato)
{
    std::lock_guard<std::mutex> lgm(mtx);
    TimeSlice& tu = get_current_timeslice();
    return tu.get_locations(ato);
}

TimeSlice *
TimeOctomap::find(const time_pt& time_p)
{
    for (TimeSlice& tu : time_circle)
        if (tu == time_p) return &tu;
    return nullptr;
}

/// get_locations_of_atom_at_time -- get zero, one or more locations
/// (3D coordinates) of an atom at the given time.  The map does allow
/// a single atom to be present at multiple locations, and this will
/// retreive all of them.  If the atom is not present at this time,
/// an empty list will be reserved.
point3d_list TimeOctomap::get_locations_of_atom_at_time(const time_pt& time_p,
                                                        const Handle& ato)
{
    std::lock_guard<std::mutex> lgm(mtx);
    TimeSlice * it = find(time_p);
    if (it == nullptr) return point3d_list();
    return it->get_locations(ato);
}

void
TimeOctomap::remove_atom_at_current_time(const Handle& ato)
{
    std::lock_guard<std::mutex> lgm(mtx);
    TimeSlice& tu = get_current_timeslice();
    tu.remove_atom(ato);
}

void
TimeOctomap::remove_atom_at_time(const time_pt& time_p,
                                 const Handle& ato)
{
    std::lock_guard<std::mutex> lgm(mtx);
    auto tu = find(time_p);
    if (tu == nullptr) return;
    tu->remove_atom(ato);
}

/// Remove all occurences of atom in all time-slices
void TimeOctomap::remove_atom(const Handle& ato)
{
    std::lock_guard<std::mutex> lgm(mtx);
    for (auto& tu : time_circle) tu.remove_atom(ato);
}

//////spatial relations
//later instead of get a location, use get nearest location or get furthest location
bool
TimeOctomap::get_a_location(const time_pt& time_p,const Handle& ato_target,point3d& location)
{
    //get atom location
    point3d_list target_list = get_locations_of_atom_at_time(time_p, ato_target);
    if (target_list.size() < 1)
        return false;
    location = target_list.front();
    return true;
}

point3d
TimeOctomap::get_spatial_relations(const time_pt& time_p,const Handle& ato_obs,const Handle& ato_target,const Handle& ato_ref)
{
    //reference and observer cant be the same location
    point3d res(-1.0,-1.0,-1.0);
    point3d v1,v2,v3;
    double eps=map_res*0.1;
    if (!get_a_location(time_p,ato_obs,v1))
        return res;
    if (!get_a_location(time_p,ato_target,v2))
        return res;
    if (!get_a_location(time_p,ato_ref,v3))
        return res;
    //calculate res
    //translate obs to origin and relatively move others
    //rotate vector obs target to be on an axis, relatively rotate ref
    //see if on left or right, up or down, front or back
    point3d orv=v3-v1;
    if (abs(orv.x())<=eps && abs(orv.y())<=eps && abs(orv.z())<=eps)
        return res;
    point3d otv=v2-v1;
    double th=atan2(orv.y(),orv.x());
    double cx,cy,dx,dy;
    //rotate around z to zx plane
    rot2d(orv.x(),orv.y(),-1.0*th,cx,cy);
    orv=point3d(cx,0.0,orv.z());
    //rotate around z
    rot2d(otv.x(),otv.y(),-1.0*th,dx,dy);
    otv=point3d(dx,dy,otv.z());
    th=atan2(orv.z(),orv.x());
    //rotate around y to x axis
    rot2d(orv.x(),orv.z(),-1.0*th,cx,cy);
    orv=point3d(cx,0.0,0.0);
    //rotate around y axis
    rot2d(otv.x(),otv.z(),-1.0*th,dx,dy);
    otv=point3d(dx,otv.y(),dy);
    res=otv-orv;

    //x .. ahead=2, behind=1,aligned=0
    //y .. right,left,align
    //z .. above,below,align
    double px,py,pz;
    if (res.x()>eps)
        px=1.0;
    else if (res.x()<-1.0*eps)
        px=2.0;
    else
        px=0.0;

    if (res.y()>eps)
        py=2.0;
    else if (res.y()<-1.0*eps)
        py=1.0;
    else
        py=0.0;

    if (res.z()>eps)
        pz=2.0;
    else if (res.z()<-1.0*eps)
        pz=1.0;
    else
        pz=0.0;
    res=point3d(px,py,pz);
    return res;
}

//not normalized
bool TimeOctomap::get_direction_vector(const time_pt& time_p,
                                       const Handle& ato_obs,
                                       const Handle& ato_target,
                                       point3d& dir)
{
    //direction vector
    point3d tarh;
    point3d refh;
    if (!get_a_location(time_p,ato_target,tarh))
        return false;
    if (!get_a_location(time_p,ato_obs,refh))
        return false;
    dir = (tarh-refh);
    return true;
}

// 2=far,1=near,0=touching, -1 unknown
int TimeOctomap::get_angular_nearness(const time_pt& time_p,
                                      const Handle& ato_obs,
                                      const Handle& ato_target,
                                      const Handle& ato_ref)
{
    point3d dir1,dir2;
    if (not get_direction_vector(time_p, ato_obs, ato_target, dir1))
        return -1;
    if (not get_direction_vector(time_p, ato_obs, ato_ref, dir2))
        return -1;
    double ang = ang_vec(dir1,dir2);
    if (ang <= TOUCH_ANGLE)
        return 0;
    else if (ang <= NEAR_ANGLE)
        return 1;
    return 2;
}

//<-elipson=unknown,>=0 distance
double
TimeOctomap::get_distance_between(const time_pt& time_p,
                                  const Handle& ato_target,
                                  const Handle& ato_ref)
{
    //get atom location
    point3d tarh;
    point3d refh;
    if (!get_a_location(time_p, ato_target, tarh))
        return (-1.0);
    if (!get_a_location(time_p, ato_ref, refh))
        return (-1.0);

    double dist=sqrt(sqr(tarh.x()-refh.x())+sqr(tarh.y()-refh.y())+sqr(tarh.z()-refh.z()));
    return dist;
}

// -----------------------------------------------------------------
//
namespace std {

std::ostream& operator<<(std::ostream& out, const opencog::time_pt& pt)
{
    // XXX FIXME -- make this print milliseconds
    out << std::chrono::system_clock::to_time_t(pt);
    return out;
}

std::ostream& operator<<(std::ostream& out, const opencog::duration_c& du)
{
    out << "foooo duration";
    return out;
}

std::ostream& operator<<(std::ostream& out, const opencog::time_list& tl)
{
    out << "(";
    for (const opencog::time_pt& pt : tl)
        out << std::chrono::system_clock::to_time_t(pt) << " ";
    out << ")";
    return out;
}

std::ostream& operator<<(std::ostream& out, const octomap::point3d_list& pl)
{
    out << "(";
    for (const auto& pt : pl)
        out << pt << " ";
    out << ")";
    return out;
}
}
