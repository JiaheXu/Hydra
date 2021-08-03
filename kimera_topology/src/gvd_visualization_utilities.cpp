#include "kimera_topology/gvd_visualization_utilities.h"

#include <kimera_dsg_visualizer/colormap_utils.h>
#include <tf2_eigen/tf2_eigen.h>

#include <random>

using visualization_msgs::Marker;

namespace kimera {
namespace topology {

double computeRatio(double min, double max, double value) {
  double ratio = (value - min) / (max - min);
  ratio = !std::isfinite(ratio) ? 0.0 : ratio;
  return ratio;
}

double getRatioFromDistance(const GvdVisualizerConfig& config, const GvdVoxel& voxel) {
  return computeRatio(config.gvd_min_distance, config.gvd_max_distance, voxel.distance);
}

double getRatioFromBasisPoints(const GvdVisualizerConfig& config,
                               const GvdVoxel& voxel) {
  return computeRatio(static_cast<double>(config.min_num_basis),
                      static_cast<double>(config.max_num_basis),
                      static_cast<double>(voxel.num_extra_basis));
}

double getRatio(const GvdVisualizerConfig& config, const GvdVoxel& voxel) {
  switch (static_cast<GvdVisualizationMode>(config.gvd_mode)) {
    case GvdVisualizationMode::BASIS_POINTS:
      return getRatioFromBasisPoints(config, voxel);
    case GvdVisualizationMode::DISTANCE:
    case GvdVisualizationMode::DEFAULT:
    default:
      return getRatioFromDistance(config, voxel);
  }
  return 0.0;
}

Marker makeGvdMarker(const GvdVisualizerConfig& config,
                     const ColormapConfig& colors,
                     const Layer<GvdVoxel>& layer) {
  BlockIndexList blocks;
  layer.getAllAllocatedBlocks(&blocks);

  Marker marker;
  marker.type = Marker::CUBE_LIST;
  marker.action = Marker::ADD;
  marker.id = 0;
  marker.ns = "gvd_markers";

  Eigen::Vector3d identity_pos = Eigen::Vector3d::Zero();
  tf2::convert(identity_pos, marker.pose.position);
  tf2::convert(Eigen::Quaterniond::Identity(), marker.pose.orientation);

  marker.scale.x = layer.voxel_size();
  marker.scale.y = layer.voxel_size();
  marker.scale.z = layer.voxel_size();

  for (const auto& idx : blocks) {
    const auto& block = layer.getBlockByIndex(idx);
    for (size_t i = 0; i < block.num_voxels(); ++i) {
      const auto& voxel = block.getVoxelByLinearIndex(i);
      if (!voxel.observed || voxel.num_extra_basis < config.basis_threshold) {
        continue;
      }

      Eigen::Vector3d voxel_pos =
          block.computeCoordinatesFromLinearIndex(i).cast<double>();
      geometry_msgs::Point marker_pos;
      tf2::convert(voxel_pos, marker_pos);
      marker.points.push_back(marker_pos);

      double ratio = getRatio(config, voxel);
      NodeColor color = dsg_utils::interpolateColorMap(colors, ratio);

      std_msgs::ColorRGBA color_msg = dsg_utils::makeColorMsg(color, config.gvd_alpha);
      marker.colors.push_back(color_msg);
    }
  }

  return marker;
}

Marker makeEsdfMarker(const GvdVisualizerConfig& config,
                      const ColormapConfig& colors,
                      const Layer<GvdVoxel>& layer) {
  BlockIndexList blocks;
  layer.getAllAllocatedBlocks(&blocks);

  Marker marker;
  marker.type = Marker::CUBE_LIST;
  marker.action = Marker::ADD;
  marker.id = 0;
  marker.ns = "esdf_slice_markers";

  Eigen::Vector3d identity_pos = Eigen::Vector3d::Zero();
  tf2::convert(identity_pos, marker.pose.position);
  tf2::convert(Eigen::Quaterniond::Identity(), marker.pose.orientation);

  marker.scale.x = layer.voxel_size();
  marker.scale.y = layer.voxel_size();
  marker.scale.z = layer.voxel_size();

  const FloatingPoint voxel_size = layer.voxel_size();
  const FloatingPoint half_voxel_size = voxel_size / 2.0;
  // rounds down and points the slice at the middle of the nearest voxel boundary
  const FloatingPoint slice_height =
      std::floor(config.slice_height / voxel_size) * voxel_size + half_voxel_size;

  for (const auto& idx : blocks) {
    const auto& block = layer.getBlockByIndex(idx);
    for (size_t i = 0; i < block.num_voxels(); ++i) {
      const auto& voxel = block.getVoxelByLinearIndex(i);
      if (!voxel.observed) {
        continue;
      }

      Eigen::Vector3d voxel_pos =
          block.computeCoordinatesFromLinearIndex(i).cast<double>();
      if (voxel_pos(2) < slice_height - half_voxel_size ||
          voxel_pos(2) > slice_height + half_voxel_size) {
        continue;
      }

      geometry_msgs::Point marker_pos;
      tf2::convert(voxel_pos, marker_pos);
      marker.points.push_back(marker_pos);

      double ratio = computeRatio(
          config.esdf_min_distance, config.esdf_max_distance, voxel.distance);
      NodeColor color = dsg_utils::interpolateColorMap(colors, ratio);

      std_msgs::ColorRGBA color_msg = dsg_utils::makeColorMsg(color, config.esdf_alpha);
      marker.colors.push_back(color_msg);
    }
  }

  return marker;
}

Marker makeGvdEdgeMarker(const Layer<GvdVoxel>& layer,
                         const GraphExtractor::EdgeInfoMap& edge_info_map,
                         const GraphExtractor::NodeIdRootMap& id_root_index_map) {
  const double alpha = 0.8;
  Marker marker;
  marker.type = Marker::CUBE_LIST;
  marker.action = Marker::ADD;
  marker.id = 0;
  marker.ns = "gvd_edge_markers";

  Eigen::Vector3d identity_pos = Eigen::Vector3d::Zero();
  tf2::convert(identity_pos, marker.pose.position);
  tf2::convert(Eigen::Quaterniond::Identity(), marker.pose.orientation);

  marker.scale.x = layer.voxel_size();
  marker.scale.y = layer.voxel_size();
  marker.scale.z = layer.voxel_size();

  for (const auto& id_index_pair : id_root_index_map) {
    Eigen::Vector3d voxel_pos = getVoxelPosition(layer, id_index_pair.second);

    geometry_msgs::Point marker_pos;
    tf2::convert(voxel_pos, marker_pos);
    marker.points.push_back(marker_pos);
    marker.colors.push_back(dsg_utils::makeColorMsg(NodeColor::Zero(), alpha));
  }

  size_t next_color_id = 0;
  std::map<size_t, size_t> edge_color_map;
  for (const auto& id_edge_pair : edge_info_map) {
    std::set<size_t> neighbor_colors;
    for (auto other_edge : id_edge_pair.second.connections) {
      if (!edge_color_map.count(other_edge)) {
        continue;
      }

      neighbor_colors.insert(edge_color_map.at(other_edge));
    }

    bool found_color = false;
    for (size_t i = 0; i < next_color_id; ++i) {
      if (neighbor_colors.count(i)) {
        continue;
      }

      found_color = true;
      edge_color_map[id_edge_pair.first] = i;
      break;
    }

    if (found_color) {
      continue;
    }

    edge_color_map[id_edge_pair.first] = next_color_id;
    next_color_id++;
  }

  std::vector<double> hues;
  for (size_t i = 0; i < next_color_id; ++i) {
    hues.push_back(static_cast<double>(i) / static_cast<double>(next_color_id));
  }
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(hues.begin(), hues.end(), g);

  for (const auto& id_edge_pair : edge_info_map) {
    cv::Mat hls_value(1, 1, CV_32FC3);
    hls_value.at<float>(0) = hues.at(edge_color_map.at(id_edge_pair.first)) * 360.0;
    hls_value.at<float>(1) = 0.7;
    hls_value.at<float>(2) = 0.9;

    cv::Mat bgr;
    cv::cvtColor(hls_value, bgr, cv::COLOR_HLS2BGR);
    NodeColor color;
    color(0, 0) = static_cast<uint8_t>(255 * bgr.at<float>(2));
    color(1, 0) = static_cast<uint8_t>(255 * bgr.at<float>(1));
    color(2, 0) = static_cast<uint8_t>(255 * bgr.at<float>(0));

    for (const auto& index : id_edge_pair.second.indices) {
      Eigen::Vector3d voxel_pos = getVoxelPosition(layer, index);

      geometry_msgs::Point marker_pos;
      tf2::convert(voxel_pos, marker_pos);
      marker.points.push_back(marker_pos);

      // TODO(nathan) get color
      marker.colors.push_back(dsg_utils::makeColorMsg(color, alpha));
    }
  }

  return marker;
}

inline Eigen::Vector3d getOffset(double side_length,
                                 bool x_high,
                                 bool y_high,
                                 bool z_high) {
  Eigen::Vector3d offset;
  offset(0) = x_high ? side_length : 0.0;
  offset(1) = y_high ? side_length : 0.0;
  offset(2) = z_high ? side_length : 0.0;
  return offset;
}

geometry_msgs::Point getPointFromMatrix(const Eigen::MatrixXd& matrix, int col) {
  geometry_msgs::Point point;
  tf2::convert(matrix.block<3, 1>(0, col).eval(), point);
  return point;
}

void fillMarkerFromBlock(Marker& marker,
                         const Eigen::Vector3d& position,
                         double side_length) {
  Eigen::MatrixXd corners(3, 8);
  for (int c = 0; c < corners.cols(); ++c) {
    // x: lsb, y: second lsb, z: third lsb
    corners.block<3, 1>(0, c) =
        position +
        getOffset(side_length, ((c & 0x01) != 0), ((c & 0x02) != 0), ((c & 0x04) != 0));
  }

  for (int c = 0; c < corners.cols(); ++c) {
    // edges are 1-bit pertubations
    int x_neighbor = c | 0x01;
    int y_neighbor = c | 0x02;
    int z_neighbor = c | 0x04;
    if (c != x_neighbor) {
      marker.points.push_back(getPointFromMatrix(corners, c));
      marker.points.push_back(getPointFromMatrix(corners, x_neighbor));
    }
    if (c != y_neighbor) {
      marker.points.push_back(getPointFromMatrix(corners, c));
      marker.points.push_back(getPointFromMatrix(corners, y_neighbor));
    }
    if (c != z_neighbor) {
      marker.points.push_back(getPointFromMatrix(corners, c));
      marker.points.push_back(getPointFromMatrix(corners, z_neighbor));
    }
  }
}

template <typename LayerType>
Marker makeBlocksMarkerImpl(const LayerType& layer) {
  Marker marker;
  marker.type = Marker::LINE_LIST;
  marker.action = Marker::ADD;
  marker.id = 0;
  marker.color.r = 0.662;
  marker.color.g = 0.0313;
  marker.color.b = 0.7607;
  marker.color.a = 0.8;
  marker.scale.x = 0.01;
  marker.scale.y = 0.01;
  marker.scale.z = 0.01;

  Eigen::Vector3d identity_pos = Eigen::Vector3d::Zero();
  tf2::convert(identity_pos, marker.pose.position);
  tf2::convert(Eigen::Quaterniond::Identity(), marker.pose.orientation);

  BlockIndexList blocks;
  layer.getAllAllocatedBlocks(&blocks);

  for (const auto& idx : blocks) {
    const auto& block = layer.getBlockByIndex(idx);
    Eigen::Vector3f block_pos = block.origin();
    fillMarkerFromBlock(marker, block_pos.cast<double>(), block.block_size());
  }

  return marker;
}

Marker makeBlocksMarker(const Layer<TsdfVoxel>& layer) {
  return makeBlocksMarkerImpl(layer);
}

Marker makeBlocksMarker(const Layer<GvdVoxel>& layer) {
  return makeBlocksMarkerImpl(layer);
}

}  // namespace topology
}  // namespace kimera
