#pragma once
#include "kimera_dsg_builder/incremental_types.h"

#include <unordered_set>

namespace kimera {
namespace incremental {

using ActiveNodeSet = std::unordered_set<NodeId>;
using Components = std::vector<std::vector<NodeId>>;
using RoomMap = std::map<NodeId, std::set<NodeId>>;

struct ClusterResults {
  std::map<size_t, std::unordered_set<NodeId>> clusters;
  std::map<NodeId, size_t> labels;
  size_t total_iters;
  bool valid = false;
};

void updateRoomCentroid(const SceneGraph& graph, NodeId room_id);

std::optional<size_t> getLongestSequence(const std::vector<size_t>& values);

std::optional<size_t> getMedianComponentSize(const std::vector<size_t>& values);

IsolatedSceneGraphLayer::Ptr getActiveSubgraph(const SceneGraph& graph,
                                               LayerId layer_id,
                                               const ActiveNodeSet& active_nodes);

ClusterResults clusterGraph(const SceneGraphLayer& layer,
                            const Components& components,
                            size_t max_iters = 5,
                            bool use_sparse = false,
                            double sparse_tolerance = 1.0e-5);

ClusterResults clusterGraphByModularity(const SceneGraphLayer& layer,
                                        const Components& components,
                                        size_t max_iters = 5,
                                        double gamma = 1.0);

class RoomFinder {
 public:
  struct Config {
    double min_dilation_m = 0.1;
    double max_dilation_m = 0.7;
    size_t num_steps = 5;
    size_t min_component_size = 15;
    char room_prefix = 'R';
    size_t max_kmeans_iters = 5;
    double room_vote_min_overlap = 0.3;
    size_t min_room_size = 10;
    bool use_sparse_eigen_decomp = true;
    double sparse_decomp_tolerance = 1.0e-5;
    double max_modularity_iters = 5;
    double modularity_gamma = 1.0;
    enum class ClusterMode {
      SPECTRAL,
      MODULARITY,
      NONE
    } clustering_mode = ClusterMode::MODULARITY;
  };

  explicit RoomFinder(const Config& config);

  virtual ~RoomFinder() = default;

  void findRooms(SharedDsgInfo& dsg, const ActiveNodeSet& active_nodes);

 protected:
  std::vector<double> getThresholds() const;

  Components getBestComponents(const SceneGraphLayer& places,
                               const std::vector<double>& thresholds) const;

  void updateRoomsFromClusters(SharedDsgInfo& dsg,
                               ClusterResults& cluster_results,
                               const RoomMap& previous_rooms,
                               const std::map<size_t, NodeId>& rooms_to_clusters);

 protected:
  Config config_;
  NodeSymbol next_room_id_;
};

}  // namespace incremental
}  // namespace kimera