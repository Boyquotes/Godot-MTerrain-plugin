#include "mocttree.h"


#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>


#include "mtool.h"
#include "octmesh/moctmesh.h"


//MOctTree::Octant* MOctTree::Octant::octs141 = nullptr;

void MOctTree::_bind_methods(){
	ADD_SIGNAL(MethodInfo("update_finished"));

	ClassDB::bind_method(D_METHOD("get_oct_id"), &MOctTree::get_oct_id);
	ClassDB::bind_method(D_METHOD("clear_oct_id"), &MOctTree::clear_oct_id);
	ClassDB::bind_method(D_METHOD("remove_oct_id"), &MOctTree::remove_oct_id);
	
	ClassDB::bind_method(D_METHOD("point_process_finished"), &MOctTree::point_process_finished);
	ClassDB::bind_method(D_METHOD("get_point_update_dictionary_array","oct_id"), &MOctTree::get_point_update_dictionary_array);
	ClassDB::bind_method(D_METHOD("insert_points","points","ids","oct_id"), &MOctTree::insert_points);
	ClassDB::bind_method(D_METHOD("get_ids","search_bound"), &MOctTree::get_ids);
	ClassDB::bind_method(D_METHOD("get_ids_exclude","search_bound","exclude_bound"), &MOctTree::get_ids_exclude);
	ClassDB::bind_method(D_METHOD("set_camera_node"), &MOctTree::set_camera_node);


	ClassDB::bind_method(D_METHOD("get_points_count"), &MOctTree::get_points_count);
	ClassDB::bind_method(D_METHOD("get_oct_id_points_count","oct_id"), &MOctTree::get_oct_id_points_count);
	ClassDB::bind_method(D_METHOD("set_lod_setting","lod"), &MOctTree::set_lod_setting);

	ClassDB::bind_method(D_METHOD("set_custom_capacity","input"), &MOctTree::set_custom_capacity);

	ClassDB::bind_method(D_METHOD("set_is_octmesh_updater","input"), &MOctTree::set_is_octmesh_updater);
	ClassDB::bind_method(D_METHOD("get_is_octmesh_updater"), &MOctTree::get_is_octmesh_updater);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL,"is_octmesh_updater"),"set_is_octmesh_updater","get_is_octmesh_updater");
}

MOctTree::PointMoveReq::PointMoveReq(){
	
}

MOctTree::PointMoveReq::PointMoveReq(int32_t _p_id,uint16_t _oct_id,Vector3 _old_pos,Vector3 _new_pos):
p_id(_p_id),oct_id(_oct_id),old_pos(_old_pos),new_pos(_new_pos)
{

}

uint64_t MOctTree::PointMoveReq::hash() const{
	uint64_t uid = (uint64_t)((uint32_t)p_id) << 16;
	return uid | (uint64_t)oct_id;
}

bool MOctTree::PointMoveReq::operator<(const PointMoveReq& other) const{
	return hash() < other.hash();
}


MOctTree::Octant::Octant(){
}

MOctTree::Octant::~Octant(){
	if(octs){
		memdelete_arr(octs);
	}
}

bool MOctTree::Octant::insert_point(const Vector3& p_point,int32_t p_id, uint16_t oct_id, const uint16_t capacity){
	if(!has_point(p_point)){
		return false;
	}
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			if(octs[i].insert_point(p_point,p_id,oct_id,capacity)){
				return true;
			}
		}
		ERR_FAIL_V_MSG(false,"Arrive end without consuming point!");
	}
	if(points.size()>capacity){
		divide();
		// Clearing our points and putting them inside child
		for(uint32_t j=0; j < points.size(); j++){
			bool consume = false;
			for(uint8_t i=0; i<8; i++){
				if(octs[i].insert_point(points[j],capacity)){
					consume = true;
					break;
				}
			}
			if(!consume){
				WARN_PRINT("Child Octant don't consume point"); // Only for now for debug
			}
		}
		points.clear();
		//Adding current point
		for(uint8_t i=0; i<8; i++){
			if(octs[i].insert_point(p_point,p_id,oct_id,capacity)){
				return true;
			}
		}
		ERR_FAIL_V_MSG(false,"Arrive end without consuming point");
	}
	points.push_back(OctPoint(p_id,p_point,oct_id));
	return true;
}

bool MOctTree::Octant::insert_point(const OctPoint& p_point, const uint16_t capacity){
	if(!has_point(p_point.position)){
		return false;
	}
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			if(octs[i].insert_point(p_point,capacity)){
				return true;
			}
		}
		ERR_FAIL_V_MSG(false,"Arrive end without consuming point!");
	}
	if(points.size()>capacity){
		divide();
		// Clearing our points and putting them inside child
		for(uint32_t j=0; j < points.size(); j++){
			bool consume = false;
			for(uint8_t i=0; i<8; i++){
				if(octs[i].insert_point(points[j],capacity)){
					consume = true;
					break;
				}
			}
			if(!consume){
				WARN_PRINT("Child Octant don't consume point"); // Only for now for debug
			}
		}
		points.clear();
		//Adding current point
		for(uint8_t i=0; i<8; i++){
			if(octs[i].insert_point(p_point,capacity)){
				return true;
			}
		}
		ERR_FAIL_V_MSG(false,"Arrive end without consuming point");
	}
	points.push_back(p_point);
	return true;
}

