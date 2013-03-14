#!/bin/bash
cat world_segments/common_top.world world_segments/objects_driving.world world_segments/objects_walking.world  world_segments/objects_manipulation.world  world_segments/common_bottom.world > test_combined.world

# control - nothing in world:
cat world_segments/common_top.world world_segments/common_bottom.world > test_empty.world
# driving:
cat world_segments/common_top.world world_segments/objects_driving.world world_segments/common_bottom.world > test_driving.world
# walking:
cat world_segments/common_top.world world_segments/objects_walking.world world_segments/common_bottom.world > test_walking.world
# manipulation:
cat world_segments/common_top.world world_segments/objects_manipulation.world world_segments/common_bottom.world > test_manipulation.world

# for sisir:
cat world_segments/common_top.world world_segments/objects_manipulation.world world_segments/floatinghands_bottom.world > test_manipulation_floating_hands.world

