%% Setup our simulink objects and lcm monitor
warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
warning('off','Drake:RigidBodyManipulator:UnsupportedVelocityLimits')

doVisualization = false;
doPublish = true;
rbmoptions.floating = true;
if ~doVisualization
  rbmoptions.visual = false;
end
r = RigidBodyManipulator(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),struct('floating',true));
atlas = DRCAtlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),rbmoptions);

%%
lcm_mon = drillTaskLCMMonitor(atlas, true);

left_hand_axis = [0;sqrt(3)/2;-.5];
right_hand_axis = [0;-sqrt(3)/2;-.5];
left_hand_pt = [0;.1902;.015];
right_hand_pt = -[0;.1902;.015];
planner = ladderHandPlanner(r,atlas,left_hand_axis, left_hand_pt, right_hand_axis,right_hand_pt, ...
        doVisualization, doPublish);
      
%%
while true
  [ctrl_type, ctrl_data] = lcm_mon.getDrillControlMsg();

  switch ctrl_type
    case drc.drill_control_t.LADDER_STRAIGHTEN_LEFT
      ladder = lcm_mon.getLadderAffordance();
      if ~isempty(ladder)
        q0 = lcm_mon.getStateEstimate();
        [q_end, snopt_info, infeasible_constraint] = planner.straightenLeftHand(q0, ladder.forward);
      else
        send_status(4,0,0,'No ladder found, cannot straighten left hand');
      end
    case drc.drill_control_t.LADDER_STRAIGHTEN_RIGHT
      ladder = lcm_mon.getLadderAffordance();
      if ~isempty(ladder)
        q0 = lcm_mon.getStateEstimate();
        [q_end, snopt_info, infeasible_constraint] = planner.straightenRightHand(q0, ladder.forward);
      else
        send_status(4,0,0,'No ladder found, cannot straighten left hand');
      end
    case drc.drill_control_t.STRAIGHTEN_BACK_FIXED_HANDS
      if sizecheck(ctrl_data, [3 1])
        back_joints_to_zero = ctrl_data;
        q0 = lcm_mon.getStateEstimate();
        [xtraj, snopt_info, infeasible_constraint] = planner.straightenBackFixedHands(q0, back_joints_to_zero);
      else
        send_status(4,0,0,'Invalid size of control data. Expected 3x1');
      end
  end
end