MOctTree::OctPoint* MOctTree::Octant::insert_point_ret_octpoint(const OctPoint& p_point, const uint16_t capacity){
	if(!has_point(p_point.position)){
		return nullptr;
	}
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			OctPoint* res = octs[i].insert_point_ret_octpoint(p_point,capacity);
			if(res!=nullptr){
				return res;
			}
		}
		ERR_FAIL_V_MSG(nullptr,"Arrive end without consuming point!");
	}
	if(points.size()>capacity){
		divide();
		// Clearing our points and putting them inside child
		for(uint32_t j=0; j < points.size(); j++){
			//bool consume = false;
			for(uint8_t i=0; i<8; i++){
				if(octs[i].insert_point(points[j],capacity)){
					//consume = true;
					break;
				}
			}
			//if(!consume){
			//	WARN_PRINT("Child Octant don't consume point"); // Only for now for debug
			//}
		}
		points.clear();
		//Adding current point
		for(uint8_t i=0; i<8; i++){
			OctPoint* res = octs[i].insert_point_ret_octpoint(p_point,capacity);
			if(res!=nullptr){
				return res;
			}
		}
		ERR_FAIL_V_MSG(nullptr,"Arrive end without consuming point");
	}
	points.push_back(p_point);
	return points.ptrw() + points.size() - 1;
}


MOctTree::Octant* MOctTree::Octant::find_octant_by_point(const int32_t id,const uint16_t oct_id,Vector3 pos,int& point_index){
	if(!has_point(pos)){
		return nullptr;
	}
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			MOctTree::Octant* res = octs[i].find_octant_by_point(id,oct_id,pos,point_index);
			if(res != nullptr){
				return res;
			}
		}
		return nullptr;
	}
	for(int i=0;i<points.size();i++){
		OctPoint p = points[i];
		if( p.id==id && p.oct_id==oct_id){
			point_index = i;
			return this;
		}
	}
	return nullptr;
}

MOctTree::Octant* MOctTree::Octant::find_octant_by_point_classic(const int32_t id,const uint16_t oct_id,int& point_index){
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			MOctTree::Octant* res = octs[i].find_octant_by_point_classic(id,oct_id,point_index);
			if(res != nullptr){
				return res;
			}
		}
		return nullptr;
	}
	for(int i=0;i<points.size();i++){
		OctPoint p = points[i];
		if( p.id==id && p.oct_id==oct_id){
			point_index = i;
			return this;
		}
	}
	return nullptr;
}

void MOctTree::Octant::divide(){
	octs = memnew_arr(Octant, 8);
	//Vector3 half_size;
	Vector3 middle = start + (end - start)/2;
	// First Level
	octs[0].start = start;
	octs[1].start = Vector3(middle.x,start.y,start.z);
	octs[2].start = Vector3(start.x,start.y,middle.z);
	octs[3].start = Vector3(middle.x,start.y,middle.z);
	// Second Level
	octs[4].start = Vector3(start.x,middle.y,start.z);
	octs[5].start = Vector3(middle.x,middle.y,start.z);
	octs[6].start = Vector3(start.x,middle.y,middle.z);
	octs[7].start = middle;
	// Seting size
	octs[0].end = middle;
	octs[1].end = Vector3(end.x,middle.y,middle.z);
	octs[2].end = Vector3(middle.x,middle.y,end.z);
	octs[3].end = Vector3(end.x,middle.y,end.z);
	// Second Level
	octs[4].end = Vector3(middle.x,end.y,middle.z);
	octs[5].end = Vector3(end.x,end.y,middle.z);
	octs[6].end = Vector3(middle.x,end.y,end.z);
	octs[7].end = end;

}

bool MOctTree::Octant::intersects(const Pair<Vector3,Vector3>& bound) const {
	if (start.x >= bound.second.x) {
		return false;
	}
	if (end.x <= bound.first.x) {
		return false;
	}
	if (start.y >= bound.second.y) {
		return false;
	}
	if (end.y <= bound.first.y) {
		return false;
	}
	if (start.z >= bound.second.z) {
		return false;
	}
	if (end.z <= bound.first.z) {
		return false;
	}
	return true;
}

_FORCE_INLINE_ bool MOctTree::Octant::encloses_by(const Pair<Vector3,Vector3>& bound) const{
	return has_point(bound,start) && has_point(bound,end);
	/*
	return (
			(bound.first.x <= start.x) &&
			(bound.second.x > end.x) &&
			(bound.first.y <= start.y) &&
			(bound.second.y > end.y) &&
			(bound.first.z <= start.z) &&
			(bound.second.z > end.z));
	*/
}

