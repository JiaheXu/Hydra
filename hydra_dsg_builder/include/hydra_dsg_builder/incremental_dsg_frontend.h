#pragma once
#include "hydra_dsg_builder/frontend_config.h"
#include "hydra_dsg_builder/incremental_mesh_segmenter.h"
#include "hydra_dsg_builder/incremental_types.h"

#include <geometry_msgs/TransformStamped.h>
#include <ros/callback_queue.h>
#include <ros/ros.h>
#include <tf2_ros/transform_listener.h>

#include <hydra_msgs/ActiveLayer.h>
#include <hydra_msgs/ActiveMesh.h>
#include <kimera_dsg/node_symbol.h>
#include <kimera_dsg/scene_graph_logger.h>
#include <kimera_pgmo/MeshFrontend.h>
#include <hydra_topology/nearest_neighbor_utilities.h>
#include <pose_graph_tools/PoseGraph.h>

#include <memory>
#include <mutex>

namespace hydra {
namespace incremental {

using PlacesLayerMsg = hydra_msgs::ActiveLayer;
using topology::NearestNodeFinder;

struct PlacesQueueState {
  bool empty = true;
  uint64_t timestamp_ns = 0;
};

class DsgFrontend {
 public:
  DsgFrontend(const ros::NodeHandle& nh, const SharedDsgInfo::Ptr& dsg);

  virtual ~DsgFrontend();

  void start();

  void stop();

  pcl::PolygonMesh getFrontendMesh() const {
    pcl::PolygonMesh mesh;
    mesh.polygons = mesh_frontend_.getFullMeshFaces();

    const auto vertices = mesh_frontend_.getFullMeshVertices();
    pcl::toPCLPointCloud2(*vertices, mesh.cloud);
    return mesh;
  }

  std::vector<ros::Time> getFrontendMeshStamps() const {
    return mesh_frontend_.getFullMeshTimes();
  }

 private:
  void handleActivePlaces(const PlacesLayerMsg::ConstPtr& msg);

  void handleLatestMesh(const hydra_msgs::ActiveMesh::ConstPtr& msg);

  void handleLatestPoseGraph(const pose_graph_tools::PoseGraph::ConstPtr& msg);

  void startMeshFrontend();

  void runMeshFrontend();

  void startPlaces();

  void runPlaces();

  PlacesQueueState getPlacesQueueState();

  void processLatestPlacesMsg(const PlacesLayerMsg::ConstPtr& msg);

  void addPlaceObjectEdges(NodeIdSet* extra_objects_to_check = nullptr);

  void updatePlaceMeshMapping();

  void addAgentPlaceEdges();

  std::optional<Eigen::Vector3d> getLatestPose();

 private:
  ros::NodeHandle nh_;
  std::atomic<bool> should_shutdown_{false};

  DsgFrontendConfig config_;

  SharedDsgInfo::Ptr dsg_;
  kimera_pgmo::MeshFrontend mesh_frontend_;
  std::unique_ptr<MeshSegmenter> segmenter_;

  std::mutex mesh_frontend_mutex_;
  std::atomic<uint64_t> last_mesh_timestamp_;
  std::queue<hydra_msgs::ActiveMesh::ConstPtr> mesh_queue_;

  std::mutex places_queue_mutex_;
  std::atomic<uint64_t> last_places_timestamp_;
  std::queue<PlacesLayerMsg::ConstPtr> places_queue_;

  ros::Subscriber mesh_sub_;
  std::unique_ptr<ros::CallbackQueue> mesh_frontend_ros_queue_;
  std::unique_ptr<std::thread> mesh_frontend_thread_;
  tf2_ros::Buffer tf_buffer_;
  std::unique_ptr<tf2_ros::TransformListener> tf_listener_;

  ros::Subscriber active_places_sub_;
  std::unique_ptr<NearestNodeFinder> places_nn_finder_;
  std::unique_ptr<std::thread> places_thread_;
  NodeIdSet unlabeled_place_nodes_;
  NodeIdSet previous_active_places_;

  std::set<NodeId> deleted_agent_edge_indices_;
  std::map<LayerPrefix, size_t> last_agent_edge_index_;

  char robot_prefix_;
  ros::Subscriber pose_graph_sub_;

  SceneGraphLogger frontend_graph_logger_;
};

}  // namespace incremental
}  // namespace hydra