add_definitions(
    -ggdb3 
    -std=gnu99
    )

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(local_map)


set(PCL_LIBRARIES  pcl_search-1.7 pcl_filters-1.7 pcl_features-1.7
	pcl_segmentation-1.7 pcl_sample_consensus-1.7
	pcl_io-1.7)

add_executable(drc-local-map-static-lidar local_map.cpp  )
pods_use_pkg_config_packages(drc-local-map-static-lidar
    lcmtypes_velodyne
    path-util
    bot2-param-client
        pointcloud_lcm        pointcloud_vis        pointcloud_math
	lcmtypes_drc_lcmtypes  lcmtypes_visualization
       ${PCL_LIBRARIES})

pods_install_executables(drc-local-map-static-lidar )