_FORCE_INLINE_ bool MOctTree::Octant::encloses_between(const Pair<Vector3,Vector3>& include, const Pair<Vector3,Vector3>& exclude) const{
	return has_point(include,start) && has_point(include,end) && !has_point(exclude,start) && !has_point(exclude,end);
}

bool MOctTree::Octant::has_point(const Pair<Vector3,Vector3>& bound, const Vector3& point) {
	if (point.x <= bound.first.x) {
		return false;
	}
	if (point.y <= bound.first.y) {
		return false;
	}
	if (point.z <= bound.first.z) {
		return false;
	}
	if (point.x > bound.second.x) {
		return false;
	}
	if (point.y > bound.second.y) {
		return false;
	}
	if (point.z > bound.second.z) {
		return false;
	}

	return true;
}

bool MOctTree::Octant::has_point(const Vector3& point) const{
	if (point.x <= start.x) {
		return false;
	}
	if (point.y <= start.y) {
		return false;
	}
	if (point.z <= start.z) {
		return false;
	}
	if (point.x > end.x) {
		return false;
	}
	if (point.y > end.y) {
		return false;
	}
	if (point.z > end.z) {
		return false;
	}

	return true;
}

void MOctTree::Octant::get_ids(const Pair<Vector3,Vector3>& bound, PackedInt32Array& _ids){
	if(!octs){
		if(encloses_by(bound)){
			for(uint32_t i=0; i < points.size(); i++){
				_ids.push_back(points[i].id);
			}
			return;
		}
	}
	if(intersects(bound)){
		if(octs){
			for(uint8_t i=0; i < 8; i++){
				octs[i].get_ids(bound, _ids);
			}
		} else {
			for(uint32_t i=0; i < points.size(); i++){
				if(has_point(bound,points[i].position)){
					_ids.push_back(points[i].id);
				}
			}
		}
	}
	// If not intersect do nothing
}

void MOctTree::Octant::update_lod_zero(const OctUpdateInfo& update_info,HashMap<uint16_t,Vector<PointUpdate>>& u_info){
	if(intersects(update_info.bound)){
		if(octs){
			for(uint8_t i=0; i < 8; i++){
				octs[i].update_lod_zero(update_info, u_info);
			}
		} else {
			for(uint32_t i=0; i < points.size(); i++){
				OctPoint* p = points.ptrw() + i;
				if(has_point(update_info.bound,p->position)){
					p->update_id = update_info.update_id; //This means this object is take care in this update
					//Check if lod of p is changed
					//If yes then this point is changed
					//Otherwise nothing to do
					if(p->lod != update_info.lod){
						PointUpdate pupdate;
						pupdate.last_lod = p->lod;
						pupdate.lod =update_info.lod;
						pupdate.id = p->id;

						u_info[p->oct_id].push_back(pupdate);
						p->lod = update_info.lod;
					}
				}
			}
		}
	}
}

void MOctTree::Octant::update_lod(const OctUpdateInfo& update_info,HashMap<uint16_t,Vector<PointUpdate>>& u_info){
	if(intersects(update_info.bound) && !encloses_by(update_info.exclude_bound)){
		if(octs){
			for(uint8_t i=0; i < 8; i++){
				octs[i].update_lod(update_info, u_info);
			}
		} else {
			for(uint32_t i=0; i < points.size(); i++){
				OctPoint* p = points.ptrw() + i;
				if(p->update_id != update_info.update_id && has_point(update_info.bound,p->position)){
					p->update_id = update_info.update_id; //This means this object is take care in this update
					//Check if lod of p is changed
					//If yes then this point is changed
					//Otherwise nothing to do
					if(p->lod != update_info.lod){
						PointUpdate pupdate;
						pupdate.last_lod = p->lod;
						pupdate.lod =update_info.lod;
						pupdate.id = p->id;

						u_info[p->oct_id].append(pupdate);

						p->lod = update_info.lod;
					}
				}
			}
		}
	}
}

void MOctTree::Octant::get_ids_exclude(const Pair<Vector3,Vector3>& bound,const Pair<Vector3,Vector3>& exclude_bound, PackedInt32Array& _ids){
	if(!octs){
		if(encloses_between(bound,exclude_bound)){
			for(uint32_t i=0; i < points.size(); i++){
				_ids.push_back(points[i].id);
			}
			return;
		}
	}
	if(intersects(bound) && !encloses_by(exclude_bound)){
		if(octs){
			for(uint8_t i=0; i < 8; i++){
				octs[i].get_ids_exclude(bound,exclude_bound, _ids);
			}
		} else {
			for(uint32_t i=0; i < points.size(); i++){
				//_ids.push_back(points[i].id);
				if(has_point(bound,points[i].position) && !has_point(exclude_bound,points[i].position)){
					_ids.push_back(points[i].id);
				}
			}
		}
	}
}

void MOctTree::Octant::get_all_bounds(Vector<Pair<Vector3,Vector3>>& bounds){
	bounds.push_back({start,end});
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			octs[i].get_all_bounds(bounds);
		}
	}
}

