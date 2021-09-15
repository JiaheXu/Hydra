#pragma once
#include <kimera_dsg_visualizer/dsg_visualizer_plugin.h>
#include <kimera_dsg_visualizer/visualizer_types.h>
#include <kimera_pgmo/DeformationGraph.h>
#include <visualization_msgs/Marker.h>

namespace kimera {

struct PMGraphPluginConfig {
  explicit PMGraphPluginConfig(const ros::NodeHandle& nh);

  std::string frame = "world";
  double mesh_edge_scale = 0.005;
  double mesh_edge_alpha = 0.8;
  double mesh_marker_scale = 0.1;
  double mesh_marker_alpha = 0.8;
  NodeColor leaf_color;
  NodeColor interior_color;
  LayerConfig layer_config;
};

class MeshPlaceConnectionsPlugin : public DsgVisualizerPlugin {
 public:
  MeshPlaceConnectionsPlugin(const ros::NodeHandle& nh,
                             const std::string& name,
                             char vertex_prefix,
                             const kimera_pgmo::DeformationGraphPtr& graph);

  virtual ~MeshPlaceConnectionsPlugin() = default;

  void draw(const DynamicSceneGraph& graph) override;

 protected:
  ros::Publisher marker_pub_;
  char vertex_prefix_;
  kimera_pgmo::DeformationGraphPtr deformation_graph_;
  PMGraphPluginConfig config_;
};

}  // namespace kimera