void MOctTree::Octant::get_all_data(Vector<OctPoint>& data){
	data.append_array(points);
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			octs[i].get_all_data(data);
		}
	}
}

void MOctTree::Octant::append_all_ids_to(PackedInt32Array& _ids){
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			octs[i].append_all_ids_to(_ids);
		}
		return;
	}
	for(int32_t i=0; i < points.size(); i++){
		_ids.push_back(points[i].id);
	}
}

void MOctTree::Octant::get_points_count(int& count){
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			octs[i].get_points_count(count);
		}
		return;
	}
	count += points.size();
}
void MOctTree::Octant::get_oct_id_points_count(uint16_t oct_id,int& count){
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			octs[i].get_oct_id_points_count(oct_id,count);
		}
		return;
	}
	for(int32_t i=0; i < points.size(); i++){
		if(points[i].oct_id == oct_id){
			count++;
		}
	}
}

bool MOctTree::Octant::check_id_exist_classic(int32_t id){
	bool found = false;
	if(octs!=nullptr){
		for(uint8_t i=0; i < 8; i++){
			if(octs[i].check_id_exist_classic(id)){
				return true;
			}
		}
		return false;
	}
	for(OctPoint p : points){
		if(p.id == id){
			return true;
		}
	}
	return false;
}

void MOctTree::Octant::get_point(int32_t id, Vector3 pos,Pair<Octant*,int>& pinfo){

}

bool MOctTree::Octant::remove_point(int32_t id,Vector3& pos,uint16_t oct_id){
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			if(octs[i].remove_point(id,pos,oct_id)){
				return true;
			}
		}
		return false;
	}
	for(int32_t i=0; i < points.size(); i++){
		if(points[i].id==id && points[i].oct_id==oct_id){
			points.remove_at(i);
			return true;
		}
	}
	return false;
}

void MOctTree::Octant::clear(){
	if(octs){
		memdelete_arr(octs);
		octs = nullptr;
	}
	points.clear();
}

void MOctTree::Octant::remove_points_with_oct_id(uint16_t oct_id){
	if(octs){
		for(uint8_t i=0; i < 8; i++){
			octs[i].remove_points_with_oct_id(oct_id);
		}
		return;
	}
	for(int32_t i=points.size() - 1; i >= 0; i--){
		if(points[i].oct_id == oct_id){
			points.remove_at(i);
		}
	}
}

///////////////////////////////////////////////
//// Main MOctTree ////////////////////////////
///////////////////////////////////////////////

MOctTree::MOctTree(){
	lod_setting.push_back(100);
	lod_setting.push_back(200);
	lod_setting.push_back(600);
	lod_setting.push_back(1000);

	set_process(true);
}

MOctTree::~MOctTree(){
	if(is_updating){
		WorkerThreadPool::get_singleton()->wait_for_task_completion(tid);
	}
}

int MOctTree::get_oct_id(){
	std::lock_guard<std::mutex> lock(oct_mutex);
	last_oct_id++;
	int new_oct_id = last_oct_id;
	oct_ids.insert(new_oct_id);
	update_change_info.insert(new_oct_id,Vector<PointUpdate>());
	return new_oct_id;
}

void MOctTree::clear_oct_id(int oct_id){
	ERR_FAIL_COND(!oct_ids.has(oct_id));
	std::lock_guard<std::mutex> lock(oct_mutex);
	root.remove_points_with_oct_id(oct_id);
}

void MOctTree::remove_oct_id(int oct_id){
	ERR_FAIL_COND(!oct_ids.has(oct_id));
	std::lock_guard<std::mutex> lock(oct_mutex);
	if(disable_octtree){
		return;
	}
	if(waiting_oct_ids.has(oct_id)){
		waiting_oct_ids.erase(oct_id);
	}
	oct_ids.erase(oct_id);
	root.remove_points_with_oct_id(oct_id);
	check_point_process_finished();
}

bool MOctTree::remove_point(int32_t id,Vector3& pos,uint16_t oct_id){
	std::lock_guard<std::mutex> lock(oct_mutex);
	if(disable_octtree){
		return true;
	}
	bool res = root.remove_point(id,pos,oct_id);
	if(unlikely(!res)){
		WARN_PRINT("Can not find point with ID "+itos(id)+" OCT_ID "+itos(oct_id)+" to remove!");
	} else {
		point_count--;
	}
	return res;
}

void MOctTree::set_camera_node(Node3D* camera){
	camera_node = camera;
}

void MOctTree::update_camera_position(){
	if(camera_node && UtilityFunctions::is_instance_valid(camera_node) && camera_node->is_inside_tree()){
		camera_position = camera_node->get_global_position();
		return;
	}
	if(Engine::get_singleton()->is_editor_hint()){
		Node3D* cam = MTool::find_editor_camera(true);
		if(cam){
			camera_position = cam->get_global_position();
		} else if(!is_camera_warn_print) {
			is_camera_warn_print = true;
			WARN_PRINT("Can not find editor camera you can set that manually by set_camera_node");
		}
		return;
	}
	Viewport* viewport = get_viewport();
	if(!viewport && !is_camera_warn_print){
		if(!is_camera_warn_print){
			WARN_PRINT("Can not find viewport, You can set that manually by set_camera_node");
			is_camera_warn_print = true;
		}
		return;
	}
	Camera3D* cam = viewport->get_camera_3d();
	if(!cam){
		if(!is_camera_warn_print){
			WARN_PRINT("Can not find camera, You can set that manually by set_camera_node");
			is_camera_warn_print = true;
		}
		return;
	}
	camera_position = cam->get_global_position();
}

uint32_t MOctTree::get_capacity(int p_count){
	uint32_t fcapacity;
	if(custom_capacity==0){
		fcapacity = pow(point_count, 0.365)/2;
		fcapacity = CLAMP(fcapacity, 5, MAX_CAPACITY);
	} else {
		fcapacity = custom_capacity;
	}
	return fcapacity;
}

void MOctTree::insert_points(const PackedVector3Array& points,const PackedInt32Array ids, int oct_id){
	if(points.size()==0 || disable_octtree){
		return;
	}
	std::lock_guard<std::mutex> lock(oct_mutex);
	update_lod_include_root_bound = true;
	ERR_FAIL_COND(!oct_ids.has(oct_id));
	ERR_FAIL_COND(points.size() != ids.size());
	//In case we create oct tree before and new points
	//Will not inside the boundary of root octant
	//we must recreate entire oct tree
	Vector<OctPoint> rpoints; //if rpoints.size() != 0 then we have recreate
	if(point_count!=0){
		for(int i=0; i < points.size(); i++){
			if(!root.has_point(points[i])){
				root.get_all_data(rpoints);
				root.clear();
				point_count=0;
				break;
			}
		}
	}
	//Getting the bound
	if(rpoints.size()!=0 || point_count==0){ // updating bound only with these condition
		Pair<Vector3,Vector3> bound;
		bound.first = points[0];
		bound.second = points[0];
		for(int i=1; i < points.size(); i++){
			Vector3 p = points[i];
			bound.first.x = std::min(bound.first.x, p.x);
			bound.first.y = std::min(bound.first.y, p.y);
			bound.first.z = std::min(bound.first.z, p.z);

			bound.second.x = std::max(bound.second.x, p.x);
			bound.second.y = std::max(bound.second.y, p.y);
			bound.second.z = std::max(bound.second.z, p.z);
		}
		if(rpoints.size()!=0){ // considering last bound
			bound.first.x = std::min(bound.first.x, root.start.x);
			bound.first.y = std::min(bound.first.y, root.start.y);
			bound.first.z = std::min(bound.first.z, root.start.z);

			bound.second.x = std::max(bound.second.x, root.end.x);
			bound.second.y = std::max(bound.second.y, root.end.y);
			bound.second.z = std::max(bound.second.z, root.end.z);	
		}
		root.start = bound.first - Vector3(EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN);
		root.end = bound.second + Vector3(EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN);
	}
	//point count
	if(rpoints.size()==0){ // Is not recreate
		point_count += points.size();
	} else { // Is Recreate
		point_count = points.size() + rpoints.size();
	}
	//capacity
	uint32_t fcapacity = get_capacity(point_count);
	//UtilityFunctions::print("capacity ",fcapacity, " rpoint.size ",rpoints.size()," insert oct_id ",oct_id, " point count ",point_count);
	//inserting
	for(int32_t i=0; i < rpoints.size(); i++) {
		bool res = root.insert_point(rpoints[i],fcapacity);
		if(unlikely(!res)){
			WARN_PRINT("Point "+itos(rpoints[i].id)+" at position "+UtilityFunctions::str(rpoints[i].position)+" Did not inserted!");
		}
	}
	for(int32_t i=0; i < points.size(); i++) {
		bool res = root.insert_point(points[i],ids[i],oct_id,fcapacity);
		if(unlikely(!res)){
			WARN_PRINT("Point "+itos(ids[i])+" at position "+UtilityFunctions::str(points[i])+" Did not inserted!");
		}
	}

	/*
	int no_insert_count = 0;
	for(int32_t id : ids){
		bool res = root.check_id_exist_classic(id);
		if(!res){
			no_insert_count++;
			WARN_PRINT("ID is not inserted "+itos(id));
		}
	}
	if(no_insert_count!=0){
		WARN_PRINT("in total no insert: "+itos(no_insert_count));
	}
	*/
}
// Insert a point and claculate its LOD and return that! in case of error it will return -1
bool MOctTree::insert_point(const Vector3& pos,const int32_t id, int oct_id){
	std::lock_guard<std::mutex> lock(oct_mutex);
	if(disable_octtree){
		return true;
	}
	if(get_pos_lod_classic(pos) >= lod_setting.size()){
		update_lod_include_root_bound = true;
	}
	ERR_FAIL_COND_V(!oct_ids.has(oct_id),INVALID_OCT_POINT_ID);
	//In case we create oct tree before and new points
	//Will not inside the boundary of root octant
	//we must recreate entire oct tree
	Vector<OctPoint> rpoints; //if rpoints.size() != 0 then we have recreate
	if(point_count!=0){
		if(!root.has_point(pos)){
			root.get_all_data(rpoints);
			root.clear();
			root.start.x = std::min(root.start.x, pos.x);
			root.start.y = std::min(root.start.y, pos.y);
			root.start.z = std::min(root.start.z, pos.z);

			root.end.x = std::max(root.end.x, pos.x);
			root.end.y = std::max(root.end.y, pos.y);
			root.end.z = std::max(root.end.z, pos.z);	
		}
	} else {
		root.start = pos - Vector3(EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN);
		root.end = pos + Vector3(EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN);
	}
	point_count++;
	uint32_t fcapacity = get_capacity(point_count);
	for(int32_t i=0; i < rpoints.size(); i++) {
		bool res = root.insert_point(rpoints[i],fcapacity);
		if(unlikely(!res)){
			WARN_PRINT("Point "+itos(rpoints[i].id)+" at position "+UtilityFunctions::str(rpoints[i].position)+" Did not inserted!");
		}
	}

	OctPoint p(id,pos,oct_id);
	bool res = root.insert_point(p,fcapacity);
	ERR_FAIL_COND_V_MSG(!res,res,"Can not re-insert point at move");
	ERR_FAIL_COND_V(get_points_count()!=point_count,res);
	return res;
}

//Must be called only in update_lod
//in case it is not updated updated_lod = -1
void MOctTree::move_point(const PointMoveReq& mp,int8_t updated_lod,uint8_t update_id){
	int point_index = -1;
	Octant* poct = nullptr;
	poct = root.find_octant_by_point(mp.p_id,mp.oct_id,mp.old_pos,point_index);
	if(poct==nullptr){
		poct = root.find_octant_by_point_classic(mp.p_id,mp.oct_id,point_index);
		if(poct!=nullptr){
			WARN_PRINT("Used Classic method to find octant of move point! Maybe you not provide the excat oct_tree position for move");
		}
	}
	ERR_FAIL_COND_MSG(poct==nullptr,"can not find octant of moved point!");
	if(poct->has_point(mp.new_pos)){ // No Octant change just update new_pos
		OctPoint* p = poct->points.ptrw() + point_index;
		p->position = mp.new_pos;
		if(updated_lod!=-1 && updated_lod!=p->lod){
			PointUpdate up;
			up.id = p->id;
			up.last_lod = p->lod;
			up.lod = updated_lod;
			update_change_info[p->oct_id].push_back(up);
			p->lod = updated_lod;
		}
		p->update_id = update_id;
		return;
	}
	OctPoint p = poct->points[point_index];
	poct->points.remove_at(point_index);
	p.position = mp.new_pos;
	Vector<OctPoint> rpoints;
	bool is_reacreate = false;
	if(!root.has_point(p.position)){ // in case this go outside of the root bound we need to re-create the entire octtree
		is_reacreate = true;	
		root.get_all_data(rpoints);
		root.clear();
		root.start.x = std::min(root.start.x, p.position.x);
		root.start.y = std::min(root.start.y, p.position.y);
		root.start.z = std::min(root.start.z, p.position.z);

		root.end.x = std::max(root.end.x, p.position.x);
		root.end.y = std::max(root.end.y, p.position.y);
		root.end.z = std::max(root.end.z, p.position.z);
		root.start = root.start - Vector3(EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN);
		root.end = root.end + Vector3(EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN,EXTRA_BOUND_MARGIN);
	}
	uint32_t fcapacity = get_capacity(point_count);
	for(OctPoint tree_p : rpoints){
		bool _res = root.insert_point(tree_p,fcapacity);
		if(!_res){
			UtilityFunctions::print("Can not consume point.",root.has_point(tree_p.position));
		}
	}
	if(updated_lod!=-1 && updated_lod!=p.lod){
		PointUpdate up;
		up.id = p.id;
		up.last_lod = p.lod;
		up.lod = updated_lod;
		update_change_info[p.oct_id].push_back(up);
		p.lod = updated_lod;
	}
	p.update_id = update_id;
	//UtilityFunctions::print("oct change ",p.position);
	bool res = root.insert_point(p,fcapacity);
}

void MOctTree::add_move_req(const PointMoveReq& mv_data){
	std::lock_guard<std::mutex> lock(move_req_mutex);
	if(disable_octtree){
		return;
	}
	if(moves_req_cache.has(mv_data)){
		auto el = moves_req_cache.find(mv_data);
		el->get().new_pos = mv_data.new_pos;
		auto el2 = moves_req_cache.find(mv_data);
		//UtilityFunctions::print("2old ",el2->get().old_pos," new ",el2->get().new_pos);
	} else {
		moves_req_cache.insert(mv_data);
	}
}

void MOctTree::release_move_req_cache(){
	std::lock_guard<std::mutex> lock(move_req_mutex);
	moves_req = moves_req_cache;
	moves_req_cache.clear();
}

int8_t MOctTree::get_pos_lod_classic(const Vector3& pos){
	real_t dis = camera_position.distance_to(pos);
	int8_t lod = lod_setting.size();
	for(int8_t i=0;i<lod_setting.size();i++){
		if(dis < lod_setting[i]){
			lod = i;
			return lod;
		}
	}
	return lod;
}

PackedInt32Array MOctTree::get_ids(const AABB& search_bound){
	Time* time = Time::get_singleton();
	float t0 = time->get_ticks_usec();
	PackedInt32Array out;
	Pair<Vector3,Vector3> search_b = {search_bound.position, search_bound.position + search_bound.size};
	root.get_ids(std::move(search_b), out);
	float t1 = time->get_ticks_usec();
	UtilityFunctions::print("Fount with OctTree ",out.size(), " DT ",t1-t0);
	return out;
}

PackedInt32Array MOctTree::get_ids_exclude(const AABB& search_bound, const AABB& exclude_bound){
	PackedInt32Array out;
	root.get_ids_exclude({search_bound.position, search_bound.position + search_bound.size},
	                    	{exclude_bound.position, exclude_bound.position + exclude_bound.size},out);
	return out;
}

void MOctTree::update_lod(bool include_root_bound){
	std::lock_guard<std::mutex> lock(oct_mutex);
	ERR_FAIL_COND(lod_setting.size()==0);
	ERR_FAIL_COND(lod_setting[0] < 0.1);
	clear_update_change_info();
	update_id++;
	if(update_id==0){
		update_id=1;
	}
	///////////////////////////////////////////////
	//              Moving points
	///////////////////////////////////////////////
	// Only Max Lod will not get updated further down
	// So move_point with max lod will get updated in move function
	// In case it is not in max_lod_exclude we leave that to be updated down
	// Also in case its not get updated here update id should not be equale to current update ID
	// in case it is not updated it update lod will remain -1
	float half = lod_setting[lod_setting.size()-1]/2.0;
	Pair<Vector3,Vector3> max_lod_exclude;
	max_lod_exclude.first = camera_position - Vector3(half,half,half);
	max_lod_exclude.second = camera_position + Vector3(half,half,half);
	for(PointMoveReq mp : moves_req){
		int8_t _update_lod = -1;
		uint8_t _updated_id = update_id - 1;
		if(!Octant::has_point(max_lod_exclude,mp.new_pos)){
			_update_lod = lod_setting.size();
			_updated_id = update_id;
		}
		move_point(mp,_update_lod,_updated_id);
		
	}
	///////////////////////////////////////////////
	//              updating LOD points
	///////////////////////////////////////////////
	Pair<Vector3,Vector3> last_bound;
	// Updating LOD 0 -> as it does not need exclude region we do that differently here
	{
		float half = lod_setting[0]/2.0;
		last_bound.first = camera_position - Vector3(half,half,half);
		last_bound.second = camera_position + Vector3(half,half,half);
		OctUpdateInfo update_info;
		update_info.bound = last_bound;
		// No exclude bound for LOD0
		update_info.update_id = update_id;
		update_info.lod = 0;
		root.update_lod_zero(update_info, update_change_info);
	}
	
	for(uint16_t i=1; i < lod_setting.size() ; i++){
		OctUpdateInfo update_info;
		update_info.exclude_bound = last_bound;
		float half = lod_setting[i]/2.0;
		update_info.bound.first = camera_position - Vector3(half,half,half);
		update_info.bound.second = camera_position + Vector3(half,half,half);
		update_info.update_id = update_id;
		update_info.lod = i;
		root.update_lod(update_info, update_change_info);
		last_bound = update_info.bound;
	}

	// This is the last LOD which for everyother point beyond lodsetting defention
	// in case include_root_bound is true we go up to end of our boundary
	// in case include_root_bound is false we can only check last update boundary for (last_lod - 1)
	Pair<Vector3,Vector3> final_boundary;
	int8_t final_lod = lod_setting.size();
	if(include_root_bound){
		final_boundary.first = root.start;
		final_boundary.second = root.end;
	} else {
		final_boundary = last_update_boundary;
	}
	OctUpdateInfo update_info;
	update_info.lod = final_lod;
	update_info.update_id = update_id;
	update_info.bound = final_boundary;
	update_info.exclude_bound = last_bound;

	root.update_lod(update_info, update_change_info);
	last_update_boundary = last_bound;
	update_lod_include_root_bound = false;
}

void MOctTree::clear_update_change_info(){
	for(HashMap<uint16_t,Vector<PointUpdate>>::Iterator it=update_change_info.begin();it != update_change_info.end(); ++it){
		it->value.clear();
	}
}


int MOctTree::get_points_count(){
	int count = 0;
	root.get_points_count(count);
	return count;
}

int MOctTree::get_oct_id_points_count(int oct_id){
	ERR_FAIL_COND_V(!oct_ids.has(oct_id),0);
	int count = 0;
	root.get_oct_id_points_count(oct_id,count);
	return count;
}


void MOctTree::set_lod_setting(const PackedFloat32Array _lod_setting){
	ERR_FAIL_COND(_lod_setting.size() > 255);
	ERR_FAIL_COND(_lod_setting.size() < 1);
	ERR_FAIL_COND(_lod_setting[0] < 0.01);
	for(int i=1; i < _lod_setting.size(); i++){
		ERR_FAIL_COND(_lod_setting[i] <= _lod_setting[i-1]);
	}
	update_lod_include_root_bound = true;
	lod_setting = _lod_setting;
}

void MOctTree::set_custom_capacity(int input){
	input = CLAMP(input, 0 , MAX_CAPACITY);
	custom_capacity = input;
}

void MOctTree::thread_update(void* instance){
	MOctTree* _oct = static_cast<MOctTree*>(instance);
	_oct->update_lod(_oct->update_lod_include_root_bound);
}

void MOctTree::set_is_octmesh_updater(bool input){
	is_octmesh_updater = input;
	if(input){
		bool is_octtree_changed = MOctMesh::set_octtree(this);
	} else {
		MOctMesh::remove_octtree(this);
	}

}
bool MOctTree::get_is_octmesh_updater(){
	return is_octmesh_updater;
}

bool MOctTree::is_valid_octmesh_updater(){
	return is_octmesh_updater && MOctMesh::is_my_octtree(this);
}

void MOctTree::process_tick(){
	if(is_updating){
		if(WorkerThreadPool::get_singleton()->is_task_completed(tid)){
			is_updating = false;
			WorkerThreadPool::get_singleton()->wait_for_task_completion(tid);
			is_point_process_wait = true;
			waiting_oct_ids = oct_ids;
			send_update_signal();
		}
		return;
	} else {
		if(is_valid_octmesh_updater()){
			MOctMesh::update_tick();
		}
	}
	if(is_first_update){
		if(is_valid_octmesh_updater()){
			MOctMesh::insert_points();
		}
		update_camera_position();
		is_first_update = false;
		update_lod(true);
		is_updating = false;
		is_point_process_wait = true;
		waiting_oct_ids = oct_ids;
		send_update_signal();
	}
}

void MOctTree::_notification(int p_what){
	switch (p_what)
	{
	case NOTIFICATION_PROCESS:
		process_tick();
		break;
	case NOTIFICATION_READY:
		ERR_BREAK_MSG(!is_inside_tree(),"MoctTree Condition ready with with not inside tree");
		update_scenario();
		is_ready = true;
		if(!get_parent()->is_class("Window")){
			ERR_FAIL_EDMSG("MOctTree should be added as singlton with active tool mode! Create a gdscript which inherit from MOctTree with active tool mode and add that as singlton in project setting");
		}
		if(!MTool::is_editor_plugin_active() && Engine::get_singleton()->is_editor_hint()){
			ERR_FAIL_EDMSG("Make sure to activate MTerrain plugin otherwise gizmo selction will not work!");
		}
		break;
	case NOTIFICATION_EXIT_TREE:
		disable_octtree = true;
		if(is_updating){
			is_updating = false;
			WorkerThreadPool::get_singleton()->wait_for_task_completion(tid);
		}
		break;
	}
}

void MOctTree::point_process_finished(int oct_id){
	ERR_FAIL_COND(!waiting_oct_ids.has(oct_id));
	ERR_FAIL_COND(!is_point_process_wait);
	waiting_oct_ids.erase(oct_id);
	check_point_process_finished();
}

void MOctTree::check_point_process_finished(){
	if(disable_octtree){
		return;
	}
	if(is_point_process_wait && waiting_oct_ids.size()==0){
		release_move_req_cache();
		update_camera_position();
		is_point_process_wait = false;
		tid = WorkerThreadPool::get_singleton()->add_native_task(&MOctTree::thread_update,(void*)this,true);
		is_updating = true;
	}
}

void MOctTree::send_update_signal(){
	update_scenario();
	if(is_valid_octmesh_updater()){
		MOctMesh::octtree_update(&update_change_info[MOctMesh::get_oct_id()]);
	}
	emit_signal("update_finished");
}


Array MOctTree::get_point_update_dictionary_array(int oct_id){
	Array arr;
	uint16_t id = oct_id;
	ERR_FAIL_COND_V(!update_change_info.has(id), arr);
	//UtilityFunctions::print("OCT SIZE ",update_change_info.get(id).size(), " FOR OCT ID ",oct_id);
	for(int i=0; i < update_change_info[id].size(); i++){
		PointUpdate p = update_change_info[id].get(i);
		Dictionary dic;
		dic["lod"] = (int)p.lod;
		dic["last_lod"] = (int)p.last_lod;
		dic["id"] = (int)p.id;
		arr.push_back(dic);
	}
	return arr;
}


void MOctTree::update_scenario(){
	scenario = get_world_3d()->get_scenario();
}

RID MOctTree::get_scenario(){
	return scenario;
}

